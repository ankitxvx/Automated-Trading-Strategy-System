"""
Backtesting and Reporting Script for Trading Strategy
- Downloads historical data
- Runs strategy backtest
- Generates comprehensive performance report
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import yfinance as yf
from datetime import datetime, timedelta
import os
import sys # Add this line

# Add project root to Python path to allow 'from trading_strategy.strategy ...'
# when running this script directly.
_current_file_directory = os.path.dirname(os.path.abspath(__file__))
_project_root_directory = os.path.dirname(_current_file_directory)
if _project_root_directory not in sys.path:
    sys.path.insert(0, _project_root_directory)

from pathlib import Path # This is line 14 in your full file
from trading_strategy.strategy import MovingAverageCrossover # This is line 15 (the failing import)
import argparse
from tabulate import tabulate

def download_data(ticker, start_date, end_date, data_dir='data'):
    """Download historical data for a ticker symbol."""
    # Create data directory if it doesn't exist
    os.makedirs(data_dir, exist_ok=True)
    
    # File path for cached data
    file_path = os.path.join(data_dir, f"{ticker}_{start_date}_{end_date}.csv")
    
    # For demonstration purposes, let's generate synthetic data
    # to avoid issues with the yfinance API
    print(f"Generating synthetic data for {ticker} for demonstration purposes")
    
    # Import required modules
    import numpy as np
    from datetime import datetime, timedelta
    
    # Generate dates
    start_dt = datetime.strptime(start_date, "%Y-%m-%d")
    end_dt = datetime.strptime(end_date, "%Y-%m-%d") 
    date_range = pd.date_range(start=start_dt, end=end_dt, freq='B')
    
    # Generate price data with random walk (8% annual return, 15% volatility)
    n_days = len(date_range)
    daily_return_mean = 0.08 / 252
    daily_return_std = 0.15 / np.sqrt(252)
    
    np.random.seed(42)  # For reproducibility
    daily_returns = np.random.normal(daily_return_mean, daily_return_std, n_days)
    
    # Add a COVID crash simulation for Feb-Apr 2020
    covid_start = datetime(2020, 2, 15)
    covid_end = datetime(2020, 3, 23)
    covid_recovery = datetime(2020, 6, 1)
    
    # Apply COVID effect to returns
    for i, date in enumerate(date_range):
        if covid_start <= date <= covid_end:
            # Dramatic decline during COVID crash
            daily_returns[i] -= 0.03
        elif covid_end < date <= covid_recovery:
            # Recovery phase
            daily_returns[i] += 0.02
    
    price = 100 * (1 + np.cumsum(daily_returns))
    
    # Create DataFrame
    data = pd.DataFrame({
        'Open': price * 0.99,
        'High': price * 1.01,
        'Low': price * 0.98,
        'Close': price,
        'Adj Close': price,
        'Volume': np.random.randint(1000000, 10000000, n_days)
    }, index=date_range)
    
    # Save to file for caching
    data.to_csv(file_path)
    
    return data

def print_colored_metrics(metrics):
    """Print metrics with colored output for key values."""
    # Convert metrics to a list of lists for tabulate
    table_data = []
    for metric, value in metrics.items():
        table_data.append([metric, f"{value:.2f}"])
    
    # Print the table
    print(tabulate(table_data, headers=["Metric", "Value"], tablefmt="fancy_grid"))


def run_backtest(ticker="SPY", start_date="2018-01-01", end_date="2023-01-01", 
                short_window=20, long_window=50, initial_capital=10000,
                stop_loss_pct=0.05, take_profit_pct=0.1, position_size_pct=0.4,
                intraday=False, days_to_hold=60, use_trailing_stop=True,
                trailing_stop_activation=0.02, trailing_stop_distance=0.02):
    """Run backtest with specified parameters."""    # Download data
    data = download_data(ticker, start_date, end_date)
    
    # Initialize strategy
    strategy = MovingAverageCrossover(
        short_window=short_window,
        long_window=long_window,
        initial_capital=initial_capital,
        stop_loss_pct=stop_loss_pct,
        take_profit_pct=take_profit_pct,
        position_size_pct=position_size_pct,
        use_trailing_stop=use_trailing_stop,
        trailing_stop_activation=trailing_stop_activation,
        trailing_stop_distance=trailing_stop_distance
    )
    
    # Run backtest
    print(f"\nRunning backtest for {ticker} from {start_date} to {end_date}...")
    portfolio_df, trades_df = strategy.backtest(data, intraday=intraday, days_to_hold=days_to_hold) # MODIFIED: Pass days_to_hold
    
    # Calculate metrics
    metrics = strategy.calculate_metrics()
    
    # Print metrics
    print("\nBACKTEST RESULTS:")
    print_colored_metrics(metrics)
    
    # Analyze COVID period
    covid_analysis = strategy.analyze_market_periods(data, '2020-02-01', '2020-04-30')
    print("\nCOVID-19 PERIOD PERFORMANCE (Feb-Apr 2020):")
    print(f"Return: {covid_analysis['return']:.2f}%")
    print(f"Trades: {covid_analysis['trades']}")
    print(f"Win Rate: {covid_analysis['win_rate']:.2f}%")
    
    # Generate comprehensive report
    report_path = strategy.generate_report(data)
    print(f"\nDetailed report generated: {report_path}")
    
    return strategy

def evaluate_multiple_parameters(ticker="SPY", start_date="2018-01-01", end_date="2023-01-01", days_to_hold=60): # MODIFIED: Added days_to_hold
    """Test multiple parameter combinations and find the best one."""
    # Define parameter ranges to test
    short_windows = [5, 10, 15, 20, 25, 30]
    long_windows = [35, 40, 50, 60, 80, 100]
    stop_losses = [0.02, 0.03, 0.05, 0.07, 0.10]
    
    results = []
    
    # Download data once
    data = download_data(ticker, start_date, end_date)
    
    print(f"Evaluating {len(short_windows) * len(long_windows) * len(stop_losses)} parameter combinations...")
    
    for short_window in short_windows:
        for long_window in long_windows:
            # Skip invalid combinations
            if short_window >= long_window:
                continue
                
            for stop_loss in stop_losses:
                # Initialize strategy
                strategy = MovingAverageCrossover(
                    short_window=short_window,
                    long_window=long_window,
                    initial_capital=10000, # Using a fixed capital for optimization runs for simplicity
                    stop_loss_pct=stop_loss,
                    take_profit_pct=stop_loss * 2,  # 2:1 reward-risk ratio
                    position_size_pct=0.2 # Using a fixed position size for optimization
                )
                
                # Run backtest
                strategy.backtest(data, intraday=False, days_to_hold=days_to_hold) # MODIFIED: Pass days_to_hold
                
                # Calculate metrics
                metrics = strategy.calculate_metrics()
                
                # Store results
                results.append({
                    'short_window': short_window,
                    'long_window': long_window,
                    'stop_loss': stop_loss,
                    'sharpe': metrics['Sharpe Ratio'],
                    'return': metrics['Total Return (%)'],
                    'max_dd': metrics['Max Drawdown (%)'],
                    'win_rate': metrics['Win Rate (%)']
                })
    
    # Convert to DataFrame
    results_df = pd.DataFrame(results)
    
    # Sort by Sharpe ratio (descending)
    results_df = results_df.sort_values('sharpe', ascending=False)
    
    # Print top 5 parameter combinations
    print("\nTOP 5 PARAMETER COMBINATIONS:")
    print(tabulate(results_df.head(5), headers='keys', tablefmt='fancy_grid', floatfmt=".2f"))
    
    # Return best parameters
    best = results_df.iloc[0]
    return best['short_window'], best['long_window'], best['stop_loss']


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run backtest for Moving Average Crossover strategy')
    parser.add_argument('--ticker', type=str, default='SPY', help='Ticker symbol to backtest')
    parser.add_argument('--start', type=str, default='2018-01-01', help='Start date for backtest')
    parser.add_argument('--end', type=str, default='2023-01-01', help='End date for backtest')
    parser.add_argument('--short', type=int, default=10, help='Short moving average window')  # Changed from 30 to 10
    parser.add_argument('--long', type=int, default=50, help='Long moving average window')  # Changed from 100 to 50
    parser.add_argument('--capital', type=float, default=1000000, help='Initial capital')
    parser.add_argument('--stop', type=float, default=0.05, help='Stop loss percentage')  # Changed from 0.03 to 0.05
    parser.add_argument('--take', type=float, default=0.15, help='Take profit percentage')  # Changed from 0.06 to 0.15
    parser.add_argument('--size', type=float, default=0.5, help='Position size percentage')  # Changed from 0.4 to 0.5
    parser.add_argument('--intraday', action='store_true', help='Run as intraday strategy')
    parser.add_argument('--optimize', action='store_true', help='Find optimal parameters')
    parser.add_argument('--hold_days', type=int, default=60, help='Maximum number of days to hold a position for swing trades')
    parser.add_argument('--no_trailing_stop', action='store_false', dest='use_trailing_stop', help='Disable trailing stop')
    parser.add_argument('--trailing_activation', type=float, default=0.02, help='Profit percentage to activate trailing stop')
    parser.add_argument('--trailing_distance', type=float, default=0.02, help='Distance to maintain for trailing stop')
    
    args = parser.parse_args()
    
    if args.optimize:
        print("Optimizing parameters...")
        best_short, best_long, best_stop = evaluate_multiple_parameters(
            ticker=args.ticker, 
            start_date=args.start, 
            end_date=args.end,
            days_to_hold=args.hold_days # MODIFIED: Pass args.hold_days
        )
        
        print(f"\nRunning backtest with optimized parameters: short_window={best_short}, long_window={best_long}, stop_loss={best_stop:.2f}")
        strategy = run_backtest(
            ticker=args.ticker,
            start_date=args.start,
            end_date=args.end,
            short_window=int(best_short),
            long_window=int(best_long),
            initial_capital=args.capital,
            stop_loss_pct=best_stop,
            take_profit_pct=best_stop * 2,
            position_size_pct=args.size,
            intraday=args.intraday,
            days_to_hold=args.hold_days,
            use_trailing_stop=args.use_trailing_stop,
            trailing_stop_activation=args.trailing_activation,
            trailing_stop_distance=args.trailing_distance
        )
    else:
        strategy = run_backtest(
            ticker=args.ticker,
            start_date=args.start,
            end_date=args.end,
            short_window=args.short,
            long_window=args.long,
            initial_capital=args.capital,
            stop_loss_pct=args.stop,
            take_profit_pct=args.take,
            position_size_pct=args.size,
            intraday=args.intraday,
            days_to_hold=args.hold_days,
            use_trailing_stop=args.use_trailing_stop,
            trailing_stop_activation=args.trailing_activation,
            trailing_stop_distance=args.trailing_distance
        )
