#!/usr/bin/env python3
"""
Complete HFT Simulator Demo

This script demonstrates the full capabilities of the HFT simulator including:
- High-frequency market data generation
- Multiple trading strategies
- Performance monitoring
- Integration with existing trading framework
"""

import sys
import time
import argparse
from typing import List
from hft_integration import (
    HFTMarketDataFeed, 
    HFTStrategy, 
    SimpleMarketMaker, 
    IntegratedTradingSystem,
    PerformanceMonitor
)

class MomentumStrategy(HFTStrategy):
    """Simple momentum-based HFT strategy"""
    
    def __init__(self, lookback_window: int = 100, threshold: float = 0.001):
        super().__init__("MomentumStrategy")
        self.lookback_window = lookback_window
        self.threshold = threshold
        self.price_history = {}
        self.last_action = {}
    
    def on_tick(self, tick):
        symbol = tick['symbol']
        price = tick['last_price']
        
        # Initialize history
        if symbol not in self.price_history:
            self.price_history[symbol] = []
            self.last_action[symbol] = 'NONE'
        
        # Update price history
        self.price_history[symbol].append(price)
        if len(self.price_history[symbol]) > self.lookback_window:
            self.price_history[symbol].pop(0)
        
        # Need enough history to calculate momentum
        if len(self.price_history[symbol]) < 10:
            return
        
        # Calculate momentum (simple price change rate)
        recent_prices = self.price_history[symbol][-10:]
        momentum = (recent_prices[-1] - recent_prices[0]) / recent_prices[0]
        
        # Position limits
        current_position = self.get_position(symbol)
        max_position = 500
        
        # Trading logic
        if momentum > self.threshold and current_position < max_position:
            if self.last_action[symbol] != 'BUY':
                self.submit_order(symbol, 'BUY', 50, price * 1.001)  # Market take
                self.last_action[symbol] = 'BUY'
        
        elif momentum < -self.threshold and current_position > -max_position:
            if self.last_action[symbol] != 'SELL':
                self.submit_order(symbol, 'SELL', 50, price * 0.999)  # Market take
                self.last_action[symbol] = 'SELL'

class ArbitrageStrategy(HFTStrategy):
    """Simple arbitrage strategy between correlated symbols"""
    
    def __init__(self, symbol_pairs: List[tuple], threshold: float = 0.002):
        super().__init__("ArbitrageStrategy")
        self.symbol_pairs = symbol_pairs
        self.threshold = threshold
        self.latest_prices = {}
    
    def on_tick(self, tick):
        symbol = tick['symbol']
        price = tick['last_price']
        self.latest_prices[symbol] = price
        
        # Check each pair for arbitrage opportunities
        for symbol1, symbol2, expected_ratio in self.symbol_pairs:
            if symbol1 in self.latest_prices and symbol2 in self.latest_prices:
                self._check_arbitrage(symbol1, symbol2, expected_ratio)
    
    def _check_arbitrage(self, symbol1: str, symbol2: str, expected_ratio: float):
        price1 = self.latest_prices[symbol1]
        price2 = self.latest_prices[symbol2]
        
        actual_ratio = price1 / price2
        ratio_diff = abs(actual_ratio - expected_ratio) / expected_ratio
        
        if ratio_diff > self.threshold:
            # Arbitrage opportunity detected
            position_size = 25
            
            if actual_ratio > expected_ratio:
                # Symbol1 is overpriced relative to Symbol2
                self.submit_order(symbol1, 'SELL', position_size, price1 * 0.999)
                self.submit_order(symbol2, 'BUY', position_size, price2 * 1.001)
            else:
                # Symbol1 is underpriced relative to Symbol2
                self.submit_order(symbol1, 'BUY', position_size, price1 * 1.001)
                self.submit_order(symbol2, 'SELL', position_size, price2 * 0.999)

