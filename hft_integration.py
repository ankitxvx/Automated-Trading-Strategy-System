"""
HFT Simulator Integration with Existing Trading Strategy System

This module provides a bridge between the existing Python trading strategy system
and the new high-performance C++ components.
"""

import time
import threading
from typing import Dict, List, Optional, Callable
from datetime import datetime
import pandas as pd
import numpy as np

# Try to import the existing trading strategy components
try:
    from trading_strategy.strategy import MovingAverageCrossover
    from trading_strategy.paper_trading import PaperTrader
except ImportError:
    print("Warning: Could not import existing trading strategy components")
    MovingAverageCrossover = None
    PaperTrader = None

class HFTMarketDataFeed:
    """
    High-frequency market data feed that can integrate with existing strategies
    
    This provides a pure Python implementation for now, with the structure
    ready for C++ integration when Python bindings are available.
    """
    
    def __init__(self, symbols: List[str], volatility: float = 0.001):
        self.symbols = symbols
        self.volatility = volatility
        self.running = False
        self.subscribers = []
        self.current_prices = {}
        self.tick_count = 0
        
        # Initialize with some reasonable prices
        initial_prices = {
            "AAPL": 150.0,
            "GOOGL": 2500.0,
            "MSFT": 300.0,
            "TSLA": 200.0,
            "SPY": 400.0,
            "QQQ": 350.0,
            "NVDA": 800.0,
            "AMD": 100.0
        }
        
        for symbol in symbols:
            self.current_prices[symbol] = initial_prices.get(symbol, 100.0)
    
    def subscribe(self, callback: Callable):
        """Subscribe to market data updates"""
        self.subscribers.append(callback)
    
    def start(self):
        """Start the market data simulation"""
        if self.running:
            return
            
        self.running = True
        self.thread = threading.Thread(target=self._generate_data, daemon=True)
        self.thread.start()
    
    def stop(self):
        """Stop the market data simulation"""
        self.running = False
        if hasattr(self, 'thread'):
            self.thread.join(timeout=1.0)
    
    def _generate_data(self):
        """Generate high-frequency market data"""
        while self.running:
            for symbol in self.symbols:
                # Generate a tick
                tick = self._generate_tick(symbol)
                
                # Notify subscribers
                for callback in self.subscribers:
                    try:
                        callback(tick)
                    except Exception as e:
                        print(f"Error in market data callback: {e}")
                
                self.tick_count += 1
            
            # Sleep for microsecond-level precision
            time.sleep(0.0001)  # 100 microseconds = 10,000 ticks/second
    
    def _generate_tick(self, symbol: str) -> Dict:
        """Generate a single market data tick"""
        current_price = self.current_prices[symbol]
        
        # Add some randomness
        change = np.random.normal(0, self.volatility * current_price)
        new_price = max(0.01, current_price + change)
        self.current_prices[symbol] = new_price
        
        # Create spread
        spread = new_price * 0.001  # 0.1% spread
        bid_price = new_price - spread / 2
        ask_price = new_price + spread / 2
        
        return {
            'symbol': symbol,
            'timestamp': time.time_ns(),  # Nanosecond precision
            'bid_price': bid_price,
            'ask_price': ask_price,
            'bid_size': np.random.randint(100, 1000),
            'ask_size': np.random.randint(100, 1000),
            'last_price': new_price,
            'last_size': np.random.randint(10, 500)
        }
    
    def get_stats(self) -> Dict:
        """Get performance statistics"""
        return {
            'total_ticks': self.tick_count,
            'symbols': len(self.symbols),
            'running': self.running
        }

