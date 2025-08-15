#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include "../cpp/include/market_data_simulator.h"
#include "../cpp/include/fix_protocol.h"
#include "../cpp/include/thread_pool.h"
#include "../cpp/include/lockfree_queue.h"

// Simple test framework
#define TEST(name) \
    std::cout << "Running test: " << #name << "..." << std::endl; \
    test_##name(); \
    std::cout << "✓ " << #name << " passed" << std::endl;

void test_lockfree_queue() {
    hft::SPSCQueue<int, 1024> queue;
    
    // Test basic operations
    assert(queue.empty());
    assert(queue.size() == 0);
    
    // Test push and pop
    assert(queue.push(42));
    assert(!queue.empty());
    assert(queue.size() == 1);
    
    int value;
    assert(queue.pop(value));
    assert(value == 42);
    assert(queue.empty());
    
    // Test multiple items
    for (int i = 0; i < 100; ++i) {
        assert(queue.push(i));
    }
    
    assert(queue.size() == 100);
    
    for (int i = 0; i < 100; ++i) {
        assert(queue.pop(value));
        assert(value == i);
    }
    
    assert(queue.empty());
}

void test_fix_message() {
    hft::FixMessage msg;
    
    // Test field operations
    msg.set_field(35, "D"); // MSG_TYPE = NEW_ORDER_SINGLE
    msg.set_field(55, "AAPL"); // SYMBOL
    msg.set_field(54, "1"); // SIDE = BUY
    msg.set_field(38, 100); // ORDER_QTY
    msg.set_field(44, 150.50); // PRICE
    
    assert(msg.get_field(35) == "D");
    assert(msg.get_field(55) == "AAPL");
    assert(msg.get_int_field(38) == 100);
    assert(msg.get_double_field(44) == 150.50);
    
    // Test message type
    assert(msg.get_message_type() == "D");
    
    // Test validation
    msg.set_field(34, 1); // MSG_SEQ_NUM
    assert(msg.is_valid());
    
    // Test serialization
    std::string fix_string = msg.to_string();
    assert(!fix_string.empty());
    
    // Test parsing
    hft::FixMessage parsed_msg(fix_string);
    assert(parsed_msg.get_field(55) == "AAPL");
    assert(parsed_msg.get_int_field(38) == 100);
}

void test_market_data_simulator() {
    hft::MarketDataSimulator simulator;
    
    // Add symbols
    simulator.add_symbol("TEST1", 100.0);
    simulator.add_symbol("TEST2", 200.0);
    
    // Start simulation
    simulator.start();
    assert(simulator.is_running());
    
    // Get some ticks
    int tick_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (tick_count < 10 && 
           std::chrono::high_resolution_clock::now() - start_time < std::chrono::seconds(2)) {
        hft::Tick tick;
        if (simulator.get_next_tick(tick)) {
            assert(!tick.symbol.empty());
            assert(tick.bid_price > 0);
            assert(tick.ask_price > tick.bid_price);
            assert(tick.bid_size > 0);
            assert(tick.ask_size > 0);
            tick_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    assert(tick_count > 0);
    
    // Stop simulation
    simulator.stop();
    assert(!simulator.is_running());
    
    // Check stats
    auto stats = simulator.get_throughput_stats();
    assert(stats.total_messages > 0);
}

void test_thread_pool() {
    hft::ThreadPool pool(2);
    
    assert(pool.get_num_threads() == 2);
    assert(pool.is_running());
    
    // Test task submission
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 10; ++i) {
        pool.submit_detached([&counter]() {
            counter.fetch_add(1);
        });
    }
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(counter.load() == 10);
}

void test_performance_monitor() {
    hft::PerformanceMonitor monitor;
    monitor.start_monitoring();
    
    // Record some latencies
    monitor.record_latency(std::chrono::microseconds(10));
    monitor.record_latency(std::chrono::microseconds(20));
    monitor.record_latency(std::chrono::microseconds(15));
    
    // Record operations
    monitor.record_operation(64);
    monitor.record_operation(128);
    
    auto latency_stats = monitor.get_latency_stats();
    assert(latency_stats.total_messages == 3);
    assert(latency_stats.min_latency <= latency_stats.max_latency);
    
    auto throughput_stats = monitor.get_throughput_stats();
    assert(throughput_stats.total_messages == 2);
    assert(throughput_stats.total_bytes == 192);
}

void test_memory_pool() {
    hft::MemoryPool<int, 100> pool;
    
    assert(pool.capacity() == 100);
    assert(pool.allocated_count() == 0);
    
    // Allocate some objects
    std::vector<int*> ptrs;
    for (int i = 0; i < 10; ++i) {
        int* ptr = pool.allocate();
        assert(ptr != nullptr);
        *ptr = i;
        ptrs.push_back(ptr);
    }
    
    assert(pool.allocated_count() == 10);
    
    // Verify values
    for (size_t i = 0; i < ptrs.size(); ++i) {
        assert(*ptrs[i] == static_cast<int>(i));
    }
    
    // Deallocate
    for (auto ptr : ptrs) {
        pool.deallocate(ptr);
    }
    
    assert(pool.allocated_count() == 0);
}

void test_simulated_market_data_feed() {
    hft::SimulatedMarketDataFeed feed;
    
    // Set up symbols
    feed.set_initial_price("AAPL", 150.0);
    feed.set_initial_price("GOOGL", 2500.0);
    
    // Subscribe to symbols
    feed.subscribe("AAPL");
    feed.subscribe("GOOGL");
    
    auto symbols = feed.get_subscribed_symbols();
    assert(symbols.size() == 2);
    
    // Start simulation
    feed.start_simulation();
    
    // Get some ticks
    int tick_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (tick_count < 5 && 
           std::chrono::high_resolution_clock::now() - start_time < std::chrono::seconds(2)) {
        hft::Tick tick;
        if (feed.get_tick(tick)) {
            assert(tick.symbol == "AAPL" || tick.symbol == "GOOGL");
            tick_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    feed.stop_simulation();
    assert(tick_count > 0);
}

int main() {
    std::cout << "=== HFT Simulator Basic Functionality Tests ===" << std::endl;
    
    try {
        TEST(lockfree_queue);
        TEST(fix_message);
        TEST(market_data_simulator);
        TEST(thread_pool);
        TEST(performance_monitor);
        TEST(memory_pool);
        TEST(simulated_market_data_feed);
        
        std::cout << "\n✓ All tests passed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown error" << std::endl;
        return 1;
    }
    
    return 0;
}