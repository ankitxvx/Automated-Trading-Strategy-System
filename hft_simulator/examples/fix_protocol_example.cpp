#include <iostream>
#include <chrono>
#include "../cpp/include/fix_protocol.h"

int main() {
    std::cout << "=== HFT Simulator FIX Protocol Example ===" << std::endl;
    
    try {
        // Create FIX engine
        hft::FixEngine engine("CLIENT1", "EXCHANGE1");
        
        std::cout << "Created FIX engine" << std::endl;
        
        // Test FIX message creation
        hft::FixMessage msg;
        msg.set_field(hft::fix::tags::MSG_TYPE, hft::fix::msg_type::NEW_ORDER_SINGLE);
        msg.set_field(hft::fix::tags::SYMBOL, "AAPL");
        msg.set_field(hft::fix::tags::SIDE, "1"); // Buy
        msg.set_field(hft::fix::tags::ORDER_QTY, 100);
        msg.set_field(hft::fix::tags::PRICE, 150.50);
        msg.set_header_fields("CLIENT1", "EXCHANGE1");
        
        std::cout << "Created FIX message" << std::endl;
        std::cout << "Message type: " << msg.get_message_type() << std::endl;
        std::cout << "Symbol: " << msg.get_field(hft::fix::tags::SYMBOL) << std::endl;
        std::cout << "Quantity: " << msg.get_int_field(hft::fix::tags::ORDER_QTY) << std::endl;
        std::cout << "Price: " << msg.get_double_field(hft::fix::tags::PRICE) << std::endl;
        
        // Convert to string
        std::string fix_string = msg.to_string();
        std::cout << "FIX string length: " << fix_string.length() << " bytes" << std::endl;
        
        // Test parsing
        hft::FixMessage parsed_msg(fix_string);
        std::cout << "Parsed message successfully" << std::endl;
        std::cout << "Parsed symbol: " << parsed_msg.get_field(hft::fix::tags::SYMBOL) << std::endl;
        
        // Test FIX protocol adapter
        hft::FixProtocolAdapter adapter("TEST_EXCHANGE", "CLIENT1", "EXCHANGE1");
        
        std::cout << "Created FIX protocol adapter" << std::endl;
        
        // Test order creation
        hft::Order order;
        order.id = 12345;
        order.symbol = "MSFT";
        order.side = hft::OrderSide::BUY;
        order.type = hft::OrderType::LIMIT;
        order.price = 300.25;
        order.quantity = 500;
        order.status = hft::OrderStatus::PENDING;
        order.timestamp = std::chrono::high_resolution_clock::now();
        order.client_id = "CLIENT1";
        
        std::cout << "Created order: " << order.symbol 
                 << " " << (order.side == hft::OrderSide::BUY ? "BUY" : "SELL")
                 << " " << order.quantity 
                 << " @ " << order.price << std::endl;
        
        // Test market data conversion
        hft::Tick tick;
        tick.symbol = "AAPL";
        tick.bid_price = 149.95;
        tick.ask_price = 150.05;
        tick.bid_size = 1000;
        tick.ask_size = 1500;
        tick.last_price = 150.00;
        tick.last_size = 200;
        tick.timestamp = std::chrono::high_resolution_clock::now();
        
        auto market_data_msg = hft::MarketDataToFixConverter::tick_to_market_data_snapshot(tick);
        std::cout << "Converted tick to FIX market data message" << std::endl;
        std::cout << "Market data symbol: " << market_data_msg.get_field(hft::fix::tags::SYMBOL) << std::endl;
        std::cout << "Bid price: " << market_data_msg.get_double_field(hft::fix::tags::BID_PX) << std::endl;
        std::cout << "Ask price: " << market_data_msg.get_double_field(hft::fix::tags::ASK_PX) << std::endl;
        
        std::cout << "\nFIX Protocol example completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}