class HFTSimulatorInterface:
    """
    Interface class that bridges the HFT simulator with existing trading strategies
    """
    
    def __init__(self, symbols: List[str], strategy_config: Optional[Dict] = None):
        self.symbols = symbols
        self.strategy_config = strategy_config or {}
        self.market_feed = HFTMarketDataFeed(symbols)
        self.strategies = []
        self.performance_monitor = PerformanceMonitor()
        
        # Market data subscription
        self.market_feed.subscribe(self._on_market_data)
    
    def add_strategy(self, strategy):
        """Add a trading strategy to the simulator"""
        self.strategies.append(strategy)
    
    def start(self):
        """Start the HFT simulation"""
        print("Starting HFT Simulator...")
        self.performance_monitor.start()
        self.market_feed.start()
    
    def stop(self):
        """Stop the HFT simulation"""
        print("Stopping HFT Simulator...")
        self.market_feed.stop()
        self.performance_monitor.stop()
    
    def _on_market_data(self, tick: Dict):
        """Handle incoming market data"""
        start_time = time.time_ns()
        
        # Process tick with all strategies
        for strategy in self.strategies:
            try:
                if hasattr(strategy, 'on_tick'):
                    strategy.on_tick(tick)
            except Exception as e:
                print(f"Error in strategy processing: {e}")
        
        # Record latency
        end_time = time.time_ns()
        latency_ns = end_time - start_time
        self.performance_monitor.record_latency(latency_ns)
    
    def get_performance_stats(self) -> Dict:
        """Get performance statistics"""
        market_stats = self.market_feed.get_stats()
        perf_stats = self.performance_monitor.get_stats()
        
        return {
            'market_data': market_stats,
            'performance': perf_stats
        }

class PerformanceMonitor:
    """Monitor and track performance metrics"""
    
    def __init__(self):
        self.latencies = []
        self.start_time = None
        self.message_count = 0
        self.running = False
        self.lock = threading.Lock()
    
    def start(self):
        """Start performance monitoring"""
        self.start_time = time.time()
        self.running = True
    
    def stop(self):
        """Stop performance monitoring"""
        self.running = False
    
    def record_latency(self, latency_ns: int):
        """Record a latency measurement"""
        if not self.running:
            return
            
        with self.lock:
            self.latencies.append(latency_ns)
            self.message_count += 1
            
            # Keep only recent measurements to avoid memory issues
            if len(self.latencies) > 100000:
                self.latencies = self.latencies[-50000:]
    
    def get_stats(self) -> Dict:
        """Get current performance statistics"""
        with self.lock:
            if not self.latencies:
                return {
                    'min_latency_us': 0,
                    'max_latency_us': 0,
                    'avg_latency_us': 0,
                    'p99_latency_us': 0,
                    'total_messages': 0,
                    'messages_per_second': 0
                }
            
            latencies_us = [lat / 1000 for lat in self.latencies]  # Convert to microseconds
            latencies_us.sort()
            
            min_lat = min(latencies_us)
            max_lat = max(latencies_us)
            avg_lat = sum(latencies_us) / len(latencies_us)
            p99_lat = latencies_us[int(0.99 * len(latencies_us))]
            
            # Calculate throughput
            elapsed = time.time() - (self.start_time or time.time())
            msgs_per_sec = self.message_count / max(elapsed, 1.0)
            
            return {
                'min_latency_us': min_lat,
                'max_latency_us': max_lat,
                'avg_latency_us': avg_lat,
                'p99_latency_us': p99_lat,
                'total_messages': self.message_count,
                'messages_per_second': int(msgs_per_sec)
            }

class HFTStrategy:
    """Base class for HFT strategies"""
    
    def __init__(self, name: str):
        self.name = name
        self.positions = {}
        self.orders = []
        self.pnl = 0.0
    
    def on_tick(self, tick: Dict):
        """Override this method to implement strategy logic"""
        pass
    
    def submit_order(self, symbol: str, side: str, quantity: int, price: float):
        """Submit an order (simulated)"""
        order = {
            'id': len(self.orders) + 1,
            'symbol': symbol,
            'side': side,
            'quantity': quantity,
            'price': price,
            'timestamp': time.time_ns(),
            'status': 'PENDING'
        }
        self.orders.append(order)
        print(f"[{self.name}] Order submitted: {side} {quantity} {symbol} @ {price:.2f}")
        return order['id']
    
    def get_position(self, symbol: str) -> int:
        """Get current position in a symbol"""
        return self.positions.get(symbol, 0)

