#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include "../cpp/include/market_data_simulator.h"
#include "../cpp/include/lockfree_queue.h"
#include "../cpp/include/thread_pool.h"

int main() {
    std::cout << "=== HFT Simulator Performance Tests ===" << std::endl;
    
    bool all_passed = true;
    
    try {
        std::cout << "\n1. Testing lock-free queue latency..." << std::endl;
        
        // Test lock-free queue latency
        hft::SPSCQueue<int, 8192> queue;
        const size_t num_messages = 1000000;
        
        std::vector<std::chrono::nanoseconds> latencies;
        latencies.reserve(num_messages);
        
        std::thread producer([&queue, &latencies, num_messages]() {
            for (size_t i = 0; i < num_messages; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                while (!queue.push(static_cast<int>(i))) {
                    std::this_thread::yield();
                }
                auto end = std::chrono::high_resolution_clock::now();
                latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
            }
        });
        
        std::thread consumer([&queue, num_messages]() {
            size_t consumed = 0;
            int value;
            while (consumed < num_messages) {
                if (queue.pop(value)) {
                    consumed++;
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        // Analyze latencies
        std::sort(latencies.begin(), latencies.end());
        
        auto min_latency = latencies.front();
        auto max_latency = latencies.back();
        auto median_latency = latencies[latencies.size() / 2];
        auto p99_latency = latencies[static_cast<size_t>(latencies.size() * 0.99)];
        
        std::cout << "Queue latency results:" << std::endl;
        std::cout << "  Min: " << min_latency.count() << " ns" << std::endl;
        std::cout << "  Median: " << median_latency.count() << " ns" << std::endl;
        std::cout << "  P99: " << p99_latency.count() << " ns" << std::endl;
        std::cout << "  Max: " << max_latency.count() << " ns" << std::endl;
        
        // Performance requirements (adjust based on your needs)
        if (p99_latency.count() > 10000) { // 10 microseconds
            std::cout << "❌ Queue latency P99 too high: " << p99_latency.count() << " ns" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Queue latency within acceptable range" << std::endl;
        }
        
        std::cout << "\n2. Testing market data throughput..." << std::endl;
        
        // Test market data throughput
        hft::MarketDataSimulator simulator;
        simulator.add_symbol("PERF_TEST", 100.0);
        simulator.start();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        const auto test_duration = std::chrono::seconds(5);
        
        size_t tick_count = 0;
        while (std::chrono::high_resolution_clock::now() - start_time < test_duration) {
            hft::Tick tick;
            if (simulator.get_next_tick(tick)) {
                tick_count++;
            }
        }
        
        simulator.stop();
        
        auto actual_duration = std::chrono::high_resolution_clock::now() - start_time;
        auto duration_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(actual_duration).count() / 1000.0;
        
        double throughput = tick_count / duration_seconds;
        
        std::cout << "Market data throughput: " << static_cast<size_t>(throughput) << " ticks/second" << std::endl;
        std::cout << "Total ticks: " << tick_count << " in " << duration_seconds << " seconds" << std::endl;
        
        // Performance requirement
        if (throughput < 10000) { // 10k ticks/second minimum
            std::cout << "❌ Market data throughput too low: " << static_cast<size_t>(throughput) << " ticks/second" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Market data throughput acceptable" << std::endl;
        }
        
        std::cout << "\n3. Testing thread pool scalability..." << std::endl;
        
        // Test thread pool with different thread counts
        std::vector<size_t> thread_counts = {1, 2, 4, 8};
        const size_t num_tasks = 100000;
        
        for (auto thread_count : thread_counts) {
            if (thread_count > std::thread::hardware_concurrency()) {
                continue; // Skip if more threads than available cores
            }
            
            hft::ThreadPool pool(thread_count);
            std::atomic<size_t> completed{0};
            
            auto start = std::chrono::high_resolution_clock::now();
            
            for (size_t i = 0; i < num_tasks; ++i) {
                pool.submit_detached([&completed]() {
                    // Simulate light CPU work
                    volatile int sum = 0;
                    for (int j = 0; j < 100; ++j) {
                        sum += j;
                    }
                    completed.fetch_add(1);
                });
            }
            
            // Wait for completion
            while (completed.load() < num_tasks) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            double task_throughput = (num_tasks * 1000.0) / duration_ms;
            
            std::cout << "Thread pool (" << thread_count << " threads): " 
                     << static_cast<size_t>(task_throughput) << " tasks/second" << std::endl;
        }
        
        std::cout << "\n4. Testing memory allocation performance..." << std::endl;
        
        // Test memory pool vs standard allocation
        const size_t num_allocations = 1000000;
        
        // Memory pool test
        hft::MemoryPool<int, 10000> pool;
        std::vector<int*> pool_ptrs;
        pool_ptrs.reserve(1000);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_allocations; ++i) {
            int* ptr = pool.allocate();
            if (ptr) {
                pool_ptrs.push_back(ptr);
                if (pool_ptrs.size() >= 1000) {
                    for (auto p : pool_ptrs) {
                        pool.deallocate(p);
                    }
                    pool_ptrs.clear();
                }
            }
        }
        
        for (auto ptr : pool_ptrs) {
            pool.deallocate(ptr);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto pool_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Standard allocation test
        std::vector<int*> std_ptrs;
        std_ptrs.reserve(1000);
        
        start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_allocations; ++i) {
            int* ptr = new int;
            std_ptrs.push_back(ptr);
            if (std_ptrs.size() >= 1000) {
                for (auto p : std_ptrs) {
                    delete p;
                }
                std_ptrs.clear();
            }
        }
        
        for (auto ptr : std_ptrs) {
            delete ptr;
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto std_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double pool_throughput = (num_allocations * 1000000.0) / pool_duration.count();
        double std_throughput = (num_allocations * 1000000.0) / std_duration.count();
        
        std::cout << "Memory pool: " << static_cast<size_t>(pool_throughput) << " allocs/second" << std::endl;
        std::cout << "Standard allocation: " << static_cast<size_t>(std_throughput) << " allocs/second" << std::endl;
        std::cout << "Speedup: " << (pool_throughput / std_throughput) << "x" << std::endl;
        
        if (pool_throughput <= std_throughput) {
            std::cout << "⚠️  Memory pool not faster than standard allocation" << std::endl;
        } else {
            std::cout << "✓ Memory pool provides performance benefit" << std::endl;
        }
        
        std::cout << "\n5. Overall system latency test..." << std::endl;
        
        // End-to-end latency test
        hft::SimulatedMarketDataFeed feed;
        feed.set_initial_price("LATENCY_TEST", 100.0);
        feed.subscribe("LATENCY_TEST");
        feed.start_simulation();
        
        std::vector<std::chrono::nanoseconds> e2e_latencies;
        e2e_latencies.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            hft::Tick tick;
            while (!feed.get_tick(tick)) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
            
            // Simulate processing
            volatile double price = tick.bid_price + tick.ask_price;
            (void)price; // Suppress unused variable warning
            
            auto end = std::chrono::high_resolution_clock::now();
            e2e_latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
        }
        
        feed.stop_simulation();
        
        std::sort(e2e_latencies.begin(), e2e_latencies.end());
        
        auto e2e_median = e2e_latencies[e2e_latencies.size() / 2];
        auto e2e_p99 = e2e_latencies[static_cast<size_t>(e2e_latencies.size() * 0.99)];
        
        std::cout << "End-to-end latency:" << std::endl;
        std::cout << "  Median: " << e2e_median.count() << " ns" << std::endl;
        std::cout << "  P99: " << e2e_p99.count() << " ns" << std::endl;
        
        // Microsecond-level requirement
        if (e2e_p99.count() > 100000) { // 100 microseconds
            std::cout << "❌ End-to-end latency too high: " << e2e_p99.count() << " ns" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ End-to-end latency within microsecond range" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Performance test failed with error: " << e.what() << std::endl;
        return 1;
    }
    
    if (all_passed) {
        std::cout << "\n✓ All performance tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some performance tests failed!" << std::endl;
        return 1;
    }
}