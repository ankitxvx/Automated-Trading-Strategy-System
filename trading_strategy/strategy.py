"""
Trading Strategy Implementation
- Moving Average Crossover Strategy for stocks
- Includes metrics calculations, drawdowns, and performance analysis
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import yfinance as yf
from datetime import datetime, timedelta
from sklearn.metrics import mean_squared_error
import os
from pathlib import Path
import matplotlib.dates as mdates
from tqdm import tqdm
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import warnings

warnings.filterwarnings('ignore')

class MovingAverageCrossover:
    """
    Moving Average Crossover Strategy
    - Buy when short-term MA crosses above long-term MA
    - Sell when short-term MA crosses below long-term MA
    """
    
    def __init__(self, short_window=30, long_window=70, initial_capital=10000, 
                 stop_loss_pct=0.07, take_profit_pct=0.20, position_size_pct=0.5,
                 use_trailing_stop=True, trailing_stop_activation=0.06, trailing_stop_distance=0.04):
        self.short_window = short_window
        self.long_window = long_window
        self.initial_capital = initial_capital
        self.stop_loss_pct = stop_loss_pct
        self.take_profit_pct = take_profit_pct
        self.position_size_pct = position_size_pct
        self.use_trailing_stop = use_trailing_stop
        self.trailing_stop_activation = trailing_stop_activation  # When to activate trailing stop (% profit)
        self.trailing_stop_distance = trailing_stop_distance  # Distance to maintain for trailing stop (%)
        self.trades = []
        self.positions = {}
        self.daily_returns = []
        self.equity = []

    def generate_signals(self, data):
        """Generate trading signals based on moving average crossover."""
        # Create a copy of data to avoid modifying the original
        df = data.copy()
        
        # Calculate moving averages
        df['short_ma'] = df['Adj Close'].rolling(window=self.short_window, min_periods=1).mean()
        df['long_ma'] = df['Adj Close'].rolling(window=self.long_window, min_periods=1).mean()
        
        # Generate signals
        df['signal'] = 0
        # Buy signal (short MA crosses above long MA)
        df.loc[(df['short_ma'] > df['long_ma']) & (df['short_ma'].shift(1) <= df['long_ma'].shift(1)), 'signal'] = 1
        # Sell signal (short MA crosses below long MA)
        df.loc[(df['short_ma'] < df['long_ma']) & (df['short_ma'].shift(1) >= df['long_ma'].shift(1)), 'signal'] = -1
        
        return df

    def backtest(self, data, intraday=False, days_to_hold=None):
        """Run backtest on the strategy."""
        df = self.generate_signals(data)
        capital = self.initial_capital
        position = 0
        entry_price = 0
        entry_date = None
        last_trade_date = None
        trailing_stop_price = 0  # For tracking trailing stop level
        
        # Set default days_to_hold if None
        if days_to_hold is None:
            days_to_hold = 60  # Default to 60 days if not specified
        
        # For tracking portfolio value
        portfolio_value = [self.initial_capital]
        dates = [df.index[0]]
        
        # For tracking trades
        all_trades = []
        
        for i in range(1, len(df)):
            current_date = df.index[i]
            current_price = df['Adj Close'].iloc[i]
            current_signal = df['signal'].iloc[i]
            
            # For calculating daily returns
            prev_portfolio_value = portfolio_value[-1]
              # Check for take profit or stop loss if in position
            if position > 0:
                pnl_pct = (current_price - entry_price) / entry_price
                days_held = (current_date - entry_date).days
                
                # Update trailing stop if using it and in profit
                if self.use_trailing_stop and pnl_pct >= self.trailing_stop_activation:
                    # Only update if current price gives a higher stop price than previous
                    new_stop_price = current_price * (1 - self.trailing_stop_distance)
                    if new_stop_price > trailing_stop_price:
                        trailing_stop_price = new_stop_price
                
                # Take profit check
                if pnl_pct >= self.take_profit_pct:
                    # Sell at take profit
                    profit = position * (current_price - entry_price)
                    capital += position * current_price
                    
                    trade = {
                        'entry_date': entry_date,
                        'exit_date': current_date,
                        'entry_price': entry_price,
                        'exit_price': current_price,
                        'shares': position,
                        'pnl': profit,
                        'pnl_pct': pnl_pct * 100,
                        'exit_reason': 'take_profit'
                    }
                    all_trades.append(trade)
                    
                    position = 0
                    trailing_stop_price = 0  # Reset trailing stop
                    last_trade_date = current_date
                
                # Trailing stop hit (only if activated)
                elif self.use_trailing_stop and trailing_stop_price > 0 and current_price <= trailing_stop_price:
                    profit = position * (current_price - entry_price)
                    capital += position * current_price
                    
                    trade = {
                        'entry_date': entry_date,
                        'exit_date': current_date,
                        'entry_price': entry_price,
                        'exit_price': current_price,
                        'shares': position,
                        'pnl': profit,
                        'pnl_pct': pnl_pct * 100,
                        'exit_reason': 'trailing_stop'
                    }
                    all_trades.append(trade)
                    
                    position = 0
                    trailing_stop_price = 0  # Reset trailing stop
                    last_trade_date = current_date
                
                # Regular stop loss check
                elif pnl_pct <= -self.stop_loss_pct:
                    # Sell at stop loss
                    loss = position * (current_price - entry_price)
                    capital += position * current_price
                    
                    trade = {
                        'entry_date': entry_date,
                        'exit_date': current_date,
                        'entry_price': entry_price,
                        'exit_price': current_price,
                        'shares': position,
                        'pnl': loss,
                        'pnl_pct': pnl_pct * 100,
                        'exit_reason': 'stop_loss'
                    }
                    all_trades.append(trade)
                    
                    position = 0
                    trailing_stop_price = 0  # Reset trailing stop
                    last_trade_date = current_date
                
                # For intraday, close position at end of day
                elif intraday and days_held >= 1:
                    profit = position * (current_price - entry_price)
                    capital += position * current_price
                    
                    trade = {
                        'entry_date': entry_date,
                        'exit_date': current_date,
                        'entry_price': entry_price,
                        'exit_price': current_price,
                        'shares': position,
                        'pnl': profit,
                        'pnl_pct': pnl_pct * 100,
                        'exit_reason': 'day_close'
                    }
                    all_trades.append(trade)
                    
                    position = 0
                    last_trade_date = current_date
                
                # For swing trading, consider max hold time
                elif not intraday and days_held >= days_to_hold:
                    profit = position * (current_price - entry_price)
                    capital += position * current_price
                    
                    trade = {
                        'entry_date': entry_date,
                        'exit_date': current_date,
                        'entry_price': entry_price,
                        'exit_price': current_price,
                        'shares': position,
                        'pnl': profit,
                        'pnl_pct': pnl_pct * 100,
                        'exit_reason': 'time_exit'
                    }
                    all_trades.append(trade)
                    
                    position = 0
                    last_trade_date = current_date
              # Check for buy signal
            if current_signal == 1 and position == 0:
                # Only trade if enough time has passed since last trade (avoid overtrading)
                if last_trade_date is None or (current_date - last_trade_date).days >= 1:
                    # Calculate position size (% of capital)
                    position_value = capital * self.position_size_pct
                    position = int(position_value / current_price)
                    
                    if position > 0:
                        entry_price = current_price
                        entry_date = current_date
                        trailing_stop_price = 0  # Reset trailing stop for new position
                        capital -= position * current_price
              # Check for sell signal
            elif current_signal == -1 and position > 0:
                profit = position * (current_price - entry_price)
                capital += position * current_price
                
                trade = {
                    'entry_date': entry_date,
                    'exit_date': current_date,
                    'entry_price': entry_price,
                    'exit_price': current_price,
                    'shares': position,
                    'pnl': profit,
                    'pnl_pct': ((current_price - entry_price) / entry_price) * 100,
                    'exit_reason': 'signal'
                }
                all_trades.append(trade)
                
                position = 0
                trailing_stop_price = 0  # Reset trailing stop
                last_trade_date = current_date
            
            # Update trailing stop for long positions
            if position > 0 and self.use_trailing_stop:
                # Move trailing stop up if price increases
                new_trailing_stop = current_price * (1 - self.trailing_stop_distance)
                if new_trailing_stop > trailing_stop_price:
                    trailing_stop_price = new_trailing_stop
            
            # Check for trailing stop loss
            if position > 0 and current_price <= trailing_stop_price:
                # Sell at trailing stop
                loss = position * (current_price - entry_price)
                capital += position * current_price
                
                trade = {
                    'entry_date': entry_date,
                    'exit_date': current_date,
                    'entry_price': entry_price,
                    'exit_price': current_price,
                    'shares': position,
                    'pnl': loss,
                    'pnl_pct': ((current_price - entry_price) / entry_price) * 100,
                    'exit_reason': 'trailing_stop'
                }
                all_trades.append(trade)
                
                position = 0
                last_trade_date = current_date
            
            # Calculate portfolio value
            portfolio_value.append(capital + (position * current_price))
            dates.append(current_date)
            
        # Create a DataFrame of portfolio value over time
        portfolio_df = pd.DataFrame({
            'Date': dates,
            'Portfolio_Value': portfolio_value
        })
        portfolio_df.set_index('Date', inplace=True)
        
        # Calculate daily returns
        portfolio_df['Daily_Return'] = portfolio_df['Portfolio_Value'].pct_change()
        
        # Convert trades to DataFrame
        if all_trades:
            trades_df = pd.DataFrame(all_trades)
        else:
            trades_df = pd.DataFrame(columns=['entry_date', 'exit_date', 'entry_price', 'exit_price', 
                                             'shares', 'pnl', 'pnl_pct', 'exit_reason'])
        
        # Save results
        self.portfolio = portfolio_df
        self.trades = trades_df
        
        return portfolio_df, trades_df

    def calculate_metrics(self):
        """Calculate performance metrics of the strategy."""
        if len(self.portfolio) == 0:
            raise ValueError("No backtest results available. Run backtest() first.")
        
        # Extract values for metrics calculation
        portfolio_values = self.portfolio['Portfolio_Value'].values
        daily_returns = self.portfolio['Daily_Return'].dropna().values
        
        # Calculate overall return
        total_return = (portfolio_values[-1] - self.initial_capital) / self.initial_capital
        annualized_return = (1 + total_return) ** (252 / len(portfolio_values)) - 1
        
        # Calculate volatility (std dev of daily returns)
        volatility = np.std(daily_returns) * np.sqrt(252)
        
        # Calculate Sharpe ratio (using 0% as risk-free rate for simplicity)
        sharpe_ratio = annualized_return / volatility if volatility > 0 else 0
        
        # Calculate drawdowns
        drawdowns, max_drawdown, max_drawdown_duration = self.calculate_drawdowns()
        
        # Calculate trade metrics
        if len(self.trades) > 0:
            win_rate = len(self.trades[self.trades['pnl'] > 0]) / len(self.trades)
            avg_win = self.trades[self.trades['pnl'] > 0]['pnl'].mean() if len(self.trades[self.trades['pnl'] > 0]) > 0 else 0
            avg_loss = self.trades[self.trades['pnl'] < 0]['pnl'].mean() if len(self.trades[self.trades['pnl'] < 0]) > 0 else 0
            profit_factor = abs(sum(self.trades[self.trades['pnl'] > 0]['pnl'])) / abs(sum(self.trades[self.trades['pnl'] < 0]['pnl'])) if sum(self.trades[self.trades['pnl'] < 0]['pnl']) < 0 else float('inf')
        else:
            win_rate = 0
            avg_win = 0
            avg_loss = 0
            profit_factor = 0
        
        # Create metrics dictionary
        metrics = {
            'Total Return (%)': total_return * 100,
            'Annualized Return (%)': annualized_return * 100,
            'Volatility (%)': volatility * 100,
            'Sharpe Ratio': sharpe_ratio,
            'Max Drawdown (%)': max_drawdown * 100,
            'Max Drawdown Duration (Days)': max_drawdown_duration,
            'Number of Trades': len(self.trades),
            'Win Rate (%)': win_rate * 100 if len(self.trades) > 0 else 0,
            'Average Win ($)': avg_win,
            'Average Loss ($)': avg_loss,
            'Profit Factor': profit_factor
        }
        
        return metrics
    
    def calculate_drawdowns(self):
        """Calculate drawdowns and maximum drawdown."""
        portfolio_values = self.portfolio['Portfolio_Value'].values
        if len(portfolio_values) < 2: # Not enough data for drawdowns
            return [], 0, 0

        # Calculate running maximum of portfolio values
        running_max = np.maximum.accumulate(portfolio_values)
        
        # Calculate drawdown values: (running_max - current_value) / running_max
        # Initialize with zeros to handle cases where running_max is 0
        drawdown_values = np.zeros_like(portfolio_values, dtype=float)
        
        # Avoid division by zero if running_max is 0
        non_zero_mask = running_max != 0
        drawdown_values[non_zero_mask] = (running_max[non_zero_mask] - portfolio_values[non_zero_mask]) / running_max[non_zero_mask]
        
        # The drawdowns for plotting should correspond to self.portfolio.index[1:]
        # So we take drawdown_values starting from the second element
        plot_drawdowns = list(drawdown_values[1:])
        
        max_dd = np.max(drawdown_values) if len(drawdown_values) > 0 else 0
        
        # Calculate maximum drawdown duration
        in_drawdown = False
        current_dd_start_index = 0
        max_dd_duration = 0
        
        # Iterate through the full drawdown_values (length N)
        # A drawdown exists at point i if drawdown_values[i] > 0
        for i in range(len(drawdown_values)): # Iterate N times
            if drawdown_values[i] > 0 and not in_drawdown: # Start of a new drawdown
                in_drawdown = True
                current_dd_start_index = i
            elif drawdown_values[i] == 0 and in_drawdown: # End of a drawdown
                in_drawdown = False
                # Duration is number of points in drawdown; if indices are i and j, duration is j-i+1
                # Here, it ends at i-1, started at current_dd_start_index. So (i-1) - current_dd_start_index + 1 = i - current_dd_start_index
                duration = i - current_dd_start_index 
                if duration > max_dd_duration:
                    max_dd_duration = duration
        
        # If still in drawdown at the end
        if in_drawdown:
            duration = len(drawdown_values) - current_dd_start_index
            if duration > max_dd_duration:
                max_dd_duration = duration
                
        return plot_drawdowns, max_dd, max_dd_duration

    def analyze_market_periods(self, data, start_date, end_date):
        """Analyze performance during specific market periods."""
        # Filter portfolio data for the specific period
        mask = (self.portfolio.index >= start_date) & (self.portfolio.index <= end_date)
        period_portfolio = self.portfolio.loc[mask]
        
        if len(period_portfolio) == 0:
            return {
                'period': f"{start_date} to {end_date}",
                'return': 0,
                'trades': 0,
                'win_rate': 0
            }
        
        # Calculate return for the period
        period_return = (period_portfolio['Portfolio_Value'].iloc[-1] - period_portfolio['Portfolio_Value'].iloc[0]) / period_portfolio['Portfolio_Value'].iloc[0]
        
        # Find trades during the period
        trades_mask = (self.trades['entry_date'] >= start_date) & (self.trades['exit_date'] <= end_date)
        period_trades = self.trades.loc[trades_mask]
        
        # Calculate win rate
        win_rate = len(period_trades[period_trades['pnl'] > 0]) / len(period_trades) if len(period_trades) > 0 else 0
        
        return {
            'period': f"{start_date} to {end_date}",
            'return': period_return * 100,
            'trades': len(period_trades),
            'win_rate': win_rate * 100
        }

    def plot_results(self, data, save_path=None):
        """Plot backtest results and key metrics."""
        if len(self.portfolio) == 0:
            raise ValueError("No backtest results available. Run backtest() first.")
        
        # Ensure data has MA columns for plotting
        if 'short_ma' not in data.columns or 'long_ma' not in data.columns:
            data = self.generate_signals(data.copy()) # Use a copy to avoid modifying original

        # Create subplots
        fig, axs = plt.subplots(3, 1, figsize=(14, 18), gridspec_kw={'height_ratios': [2, 1, 1]})
        
        # Plot 1: Price and Moving Averages
        axs[0].plot(data.index, data['Adj Close'], label='Price', alpha=0.7)
        axs[0].plot(data.index, data['short_ma'], label=f'{self.short_window} MA', linewidth=1.5)
        axs[0].plot(data.index, data['long_ma'], label=f'{self.long_window} MA', linewidth=1.5)
        
        # Add buy/sell markers
        if len(self.trades) > 0:
            buy_dates = self.trades['entry_date'].values
            buy_prices = self.trades['entry_price'].values
            sell_dates = self.trades['exit_date'].values
            sell_prices = self.trades['exit_price'].values
            
            axs[0].scatter(buy_dates, buy_prices, color='green', marker='^', s=100, label='Buy')
            axs[0].scatter(sell_dates, sell_prices, color='red', marker='v', s=100, label='Sell')
        
        axs[0].set_title('Price, Moving Averages, and Signals')
        axs[0].set_ylabel('Price ($)')
        axs[0].legend()
        axs[0].grid(True)
        
        # Plot 2: Portfolio Value
        axs[1].plot(self.portfolio.index, self.portfolio['Portfolio_Value'], color='blue', linewidth=2)
        axs[1].set_title('Portfolio Value Over Time')
        axs[1].set_ylabel('Portfolio Value ($)')
        axs[1].grid(True)
        
        # Plot 3: Drawdowns
        drawdowns, max_dd, _ = self.calculate_drawdowns()
        dd_series = pd.Series(index=self.portfolio.index[1:], data=drawdowns)
        axs[2].fill_between(dd_series.index, 0, -dd_series.values * 100, color='red', alpha=0.3)
        axs[2].set_title(f'Drawdowns (Max: {max_dd*100:.2f}%)')
        axs[2].set_ylabel('Drawdown (%)')
        axs[2].grid(True)
        
        # Add date formatting
        for ax in axs:
            ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=45)
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path)
            plt.close()
            return save_path
        else:
            plt.show()
            return None

    def plot_interactive(self, data, save_path=None):
        """Create an interactive plot of strategy performance using Plotly."""
        if len(self.portfolio) == 0:
            raise ValueError("No backtest results available. Run backtest() first.")

        # Ensure data has MA columns for plotting
        if 'short_ma' not in data.columns or 'long_ma' not in data.columns:
            data = self.generate_signals(data.copy()) # Use a copy to avoid modifying original
            
        # Create subplots with 3 rows and 1 column
        fig = make_subplots(rows=3, cols=1, 
                            subplot_titles=('Price and Moving Averages', 'Portfolio Value', 'Drawdowns'),
                            vertical_spacing=0.1,
                            row_heights=[0.5, 0.25, 0.25])
        
        # Plot 1: Price and Moving Averages
        fig.add_trace(
            go.Scatter(x=data.index, y=data['Adj Close'], name='Price', line=dict(color='black', width=1)),
            row=1, col=1
        )
        
        fig.add_trace(
            go.Scatter(x=data.index, y=data['short_ma'], name=f'{self.short_window} MA', 
                      line=dict(color='blue', width=2)),
            row=1, col=1
        )
        
        fig.add_trace(
            go.Scatter(x=data.index, y=data['long_ma'], name=f'{self.long_window} MA', 
                      line=dict(color='orange', width=2)),
            row=1, col=1
        )
        
        # Add buy/sell markers
        if len(self.trades) > 0:
            buy_dates = self.trades['entry_date'].values
            buy_prices = self.trades['entry_price'].values
            
            fig.add_trace(
                go.Scatter(x=buy_dates, y=buy_prices, mode='markers', name='Buy',
                          marker=dict(color='green', size=10, symbol='triangle-up')),
                row=1, col=1
            )
            
            sell_dates = self.trades['exit_date'].values
            sell_prices = self.trades['exit_price'].values
            
            fig.add_trace(
                go.Scatter(x=sell_dates, y=sell_prices, mode='markers', name='Sell',
                          marker=dict(color='red', size=10, symbol='triangle-down')),
                row=1, col=1
            )
        
        # Plot 2: Portfolio Value
        fig.add_trace(
            go.Scatter(x=self.portfolio.index, y=self.portfolio['Portfolio_Value'], 
                      name='Portfolio Value', line=dict(color='blue', width=2)),
            row=2, col=1
        )
        
        # Plot 3: Drawdowns
        drawdowns, max_dd, _ = self.calculate_drawdowns()
        dd_series = pd.Series(index=self.portfolio.index[1:], data=drawdowns)
        
        fig.add_trace(
            go.Scatter(x=dd_series.index, y=-dd_series.values*100, 
                      name=f'Drawdown (Max: {max_dd*100:.2f}%)', 
                      fill='tozeroy', line=dict(color='red', width=1)),
            row=3, col=1
        )
        
        # Update layout
        fig.update_layout(
            title='Strategy Backtest Results',
            height=900,
            width=1200,
            showlegend=True,
            legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
        )
        
        # Update y-axis labels
        fig.update_yaxes(title_text='Price ($)', row=1, col=1)
        fig.update_yaxes(title_text='Value ($)', row=2, col=1)
        fig.update_yaxes(title_text='Drawdown (%)', row=3, col=1)
        
        if save_path:
            fig.write_html(save_path)
            return save_path
        else:
            fig.show()
            return None

    def generate_report(self, data, output_dir='reports', covid_period=('2020-02-01', '2020-04-30')):
        """Generate a comprehensive backtest report."""
        os.makedirs(output_dir, exist_ok=True)
        
        # Calculate metrics
        metrics = self.calculate_metrics()

        # Ensure data has MA columns for plotting and analysis if needed elsewhere
        data_for_plotting = data.copy()
        if 'short_ma' not in data_for_plotting.columns or 'long_ma' not in data_for_plotting.columns:
            data_for_plotting = self.generate_signals(data_for_plotting)
        
        # Analyze COVID period
        covid_start, covid_end = covid_period
        # Pass the original data for period analysis, as it doesn't need MA columns
        covid_analysis = self.analyze_market_periods(data, covid_start, covid_end) 
        
        # Generate plots
        plot_path = os.path.join(output_dir, 'backtest_plot.png')
        self.plot_results(data_for_plotting, save_path=plot_path) # Use augmented data
        
        interactive_plot_path = os.path.join(output_dir, 'interactive_plot.html')
        self.plot_interactive(data_for_plotting, save_path=interactive_plot_path) # Use augmented data
        
        # Generate trade log CSV
        trades_path = os.path.join(output_dir, 'trade_log.csv')
        if len(self.trades) > 0:
            self.trades.to_csv(trades_path)
        
        # Generate portfolio value CSV
        portfolio_path = os.path.join(output_dir, 'portfolio_value.csv')
        self.portfolio.to_csv(portfolio_path)
        
        # Create summary report
        report_path = os.path.join(output_dir, 'summary_report.txt')
        
        with open(report_path, 'w') as f:
            f.write("======== MOVING AVERAGE CROSSOVER STRATEGY BACKTEST REPORT ========\n\n")
            f.write("STRATEGY PARAMETERS:\n")
            f.write(f"Short Window: {self.short_window} days\n")
            f.write(f"Long Window: {self.long_window} days\n")
            f.write(f"Initial Capital: ${self.initial_capital:,.2f}\n")
            f.write(f"Position Size: {self.position_size_pct * 100:.1f}% of capital\n")
            f.write(f"Stop Loss: {self.stop_loss_pct * 100:.1f}%\n")
            f.write(f"Take Profit: {self.take_profit_pct * 100:.1f}%\n")
            if self.use_trailing_stop:
                f.write(f"Trailing Stop: Enabled (activates at {self.trailing_stop_activation * 100:.1f}% profit, {self.trailing_stop_distance * 100:.1f}% distance)\n\n")
            else:
                f.write(f"Trailing Stop: Disabled\n\n")
            
            f.write("PERFORMANCE METRICS:\n")
            for metric, value in metrics.items():
                f.write(f"{metric}: {value:,.2f}\n")
            
            f.write("\nCOVID-19 PERIOD ANALYSIS ({} to {}):\n".format(covid_start, covid_end))
            f.write(f"Return: {covid_analysis['return']:,.2f}%\n")
            f.write(f"Trades: {covid_analysis['trades']}\n")
            f.write(f"Win Rate: {covid_analysis['win_rate']:,.2f}%\n\n")
            
            f.write("TRADE SUMMARY:\n")
            if len(self.trades) > 0:
                f.write(f"Total Trades: {len(self.trades)}\n")
                f.write(f"Winning Trades: {len(self.trades[self.trades['pnl'] > 0])} ({metrics['Win Rate (%)']:,.2f}%)\n")
                f.write(f"Losing Trades: {len(self.trades[self.trades['pnl'] < 0])}\n")
                f.write(f"Average Win: ${metrics['Average Win ($)']:,.2f}\n")
                f.write(f"Average Loss: ${metrics['Average Loss ($)']:,.2f}\n")
                f.write(f"Profit Factor: {metrics['Profit Factor']:,.2f}\n\n")
                
                f.write("Exit Reasons:\n")
                exit_counts = self.trades['exit_reason'].value_counts()
                for reason, count in exit_counts.items():
                    f.write(f"  {reason}: {count} trades\n")
            else:
                f.write("No trades were executed during the backtest period.\n")
            
            f.write("\nGENERATED REPORTS:\n")
            f.write(f"Trade Log: {os.path.basename(trades_path)}\n")
            f.write(f"Portfolio Value: {os.path.basename(portfolio_path)}\n")
            f.write(f"Static Plot: {os.path.basename(plot_path)}\n")
            f.write(f"Interactive Plot: {os.path.basename(interactive_plot_path)}\n")
        
        return report_path
