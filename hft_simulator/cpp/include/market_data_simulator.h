#pragma once

#include "types.h"
#include "lockfree_queue.h"
#include <string>
#include <map>
#include <vector>
#include <random>
#include <atomic>
#include <thread>

namespace hft {

class MarketDataSimulator {
private:
    std::map<std::string, Tick> current_ticks_;
    std::vector<std::string> symbols_;
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<> price_change_dist_;
    std::uniform_real_distribution<> volatility_dist_;
    std::uniform_int_distribution<> size_dist_;
    
    // High-performance message queue
    SPSCQueue<Tick, 1000000> tick_queue_;
    
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> generator_thread_;
    
    // Performance tracking
    std::atomic<uint64_t> ticks_generated_;
    std::atomic<uint64_t> bytes_generated_;
    Timestamp start_time_;

public:
    MarketDataSimulator();
    ~MarketDataSimulator();

    // Configuration
    void add_symbol(const std::string& symbol, Price initial_price);
    void set_volatility(double volatility);
    void set_tick_frequency(int ticks_per_second);

    // Control
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Data access
    bool get_next_tick(Tick& tick);
    std::vector<Tick> get_current_snapshot();
    
    // Performance metrics
    ThroughputStats get_throughput_stats() const;
    uint64_t get_total_ticks() const { return ticks_generated_.load(); }

private:
    void generate_market_data();
    Tick generate_tick(const std::string& symbol);
    void update_price(const std::string& symbol, double volatility);
};

// Market data feed interface
class MarketDataFeed {
public:
    virtual ~MarketDataFeed() = default;
    
    virtual void subscribe(const std::string& symbol) = 0;
    virtual void unsubscribe(const std::string& symbol) = 0;
    virtual bool get_tick(Tick& tick) = 0;
    virtual std::vector<std::string> get_subscribed_symbols() const = 0;
};

// Simulated market data feed
class SimulatedMarketDataFeed : public MarketDataFeed {
private:
    std::unique_ptr<MarketDataSimulator> simulator_;
    std::vector<std::string> subscribed_symbols_;

public:
    SimulatedMarketDataFeed();
    ~SimulatedMarketDataFeed() override;

    void subscribe(const std::string& symbol) override;
    void unsubscribe(const std::string& symbol) override;
    bool get_tick(Tick& tick) override;
    std::vector<std::string> get_subscribed_symbols() const override;
    
    // Configuration methods
    void set_initial_price(const std::string& symbol, Price price);
    void set_volatility(double volatility);
    void start_simulation();
    void stop_simulation();
};

} // namespace hft