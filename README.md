# Large-Scale Time-Series Order Book Data Warehouse

## Overview
This project is a **high-performance market data warehouse** designed to support **efficient research and large-scale simulations**. It ingests raw order log files, updates order books for multiple tradable instruments, persists full time-series snapshots in a compact binary format, and provides a fast query engine for retrieving historical order book states.

The entire solution is written in **C++ (using C++17)** and is designed to run on **Windows OS** without external libraries.

---

## Features
- **High-speed order book processing**
- **Efficient binary storage format** for fast retrieval
- **Multithreaded processing** for improved performance
- **Custom indexing and binary search** for rapid query execution
- **Robust error handling and logging**
- **Comprehensive testing framework** with unit and integration tests

---

## Design Choices
### 1. Snapshot Storage Format
- Fixed-size **binary format** with direct access capability.
- Indexed using a **separate .idx file** for fast lookups.

### 2. Order Book Data Structures
- **STL Containers**: `unordered_map` for fast lookups, `std::map` for bid/ask levels.

### 3. Concurrency in Processing
- **Multi-threaded file processing** (one thread per order log file).
- **Mutex-based synchronization** for safe concurrent writes.

### 4. Query Engine and Indexing
- **Binary search** over indexed snapshots for fast queries.

### 5. Error Handling and Logging
- **Exception handling** with synchronized logging.

---

## Implemented Components
### 1. Core Components
- **Order Model (`Order.h`)**: Represents orders with attributes like timestamp, order ID, price, quantity, etc.
- **Order Book (`OrderBook.h/.cpp`)**: Manages order book state for each symbol.
- **Snapshot Persistence (`Snapshot.h`)**: Handles storage of fixed-size binary snapshots.
- **Book Processor (`BookProcessor.h/.cpp`)**: Reads and processes raw log files.
- **Query Engine (`QueryEngine.h/.cpp`)**: Performs binary search over indexed snapshots.

### 2. Testing Infrastructure
- **Unit tests** for core components (order book, snapshot handling, query engine).
- **Integration tests** for end-to-end order processing and querying.

---

## Future Enhancements
- **Storage optimizations**: Delta-based snapshots, memory-mapped file access.
- **Advanced query capabilities**: Filtering, caching, and query language (DSL).
- **Order book enhancements**: Optimized data structures, trade matching engine.
- **Robust logging and monitoring**: Advanced logging system and performance profiling.
- **Automated testing and CI/CD**: Google Test framework and GitHub Actions pipeline.

---

## Compilation and Execution

### 1. **Environment Requirements**
- **OS**: Windows (MinGW for `make`)
- **Compiler**: `g++` (MinGW-w64 recommended)
- **Dependencies**: No external libraries (C++17 standard library only)

### 2. **Build and Test Commands**
| Command             | Purpose                                      |
|---------------------|----------------------------------------------|
| `make`             | Compile the main application (`orderbook.exe`) |
| `make test`        | Compile and run tests (`orderbook_tests.exe`) |
| `make clean`       | Remove compiled files (`.exe`, `.o`)         |

### 3. **Execution Modes**
#### **Data Processing Mode (Default)**

To run the order book processing mode, use the following command:

    ./orderbook

This command reads raw order log files (e.g., SCH.log, SCS.log) stored in the Data/ directory. The system processes these logs and generates corresponding binary snapshot files (SCH.snap, SCS.snap).

Query Mode

To query the order book, use the following command format:./orderbook query <symbols> <startEpoch> <endEpoch> [<fields>]

Example Queries:

    Query full order book for a symbol:
    ./orderbook query SCH 1609724964077464154 1609724964129550454

    Query multiple symbols:
    ./orderbook query SCH,SCS 1609724964077464154 1609724964129550454

    Query specific fields:
    ./orderbook query SCH 1609724964077464154 1609724964129550454 symbol,epoch,bid1p,bid1q,ask1p,ask1q

Contribution

Contributions are welcome! If you would like to contribute, please fork the repository and submit a pull request with your changes.
License

This project is licensed under the MIT License.
Contact

For any issues or questions, please open an issue in the repository.
