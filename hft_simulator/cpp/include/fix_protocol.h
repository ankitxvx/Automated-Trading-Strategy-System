#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <memory>
#include <functional>
#include <iostream>

namespace hft {

// FIX protocol constants
namespace fix {
    constexpr char SOH = '\001';  // Start of Header
    
    // Message types
    namespace msg_type {
        constexpr const char* LOGON = "A";
        constexpr const char* LOGOUT = "5";
        constexpr const char* HEARTBEAT = "0";
        constexpr const char* TEST_REQUEST = "1";
        constexpr const char* NEW_ORDER_SINGLE = "D";
        constexpr const char* ORDER_CANCEL_REQUEST = "F";
        constexpr const char* EXECUTION_REPORT = "8";
        constexpr const char* MARKET_DATA_REQUEST = "V";
        constexpr const char* MARKET_DATA_SNAPSHOT = "W";
        constexpr const char* MARKET_DATA_INCREMENTAL_REFRESH = "X";
    }
    
    // Field tags
    namespace tags {
        constexpr int BEGIN_STRING = 8;
        constexpr int BODY_LENGTH = 9;
        constexpr int CHECKSUM = 10;
        constexpr int MSG_TYPE = 35;
        constexpr int MSG_SEQ_NUM = 34;
        constexpr int SENDER_COMP_ID = 49;
        constexpr int TARGET_COMP_ID = 56;
        constexpr int SENDING_TIME = 52;
        constexpr int SYMBOL = 55;
        constexpr int SIDE = 54;
        constexpr int ORDER_QTY = 38;
        constexpr int PRICE = 44;
        constexpr int ORDER_ID = 37;
        constexpr int EXEC_ID = 17;
        constexpr int EXEC_TYPE = 150;
        constexpr int ORDER_STATUS = 39;
        constexpr int LAST_SHARES = 32;
        constexpr int LAST_PX = 31;
        constexpr int BID_PX = 132;
        constexpr int ASK_PX = 133;
        constexpr int BID_SIZE = 134;
        constexpr int ASK_SIZE = 135;
    }
}

class FixMessage {
private:
    std::map<int, std::string> fields_;
    std::string raw_message_;

public:
    FixMessage();
    explicit FixMessage(const std::string& raw_msg);

    // Field manipulation
    void set_field(int tag, const std::string& value);
    void set_field(int tag, int value);
    void set_field(int tag, double value);
    
    std::string get_field(int tag) const;
    int get_int_field(int tag) const;
    double get_double_field(int tag) const;
    
    bool has_field(int tag) const;
    void remove_field(int tag);

    // Message construction
    std::string to_string() const;
    void from_string(const std::string& msg);
    
    // Validation
    bool is_valid() const;
    std::string calculate_checksum() const;

    // Helper methods
    std::string get_message_type() const;
    void set_header_fields(const std::string& sender_id, const std::string& target_id);
    
private:
    void calculate_body_length();
    void parse_message(const std::string& msg);
};

class FixEngine {
private:
    std::string sender_comp_id_;
    std::string target_comp_id_;
    int next_seq_num_;
    bool logged_on_;
    
    // Message handlers
    std::map<std::string, std::function<void(const FixMessage&)>> message_handlers_;

public:
    FixEngine(const std::string& sender_id, const std::string& target_id);
    ~FixEngine();

    // Session management
    bool logon();
    void logout();
    bool is_logged_on() const { return logged_on_; }
    
    // Message sending
    void send_message(FixMessage& msg);
    void send_heartbeat();
    void send_test_request(const std::string& test_req_id);
    
    // Order management
    void send_new_order(const Order& order);
    void send_cancel_request(OrderId order_id, const std::string& symbol);
    
    // Market data
    void subscribe_market_data(const std::vector<std::string>& symbols);
    void unsubscribe_market_data(const std::vector<std::string>& symbols);
    
    // Message processing
    void process_message(const std::string& raw_msg);
    void set_message_handler(const std::string& msg_type, 
                           std::function<void(const FixMessage&)> handler);

private:
    void handle_logon(const FixMessage& msg);
    void handle_logout(const FixMessage& msg);
    void handle_heartbeat(const FixMessage& msg);
    void handle_test_request(const FixMessage& msg);
    void handle_execution_report(const FixMessage& msg);
    void handle_market_data(const FixMessage& msg);
    
    std::string get_current_time_string() const;
    int get_next_seq_num() { return next_seq_num_++; }
};

// Market data to FIX converter
class MarketDataToFixConverter {
public:
    static FixMessage tick_to_market_data_snapshot(const Tick& tick);
    static FixMessage trade_to_execution_report(const Trade& trade);
    static std::vector<FixMessage> orderbook_to_market_data(
        const std::string& symbol, 
        const std::vector<OrderBookLevel>& bids,
        const std::vector<OrderBookLevel>& asks);
};

// FIX protocol adapter for exchange simulation
class FixProtocolAdapter {
private:
    std::unique_ptr<FixEngine> fix_engine_;
    std::string exchange_name_;
    
public:
    FixProtocolAdapter(const std::string& exchange_name, 
                      const std::string& sender_id, 
                      const std::string& target_id);
    ~FixProtocolAdapter();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    
    // Trading interface
    void submit_order(const Order& order);
    void cancel_order(OrderId order_id, const std::string& symbol);
    
    // Market data interface
    void subscribe_market_data(const std::vector<std::string>& symbols);
    void unsubscribe_market_data(const std::vector<std::string>& symbols);
    
    // Message callbacks
    void set_execution_handler(std::function<void(const FixMessage&)> handler);
    void set_market_data_handler(std::function<void(const FixMessage&)> handler);
    
    // Processing
    void process_incoming_messages();
};

} // namespace hft