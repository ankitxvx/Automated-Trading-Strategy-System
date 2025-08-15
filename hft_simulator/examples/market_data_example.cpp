#include <iostream>
#include <chrono>
#include <thread>
#include "../cpp/include/market_data_simulator.h"
#include "../cpp/include/thread_pool.h"

int main() {
    std::cout << "=== HFT Simulator Market Data Example ===" << std::endl;
    
    try {
        // Create market data simulator
        hft::MarketDataSimulator simulator;
        
        // Add some symbols
        simulator.add_symbol("AAPL", 150.0);
        simulator.add_symbol("GOOGL", 2500.0);
        simulator.add_symbol("MSFT", 300.0);
        
        std::cout << "Added symbols to simulator" << std::endl;
        
        // Set volatility
        simulator.set_volatility(0.001);
        
        // Start simulation
        std::cout << "Starting market data simulation..." << std::endl;
        simulator.start();
        
        // Consume market data for 5 seconds
        auto start_time = std::chrono::high_resolution_clock::now();
        int tick_count = 0;
        
        while (std::chrono::high_resolution_clock::now() - start_time < std::chrono::seconds(5)) {
            hft::Tick tick;
            if (simulator.get_next_tick(tick)) {
                tick_count++;
                if (tick_count % 1000 == 0) {
                    std::cout << "Tick #" << tick_count 
                             << " Symbol: " << tick.symbol 
                             << " Bid: " << tick.bid_price 
                             << " Ask: " << tick.ask_price << std::endl;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        
        // Stop simulation
        simulator.stop();
        
        // Get performance stats
        auto stats = simulator.get_throughput_stats();
        std::cout << "\n=== Performance Statistics ===" << std::endl;
        std::cout << "Total ticks generated: " << stats.total_messages << std::endl;
        std::cout << "Ticks per second: " << stats.messages_per_second << std::endl;
        std::cout << "Total bytes: " << stats.total_bytes << std::endl;
        
        std::cout << "\n=== Thread Pool Example ===" << std::endl;
        
        // Test thread pool
        hft::ThreadPool pool(4);
        
        for (int i = 0; i < 10; ++i) {
            pool.submit_detached([i]() {
                std::cout << "Task " << i << " running on thread " 
                         << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            });
        }
        
        // Wait for tasks to complete
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "\nExample completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}