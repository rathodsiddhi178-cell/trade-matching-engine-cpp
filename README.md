# Stock Exchange Matching Engine

A high-performance stock exchange matching engine developed in C++. The project simulates the core functionality of an electronic trading system by matching buy and sell orders using the Price-Time Priority algorithm while focusing on efficient memory management and optimized data structures.

---

## Features

- Price-Time Priority order matching
- Buy and Sell order books
- Partial and complete order execution
- Order cancellation and modification
- Active order lookup using a custom hash map
- Memory pool allocator for efficient object management
- Binary search optimized price-level lookup
- Fixed-point price representation for accurate calculations
- File-based persistence (Save & Load orders)
- Interactive command-line interface
- Exception handling with custom exception classes
- Trading statistics and order book visualization

---

## Data Structures Used

- Custom Memory Pool Allocator
- Hash Map (Open Addressing with Linear Probing)
- Doubly Linked Lists
- Binary Search
- Arrays
- Custom Price Level Index
- Fixed-Point Arithmetic

---

## Technologies

- C++
- Object-Oriented Programming
- File Handling
- Exception Handling
- Data Structures & Algorithms

---

## Project Structure

```
main.cpp
README.md
```

---

## How to Run

Compile:

```bash
g++ main.cpp -o trade_engine
```

Run:

```bash
./trade_engine
```

---

## Sample Commands

```
BUY 100 20 Rahul
SELL 100 10 Neha
BOOK
VIEW 1
STATS
SAVE orders.txt
LOAD orders.txt
DONE
```

---

## Key Highlights

- Implements Price-Time Priority matching similar to modern electronic exchanges.
- Uses custom memory pools instead of repeated dynamic allocations.
- Implements a custom hash table instead of STL containers.
- Supports partial fills, order cancellation, and modification.
- Optimized for fast order lookup and execution.

---

## Future Improvements

- Multi-threaded order processing
- Dynamic memory pool expansion
- Support for multiple trading symbols
- Trade history and reporting
- Network-based order submission

---

## Author

Developed as a systems programming and data structures project in C++.
