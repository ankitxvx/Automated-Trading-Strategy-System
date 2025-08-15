#include "../include/thread_pool.h"
#include <algorithm>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>

namespace hft {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    workers_.reserve(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true);
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::submit_detached(std::function<void()> task) {
    if (!task_queue_.push(std::move(task))) {
        throw std::runtime_error("Task queue is full");
    }
}

void ThreadPool::worker_thread() {
    std::function<void()> task;
    
    while (!stop_.load()) {
        if (task_queue_.pop(task)) {
            try {
                task();
            } catch (const std::exception& e) {
                // Log error but don't crash the worker
                std::cerr << "Task execution error: " << e.what() << std::endl;
            }
        } else {
            // No task available, yield to avoid busy waiting
            std::this_thread::yield();
        }
    }
}

// HighResTimer implementation
HighResTimer::HighResTimer(Duration interval, std::function<void()> callback)
    : running_(false), interval_(interval), callback_(std::move(callback)) {
}

HighResTimer::~HighResTimer() {
    stop();
}

void HighResTimer::start() {
    if (running_.load()) return;
    
    running_.store(true);
    timer_thread_ = std::thread(&HighResTimer::timer_loop, this);
}

void HighResTimer::stop() {
    if (!running_.load()) return;
    
    running_.store(false);
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void HighResTimer::timer_loop() {
    auto next_tick = std::chrono::high_resolution_clock::now();
    
    while (running_.load()) {
        next_tick += interval_;
        
        if (callback_) {
            callback_();
        }
        
        std::this_thread::sleep_until(next_tick);
    }
}

// CpuOptimizer implementation
bool CpuOptimizer::set_thread_affinity(std::thread& thread, int cpu_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    
    return pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    return false; // Not implemented for non-Linux systems
#endif
}

bool CpuOptimizer::set_current_thread_affinity(int cpu_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    return false;
#endif
}

int CpuOptimizer::get_cpu_count() {
    return std::thread::hardware_concurrency();
}

std::vector<int> CpuOptimizer::get_available_cpus() {
    std::vector<int> cpus;
    int cpu_count = get_cpu_count();
    
    for (int i = 0; i < cpu_count; ++i) {
        cpus.push_back(i);
    }
    
    return cpus;
}

void CpuOptimizer::prefetch_memory(const void* addr, size_t size) {
    const char* ptr = static_cast<const char*>(addr);
    const size_t cache_line_size = 64; // Typical cache line size
    
    for (size_t i = 0; i < size; i += cache_line_size) {
        __builtin_prefetch(ptr + i, 0, 3); // Prefetch for read, high locality
    }
}

void CpuOptimizer::flush_cache_line(const void* addr) {
#ifdef __x86_64__
    asm volatile("clflush %0" : : "m"(*(const char*)addr) : "memory");
#endif
}

bool CpuOptimizer::set_high_priority(std::thread& thread) {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_OTHER);
    
    return pthread_setschedparam(thread.native_handle(), SCHED_OTHER, &param) == 0;
#else
    return false;
#endif
}

bool CpuOptimizer::set_realtime_priority(std::thread& thread) {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    
    return pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &param) == 0;
#else
    return false;
#endif
}

// PerformanceMonitor implementation
PerformanceMonitor::PerformanceMonitor() 
    : total_operations_(0), total_bytes_(0) {
    latency_samples_.reserve(10000); // Pre-allocate for performance
}

void PerformanceMonitor::record_latency(Duration latency) {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    latency_samples_.push_back(latency);
    
    // Keep only recent samples to avoid memory bloat
    if (latency_samples_.size() > 100000) {
        latency_samples_.erase(latency_samples_.begin(), 
                              latency_samples_.begin() + 50000);
    }
}

void PerformanceMonitor::record_operation(uint64_t bytes) {
    total_operations_.fetch_add(1);
    total_bytes_.fetch_add(bytes);
}

LatencyStats PerformanceMonitor::get_latency_stats() const {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    
    if (latency_samples_.empty()) {
        return {Duration::zero(), Duration::zero(), Duration::zero(), Duration::zero(), 0};
    }
    
    auto samples_copy = latency_samples_;
    std::sort(samples_copy.begin(), samples_copy.end());
    
    LatencyStats stats;
    stats.min_latency = samples_copy.front();
    stats.max_latency = samples_copy.back();
    stats.p99_latency = calculate_percentile(0.99);
    stats.total_messages = samples_copy.size();
    
    // Calculate average
    Duration total = Duration::zero();
    for (const auto& sample : samples_copy) {
        total += sample;
    }
    stats.avg_latency = total / samples_copy.size();
    
    return stats;
}

ThroughputStats PerformanceMonitor::get_throughput_stats() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    
    ThroughputStats stats;
    stats.total_messages = total_operations_.load();
    stats.total_bytes = total_bytes_.load();
    
    if (duration > 0) {
        stats.messages_per_second = stats.total_messages / duration;
        stats.bytes_per_second = stats.total_bytes / duration;
    } else {
        stats.messages_per_second = 0;
        stats.bytes_per_second = 0;
    }
    
    return stats;
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(samples_mutex_);
    latency_samples_.clear();
    total_operations_.store(0);
    total_bytes_.store(0);
    start_time_ = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::start_monitoring() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

Duration PerformanceMonitor::calculate_percentile(double percentile) const {
    if (latency_samples_.empty()) {
        return Duration::zero();
    }
    
    auto samples_copy = latency_samples_;
    std::sort(samples_copy.begin(), samples_copy.end());
    
    size_t index = static_cast<size_t>(percentile * samples_copy.size());
    if (index >= samples_copy.size()) {
        index = samples_copy.size() - 1;
    }
    
    return samples_copy[index];
}

} // namespace hft