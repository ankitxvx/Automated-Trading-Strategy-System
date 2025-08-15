#pragma once

#include "types.h"
#include "lockfree_queue.h"
#include <vector>
#include <functional>
#include <mutex>
#include <iostream>
#include <thread>
#include <atomic>
#include <future>

namespace hft {

// Thread pool for high-performance task execution
class ThreadPool {
private:
    std::vector<std::thread> workers_;
    MPSCQueue<std::function<void()>, 10000> task_queue_;
    std::atomic<bool> stop_;

public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Submit task and get future
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        if (!task_queue_.push([task](){ (*task)(); })) {
            throw std::runtime_error("Task queue is full");
        }
        
        return result;
    }

    // Submit task without future (fire and forget)
    void submit_detached(std::function<void()> task);
    
    size_t get_num_threads() const { return workers_.size(); }
    bool is_running() const { return !stop_.load(); }

private:
    void worker_thread();
};

// High-performance timer for scheduling
class HighResTimer {
private:
    std::atomic<bool> running_;
    std::thread timer_thread_;
    Duration interval_;
    std::function<void()> callback_;

public:
    HighResTimer(Duration interval, std::function<void()> callback);
    ~HighResTimer();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    void set_interval(Duration new_interval) { interval_ = new_interval; }
    Duration get_interval() const { return interval_; }

private:
    void timer_loop();
};

// CPU affinity and optimization utilities
class CpuOptimizer {
public:
    static bool set_thread_affinity(std::thread& thread, int cpu_id);
    static bool set_current_thread_affinity(int cpu_id);
    static int get_cpu_count();
    static std::vector<int> get_available_cpus();
    
    // Memory optimization
    static void prefetch_memory(const void* addr, size_t size);
    static void flush_cache_line(const void* addr);
    
    // Thread priority
    static bool set_high_priority(std::thread& thread);
    static bool set_realtime_priority(std::thread& thread);
};

// Memory pool for object allocation
template<typename T, size_t PoolSize>
class MemoryPool {
private:
    alignas(64) std::array<T, PoolSize> pool_;
    alignas(64) std::atomic<size_t> next_index_{0};
    alignas(64) std::array<std::atomic<bool>, PoolSize> allocated_;

public:
    MemoryPool() {
        for (size_t i = 0; i < PoolSize; ++i) {
            allocated_[i].store(false, std::memory_order_relaxed);
        }
    }

    T* allocate() {
        for (size_t attempts = 0; attempts < PoolSize; ++attempts) {
            size_t index = next_index_.fetch_add(1, std::memory_order_relaxed) % PoolSize;
            
            bool expected = false;
            if (allocated_[index].compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                return &pool_[index];
            }
        }
        return nullptr; // Pool exhausted
    }

    void deallocate(T* ptr) {
        if (ptr >= &pool_[0] && ptr < &pool_[PoolSize]) {
            size_t index = ptr - &pool_[0];
            allocated_[index].store(false, std::memory_order_release);
        }
    }

    size_t capacity() const { return PoolSize; }
    
    size_t allocated_count() const {
        size_t count = 0;
        for (size_t i = 0; i < PoolSize; ++i) {
            if (allocated_[i].load(std::memory_order_acquire)) {
                ++count;
            }
        }
        return count;
    }
};

// Performance monitoring and profiling
class PerformanceMonitor {
private:
    std::vector<Duration> latency_samples_;
    std::atomic<uint64_t> total_operations_;
    std::atomic<uint64_t> total_bytes_;
    Timestamp start_time_;
    mutable std::mutex samples_mutex_;

public:
    PerformanceMonitor();

    void record_latency(Duration latency);
    void record_operation(uint64_t bytes = 0);
    
    LatencyStats get_latency_stats() const;
    ThroughputStats get_throughput_stats() const;
    
    void reset();
    void start_monitoring();

private:
    Duration calculate_percentile(double percentile) const;
};

} // namespace hft