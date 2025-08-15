#include "../include/market_data_simulator.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace hft {

MarketDataSimulator::MarketDataSimulator() 
    : gen_(rd_()), 
      price_change_dist_(-0.001, 0.001),
      volatility_dist_(0.8, 1.2),
      size_dist_(100, 10000),
      running_(false),
      ticks_generated_(0),
      bytes_generated_(0) {
}

MarketDataSimulator::~MarketDataSimulator() {
    stop();
}

void MarketDataSimulator::add_symbol(const std::string& symbol, Price initial_price) {
    symbols_.push_back(symbol);
    
    Tick initial_tick;
    initial_tick.symbol = symbol;
    initial_tick.bid_price = initial_price * 0.999;
    initial_tick.ask_price = initial_price * 1.001;
    initial_tick.bid_size = size_dist_(gen_);
    initial_tick.ask_size = size_dist_(gen_);
    initial_tick.last_price = initial_price;
    initial_tick.last_size = size_dist_(gen_);
    initial_tick.timestamp = std::chrono::high_resolution_clock::now();
    
    current_ticks_[symbol] = initial_tick;
}

void MarketDataSimulator::set_volatility(double volatility) {
    volatility_dist_ = std::uniform_real_distribution<>(1.0 - volatility, 1.0 + volatility);
}

void MarketDataSimulator::start() {
    if (running_.load()) return;
    
    running_.store(true);
    start_time_ = std::chrono::high_resolution_clock::now();
    generator_thread_ = std::make_unique<std::thread>(&MarketDataSimulator::generate_market_data, this);
}

void MarketDataSimulator::stop() {
    if (!running_.load()) return;
    
    running_.store(false);
    if (generator_thread_ && generator_thread_->joinable()) {
        generator_thread_->join();
    }
}

bool MarketDataSimulator::get_next_tick(Tick& tick) {
    return tick_queue_.pop(tick);
}

std::vector<Tick> MarketDataSimulator::get_current_snapshot() {
    std::vector<Tick> snapshot;
    snapshot.reserve(current_ticks_.size());
    
    for (const auto& pair : current_ticks_) {
        snapshot.push_back(pair.second);
    }
    
    return snapshot;
}

ThroughputStats MarketDataSimulator::get_throughput_stats() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    
    ThroughputStats stats;
    stats.total_messages = ticks_generated_.load();
    stats.total_bytes = bytes_generated_.load();
    
    if (duration > 0) {
        stats.messages_per_second = stats.total_messages / duration;
        stats.bytes_per_second = stats.total_bytes / duration;
    } else {
        stats.messages_per_second = 0;
        stats.bytes_per_second = 0;
    }
    
    return stats;
}

void MarketDataSimulator::generate_market_data() {
    auto next_tick_time = std::chrono::high_resolution_clock::now();
    const auto tick_interval = std::chrono::microseconds(1000); // 1ms = 1000 ticks/second
    
    while (running_.load()) {
        for (const auto& symbol : symbols_) {
            if (!running_.load()) break;
            
            Tick tick = generate_tick(symbol);
            
            if (tick_queue_.push(std::move(tick))) {
                ticks_generated_.fetch_add(1);
                bytes_generated_.fetch_add(sizeof(Tick));
            }
        }
        
        next_tick_time += tick_interval;
        std::this_thread::sleep_until(next_tick_time);
    }
}

Tick MarketDataSimulator::generate_tick(const std::string& symbol) {
    auto& current_tick = current_ticks_[symbol];
    
    // Update prices with some randomness
    update_price(symbol, 0.001);
    
    Tick new_tick = current_tick;
    new_tick.timestamp = std::chrono::high_resolution_clock::now();
    
    // Occasionally update sizes
    if (gen_() % 10 == 0) {
        new_tick.bid_size = size_dist_(gen_);
        new_tick.ask_size = size_dist_(gen_);
    }
    
    // Occasionally generate a trade
    if (gen_() % 5 == 0) {
        // Trade occurs at bid or ask
        if (gen_() % 2 == 0) {
            new_tick.last_price = new_tick.bid_price;
        } else {
            new_tick.last_price = new_tick.ask_price;
        }
        new_tick.last_size = size_dist_(gen_) / 10; // Smaller trade sizes
    }
    
    current_ticks_[symbol] = new_tick;
    return new_tick;
}

void MarketDataSimulator::update_price(const std::string& symbol, double volatility) {
    auto& tick = current_ticks_[symbol];
    
    double change_factor = volatility_dist_(gen_);
    double price_change = price_change_dist_(gen_) * volatility;
    
    // Update mid price
    double mid_price = (tick.bid_price + tick.ask_price) / 2.0;
    mid_price *= (1.0 + price_change);
    
    // Update bid/ask with spread
    double spread = mid_price * 0.001; // 0.1% spread
    tick.bid_price = mid_price - spread / 2.0;
    tick.ask_price = mid_price + spread / 2.0;
    
    // Ensure prices are positive
    tick.bid_price = std::max(tick.bid_price, 0.01);
    tick.ask_price = std::max(tick.ask_price, tick.bid_price + 0.01);
}

// SimulatedMarketDataFeed implementation
SimulatedMarketDataFeed::SimulatedMarketDataFeed() 
    : simulator_(std::make_unique<MarketDataSimulator>()) {
}

SimulatedMarketDataFeed::~SimulatedMarketDataFeed() {
    stop_simulation();
}

void SimulatedMarketDataFeed::subscribe(const std::string& symbol) {
    auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
    if (it == subscribed_symbols_.end()) {
        subscribed_symbols_.push_back(symbol);
    }
}

void SimulatedMarketDataFeed::unsubscribe(const std::string& symbol) {
    auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), symbol);
    if (it != subscribed_symbols_.end()) {
        subscribed_symbols_.erase(it);
    }
}

bool SimulatedMarketDataFeed::get_tick(Tick& tick) {
    if (!simulator_->get_next_tick(tick)) {
        return false;
    }
    
    // Only return ticks for subscribed symbols
    auto it = std::find(subscribed_symbols_.begin(), subscribed_symbols_.end(), tick.symbol);
    return it != subscribed_symbols_.end();
}

std::vector<std::string> SimulatedMarketDataFeed::get_subscribed_symbols() const {
    return subscribed_symbols_;
}

void SimulatedMarketDataFeed::set_initial_price(const std::string& symbol, Price price) {
    simulator_->add_symbol(symbol, price);
}

void SimulatedMarketDataFeed::set_volatility(double volatility) {
    simulator_->set_volatility(volatility);
}

void SimulatedMarketDataFeed::start_simulation() {
    simulator_->start();
}

void SimulatedMarketDataFeed::stop_simulation() {
    simulator_->stop();
}

} // namespace hft