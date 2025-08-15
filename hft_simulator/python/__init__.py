"""
High-Frequency Trading Simulator Python Module

This module provides high-performance market data simulation, FIX protocol support,
and thread pools for building HFT systems.
"""

import time
import threading
from typing import List, Optional, Callable, Dict, Any
from dataclasses import dataclass
from datetime import datetime, timedelta

try:
    from . import hft_simulator_py as _cpp
except ImportError:
    # Fallback for development
    import hft_simulator_py as _cpp

__version__ = "1.0.0"

# Re-export C++ enums and classes
OrderSide = _cpp.OrderSide
OrderType = _cpp.OrderType  
OrderStatus = _cpp.OrderStatus
Tick = _cpp.Tick
Trade = _cpp.Trade
Order = _cpp.Order
LatencyStats = _cpp.LatencyStats
ThroughputStats = _cpp.ThroughputStats

# Re-export C++ classes
MarketDataSimulator = _cpp.MarketDataSimulator
SimulatedMarketDataFeed = _cpp.SimulatedMarketDataFeed
FixMessage = _cpp.FixMessage
FixEngine = _cpp.FixEngine
FixProtocolAdapter = _cpp.FixProtocolAdapter
ThreadPool = _cpp.ThreadPool
PerformanceMonitor = _cpp.PerformanceMonitor

# FIX protocol constants
fix = _cpp.fix

@dataclass
class MarketDataConfig:
    """Configuration for market data simulation"""
    symbols: List[str]
    initial_prices: Dict[str, float]
    volatility: float = 0.001
    tick_frequency: int = 1000  # ticks per second
    
@dataclass
class PerformanceMetrics:
    """Performance metrics container"""
    latency_stats: LatencyStats
    throughput_stats: ThroughputStats
    timestamp: datetime

class HFTSimulator:
    """
    High-level interface for the HFT Simulator
    
    This class provides a simplified interface to the C++ components,
    making it easy to set up and run high-frequency trading simulations.
    """
    
    def __init__(self, config: MarketDataConfig):
        self.config = config
        self.market_feed = SimulatedMarketDataFeed()
        self.fix_adapter = None
        self.thread_pool = ThreadPool(4)  # Default to 4 threads
        self.performance_monitor = PerformanceMonitor()
        self.running = False
        
        # Set up market data
        for symbol, price in config.initial_prices.items():
            self.market_feed.set_initial_price(symbol, price)
            self.market_feed.subscribe(symbol)
        
        self.market_feed.set_volatility(config.volatility)
        
        # Callbacks
        self.tick_handlers: List[Callable[[Tick], None]] = []
        self.order_handlers: List[Callable[[Order], None]] = []
        
    def add_tick_handler(self, handler: Callable[[Tick], None]):
        """Add a callback for market data ticks"""
        self.tick_handlers.append(handler)
        
    def add_order_handler(self, handler: Callable[[Order], None]):
        """Add a callback for order events"""
        self.order_handlers.append(handler)
        
    def setup_fix_connection(self, exchange_name: str, sender_id: str, target_id: str):
        """Set up FIX protocol connection"""
        self.fix_adapter = FixProtocolAdapter(exchange_name, sender_id, target_id)
        
    def start(self):
        """Start the HFT simulator"""
        if self.running:
            return
            
        self.running = True
        self.performance_monitor.start_monitoring()
        self.market_feed.start_simulation()
        
        # Start market data processing thread
        self.thread_pool.submit_detached(self._process_market_data)
        
        if self.fix_adapter:
            self.fix_adapter.connect()
            
    def stop(self):
        """Stop the HFT simulator"""
        if not self.running:
            return
            
        self.running = False
        self.market_feed.stop_simulation()
        
        if self.fix_adapter:
            self.fix_adapter.disconnect()
            
    def submit_order(self, symbol: str, side: OrderSide, quantity: int, 
                    price: float, order_type: OrderType = OrderType.LIMIT) -> int:
        """Submit a trading order"""
        order = Order()
        order.id = int(time.time() * 1000000)  # Microsecond timestamp as ID
        order.symbol = symbol
        order.side = side
        order.type = order_type
        order.price = price
        order.quantity = quantity
        order.status = OrderStatus.PENDING
        order.timestamp = _cpp.get_current_timestamp()
        
        if self.fix_adapter and self.fix_adapter.is_connected():
            self.fix_adapter.submit_order(order)
        
        # Notify handlers
        for handler in self.order_handlers:
            try:
                handler(order)
            except Exception as e:
                print(f"Error in order handler: {e}")
                
        return order.id
        
    def get_performance_metrics(self) -> PerformanceMetrics:
        """Get current performance metrics"""
        return PerformanceMetrics(
            latency_stats=self.performance_monitor.get_latency_stats(),
            throughput_stats=self.performance_monitor.get_throughput_stats(),
            timestamp=datetime.now()
        )
        
    def get_latest_tick(self, symbol: str) -> Optional[Tick]:
        """Get the latest tick for a symbol"""
        tick = self.market_feed.get_tick()
        if tick and tick.symbol == symbol:
            return tick
        return None
        
    def _process_market_data(self):
        """Internal method to process market data"""
        while self.running:
            tick = self.market_feed.get_tick()
            if tick:
                start_time = _cpp.get_current_timestamp()
                
                # Process tick with handlers
                for handler in self.tick_handlers:
                    try:
                        handler(tick)
                    except Exception as e:
                        print(f"Error in tick handler: {e}")
                
                # Record latency
                end_time = _cpp.get_current_timestamp()
                latency = end_time - start_time
                self.performance_monitor.record_latency(latency)
                self.performance_monitor.record_operation(64)  # Approximate tick size
            else:
                time.sleep(0.001)  # 1ms sleep if no data

