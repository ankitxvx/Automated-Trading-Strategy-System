#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include "../cpp/include/types.h"
#include "../cpp/include/market_data_simulator.h"
#include "../cpp/include/fix_protocol.h"
#include "../cpp/include/thread_pool.h"

namespace py = pybind11;

PYBIND11_MODULE(hft_simulator_py, m) {
    m.doc() = "High-Frequency Trading Simulator Python Bindings";
    
    // Basic types
    py::enum_<hft::OrderSide>(m, "OrderSide")
        .value("BUY", hft::OrderSide::BUY)
        .value("SELL", hft::OrderSide::SELL);
    
    py::enum_<hft::OrderType>(m, "OrderType")
        .value("MARKET", hft::OrderType::MARKET)
        .value("LIMIT", hft::OrderType::LIMIT)
        .value("STOP", hft::OrderType::STOP);
    
    py::enum_<hft::OrderStatus>(m, "OrderStatus")
        .value("PENDING", hft::OrderStatus::PENDING)
        .value("FILLED", hft::OrderStatus::FILLED)
        .value("PARTIALLY_FILLED", hft::OrderStatus::PARTIALLY_FILLED)
        .value("CANCELLED", hft::OrderStatus::CANCELLED)
        .value("REJECTED", hft::OrderStatus::REJECTED);
    
    // Data structures
    py::class_<hft::Tick>(m, "Tick")
        .def(py::init<>())
        .def_readwrite("symbol", &hft::Tick::symbol)
        .def_readwrite("bid_price", &hft::Tick::bid_price)
        .def_readwrite("ask_price", &hft::Tick::ask_price)
        .def_readwrite("bid_size", &hft::Tick::bid_size)
        .def_readwrite("ask_size", &hft::Tick::ask_size)
        .def_readwrite("last_price", &hft::Tick::last_price)
        .def_readwrite("last_size", &hft::Tick::last_size)
        .def_readwrite("timestamp", &hft::Tick::timestamp)
        .def("__repr__", [](const hft::Tick& t) {
            return "<Tick symbol=" + t.symbol + 
                   " bid=" + std::to_string(t.bid_price) + 
                   " ask=" + std::to_string(t.ask_price) + ">";
        });
    
    py::class_<hft::Trade>(m, "Trade")
        .def(py::init<>())
        .def_readwrite("symbol", &hft::Trade::symbol)
        .def_readwrite("price", &hft::Trade::price)
        .def_readwrite("quantity", &hft::Trade::quantity)
        .def_readwrite("timestamp", &hft::Trade::timestamp)
        .def_readwrite("buyer_id", &hft::Trade::buyer_id)
        .def_readwrite("seller_id", &hft::Trade::seller_id);
    
    py::class_<hft::Order>(m, "Order")
        .def(py::init<>())
        .def_readwrite("id", &hft::Order::id)
        .def_readwrite("symbol", &hft::Order::symbol)
        .def_readwrite("side", &hft::Order::side)
        .def_readwrite("type", &hft::Order::type)
        .def_readwrite("price", &hft::Order::price)
        .def_readwrite("quantity", &hft::Order::quantity)
        .def_readwrite("filled_quantity", &hft::Order::filled_quantity)
        .def_readwrite("status", &hft::Order::status)
        .def_readwrite("timestamp", &hft::Order::timestamp)
        .def_readwrite("client_id", &hft::Order::client_id);
    
    py::class_<hft::LatencyStats>(m, "LatencyStats")
        .def(py::init<>())
        .def_readwrite("min_latency", &hft::LatencyStats::min_latency)
        .def_readwrite("max_latency", &hft::LatencyStats::max_latency)
        .def_readwrite("avg_latency", &hft::LatencyStats::avg_latency)
        .def_readwrite("p99_latency", &hft::LatencyStats::p99_latency)
        .def_readwrite("total_messages", &hft::LatencyStats::total_messages);
    
    py::class_<hft::ThroughputStats>(m, "ThroughputStats")
        .def(py::init<>())
        .def_readwrite("messages_per_second", &hft::ThroughputStats::messages_per_second)
        .def_readwrite("bytes_per_second", &hft::ThroughputStats::bytes_per_second)
        .def_readwrite("total_messages", &hft::ThroughputStats::total_messages)
        .def_readwrite("total_bytes", &hft::ThroughputStats::total_bytes);
    
    // Market Data Simulator
    py::class_<hft::MarketDataSimulator>(m, "MarketDataSimulator")
        .def(py::init<>())
        .def("add_symbol", &hft::MarketDataSimulator::add_symbol)
        .def("set_volatility", &hft::MarketDataSimulator::set_volatility)
        .def("start", &hft::MarketDataSimulator::start)
        .def("stop", &hft::MarketDataSimulator::stop)
        .def("is_running", &hft::MarketDataSimulator::is_running)
        .def("get_next_tick", [](hft::MarketDataSimulator& sim) {
            hft::Tick tick;
            bool success = sim.get_next_tick(tick);
            return success ? py::cast(tick) : py::none();
        })
        .def("get_current_snapshot", &hft::MarketDataSimulator::get_current_snapshot)
        .def("get_throughput_stats", &hft::MarketDataSimulator::get_throughput_stats)
        .def("get_total_ticks", &hft::MarketDataSimulator::get_total_ticks);
    
    // Market Data Feed Interface
    py::class_<hft::MarketDataFeed>(m, "MarketDataFeed")
        .def("subscribe", &hft::MarketDataFeed::subscribe)
        .def("unsubscribe", &hft::MarketDataFeed::unsubscribe)
        .def("get_tick", [](hft::MarketDataFeed& feed) {
            hft::Tick tick;
            bool success = feed.get_tick(tick);
            return success ? py::cast(tick) : py::none();
        })
        .def("get_subscribed_symbols", &hft::MarketDataFeed::get_subscribed_symbols);
    
    // Simulated Market Data Feed
    py::class_<hft::SimulatedMarketDataFeed, hft::MarketDataFeed>(m, "SimulatedMarketDataFeed")
        .def(py::init<>())
        .def("set_initial_price", &hft::SimulatedMarketDataFeed::set_initial_price)
        .def("set_volatility", &hft::SimulatedMarketDataFeed::set_volatility)
        .def("start_simulation", &hft::SimulatedMarketDataFeed::start_simulation)
        .def("stop_simulation", &hft::SimulatedMarketDataFeed::stop_simulation);
    
    // FIX Protocol
    py::class_<hft::FixMessage>(m, "FixMessage")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def("set_field", py::overload_cast<int, const std::string&>(&hft::FixMessage::set_field))
        .def("set_field", py::overload_cast<int, int>(&hft::FixMessage::set_field))
        .def("set_field", py::overload_cast<int, double>(&hft::FixMessage::set_field))
        .def("get_field", &hft::FixMessage::get_field)
        .def("get_int_field", &hft::FixMessage::get_int_field)
        .def("get_double_field", &hft::FixMessage::get_double_field)
        .def("has_field", &hft::FixMessage::has_field)
        .def("to_string", &hft::FixMessage::to_string)
        .def("from_string", &hft::FixMessage::from_string)
        .def("is_valid", &hft::FixMessage::is_valid)
        .def("get_message_type", &hft::FixMessage::get_message_type);
    
    py::class_<hft::FixEngine>(m, "FixEngine")
        .def(py::init<const std::string&, const std::string&>())
        .def("logon", &hft::FixEngine::logon)
        .def("logout", &hft::FixEngine::logout)
        .def("is_logged_on", &hft::FixEngine::is_logged_on)
        .def("send_new_order", &hft::FixEngine::send_new_order)
        .def("process_message", &hft::FixEngine::process_message);
    
    py::class_<hft::FixProtocolAdapter>(m, "FixProtocolAdapter")
        .def(py::init<const std::string&, const std::string&, const std::string&>())
        .def("connect", &hft::FixProtocolAdapter::connect)
        .def("disconnect", &hft::FixProtocolAdapter::disconnect)
        .def("is_connected", &hft::FixProtocolAdapter::is_connected)
        .def("submit_order", &hft::FixProtocolAdapter::submit_order);
    
    // Thread Pool
    py::class_<hft::ThreadPool>(m, "ThreadPool")
        .def(py::init<size_t>())
        .def("submit_detached", &hft::ThreadPool::submit_detached)
        .def("get_num_threads", &hft::ThreadPool::get_num_threads)
        .def("is_running", &hft::ThreadPool::is_running);
    
    // Performance Monitor
    py::class_<hft::PerformanceMonitor>(m, "PerformanceMonitor")
        .def(py::init<>())
        .def("record_latency", &hft::PerformanceMonitor::record_latency)
        .def("record_operation", &hft::PerformanceMonitor::record_operation)
        .def("get_latency_stats", &hft::PerformanceMonitor::get_latency_stats)
        .def("get_throughput_stats", &hft::PerformanceMonitor::get_throughput_stats)
        .def("reset", &hft::PerformanceMonitor::reset)
        .def("start_monitoring", &hft::PerformanceMonitor::start_monitoring);
    
    // Utility functions
    m.def("get_current_timestamp", []() {
        return std::chrono::high_resolution_clock::now();
    });
    
    m.def("duration_to_nanoseconds", [](const std::chrono::nanoseconds& duration) {
        return duration.count();
    });
    
    m.def("duration_to_microseconds", [](const std::chrono::nanoseconds& duration) {
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    });
    
    // Constants
    py::module fix_module = m.def_submodule("fix", "FIX protocol constants");
    
    py::module msg_types = fix_module.def_submodule("msg_type", "FIX message types");
    msg_types.attr("LOGON") = hft::fix::msg_type::LOGON;
    msg_types.attr("LOGOUT") = hft::fix::msg_type::LOGOUT;
    msg_types.attr("HEARTBEAT") = hft::fix::msg_type::HEARTBEAT;
    msg_types.attr("NEW_ORDER_SINGLE") = hft::fix::msg_type::NEW_ORDER_SINGLE;
    msg_types.attr("EXECUTION_REPORT") = hft::fix::msg_type::EXECUTION_REPORT;
    msg_types.attr("MARKET_DATA_SNAPSHOT") = hft::fix::msg_type::MARKET_DATA_SNAPSHOT;
    
    py::module tags = fix_module.def_submodule("tags", "FIX field tags");
    tags.attr("MSG_TYPE") = hft::fix::tags::MSG_TYPE;
    tags.attr("SYMBOL") = hft::fix::tags::SYMBOL;
    tags.attr("SIDE") = hft::fix::tags::SIDE;
    tags.attr("ORDER_QTY") = hft::fix::tags::ORDER_QTY;
    tags.attr("PRICE") = hft::fix::tags::PRICE;
    tags.attr("ORDER_ID") = hft::fix::tags::ORDER_ID;
    tags.attr("BID_PX") = hft::fix::tags::BID_PX;
    tags.attr("ASK_PX") = hft::fix::tags::ASK_PX;
}