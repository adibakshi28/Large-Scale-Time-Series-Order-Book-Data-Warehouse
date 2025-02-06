#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <cstdint>
#include <fstream>
#include <cstring>

// A fixed order book snapshot structure for efficient storage and retrieval.
// This structure uses fixed arrays for bid and ask levels (5 levels each).
struct Snapshot {
    char symbol[8];            // Fixed-length symbol (zero-terminated if possible)
    int64_t epoch;             // Timestamp in nanoseconds

    double bidPrices[5];       // 5 best bid prices (highest first)
    int32_t bidQuantities[5];  // Corresponding bid quantities

    double askPrices[5];       // 5 best ask prices (lowest first)
    int32_t askQuantities[5];  // Corresponding ask quantities

    double lastTradePrice;     // Last trade price (or -1 if none)
    int32_t lastTradeQuantity; // Last trade quantity (or 0 if none)
};

// Write a Snapshot to a binary stream in fixed format.
inline bool writeBinarySnapshot(std::ofstream &ofs, const Snapshot &snap) {
    ofs.write(reinterpret_cast<const char*>(&snap), sizeof(snap));
    return ofs.good();
}

// Read a Snapshot from a binary stream in fixed format.
inline bool readBinarySnapshot(std::ifstream &ifs, Snapshot &snap) {
    ifs.read(reinterpret_cast<char*>(&snap), sizeof(snap));
    return ifs.gcount() == sizeof(snap);
}

#endif