class SimpleMarketMaker(HFTStrategy):
    """Simple market making strategy"""
    
    def __init__(self, spread: float = 0.01, max_position: int = 1000):
        super().__init__("SimpleMarketMaker")
        self.spread = spread
        self.max_position = max_position
        self.last_quotes = {}
    
    def on_tick(self, tick: Dict):
        """Update quotes based on new market data"""
        symbol = tick['symbol']
        
        # Simple risk check
        current_position = self.get_position(symbol)
        if abs(current_position) >= self.max_position:
            return
        
        # Calculate quote prices
        mid_price = (tick['bid_price'] + tick['ask_price']) / 2.0
        bid_price = mid_price - self.spread / 2.0
        ask_price = mid_price + self.spread / 2.0
        
        # Place quotes (simplified - in real implementation would manage existing orders)
        if current_position <= 0:  # Can buy more
            self.submit_order(symbol, 'BUY', 100, bid_price)
        
        if current_position >= 0:  # Can sell more
            self.submit_order(symbol, 'SELL', 100, ask_price)
        
        self.last_quotes[symbol] = tick

class IntegratedTradingSystem:
    """
    Integration of HFT simulator with existing trading strategy system
    """
    
    def __init__(self, symbols: List[str]):
        self.symbols = symbols
        self.hft_simulator = HFTSimulatorInterface(symbols)
        
        # Add HFT strategies
        market_maker = SimpleMarketMaker(spread=0.02)
        self.hft_simulator.add_strategy(market_maker)
        
        # Integration with existing system (if available)
        if MovingAverageCrossover:
            self.traditional_strategy = self._create_traditional_strategy()
        else:
            self.traditional_strategy = None
    
    def _create_traditional_strategy(self):
        """Create a traditional moving average strategy"""
        # This would integrate with the existing strategy system
        return MovingAverageCrossover(
            short_window=20,
            long_window=50,
            initial_capital=100000,
            stop_loss_pct=0.05,
            take_profit_pct=0.1,
            position_size_pct=0.2
        )
    
    def run_simulation(self, duration_seconds: int = 30):
        """Run the integrated trading simulation"""
        print(f"Running integrated trading simulation for {duration_seconds} seconds...")
        
        try:
            # Start HFT simulation
            self.hft_simulator.start()
            
            # Run for specified duration
            start_time = time.time()
            while time.time() - start_time < duration_seconds:
                time.sleep(1)
                
                # Print periodic stats
                if int(time.time() - start_time) % 5 == 0:
                    stats = self.hft_simulator.get_performance_stats()
                    print(f"Stats: {stats['performance']['total_messages']} messages, "
                          f"{stats['performance']['messages_per_second']} msg/s, "
                          f"avg latency: {stats['performance']['avg_latency_us']:.2f} μs")
            
        finally:
            self.hft_simulator.stop()
            
        # Final statistics
        final_stats = self.hft_simulator.get_performance_stats()
        print(f"\n=== Final Performance Statistics ===")
        print(f"Total messages processed: {final_stats['performance']['total_messages']}")
        print(f"Average latency: {final_stats['performance']['avg_latency_us']:.2f} μs")
        print(f"P99 latency: {final_stats['performance']['p99_latency_us']:.2f} μs")
        print(f"Throughput: {final_stats['performance']['messages_per_second']} messages/second")

def run_hft_example():
    """Run an example of the HFT simulator"""
    symbols = ["AAPL", "GOOGL", "MSFT", "TSLA"]
    
    print("=== High-Frequency Trading Simulator Demo ===")
    print(f"Symbols: {symbols}")
    
    # Create and run integrated system
    system = IntegratedTradingSystem(symbols)
    system.run_simulation(duration_seconds=10)

if __name__ == "__main__":
    run_hft_example()