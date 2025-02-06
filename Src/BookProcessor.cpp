#include "BookProcessor.h"
#include "Order.h"
#include "OrderBook.h"
#include "Snapshot.h"
#include "Progress.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Global mutex to synchronize console output.
std::mutex coutMutex;
// Global mutex to synchronize file writes for snapshot and index files.
std::mutex fileWriteMutex;

// Structure for index entry (epoch, offset)
struct IndexEntry {
    int64_t epoch;
    int64_t offset;
};

bool BookProcessor::parseLine(const std::string &line, Order &order) {
    std::istringstream iss(line);
    std::string epochStr, orderId, symbol, sideStr, categoryStr, priceStr, quantityStr;
    if (!(iss >> epochStr >> orderId >> symbol >> sideStr >> categoryStr >> priceStr >> quantityStr))
        return false;
    try {
        order.epoch = std::stoll(epochStr);
        order.orderId = orderId;
        order.symbol = symbol;
        order.side = (sideStr == "BUY") ? OrderSide::BUY : OrderSide::SELL;
        if (categoryStr == "NEW")
            order.category = OrderCategory::NEW;
        else if (categoryStr == "CANCEL")
            order.category = OrderCategory::CANCEL;
        else if (categoryStr == "TRADE")
            order.category = OrderCategory::TRADE;
        else
            return false;
        order.price = std::stod(priceStr);
        order.quantity = std::stoi(quantityStr);
    } catch (const std::exception &ex) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "Parsing error: " << ex.what() << " in line: " << line << std::endl;
        return false;
    }
    return true;
}

void BookProcessor::writeSnapshotBinary(const Snapshot &snapshot, const std::string &symbol) {
    std::string snapFilename = symbol + ".snap";
    std::string idxFilename = symbol + ".idx";

    // Write snapshot and index entry with thread-safe file access.
    {
        std::lock_guard<std::mutex> lock(fileWriteMutex);
        // Open snapshot file in append mode.
        std::ofstream ofs(snapFilename, std::ios::binary | std::ios::app);
        if (!ofs.is_open()) {
            std::lock_guard<std::mutex> lockOut(coutMutex);
            std::cerr << "Error: Failed to open snapshot file: " << snapFilename << std::endl;
            return;
        }
        // Get current offset.
        std::streampos offset = ofs.tellp();
        if (!writeBinarySnapshot(ofs, snapshot)) {
            std::lock_guard<std::mutex> lockOut(coutMutex);
            std::cerr << "Error writing snapshot to file: " << snapFilename << std::endl;
        }
        ofs.close();

        // Write index entry.
        std::ofstream idxOfs(idxFilename, std::ios::binary | std::ios::app);
        if (!idxOfs.is_open()) {
            std::lock_guard<std::mutex> lockOut(coutMutex);
            std::cerr << "Error: Failed to open index file: " << idxFilename << std::endl;
            return;
        }
        // Create index entry.
        IndexEntry entry;
        entry.epoch = snapshot.epoch;
        entry.offset = static_cast<int64_t>(offset);
        idxOfs.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
        idxOfs.close();
    }
}

void BookProcessor::processFile(const std::string &filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "Error: Failed to open file: " << filePath << std::endl;
        return;
    }
    std::string line;
    OrderBook orderBook("");
    bool orderBookInitialized = false;
    while (std::getline(ifs, line)) {
        if (line.empty())
            continue;
        // Update global processed bytes counter.
        g_bytesProcessed += static_cast<uint64_t>(line.size() + 1);
        Order order;
        if (!parseLine(line, order)) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Warning: Failed to parse line: " << line << std::endl;
            continue;
        }
        if (!orderBookInitialized) {
            orderBook = OrderBook(order.symbol);
            orderBookInitialized = true;
        }
        try {
            orderBook.processOrder(order);
        } catch (const std::exception &ex) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Error processing order " << order.orderId << ": " << ex.what() << std::endl;
            continue;
        }
        // Get the snapshot and write it.
        try {
            Snapshot snap = orderBook.getSnapshot(order.epoch);
            // Ensure symbol is fixed length
            std::strncpy(snap.symbol, order.symbol.c_str(), sizeof(snap.symbol)-1);
            snap.symbol[sizeof(snap.symbol)-1] = '\0';
            writeSnapshotBinary(snap, order.symbol);
        } catch (const std::exception &ex) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Error generating snapshot at epoch " << order.epoch << ": " << ex.what() << std::endl;
        }
    }
    ifs.close();
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Completed processing file: " << filePath << std::endl;
    }
}

void BookProcessor::process() {
    std::vector<std::thread> threads;
    for (const auto &filePath : filePaths_) {
        threads.emplace_back([this, filePath]() {
            this->processFile(filePath);
        });
    }
    for (auto &t : threads) {
        if (t.joinable())
            t.join();
    }
}

BookProcessor::BookProcessor(const std::vector<std::string>& filePaths)
    : filePaths_(filePaths)
{}
