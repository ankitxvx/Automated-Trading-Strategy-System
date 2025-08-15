#include "../include/fix_protocol.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

namespace hft {

// FixMessage implementation
FixMessage::FixMessage() {
    set_field(fix::tags::BEGIN_STRING, "FIX.4.4");
}

FixMessage::FixMessage(const std::string& raw_msg) : FixMessage() {
    from_string(raw_msg);
}

void FixMessage::set_field(int tag, const std::string& value) {
    fields_[tag] = value;
}

void FixMessage::set_field(int tag, int value) {
    fields_[tag] = std::to_string(value);
}

void FixMessage::set_field(int tag, double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << value;
    fields_[tag] = ss.str();
}

std::string FixMessage::get_field(int tag) const {
    auto it = fields_.find(tag);
    return (it != fields_.end()) ? it->second : "";
}

int FixMessage::get_int_field(int tag) const {
    std::string value = get_field(tag);
    return value.empty() ? 0 : std::stoi(value);
}

double FixMessage::get_double_field(int tag) const {
    std::string value = get_field(tag);
    return value.empty() ? 0.0 : std::stod(value);
}

bool FixMessage::has_field(int tag) const {
    return fields_.find(tag) != fields_.end();
}

void FixMessage::remove_field(int tag) {
    fields_.erase(tag);
}

std::string FixMessage::to_string() const {
    std::stringstream ss;
    
    // Build message body first to calculate length
    std::stringstream body;
    for (const auto& field : fields_) {
        if (field.first != fix::tags::BEGIN_STRING && 
            field.first != fix::tags::BODY_LENGTH && 
            field.first != fix::tags::CHECKSUM) {
            body << field.first << "=" << field.second << fix::SOH;
        }
    }
    
    std::string body_str = body.str();
    int body_length = body_str.length();
    
    // Build full message
    ss << fix::tags::BEGIN_STRING << "=" << get_field(fix::tags::BEGIN_STRING) << fix::SOH;
    ss << fix::tags::BODY_LENGTH << "=" << body_length << fix::SOH;
    ss << body_str;
    
    // Calculate and append checksum
    std::string message_without_checksum = ss.str();
    int checksum = 0;
    for (char c : message_without_checksum) {
        checksum += static_cast<unsigned char>(c);
    }
    checksum %= 256;
    
    ss << fix::tags::CHECKSUM << "=" << std::setfill('0') << std::setw(3) << checksum << fix::SOH;
    
    return ss.str();
}

void FixMessage::from_string(const std::string& msg) {
    raw_message_ = msg;
    parse_message(msg);
}

bool FixMessage::is_valid() const {
    // Basic validation
    return has_field(fix::tags::BEGIN_STRING) && 
           has_field(fix::tags::MSG_TYPE) && 
           has_field(fix::tags::MSG_SEQ_NUM);
}

std::string FixMessage::get_message_type() const {
    return get_field(fix::tags::MSG_TYPE);
}

void FixMessage::set_header_fields(const std::string& sender_id, const std::string& target_id) {
    set_field(fix::tags::SENDER_COMP_ID, sender_id);
    set_field(fix::tags::TARGET_COMP_ID, target_id);
    
    // Set sending time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    
    std::stringstream time_ss;
    time_ss << std::put_time(&tm, "%Y%m%d-%H:%M:%S");
    set_field(fix::tags::SENDING_TIME, time_ss.str());
}

void FixMessage::parse_message(const std::string& msg) {
    fields_.clear();
    
    size_t pos = 0;
    while (pos < msg.length()) {
        size_t eq_pos = msg.find('=', pos);
        if (eq_pos == std::string::npos) break;
        
        size_t soh_pos = msg.find(fix::SOH, eq_pos);
        if (soh_pos == std::string::npos) soh_pos = msg.length();
        
        int tag = std::stoi(msg.substr(pos, eq_pos - pos));
        std::string value = msg.substr(eq_pos + 1, soh_pos - eq_pos - 1);
        
        fields_[tag] = value;
        pos = soh_pos + 1;
    }
}

// FixEngine implementation
FixEngine::FixEngine(const std::string& sender_id, const std::string& target_id)
    : sender_comp_id_(sender_id), target_comp_id_(target_id), 
      next_seq_num_(1), logged_on_(false) {
    
    // Set up default message handlers
    set_message_handler(fix::msg_type::LOGON, 
        [this](const FixMessage& msg) { handle_logon(msg); });
    set_message_handler(fix::msg_type::LOGOUT, 
        [this](const FixMessage& msg) { handle_logout(msg); });
    set_message_handler(fix::msg_type::HEARTBEAT, 
        [this](const FixMessage& msg) { handle_heartbeat(msg); });
    set_message_handler(fix::msg_type::TEST_REQUEST, 
        [this](const FixMessage& msg) { handle_test_request(msg); });
    set_message_handler(fix::msg_type::EXECUTION_REPORT, 
        [this](const FixMessage& msg) { handle_execution_report(msg); });
    set_message_handler(fix::msg_type::MARKET_DATA_SNAPSHOT, 
        [this](const FixMessage& msg) { handle_market_data(msg); });
}

FixEngine::~FixEngine() {
    if (logged_on_) {
        logout();
    }
}

bool FixEngine::logon() {
    FixMessage logon_msg;
    logon_msg.set_field(fix::tags::MSG_TYPE, fix::msg_type::LOGON);
    logon_msg.set_header_fields(sender_comp_id_, target_comp_id_);
    logon_msg.set_field(fix::tags::MSG_SEQ_NUM, get_next_seq_num());
    
    send_message(logon_msg);
    logged_on_ = true;
    return true;
}

void FixEngine::logout() {
    if (!logged_on_) return;
    
    FixMessage logout_msg;
    logout_msg.set_field(fix::tags::MSG_TYPE, fix::msg_type::LOGOUT);
    logout_msg.set_header_fields(sender_comp_id_, target_comp_id_);
    logout_msg.set_field(fix::tags::MSG_SEQ_NUM, get_next_seq_num());
    
    send_message(logout_msg);
    logged_on_ = false;
}

void FixEngine::send_message(FixMessage& msg) {
    if (!msg.has_field(fix::tags::MSG_SEQ_NUM)) {
        msg.set_field(fix::tags::MSG_SEQ_NUM, get_next_seq_num());
    }
    
    if (!msg.has_field(fix::tags::SENDER_COMP_ID)) {
        msg.set_header_fields(sender_comp_id_, target_comp_id_);
    }
    
    std::string fix_msg = msg.to_string();
    std::cout << "Sending FIX message: " << fix_msg << std::endl;
}

void FixEngine::send_new_order(const Order& order) {
    FixMessage new_order_msg;
    new_order_msg.set_field(fix::tags::MSG_TYPE, fix::msg_type::NEW_ORDER_SINGLE);
    new_order_msg.set_header_fields(sender_comp_id_, target_comp_id_);
    new_order_msg.set_field(fix::tags::MSG_SEQ_NUM, get_next_seq_num());
    
    new_order_msg.set_field(fix::tags::ORDER_ID, std::to_string(order.id));
    new_order_msg.set_field(fix::tags::SYMBOL, order.symbol);
    new_order_msg.set_field(fix::tags::SIDE, (order.side == OrderSide::BUY) ? "1" : "2");
    new_order_msg.set_field(fix::tags::ORDER_QTY, static_cast<int>(order.quantity));
    new_order_msg.set_field(fix::tags::PRICE, order.price);
    
    send_message(new_order_msg);
}

void FixEngine::process_message(const std::string& raw_msg) {
    FixMessage msg(raw_msg);
    
    if (!msg.is_valid()) {
        std::cerr << "Invalid FIX message received" << std::endl;
        return;
    }
    
    std::string msg_type = msg.get_message_type();
    auto handler_it = message_handlers_.find(msg_type);
    
    if (handler_it != message_handlers_.end()) {
        handler_it->second(msg);
    } else {
        std::cout << "No handler for message type: " << msg_type << std::endl;
    }
}

void FixEngine::set_message_handler(const std::string& msg_type, 
                                   std::function<void(const FixMessage&)> handler) {
    message_handlers_[msg_type] = handler;
}

void FixEngine::handle_logon(const FixMessage& msg) {
    std::cout << "Received LOGON message" << std::endl;
    logged_on_ = true;
}

void FixEngine::handle_logout(const FixMessage& msg) {
    std::cout << "Received LOGOUT message" << std::endl;
    logged_on_ = false;
}

void FixEngine::handle_heartbeat(const FixMessage& msg) {
    std::cout << "Received HEARTBEAT message" << std::endl;
}

void FixEngine::handle_test_request(const FixMessage& msg) {
    std::cout << "Received TEST_REQUEST message" << std::endl;
    send_heartbeat();
}

void FixEngine::handle_execution_report(const FixMessage& msg) {
    std::cout << "Received EXECUTION_REPORT message" << std::endl;
    // Parse execution details and notify application
}

void FixEngine::handle_market_data(const FixMessage& msg) {
    std::cout << "Received MARKET_DATA message" << std::endl;
    // Parse market data and notify application
}

void FixEngine::send_heartbeat() {
    FixMessage heartbeat_msg;
    heartbeat_msg.set_field(fix::tags::MSG_TYPE, fix::msg_type::HEARTBEAT);
    heartbeat_msg.set_header_fields(sender_comp_id_, target_comp_id_);
    heartbeat_msg.set_field(fix::tags::MSG_SEQ_NUM, get_next_seq_num());
    
    send_message(heartbeat_msg);
}

// MarketDataToFixConverter implementation
FixMessage MarketDataToFixConverter::tick_to_market_data_snapshot(const Tick& tick) {
    FixMessage msg;
    msg.set_field(fix::tags::MSG_TYPE, fix::msg_type::MARKET_DATA_SNAPSHOT);
    msg.set_field(fix::tags::SYMBOL, tick.symbol);
    msg.set_field(fix::tags::BID_PX, tick.bid_price);
    msg.set_field(fix::tags::ASK_PX, tick.ask_price);
    msg.set_field(fix::tags::BID_SIZE, static_cast<int>(tick.bid_size));
    msg.set_field(fix::tags::ASK_SIZE, static_cast<int>(tick.ask_size));
    
    return msg;
}

// FixProtocolAdapter implementation
FixProtocolAdapter::FixProtocolAdapter(const std::string& exchange_name, 
                                     const std::string& sender_id, 
                                     const std::string& target_id)
    : exchange_name_(exchange_name),
      fix_engine_(std::make_unique<FixEngine>(sender_id, target_id)) {
}

FixProtocolAdapter::~FixProtocolAdapter() {
    disconnect();
}

bool FixProtocolAdapter::connect() {
    return fix_engine_->logon();
}

void FixProtocolAdapter::disconnect() {
    fix_engine_->logout();
}

bool FixProtocolAdapter::is_connected() const {
    return fix_engine_->is_logged_on();
}

void FixProtocolAdapter::submit_order(const Order& order) {
    fix_engine_->send_new_order(order);
}

void FixProtocolAdapter::set_execution_handler(std::function<void(const FixMessage&)> handler) {
    fix_engine_->set_message_handler(fix::msg_type::EXECUTION_REPORT, handler);
}

void FixProtocolAdapter::set_market_data_handler(std::function<void(const FixMessage&)> handler) {
    fix_engine_->set_message_handler(fix::msg_type::MARKET_DATA_SNAPSHOT, handler);
}

} // namespace hft