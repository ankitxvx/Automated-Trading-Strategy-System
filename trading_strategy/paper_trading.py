"""
Paper Trading Simulation Script
- Simulates paper trading using the Moving Average Crossover strategy
- Sends trade alerts and keeps track of open positions
"""

import pandas as pd
import numpy as np
import yfinance as yf
from datetime import datetime, timedelta
import time
import os
from trading_strategy.strategy import MovingAverageCrossover
import argparse
from tabulate import tabulate
import json

class PaperTrader:
    """Paper trading simulator for automated strategy execution."""
    
    def __init__(self, strategy, ticker, initial_capital=10000, data_dir='data'):
        self.strategy = strategy
        self.ticker = ticker
        self.initial_capital = initial_capital
        self.data_dir = data_dir
        self.positions = {}
        self.trade_log = []
        self.capital = initial_capital
        
        # Create data directory
        os.makedirs(data_dir, exist_ok=True)
        
        # File path for storing positions
        self.positions_file = os.path.join(data_dir, f"{ticker}_positions.json")
        self.trade_log_file = os.path.join(data_dir, f"{ticker}_trade_log.csv")
        
        # Load existing positions if any
        self.load_positions()
    
    def load_positions(self):
        """Load positions from file if it exists."""
        if os.path.exists(self.positions_file):
            with open(self.positions_file, 'r') as f:
                positions_data = json.load(f)
                self.positions = positions_data['positions']
                self.capital = positions_data['capital']
                
                # Convert string dates to datetime
                for symbol, pos in self.positions.items():
                    if 'entry_date' in pos:
                        pos['entry_date'] = datetime.fromisoformat(pos['entry_date'])
        
        # Load trade log if exists
        if os.path.exists(self.trade_log_file):
            self.trade_log = pd.read_csv(self.trade_log_file)
    
    def save_positions(self):
        """Save positions to file."""
        # Convert datetime to string for JSON serialization
        positions_copy = {}
        for symbol, pos in self.positions.items():
            positions_copy[symbol] = pos.copy()
            if 'entry_date' in positions_copy[symbol]:
                positions_copy[symbol]['entry_date'] = positions_copy[symbol]['entry_date'].isoformat()
        
        positions_data = {
            'positions': positions_copy,
            'capital': self.capital
        }
        
        with open(self.positions_file, 'w') as f:
            json.dump(positions_data, f, indent=4)
        
        # Save trade log
        if isinstance(self.trade_log, list) and len(self.trade_log) > 0:
            pd.DataFrame(self.trade_log).to_csv(self.trade_log_file, index=False)
        elif isinstance(self.trade_log, pd.DataFrame):
            self.trade_log.to_csv(self.trade_log_file, index=False)
    
    def get_current_data(self, lookback_days=100):
        """Get current market data."""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=lookback_days)
        
        # Download data
        data = yf.download(self.ticker, start=start_date, end=end_date)
        
        return data
    
    def check_for_signals(self):
        """Check for trading signals and execute paper trades."""
        print(f"\n--- Checking for signals on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ---")
        
        # Get current market data
        data = self.get_current_data()
        
        # Generate signals
        signals_data = self.strategy.generate_signals(data)
        
        # Get latest data point
        latest_data = signals_data.iloc[-1]
        prev_data = signals_data.iloc[-2]
        latest_date = latest_data.name
        latest_price = latest_data['Adj Close']
        latest_signal = latest_data['signal']
        
        print(f"Latest data: {latest_date.strftime('%Y-%m-%d')}")
        print(f"Price: ${latest_price:.2f}")
        print(f"Signal: {latest_signal}")
        
        # Check for buy signal
        if latest_signal == 1 and self.ticker not in self.positions:
            self.execute_buy(latest_date, latest_price)
        
        # Check for sell signal
        elif latest_signal == -1 and self.ticker in self.positions:
            self.execute_sell(latest_date, latest_price, reason="signal")
        
        # Check stop loss and take profit for existing position
        elif self.ticker in self.positions:
            position = self.positions[self.ticker]
            entry_price = position['entry_price']
            
            # Calculate current P&L
            pnl_pct = (latest_price - entry_price) / entry_price
            
            print(f"Current P&L: {pnl_pct*100:.2f}%")
            
            # Check stop loss
            if pnl_pct <= -self.strategy.stop_loss_pct:
                self.execute_sell(latest_date, latest_price, reason="stop_loss")
            
            # Check take profit
            elif pnl_pct >= self.strategy.take_profit_pct:
                self.execute_sell(latest_date, latest_price, reason="take_profit")
        
        # No position and no signal
        else:
            print("No action needed.")
        
        # Save positions
        self.save_positions()
        
        # Print portfolio status
        self.print_portfolio_status(latest_price)
    
    def execute_buy(self, date, price):
        """Execute a paper buy trade."""
        # Calculate position size
        position_value = self.capital * self.strategy.position_size_pct
        shares = int(position_value / price)
        
        if shares > 0:
            cost = shares * price
            
            # Check if enough capital
            if cost <= self.capital:
                # Update capital
                self.capital -= cost
                
                # Record position
                self.positions[self.ticker] = {
                    'entry_date': date,
                    'entry_price': price,
                    'shares': shares,
                    'cost': cost
                }
                
                # Log trade
                trade = {
                    'date': date.strftime('%Y-%m-%d'),
                    'action': 'BUY',
                    'ticker': self.ticker,
                    'price': price,
                    'shares': shares,
                    'value': cost,
                    'reason': 'signal'
                }
                
                if isinstance(self.trade_log, list):
                    self.trade_log.append(trade)
                else:
                    self.trade_log = self.trade_log.append(trade, ignore_index=True)
                
                print(f"\n>>> BUY ALERT: {shares} shares of {self.ticker} @ ${price:.2f} = ${cost:.2f}")
            else:
                print(f"Not enough capital to buy {shares} shares of {self.ticker}.")
        else:
            print(f"Not enough capital to buy at least 1 share of {self.ticker}.")
    
    def execute_sell(self, date, price, reason="signal"):
        """Execute a paper sell trade."""
        if self.ticker in self.positions:
            position = self.positions[self.ticker]
            shares = position['shares']
            entry_price = position['entry_price']
            cost = position['cost']
            
            # Calculate proceeds and P&L
            proceeds = shares * price
            pnl = proceeds - cost
            pnl_pct = (price - entry_price) / entry_price * 100
            
            # Update capital
            self.capital += proceeds
            
            # Log trade
            trade = {
                'date': date.strftime('%Y-%m-%d'),
                'action': 'SELL',
                'ticker': self.ticker,
                'price': price,
                'shares': shares,
                'value': proceeds,
                'pnl': pnl,
                'pnl_pct': pnl_pct,
                'reason': reason
            }
            
            if isinstance(self.trade_log, list):
                self.trade_log.append(trade)
            else:
                self.trade_log = self.trade_log.append(trade, ignore_index=True)
            
            # Remove position
            del self.positions[self.ticker]
            
            print(f"\n>>> SELL ALERT: {shares} shares of {self.ticker} @ ${price:.2f} = ${proceeds:.2f}")
            print(f"P&L: ${pnl:.2f} ({pnl_pct:.2f}%)")
            print(f"Reason: {reason}")
    
    def print_portfolio_status(self, current_price=None):
        """Print current portfolio status."""
        # Calculate total portfolio value
        portfolio_value = self.capital
        
        # Add value of open positions
        for symbol, pos in self.positions.items():
            # Get current price if not provided
            if current_price is None:
                ticker_data = yf.Ticker(symbol).history(period='1d')
                current_price = ticker_data['Close'].iloc[-1]
            
            position_value = pos['shares'] * current_price
            portfolio_value += position_value
            
            # Calculate P&L
            pnl = position_value - pos['cost']
            pnl_pct = (current_price - pos['entry_price']) / pos['entry_price'] * 100
            
            # Print position details
            print(f"\nOpen position: {symbol}")
            print(f"Shares: {pos['shares']}")
            print(f"Entry price: ${pos['entry_price']:.2f}")
            print(f"Current price: ${current_price:.2f}")
            print(f"Current value: ${position_value:.2f}")
            print(f"Unrealized P&L: ${pnl:.2f} ({pnl_pct:.2f}%)")
        
        # Print overall portfolio status
        print("\nPORTFOLIO SUMMARY:")
        print(f"Cash: ${self.capital:.2f}")
        print(f"Positions value: ${portfolio_value - self.capital:.2f}")
        print(f"Total portfolio value: ${portfolio_value:.2f}")
        
        # Calculate return
        total_return = (portfolio_value - self.initial_capital) / self.initial_capital * 100
        print(f"Total return: {total_return:.2f}%")


