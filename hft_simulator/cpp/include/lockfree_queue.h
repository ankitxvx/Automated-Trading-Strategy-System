#pragma once

#include <queue>
#include <atomic>
#include <memory>
#include <thread>

namespace hft {

// Lock-free single producer, single consumer queue
template<typename T, size_t Size>
class SPSCQueue {
private:
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) T buffer_[Size];

public:
    SPSCQueue() = default;
    ~SPSCQueue() = default;

    // Non-copyable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % Size;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % Size;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }
        
        buffer_[current_tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }
        
        item = std::move(buffer_[current_head]);
        head_.store((current_head + 1) % Size, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (tail >= head) ? (tail - head) : (Size - head + tail);
    }
};

// Lock-free multiple producer, single consumer queue
template<typename T, size_t Size>
class MPSCQueue {
private:
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) T buffer_[Size];
    alignas(64) std::atomic<bool> occupied_[Size]{};

public:
    MPSCQueue() {
        for (size_t i = 0; i < Size; ++i) {
            occupied_[i].store(false, std::memory_order_relaxed);
        }
    }

    bool push(const T& item) {
        size_t current_tail = tail_.fetch_add(1, std::memory_order_acquire) % Size;
        
        // Wait until slot is available
        while (occupied_[current_tail].load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        
        buffer_[current_tail] = item;
        occupied_[current_tail].store(true, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (!occupied_[current_head].load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }
        
        item = std::move(buffer_[current_head]);
        occupied_[current_head].store(false, std::memory_order_release);
        head_.store((current_head + 1) % Size, std::memory_order_relaxed);
        return true;
    }

    bool empty() const {
        const size_t current_head = head_.load(std::memory_order_acquire);
        return !occupied_[current_head].load(std::memory_order_acquire);
    }
};

} // namespace hft