def run_comprehensive_demo(symbols: List[str], duration: int = 30):
    """Run a comprehensive demonstration of the HFT simulator"""
    
    print("=" * 60)
    print("HIGH-FREQUENCY TRADING SIMULATOR - COMPREHENSIVE DEMO")
    print("=" * 60)
    print(f"Symbols: {symbols}")
    print(f"Duration: {duration} seconds")
    print(f"Expected throughput: 10,000+ ticks per second")
    print(f"Target latency: < 100 microseconds")
    print()
    
    # Create the HFT system
    hft_system = IntegratedTradingSystem(symbols)
    
    # Add multiple strategies
    market_maker = SimpleMarketMaker(spread=0.015, max_position=1000)
    momentum_strategy = MomentumStrategy(lookback_window=50, threshold=0.0015)
    
    # Arbitrage pairs (symbol1, symbol2, expected_ratio)
    arb_pairs = [
        ("AAPL", "MSFT", 0.5),    # AAPL typically ~0.5x MSFT
        ("GOOGL", "TSLA", 12.0),  # GOOGL typically ~12x TSLA
    ]
    arbitrage_strategy = ArbitrageStrategy(arb_pairs, threshold=0.005)
    
    hft_system.hft_simulator.add_strategy(market_maker)
    hft_system.hft_simulator.add_strategy(momentum_strategy)
    hft_system.hft_simulator.add_strategy(arbitrage_strategy)
    
    print("Strategies loaded:")
    print("  1. Market Maker - Provides liquidity with bid/ask quotes")
    print("  2. Momentum Strategy - Follows price momentum")
    print("  3. Arbitrage Strategy - Exploits price discrepancies")
    print()
    
    # Performance tracking
    start_time = time.time()
    last_stats_time = start_time
    
    try:
        print("Starting HFT simulation...")
        hft_system.hft_simulator.start()
        
        while time.time() - start_time < duration:
            time.sleep(1)
            
            # Print periodic statistics
            current_time = time.time()
            if current_time - last_stats_time >= 5:
                stats = hft_system.hft_simulator.get_performance_stats()
                elapsed = current_time - start_time
                
                print(f"\n--- Statistics at {elapsed:.0f}s ---")
                print(f"Messages processed: {stats['performance']['total_messages']:,}")
                print(f"Throughput: {stats['performance']['messages_per_second']:,} msg/sec")
                print(f"Average latency: {stats['performance']['avg_latency_us']:.2f} μs")
                print(f"P99 latency: {stats['performance']['p99_latency_us']:.2f} μs")
                
                # Strategy statistics
                total_orders = sum(len(strategy.orders) for strategy in [market_maker, momentum_strategy, arbitrage_strategy])
                print(f"Total orders generated: {total_orders:,}")
                
                last_stats_time = current_time
        
    except KeyboardInterrupt:
        print("\nReceived interrupt signal, stopping...")
    
    finally:
        print("\nStopping HFT simulation...")
        hft_system.hft_simulator.stop()
    
    # Final comprehensive statistics
    final_stats = hft_system.hft_simulator.get_performance_stats()
    total_time = time.time() - start_time
    
    print("\n" + "=" * 60)
    print("FINAL PERFORMANCE REPORT")
    print("=" * 60)
    
    print(f"Total simulation time: {total_time:.2f} seconds")
    print(f"Total messages processed: {final_stats['performance']['total_messages']:,}")
    print(f"Average throughput: {final_stats['performance']['messages_per_second']:,} messages/second")
    print()
    
    print("LATENCY STATISTICS:")
    print(f"  Minimum latency: {final_stats['performance']['min_latency_us']:.2f} μs")
    print(f"  Average latency: {final_stats['performance']['avg_latency_us']:.2f} μs")
    print(f"  P99 latency: {final_stats['performance']['p99_latency_us']:.2f} μs")
    print(f"  Maximum latency: {final_stats['performance']['max_latency_us']:.2f} μs")
    print()
    
    # Strategy performance
    print("STRATEGY PERFORMANCE:")
    strategies = [
        ("Market Maker", market_maker),
        ("Momentum Strategy", momentum_strategy),
        ("Arbitrage Strategy", arbitrage_strategy)
    ]
    
    total_orders = 0
    for name, strategy in strategies:
        orders = len(strategy.orders)
        total_orders += orders
        print(f"  {name}: {orders:,} orders")
    
    print(f"  Total orders: {total_orders:,}")
    print()
    
    # Performance assessment
    print("PERFORMANCE ASSESSMENT:")
    
    # Throughput assessment
    target_throughput = 10000
    actual_throughput = final_stats['performance']['messages_per_second']
    throughput_rating = "✓ EXCELLENT" if actual_throughput >= target_throughput else "⚠ BELOW TARGET"
    print(f"  Throughput: {actual_throughput:,} msg/sec (target: {target_throughput:,}) - {throughput_rating}")
    
    # Latency assessment
    target_latency = 100  # microseconds
    actual_latency = final_stats['performance']['p99_latency_us']
    latency_rating = "✓ EXCELLENT" if actual_latency <= target_latency else "⚠ ABOVE TARGET"
    print(f"  P99 Latency: {actual_latency:.2f} μs (target: ≤{target_latency} μs) - {latency_rating}")
    
    # Order generation assessment
    target_orders_per_sec = total_orders / total_time
    orders_rating = "✓ ACTIVE" if target_orders_per_sec >= 100 else "ℹ MODERATE"
    print(f"  Order Rate: {target_orders_per_sec:.0f} orders/sec - {orders_rating}")
    
    print()
    print("Demo completed successfully!")
    print("=" * 60)

def main():
    parser = argparse.ArgumentParser(description='HFT Simulator Comprehensive Demo')
    parser.add_argument('--symbols', nargs='+', default=['AAPL', 'GOOGL', 'MSFT', 'TSLA'],
                      help='Trading symbols to simulate (default: AAPL GOOGL MSFT TSLA)')
    parser.add_argument('--duration', type=int, default=30,
                      help='Simulation duration in seconds (default: 30)')
    parser.add_argument('--quick', action='store_true',
                      help='Run a quick 10-second demo')
    
    args = parser.parse_args()
    
    if args.quick:
        duration = 10
        symbols = ['AAPL', 'MSFT']
        print("Running quick demo mode...")
    else:
        duration = args.duration
        symbols = args.symbols
    
    try:
        run_comprehensive_demo(symbols, duration)
    except Exception as e:
        print(f"Error running demo: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()