# High-Frequency Trading Simulator

A production-ready high-frequency trading (HFT) simulator built with C++ and Python, featuring microsecond-level latency, FIX protocol support, and advanced performance monitoring.

## Features

### Core HFT Components (C++)
- **Market Data Simulator**: High-performance market data generation with configurable volatility
- **FIX Protocol Adapter**: Complete FIX 4.4 implementation for exchange connectivity simulation
- **Lock-free Data Structures**: SPSC and MPSC queues for ultra-low latency message passing
- **Thread Pools**: CPU-optimized thread management with affinity control
- **Memory Pools**: Lock-free object allocation for reduced GC pressure
- **Performance Monitoring**: Microsecond-precision latency and throughput tracking

### Trading Strategy Integration (Python)
- **Existing Strategy Support**: Full compatibility with the current Moving Average Crossover system
- **HFT Strategy Framework**: Base classes for implementing high-frequency strategies
- **Real-time Paper Trading**: Live simulation with microsecond timestamping
- **Performance Analytics**: Comprehensive reporting and visualization

### Advanced Features
- **Microsecond Latency**: End-to-end processing times under 100 microseconds
- **High Throughput**: Support for 10,000+ ticks per second per symbol
- **CPU Optimization**: Thread affinity, cache optimization, and NUMA awareness
- **Production Ready**: Memory management, error handling, and monitoring

## Project Structure

```
├── hft_simulator/               # C++ HFT simulator core
│   ├── cpp/
│   │   ├── include/            # Header files
│   │   └── src/               # Implementation files
│   ├── python/                # Python bindings (pybind11)
│   ├── examples/              # C++ usage examples
│   ├── tests/                 # Unit and performance tests
│   └── CMakeLists.txt         # Build configuration
├── trading_strategy/           # Existing Python trading system
├── hft_integration.py          # Python integration module
├── examples/                   # Usage examples
└── README.md
```

## Quick Start

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install cmake g++ python3-dev

# Install Python dependencies
pip install -r requirements.txt pybind11
```

### Building the C++ Components

```bash
cd hft_simulator
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON
make -j$(nproc)
```

### Running Examples

```bash
# C++ examples
./examples/fix_protocol_example
./examples/performance_benchmark

# Python integration
cd ../../
python hft_integration.py
```

### Basic Usage

```python
from hft_integration import IntegratedTradingSystem

# Create HFT system with symbols
symbols = ["AAPL", "GOOGL", "MSFT", "TSLA"]
system = IntegratedTradingSystem(symbols)

# Run simulation
system.run_simulation(duration_seconds=30)
```

## Architecture

### C++ Core Components

#### Market Data Simulator
```cpp
hft::MarketDataSimulator simulator;
simulator.add_symbol("AAPL", 150.0);
simulator.set_volatility(0.001);
simulator.start();

hft::Tick tick;
while (simulator.get_next_tick(tick)) {
    // Process tick with microsecond precision
}
```

#### FIX Protocol
```cpp
hft::FixEngine engine("CLIENT1", "EXCHANGE1");
engine.logon();

hft::Order order;
order.symbol = "AAPL";
order.side = hft::OrderSide::BUY;
order.quantity = 100;
order.price = 150.50;

engine.send_new_order(order);
```

#### Lock-free Queues
```cpp
hft::SPSCQueue<Tick, 1024> queue;

// Producer thread
queue.push(tick);

// Consumer thread
Tick received_tick;
if (queue.pop(received_tick)) {
    process_tick(received_tick);
}
```

### Performance Characteristics

| Component | Latency | Throughput |
|-----------|---------|------------|
| Lock-free Queue | < 100 ns | 10M+ msg/sec |
| FIX Message Processing | < 1 μs | 100K+ msg/sec |
| Market Data Generation | < 10 μs | 50K+ ticks/sec |
| End-to-end Processing | < 100 μs | 10K+ orders/sec |

## HFT Strategy Development

### Creating a Strategy

```python
from hft_integration import HFTStrategy

class MyHFTStrategy(HFTStrategy):
    def __init__(self):
        super().__init__("MyStrategy")
        self.threshold = 0.001
    
    def on_tick(self, tick):
        # High-frequency strategy logic
        symbol = tick['symbol']
        price = tick['last_price']
        
        # Example: Simple momentum strategy
        if self.should_buy(tick):
            self.submit_order(symbol, 'BUY', 100, price)
        elif self.should_sell(tick):
            self.submit_order(symbol, 'SELL', 100, price)
