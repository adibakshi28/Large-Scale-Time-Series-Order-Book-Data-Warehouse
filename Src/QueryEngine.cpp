#include "QueryEngine.h"
#include "Snapshot.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>
#include <vector>
#include <string>

// Structure for index entry in the index file.
struct IndexEntry {
    int64_t epoch;
    int64_t offset;
};

// Comparator used in binary search.
bool compareIndexEntry(const IndexEntry &a, const IndexEntry &b) {
    return a.epoch < b.epoch;
}

QueryEngine::QueryEngine(const std::vector<std::string>& symbolList)
    : symbolList_(symbolList)
{}

std::vector<Snapshot> QueryEngine::readSnapshotsForSymbol(const std::string &symbol, int64_t startEpoch, int64_t endEpoch) {
    std::vector<Snapshot> snapshots;
    std::string snapFilename = symbol + ".snap";
    std::string idxFilename = symbol + ".idx";

    // Read the index file.
    std::ifstream idxIfs(idxFilename, std::ios::binary);
    if (!idxIfs.is_open()) {
        std::cerr << "Error: Failed to open index file for symbol: " << symbol << std::endl;
        return snapshots;
    }
    std::vector<IndexEntry> indexEntries;
    IndexEntry entry;
    while (idxIfs.read(reinterpret_cast<char*>(&entry), sizeof(entry))) {
        indexEntries.push_back(entry);
    }
    idxIfs.close();

    if (indexEntries.empty())
        return snapshots;

    // Use binary search to find the first index entry with epoch >= startEpoch.
    IndexEntry searchKey;
    searchKey.epoch = startEpoch;
    searchKey.offset = 0;
    auto it = std::lower_bound(indexEntries.begin(), indexEntries.end(), searchKey, compareIndexEntry);
    if (it == indexEntries.end())
        return snapshots; // No snapshot in range

    // Open snapshot file.
    std::ifstream snapIfs(snapFilename, std::ios::binary);
    if (!snapIfs.is_open()) {
        std::cerr << "Error: Failed to open snapshot file for symbol: " << symbol << std::endl;
        return snapshots;
    }
    // Seek to the offset of the first relevant snapshot.
    snapIfs.seekg(it->offset, std::ios::beg);
    // Read snapshots until epoch > endEpoch.
    while (snapIfs.good() && !snapIfs.eof()) {
        Snapshot snap;
        if (!readBinarySnapshot(snapIfs, snap))
            break;
        if (snap.epoch > endEpoch)
            break;
        if (snap.epoch >= startEpoch && snap.epoch <= endEpoch) {
            snapshots.push_back(snap);
        }
    }
    snapIfs.close();
    return snapshots;
}

std::vector<Snapshot> QueryEngine::query(const QueryCriteria &criteria) {
    std::vector<Snapshot> results;
    if (criteria.startEpoch > criteria.endEpoch) {
        std::cerr << "Error: startEpoch is greater than endEpoch." << std::endl;
        return results;
    }
    std::vector<std::string> symbolsToQuery = criteria.symbols.empty() ? symbolList_ : criteria.symbols;
    for (const auto &symbol : symbolsToQuery) {
        try {
            std::vector<Snapshot> snaps = readSnapshotsForSymbol(symbol, criteria.startEpoch, criteria.endEpoch);
            results.insert(results.end(), snaps.begin(), snaps.end());
        } catch (const std::exception &ex) {
            std::cerr << "Error processing symbol " << symbol << ": " << ex.what() << std::endl;
        }
    }
    std::sort(results.begin(), results.end(), [](const Snapshot &a, const Snapshot &b) {
        return a.epoch < b.epoch;
    });
    return results;
}

// Helper: Formats a double to two decimal places.
std::string formatDouble(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}

// Helper: Formats a price level as "quantity@price".
std::string formatPriceLevel(int32_t quantity, double price) {
    if (price < 0)
        return "N.A";
    return std::to_string(quantity) + "@" + formatDouble(price);
}

// Helper: Formats a price field.
std::string formatPrice(double price) {
    if (price < 0)
        return "N.A";
    return formatDouble(price);
}