class StrategyBase:
    """Base class for trading strategies"""
    
    def __init__(self, simulator: HFTSimulator):
        self.simulator = simulator
        self.positions: Dict[str, int] = {}
        self.pnl = 0.0
        
        # Register callbacks
        simulator.add_tick_handler(self.on_tick)
        simulator.add_order_handler(self.on_order)
        
    def on_tick(self, tick: Tick):
        """Override this method to implement strategy logic"""
        pass
        
    def on_order(self, order: Order):
        """Override this method to handle order events"""
        pass
        
    def buy(self, symbol: str, quantity: int, price: float) -> int:
        """Place a buy order"""
        return self.simulator.submit_order(symbol, OrderSide.BUY, quantity, price)
        
    def sell(self, symbol: str, quantity: int, price: float) -> int:
        """Place a sell order"""
        return self.simulator.submit_order(symbol, OrderSide.SELL, quantity, price)
        
    def get_position(self, symbol: str) -> int:
        """Get current position in a symbol"""
        return self.positions.get(symbol, 0)

class SimpleMarketMaker(StrategyBase):
    """Simple market making strategy example"""
    
    def __init__(self, simulator: HFTSimulator, spread: float = 0.01, quantity: int = 100):
        super().__init__(simulator)
        self.spread = spread
        self.quantity = quantity
        self.last_quotes: Dict[str, Tick] = {}
        
    def on_tick(self, tick: Tick):
        """Update quotes based on new market data"""
        self.last_quotes[tick.symbol] = tick
        
        # Simple market making: quote around mid price
        mid_price = (tick.bid_price + tick.ask_price) / 2.0
        
        bid_price = mid_price - self.spread / 2.0
        ask_price = mid_price + self.spread / 2.0
        
        # Place quotes (in real implementation, would manage existing orders)
        if self.get_position(tick.symbol) <= 0:  # Don't accumulate too much inventory
            self.buy(tick.symbol, self.quantity, bid_price)
            
        if self.get_position(tick.symbol) >= 0:
            self.sell(tick.symbol, self.quantity, ask_price)

def create_example_config() -> MarketDataConfig:
    """Create an example configuration for testing"""
    return MarketDataConfig(
        symbols=["AAPL", "GOOGL", "MSFT", "TSLA"],
        initial_prices={
            "AAPL": 150.0,
            "GOOGL": 2500.0,
            "MSFT": 300.0,
            "TSLA": 200.0
        },
        volatility=0.002,
        tick_frequency=1000
    )

# Utility functions
def print_performance_metrics(metrics: PerformanceMetrics):
    """Print performance metrics in a readable format"""
    print(f"=== Performance Metrics at {metrics.timestamp} ===")
    
    latency = metrics.latency_stats
    print(f"Latency Statistics:")
    print(f"  Min: {_cpp.duration_to_microseconds(latency.min_latency):.2f} μs")
    print(f"  Max: {_cpp.duration_to_microseconds(latency.max_latency):.2f} μs")
    print(f"  Avg: {_cpp.duration_to_microseconds(latency.avg_latency):.2f} μs")
    print(f"  P99: {_cpp.duration_to_microseconds(latency.p99_latency):.2f} μs")
    print(f"  Total Messages: {latency.total_messages}")
    
    throughput = metrics.throughput_stats
    print(f"Throughput Statistics:")
    print(f"  Messages/sec: {throughput.messages_per_second:,.0f}")
    print(f"  Bytes/sec: {throughput.bytes_per_second:,.0f}")
    print(f"  Total Messages: {throughput.total_messages:,.0f}")
    print(f"  Total Bytes: {throughput.total_bytes:,.0f}")

def run_example():
    """Run a simple example of the HFT simulator"""
    print("Starting HFT Simulator Example...")
    
    # Create configuration
    config = create_example_config()
    
    # Create simulator
    simulator = HFTSimulator(config)
    
    # Add a simple market maker strategy
    strategy = SimpleMarketMaker(simulator, spread=0.02, quantity=100)
    
    # Add performance monitoring
    def print_stats():
        while simulator.running:
            time.sleep(5)
            metrics = simulator.get_performance_metrics()
            print_performance_metrics(metrics)
    
    stats_thread = threading.Thread(target=print_stats, daemon=True)
    
    try:
        # Start simulator
        simulator.start()
        stats_thread.start()
        
        print("Simulator running... Press Ctrl+C to stop")
        time.sleep(30)  # Run for 30 seconds
        
    except KeyboardInterrupt:
        print("\nStopping simulator...")
    finally:
        simulator.stop()
        print("Simulator stopped.")

if __name__ == "__main__":
    run_example()