```

### Integration with Existing Strategies

```python
# Use existing Moving Average strategy with HFT data feed
from trading_strategy.strategy import MovingAverageCrossover
from hft_integration import HFTMarketDataFeed

# Create HFT data feed
feed = HFTMarketDataFeed(["AAPL", "GOOGL"])

# Traditional strategy with HFT data
strategy = MovingAverageCrossover(
    short_window=20,
    long_window=50,
    initial_capital=100000
)

# Bridge HFT feed to traditional strategy
feed.subscribe(lambda tick: strategy.process_hft_tick(tick))
feed.start()
```

## Performance Monitoring

### Real-time Metrics

```python
# Get performance statistics
stats = simulator.get_performance_stats()

print(f"Latency - Min: {stats['min_latency_us']:.2f} μs")
print(f"Latency - P99: {stats['p99_latency_us']:.2f} μs")
print(f"Throughput: {stats['messages_per_second']} msg/sec")
```

### Benchmarking

```bash
# Run performance tests
./tests/performance_tests

# Expected results on modern hardware:
# - Queue latency P99: < 1,000 ns
# - Market data throughput: > 10,000 ticks/second
# - End-to-end latency: < 100,000 ns
```

## Advanced Configuration

### CPU Optimization

```cpp
// Set thread affinity for optimal performance
hft::CpuOptimizer::set_current_thread_affinity(0);
hft::CpuOptimizer::set_realtime_priority(thread);

// Memory optimization
hft::CpuOptimizer::prefetch_memory(data, size);
```

### Memory Pool Configuration

```cpp
// Pre-allocated object pool for zero-allocation trading
hft::MemoryPool<Order, 10000> order_pool;

Order* order = order_pool.allocate();
// Use order...
order_pool.deallocate(order);
```

## Production Deployment

### System Requirements
- **CPU**: Modern x86_64 with multiple cores
- **Memory**: 8GB+ RAM for large-scale simulations
- **OS**: Linux (Ubuntu 20.04+ recommended)
- **Network**: Low-latency network interface for real trading

### Performance Tuning
- Enable CPU frequency scaling
- Configure NUMA topology
- Set real-time scheduling priorities
- Optimize network stack parameters

### Monitoring and Logging
- Built-in performance metrics collection
- Latency histograms and percentile tracking
- Memory usage monitoring
- Custom logging integration

## Testing

```bash
# Run all tests
cd hft_simulator/build
make test

# Run specific test suites
./tests/basic_tests        # Functionality tests
./tests/performance_tests  # Performance benchmarks
```

## Integration Examples

### With Existing Paper Trading

```python
from trading_strategy.paper_trading import PaperTrader
from hft_integration import HFTMarketDataFeed

# Create HFT-enabled paper trader
hft_feed = HFTMarketDataFeed(["SPY"])
trader = PaperTrader(strategy, "SPY", 10000)

# Bridge HFT data to paper trader
hft_feed.subscribe(trader.process_hft_tick)
```

### Risk Management

```python
class RiskManager:
    def __init__(self, max_position=1000, max_loss=10000):
        self.max_position = max_position
        self.max_loss = max_loss
    
    def check_order(self, order, current_position, current_pnl):
        # Position limit check
        new_position = current_position + order['quantity']
        if abs(new_position) > self.max_position:
            return False
        
        # Loss limit check
        if current_pnl < -self.max_loss:
            return False
        
        return True
```

## Roadmap

- [ ] WebSocket market data integration
- [ ] Multi-asset portfolio optimization
- [ ] Machine learning strategy framework
- [ ] Real exchange connectivity
- [ ] Cloud deployment templates
- [ ] Advanced risk management
- [ ] Market microstructure analysis

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Run performance benchmarks
5. Submit a pull request

## License

This project is provided for educational and research purposes. Please review the license terms before using in production.

## Support

For questions and support:
- Create an issue on GitHub
- Review the examples and documentation
- Check the performance test results

---

**Note**: This is a high-performance trading simulator designed for educational purposes. Real trading involves significant financial risk and should only be undertaken with proper risk management and regulatory compliance.