void QueryEngine::printSnapshots(const std::vector<Snapshot> &snapshots, const QueryCriteria &criteria) const {
    // Allowed field names for selective output.
    const std::vector<std::string> allowedFields = {
        "symbol", "epoch",
        "bid1p", "bid1q", "bid2p", "bid2q", "bid3p", "bid3q", "bid4p", "bid4q", "bid5p", "bid5q",
        "ask1p", "ask1q", "ask2p", "ask2q", "ask3p", "ask3q", "ask4p", "ask4q", "ask5p", "ask5q",
        "lastTradePrice", "lastTradeQuantity"
    };

    // Default grouped view if no selective fields provided.
    if (criteria.selectedFields.empty()) {
        std::vector<std::string> outputHeaders = {
            "symbol", "epoch",
            "bid5q@bid5p", "bid4q@bid4p", "bid3q@bid3p", "bid2q@bid2p", "bid1q@bid1p",
            "X",
            "ask1q@ask1p", "ask2q@ask2p", "ask3q@ask3p", "ask4q@ask4p", "ask5q@ask5p",
            "lastTradePrice", "lastTradeQuantity"
        };

        // Print header.
        for (size_t i = 0; i < outputHeaders.size(); ++i)
            std::cout << outputHeaders[i] << (i < outputHeaders.size()-1 ? ", " : "\n");

        // Print each snapshot.
        for (const auto &snap : snapshots) {
            std::cout << snap.symbol << ", " << snap.epoch << ", ";
            // Print bid levels in reverse order (bid5 first)
            for (int i = 4; i >= 0; --i) {
                std::cout << formatPriceLevel(snap.bidQuantities[i], snap.bidPrices[i]);
                std::cout << (i > 0 ? ", " : ", ");
            }
            std::cout << "X, ";
            // Print ask levels in natural order.
            for (int i = 0; i < 5; ++i) {
                std::cout << formatPriceLevel(snap.askQuantities[i], snap.askPrices[i]);
                std::cout << (i < 4 ? ", " : ", ");
            }
            // Print last trade info.
            std::cout << (snap.lastTradePrice < 0 ? "N.A" : formatPrice(snap.lastTradePrice)) << ", ";
            std::cout << (snap.lastTradeQuantity == 0 ? "N.A" : std::to_string(snap.lastTradeQuantity)) << "\n";
        }
    } else {
        // Validate selected fields.
        std::unordered_set<std::string> validFields(allowedFields.begin(), allowedFields.end());
        bool errorFound = false;
        for (const auto &field : criteria.selectedFields) {
            if (validFields.find(field) == validFields.end()) {
                std::cerr << "Error: Unknown field \"" << field << "\" in query criteria." << std::endl;
                std::cerr << "Allowed fields -> symbol, epoch, bid1p, bid1q, bid2p, bid2q, bid3p, bid3q, bid4p, bid4q, bid5p, bid5q, ask1p, ask1q, ask2p, ask2q, ask3p, ask3q, ask4p, ask4q, ask5p, ask5q, lastTradePrice, lastTradeQuantity" << std::endl;
                errorFound = true;
            }
        }
        if (errorFound) {
            std::cerr << "Please check your selected fields and try again." << std::endl;
            return;
        }

        // Build header from allowedFields that appear in selectedFields.
        std::vector<std::string> headerToPrint;
        for (const auto &field : allowedFields) {
            if (criteria.selectedFields.find(field) != criteria.selectedFields.end())
                headerToPrint.push_back(field);
        }
        if (headerToPrint.empty()) {
            std::cerr << "Error: No valid fields selected." << std::endl;
            return;
        }
        for (size_t i = 0; i < headerToPrint.size(); ++i) {
            std::cout << headerToPrint[i] << (i < headerToPrint.size() - 1 ? ", " : "\n");
        }

        // Print each snapshot with only the selected fields.
        for (const auto &snap : snapshots) {
            std::unordered_map<std::string, std::string> fieldMap;
            fieldMap["symbol"] = snap.symbol;
            fieldMap["epoch"] = std::to_string(snap.epoch);
            // Map bid fields.
            for (int i = 0; i < 5; ++i) {
                std::string bidPriceKey = "bid" + std::to_string(i+1) + "p";
                std::string bidQtyKey = "bid" + std::to_string(i+1) + "q";
                fieldMap[bidPriceKey] = (snap.bidPrices[i] < 0 ? "N.A" : formatPrice(snap.bidPrices[i]));
                fieldMap[bidQtyKey] = (snap.bidQuantities[i] == 0 ? "N.A" : std::to_string(snap.bidQuantities[i]));
            }
            // Map ask fields.
            for (int i = 0; i < 5; ++i) {
                std::string askPriceKey = "ask" + std::to_string(i+1) + "p";
                std::string askQtyKey = "ask" + std::to_string(i+1) + "q";
                fieldMap[askPriceKey] = (snap.askPrices[i] < 0 ? "N.A" : formatPrice(snap.askPrices[i]));
                fieldMap[askQtyKey] = (snap.askQuantities[i] == 0 ? "N.A" : std::to_string(snap.askQuantities[i]));
            }
            fieldMap["lastTradePrice"] = (snap.lastTradePrice < 0 ? "N.A" : formatPrice(snap.lastTradePrice));
            fieldMap["lastTradeQuantity"] = (snap.lastTradeQuantity == 0 ? "N.A" : std::to_string(snap.lastTradeQuantity));
            
            for (size_t i = 0; i < headerToPrint.size(); ++i) {
                std::cout << fieldMap[headerToPrint[i]];
                if (i < headerToPrint.size() - 1)
                    std::cout << ", ";
            }
            std::cout << "\n";
        }
    }
}
