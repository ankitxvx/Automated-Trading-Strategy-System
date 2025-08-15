#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include "../cpp/include/lockfree_queue.h"
#include "../cpp/include/thread_pool.h"
#include "../cpp/include/market_data_simulator.h"

int main() {
    std::cout << "=== HFT Simulator Performance Benchmark ===" << std::endl;
    
    try {
        std::cout << "\n1. Lock-free Queue Performance Test" << std::endl;
        
        // Test lock-free queue performance
        const size_t num_messages = 1000000;
        hft::SPSCQueue<int, 1024> queue;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Producer thread
        std::thread producer([&queue, num_messages]() {
            for (size_t i = 0; i < num_messages; ++i) {
                while (!queue.push(static_cast<int>(i))) {
                    std::this_thread::yield();
                }
            }
        });
        
        // Consumer thread
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
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Processed " << num_messages << " messages in " 
                 << duration.count() << " microseconds" << std::endl;
        std::cout << "Throughput: " << (num_messages * 1000000.0 / duration.count()) 
                 << " messages/second" << std::endl;
        
        std::cout << "\n2. Thread Pool Performance Test" << std::endl;
        
        // Test thread pool performance
        const size_t num_tasks = 100000;
        hft::ThreadPool pool(std::thread::hardware_concurrency());
        
        std::atomic<size_t> completed_tasks{0};
        
        start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_tasks; ++i) {
            pool.submit_detached([&completed_tasks]() {
                // Simulate some work
                volatile int x = 0;
                for (int j = 0; j < 100; ++j) {
                    x += j;
                }
                completed_tasks.fetch_add(1);
            });
        }
        
        // Wait for all tasks to complete
        while (completed_tasks.load() < num_tasks) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Executed " << num_tasks << " tasks in " 
                 << duration.count() << " microseconds" << std::endl;
        std::cout << "Task throughput: " << (num_tasks * 1000000.0 / duration.count()) 
                 << " tasks/second" << std::endl;
        
        std::cout << "\n3. Market Data Latency Test" << std::endl;
        
        // Test market data latency
        hft::PerformanceMonitor monitor;
        monitor.start_monitoring();
        
        hft::MarketDataSimulator simulator;
        simulator.add_symbol("PERF_TEST", 100.0);
        simulator.start();
        
        const size_t num_ticks = 10000;
        std::vector<std::chrono::nanoseconds> latencies;
        latencies.reserve(num_ticks);
        
        for (size_t i = 0; i < num_ticks; ++i) {
            auto tick_start = std::chrono::high_resolution_clock::now();
            
            hft::Tick tick;
            while (!simulator.get_next_tick(tick)) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
            
            auto tick_end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(tick_end - tick_start);
            latencies.push_back(latency);
            monitor.record_latency(latency);
        }
        
        simulator.stop();
        
        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());
        
        auto min_latency = latencies.front();
        auto max_latency = latencies.back();
        auto median_latency = latencies[latencies.size() / 2];
        auto p99_latency = latencies[static_cast<size_t>(latencies.size() * 0.99)];
        
        std::cout << "Latency statistics for " << num_ticks << " ticks:" << std::endl;
        std::cout << "  Min: " << min_latency.count() << " ns" << std::endl;
        std::cout << "  Median: " << median_latency.count() << " ns" << std::endl;
        std::cout << "  P99: " << p99_latency.count() << " ns" << std::endl;
        std::cout << "  Max: " << max_latency.count() << " ns" << std::endl;
        
        // Performance monitor stats
        auto stats = monitor.get_latency_stats();
        std::cout << "\nPerformance Monitor Results:" << std::endl;
        std::cout << "  Total messages: " << stats.total_messages << std::endl;
        std::cout << "  Avg latency: " << stats.avg_latency.count() << " ns" << std::endl;
        std::cout << "  P99 latency: " << stats.p99_latency.count() << " ns" << std::endl;
        
        std::cout << "\n4. Memory Pool Performance Test" << std::endl;
        
        // Test memory pool
        hft::MemoryPool<int, 10000> memory_pool;
        
        const size_t num_allocations = 100000;
        start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<int*> ptrs;
        ptrs.reserve(1000);
        
        for (size_t i = 0; i < num_allocations; ++i) {
            int* ptr = memory_pool.allocate();
            if (ptr) {
                *ptr = static_cast<int>(i);
                ptrs.push_back(ptr);
                
                // Occasionally deallocate to test reuse
                if (ptrs.size() >= 1000) {
                    for (auto p : ptrs) {
                        memory_pool.deallocate(p);
                    }
                    ptrs.clear();
                }
            }
        }
        
        // Clean up remaining
        for (auto ptr : ptrs) {
            memory_pool.deallocate(ptr);
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Memory pool: " << num_allocations << " allocations in " 
                 << duration.count() << " microseconds" << std::endl;
        std::cout << "Allocation rate: " << (num_allocations * 1000000.0 / duration.count()) 
                 << " allocations/second" << std::endl;
        
        std::cout << "\nPerformance benchmark completed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}