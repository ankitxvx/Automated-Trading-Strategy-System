#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>

namespace hft {

// Forward declarations
class FixMessage;
class OrderBook;
class MarketDataFeed;

// High-precision timestamp
using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>;
using Duration = std::chrono::nanoseconds;

// Price and quantity types
using Price = double;
using Quantity = int64_t;
using OrderId = uint64_t;

// Market data structures
struct Tick {
    std::string symbol;
    Price bid_price;
    Price ask_price;
    Quantity bid_size;
    Quantity ask_size;
    Price last_price;
    Quantity last_size;
    Timestamp timestamp;
};

struct Trade {
    std::string symbol;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    OrderId buyer_id;
    OrderId seller_id;
};

struct OrderBookLevel {
    Price price;
    Quantity quantity;
    int order_count;
};

// Order types
enum class OrderSide { BUY, SELL };
enum class OrderType { MARKET, LIMIT, STOP };
enum class OrderStatus { PENDING, FILLED, PARTIALLY_FILLED, CANCELLED, REJECTED };

struct Order {
    OrderId id;
    std::string symbol;
    OrderSide side;
    OrderType type;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    OrderStatus status;
    Timestamp timestamp;
    std::string client_id;
};

// Performance metrics
struct LatencyStats {
    Duration min_latency;
    Duration max_latency;
    Duration avg_latency;
    Duration p99_latency;
    uint64_t total_messages;
};

struct ThroughputStats {
    uint64_t messages_per_second;
    uint64_t bytes_per_second;
    uint64_t total_messages;
    uint64_t total_bytes;
};

} // namespace hft