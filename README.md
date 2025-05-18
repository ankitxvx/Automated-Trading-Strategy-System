# Automated Trading Strategy System

This project implements an automated trading strategy system with comprehensive backtesting, analysis, and paper trading capabilities. The system is designed to evaluate trading strategies using historical data and allow for simulation of paper trades.

## Features

- **Moving Average Crossover Strategy**: Implements a classic trading strategy with configurable parameters
- **Comprehensive Backtesting**: Tests strategy performance over any historical period
- **Risk Management**: Includes stop-loss and take-profit mechanisms
- **Performance Metrics**: Calculates key metrics like returns, drawdowns, Sharpe ratio, and win rate
- **Market Period Analysis**: Special focus on performance during market crashes (e.g., COVID-19 period)
- **Parameter Optimization**: Find optimal strategy parameters
- **Paper Trading Simulation**: Test strategies in real-time with paper money
- **Interactive Visualizations**: View strategy performance with detailed charts

## Requirements

All requirements are specified in `requirements.txt`. Install them using:

```
pip install -r requirements.txt
```

## Project Structure

- `trading_strategy/`: Main package directory
  - `strategy.py`: Implementation of the Moving Average Crossover strategy
  - `backtest.py`: Backtesting and performance analysis
  - `paper_trading.py`: Paper trading simulation
  - `data/`: Directory for downloaded market data
  - `reports/`: Directory for generated reports and visualizations

## Usage

### Backtesting

Run a backtest with default parameters:

```
python trading_strategy/backtest.py
```

With custom parameters:

```
python trading_strategy/backtest.py --ticker AAPL --start 2019-01-01 --end 2023-01-01 --short 15 --long 45 --capital 20000 --stop 0.03 --take 0.09
```

Find optimal parameters:

```
python trading_strategy/backtest.py --ticker MSFT --optimize
```

### Paper Trading

Start paper trading with default parameters:

```
python trading_strategy/paper_trading.py
```

With custom parameters:

```
python trading_strategy/paper_trading.py --ticker AAPL --capital 20000 --interval 300 --short 15 --long 45
```

## Metrics Analyzed

The system tracks and reports the following key metrics:

1. **Returns**
   - Total return (%)
   - Annualized return (%)

2. **Drawdowns**
   - Maximum drawdown (%)
   - Maximum drawdown duration (days)

3. **Risk/Reward**
   - Volatility (%)
   - Sharpe ratio

4. **Trading Activity**
   - Number of trades
   - Win rate (%)
   - Average win amount
   - Average loss amount
   - Profit factor

5. **Market Crash Performance**
   - Performance during COVID-19 period (Feb-Apr 2020)
   - Specific market regime analysis

## Example Report

The system generates comprehensive reports including:
- Summary metrics
- Trade logs
- Portfolio value over time
- Drawdown analysis
- Interactive charts

## Important Considerations

- **Past Performance Disclaimer**: Historical performance does not guarantee future results
- **Risk Warning**: Trading involves risk and potential loss of capital
- **Paper Trading**: No real money is used in paper trading simulations
- **Strategy Limitations**: The moving average crossover strategy is presented for educational purposes
  
## Extending the System

To add new strategies:
1. Create a new strategy class similar to `MovingAverageCrossover`
2. Implement the `generate_signals` and `backtest` methods
3. Use the same backtesting framework with your new strategy

## License

This project is provided for educational purposes only.

## Author

Created for algorithmic trading strategy evaluation and simulation.
