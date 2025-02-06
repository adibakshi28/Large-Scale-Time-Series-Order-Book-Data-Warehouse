#ifndef BOOKPROCESSOR_H
#define BOOKPROCESSOR_H

#include "OrderBook.h"
#include "Snapshot.h"
#include "Order.h"
#include <string>
#include <vector>

/**
 * @brief The BookProcessor class
 * 
 * Processes raw order book update files and builds the order book.
 * Generates time-series snapshots and stores them in a persistent binary format.
 */
class BookProcessor {
public:
    /**
     * @brief Construct a new BookProcessor object
     * 
     * @param filePaths A vector of file paths containing raw order data.
     */
    explicit BookProcessor(const std::vector<std::string>& filePaths);

    /**
     * @brief Processes all provided files concurrently.
     */
    void process();

private:
    std::vector<std::string> filePaths_;  // List of raw data file paths.

    /**
     * @brief Processes a single file.
     * 
     * Reads the file line-by-line, parses orders, updates the order book,
     * and writes snapshots to disk.
     * 
     * @param filePath The path of the file to process.
     */
    void processFile(const std::string &filePath);

    /**
     * @brief Parses a single line of the log file into an Order object.
     * 
     * Expected format: epoch order_id symbol order_side order_category price quantity.
     * 
     * @param line The input line from the log file.
     * @param order The Order object to be populated.
     * @return true if parsing succeeds; false otherwise.
     */
    bool parseLine(const std::string &line, Order &order);

    /**
     * @brief Writes the snapshot to a binary file and updates the index file.
     * 
     * The snapshot is appended to a file named "<symbol>.snap" and an index
     * entry is added to "<symbol>.idx" for fast retrieval.
     * 
     * @param snapshot The snapshot to write.
     * @param symbol The symbol corresponding to the snapshot.
     */
    void writeSnapshotBinary(const Snapshot &snapshot, const std::string &symbol);
};

#endif