def run_paper_trading(ticker="SPY", initial_capital=10000, interval=60, 
                     short_window=20, long_window=50, stop_loss=0.05, take_profit=0.1):
    """Run paper trading simulation with specified parameters."""
    # Initialize strategy
    strategy = MovingAverageCrossover(
        short_window=short_window,
        long_window=long_window,
        initial_capital=initial_capital,
        stop_loss_pct=stop_loss,
        take_profit_pct=take_profit,
        position_size_pct=0.2  # Use 20% of capital per trade
    )
    
    # Initialize paper trader
    trader = PaperTrader(strategy, ticker, initial_capital)
    
    # Run initial check
    trader.check_for_signals()
    
    # Continue checking at specified interval
    while True:
        print(f"\nWaiting for {interval} seconds before next check...")
        time.sleep(interval)  # Wait for the specified interval
        trader.check_for_signals()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run paper trading for Moving Average Crossover strategy')
    
    parser.add_argument('--ticker', type=str, default='SPY', help='Ticker symbol to trade')
    parser.add_argument('--capital', type=float, default=10000, help='Initial capital')
    parser.add_argument('--interval', type=int, default=60, help='Check interval in seconds')
    parser.add_argument('--short', type=int, default=20, help='Short moving average window')
    parser.add_argument('--long', type=int, default=50, help='Long moving average window')
    parser.add_argument('--stop', type=float, default=0.05, help='Stop loss percentage')
    parser.add_argument('--take', type=float, default=0.1, help='Take profit percentage')
    
    args = parser.parse_args()
    
    run_paper_trading(
        ticker=args.ticker,
        initial_capital=args.capital,
        interval=args.interval,
        short_window=args.short,
        long_window=args.long,
        stop_loss=args.stop,
        take_profit=args.take
    )
