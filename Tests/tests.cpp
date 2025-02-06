#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include "OrderBook.h"
#include "Order.h"
#include "Snapshot.h"
#include "QueryEngine.h"
#include "BookProcessor.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::unordered_set;
using std::unordered_map;

// ----------------------------------------------------------------------
// Helper Functions for Fixed-Size Snapshot Format
// ----------------------------------------------------------------------

// Compare bid level at a given index in the snapshot.
bool compareBidLevel(const Snapshot &snap, int index, double expectedPrice, int expectedQuantity) {
    return (snap.bidPrices[index] == expectedPrice && snap.bidQuantities[index] == expectedQuantity);
}

// Compare ask level at a given index in the snapshot.
bool compareAskLevel(const Snapshot &snap, int index, double expectedPrice, int expectedQuantity) {
    return (snap.askPrices[index] == expectedPrice && snap.askQuantities[index] == expectedQuantity);
}

// Helper: Write a vector of strings to a file.
void writeToFile(const string &filename, const vector<string> &lines) {
    std::ofstream ofs(filename);
    assert(ofs.is_open());
    for (const auto &line : lines) {
        ofs << line << "\n";
    }
    ofs.close();
}

// ----------------------------------------------------------------------
// Helper: Create an Index File from a Snapshot File
// ----------------------------------------------------------------------
//
// This function opens the snapshot file (e.g. "TEST.snap"), reads each fixed‑size
// Snapshot record and writes an index record (epoch, offset) into "TEST.idx".
//
void createIndexFile(const string &symbol) {
    string snapFilename = symbol + ".snap";
    string idxFilename = symbol + ".idx";
    std::ifstream snapIfs(snapFilename, std::ios::binary);
    assert(snapIfs.is_open());
    std::ofstream idxOfs(idxFilename, std::ios::binary);
    assert(idxOfs.is_open());
    
    int64_t offset = 0;
    Snapshot snap;
    // Use our test's definition for IndexEntry.
    struct IndexEntry { int64_t epoch; int64_t offset; };
    
    while (true) {
        snapIfs.seekg(offset, std::ios::beg);
        if (!readBinarySnapshot(snapIfs, snap))
            break;
        IndexEntry entry;
        entry.epoch = snap.epoch;
        entry.offset = offset;
        idxOfs.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
        offset += sizeof(Snapshot);
    }
    snapIfs.close();
    idxOfs.close();
}

// ----------------------------------------------------------------------
// OrderBook Tests
// ----------------------------------------------------------------------
void testOrderBook() {
    cout << "Running OrderBook tests..." << endl;
    
    // Test 1: Basic NEW orders.
    {
        OrderBook ob("TEST");
        Order o1 = {0, "1", "TEST", OrderSide::BUY, OrderCategory::NEW, 10.0, 100};
        Order o2 = {1, "2", "TEST", OrderSide::SELL, OrderCategory::NEW, 11.0, 150};
        ob.processOrder(o1);
        ob.processOrder(o2);
        
        Snapshot snap = ob.getSnapshot(1);
        // Verify bid side.
        assert(compareBidLevel(snap, 0, 10.0, 100));
        for (int i = 1; i < 5; ++i)
            assert(compareBidLevel(snap, i, -1.0, 0));
        // Verify ask side.
        assert(compareAskLevel(snap, 0, 11.0, 150));
        for (int i = 1; i < 5; ++i)
            assert(compareAskLevel(snap, i, -1.0, 0));
        // Verify no trade.
        assert(snap.lastTradePrice < 0);
        assert(snap.lastTradeQuantity == 0);
    }
    
    // Test 2: NEW orders followed by CANCEL.
    {
        OrderBook ob("TEST");
        Order buy1 = {0, "b1", "TEST", OrderSide::BUY, OrderCategory::NEW, 9.6, 4};
        Order buy2 = {0, "b2", "TEST", OrderSide::BUY, OrderCategory::NEW, 9.5, 6};
        Order sell1 = {0, "s1", "TEST", OrderSide::SELL, OrderCategory::NEW, 9.7, 5};
        Order sell2 = {0, "s2", "TEST", OrderSide::SELL, OrderCategory::NEW, 9.7, 10};
        ob.processOrder(buy1);
        ob.processOrder(buy2);
        ob.processOrder(sell1);
        ob.processOrder(sell2);
        Order cancelBuy = {1, "b1", "TEST", OrderSide::BUY, OrderCategory::CANCEL, 9.6, 4};
        ob.processOrder(cancelBuy);
        Snapshot snap = ob.getSnapshot(1);
        // Verify only buy2 remains.
        assert(compareBidLevel(snap, 0, 9.5, 6));
        for (int i = 1; i < 5; ++i)
            assert(compareBidLevel(snap, i, -1.0, 0));
        // Verify aggregated SELL orders.
        assert(compareAskLevel(snap, 0, 9.7, 15));
        for (int i = 1; i < 5; ++i)
            assert(compareAskLevel(snap, i, -1.0, 0));
    }
    
    // Test 3: NEW orders followed by TRADE orders.
    {
        OrderBook ob("TEST");
        Order buy1 = {0, "b1", "TEST", OrderSide::BUY, OrderCategory::NEW, 9.6, 4};
        Order buy2 = {0, "b2", "TEST", OrderSide::BUY, OrderCategory::NEW, 9.5, 6};
        Order sell1 = {0, "s1", "TEST", OrderSide::SELL, OrderCategory::NEW, 9.7, 5};
        Order sell2 = {0, "s2", "TEST", OrderSide::SELL, OrderCategory::NEW, 9.7, 10};
        ob.processOrder(buy1);
        ob.processOrder(buy2);
        ob.processOrder(sell1);
        ob.processOrder(sell2);
        // Process a trade on SELL order s1.
        Order tradeSell = {1, "s1", "TEST", OrderSide::SELL, OrderCategory::TRADE, 9.7, 4};
        ob.processOrder(tradeSell);
        Snapshot snap = ob.getSnapshot(1);
        assert(compareAskLevel(snap, 0, 9.7, 11));
        for (int i = 1; i < 5; ++i)
            assert(compareAskLevel(snap, i, -1.0, 0));
        assert(snap.lastTradePrice == 9.7);
        assert(snap.lastTradeQuantity == 4);
        
        // Process a trade on BUY order b1.
        Order tradeBuy = {2, "b1", "TEST", OrderSide::BUY, OrderCategory::TRADE, 9.6, 4};
        ob.processOrder(tradeBuy);
        snap = ob.getSnapshot(2);
        assert(compareBidLevel(snap, 0, 9.5, 6));
        for (int i = 1; i < 5; ++i)
            assert(compareBidLevel(snap, i, -1.0, 0));
    }
    
    cout << "OrderBook tests passed (1/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// Snapshot Serialization Tests
// ----------------------------------------------------------------------
void testSnapshotSerialization() {
    cout << "Running Snapshot Serialization tests..." << endl;
    
    Snapshot snap;
    std::memset(snap.symbol, 0, sizeof(snap.symbol));
    std::strncpy(snap.symbol, "TEST", sizeof(snap.symbol) - 1);
    snap.epoch = 123456789;
    snap.lastTradePrice = 9.99;
    snap.lastTradeQuantity = 50;
    // Set bid levels.
    snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 100;
    snap.bidPrices[1] = 9.5;  snap.bidQuantities[1] = 50;
    for (int i = 2; i < 5; ++i) {
        snap.bidPrices[i] = -1.0;
        snap.bidQuantities[i] = 0;
    }
    // Set ask levels.
    snap.askPrices[0] = 10.5; snap.askQuantities[0] = 200;
    snap.askPrices[1] = 11.0; snap.askQuantities[1] = 150;
    for (int i = 2; i < 5; ++i) {
        snap.askPrices[i] = -1.0;
        snap.askQuantities[i] = 0;
    }
    
    const char* filename = "temp.snap";
    std::ofstream ofs(filename, std::ios::binary);
    assert(ofs.is_open());
    bool writeOk = writeBinarySnapshot(ofs, snap);
    ofs.close();
    assert(writeOk);
    
    Snapshot snapRead;
    std::ifstream ifs(filename, std::ios::binary);
    assert(ifs.is_open());
    bool readOk = readBinarySnapshot(ifs, snapRead);
    ifs.close();
    std::remove(filename);
    
    assert(readOk);
    // Ensure symbol matches.
    std::memset(snapRead.symbol, 0, sizeof(snapRead.symbol));
    std::strncpy(snapRead.symbol, "TEST", sizeof(snapRead.symbol) - 1);
    
    assert(snapRead.epoch == snap.epoch);
    assert(snapRead.lastTradePrice == snap.lastTradePrice);
    assert(snapRead.lastTradeQuantity == snap.lastTradeQuantity);
    for (int i = 0; i < 5; ++i)
        assert(compareBidLevel(snapRead, i, snap.bidPrices[i], snap.bidQuantities[i]));
    for (int i = 0; i < 5; ++i)
        assert(compareAskLevel(snapRead, i, snap.askPrices[i], snap.askQuantities[i]));
    
    cout << "Snapshot Serialization tests passed (2/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// QueryEngine Default Output Test
// ----------------------------------------------------------------------
void testQueryEngineDefaultOutput() {
    cout << "Running QueryEngine Default Output Test..." << endl;
    
    // Create a temporary snapshot file for symbol TEST.
    const char* filename = "TEST.snap";
    {
        std::ofstream ofs(filename, std::ios::binary);
        assert(ofs.is_open());
        Snapshot snap;
        std::memset(snap.symbol, 0, sizeof(snap.symbol));
        std::strncpy(snap.symbol, "TEST", sizeof(snap.symbol) - 1);
        snap.epoch = 1000;
        snap.lastTradePrice = 10.0;
        snap.lastTradeQuantity = 20;
        snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 100;
        for (int i = 1; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.5; snap.askQuantities[0] = 200;
        for (int i = 1; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        // Second snapshot.
        snap.epoch = 2000;
        snap.lastTradePrice = 11.0;
        snap.lastTradeQuantity = 30;
        snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 80;
        for (int i = 1; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.5; snap.askQuantities[0] = 180;
        for (int i = 1; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        ofs.close();
    }
    // Create corresponding index file.
    createIndexFile("TEST");
    
    vector<string> symbols = {"TEST"};
    QueryEngine engine(symbols);
    QueryCriteria criteria;
    criteria.startEpoch = 500;
    criteria.endEpoch = 2500;
    criteria.selectedFields = {}; // Default grouped view.
    criteria.symbols = symbols;
    
    vector<Snapshot> results = engine.query(criteria);
    // Expect two snapshots.
    assert(results.size() == 2);
    
    cout << "Default grouped output:" << endl;
    engine.printSnapshots(results, criteria);
    
    std::remove("TEST.snap");
    std::remove("TEST.idx");
    cout << "QueryEngine Default Output Test passed (3/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// QueryEngine Selective Output Test
// ----------------------------------------------------------------------
void testQueryEngineSelectiveOutput() {
    cout << "Running QueryEngine Selective Output Test..." << endl;
    
    // Create a temporary snapshot file for symbol TEST.
    const char* filename = "TEST.snap";
    {
        std::ofstream ofs(filename, std::ios::binary);
        assert(ofs.is_open());
        Snapshot snap;
        std::memset(snap.symbol, 0, sizeof(snap.symbol));
        std::strncpy(snap.symbol, "TEST", sizeof(snap.symbol) - 1);
        snap.epoch = 1500;
        snap.lastTradePrice = 12.3456;
        snap.lastTradeQuantity = 40;
        snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 100;
        snap.bidPrices[1] = 9.8;  snap.bidQuantities[1] = 60;
        for (int i = 2; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.5; snap.askQuantities[0] = 200;
        snap.askPrices[1] = 10.7; snap.askQuantities[1] = 150;
        for (int i = 2; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        ofs.close();
    }
    // Create corresponding index file.
    createIndexFile("TEST");
    
    vector<string> symbols = {"TEST"};
    QueryEngine engine(symbols);
    QueryCriteria criteria;
    criteria.startEpoch = 1000;
    criteria.endEpoch = 2000;
    criteria.symbols = symbols;
    // Select only specific fields.
    unordered_set<string> selectFields = {"symbol", "epoch", "bid2q", "bid2p", "ask2q", "ask2p", "lastTradePrice", "lastTradeQuantity"};
    criteria.selectedFields = selectFields;
    
    vector<Snapshot> results = engine.query(criteria);
    // Expect one snapshot.
    assert(results.size() == 1);
    
    cout << "Selective output:" << endl;
    engine.printSnapshots(results, criteria);
    
    std::remove("TEST.snap");
    std::remove("TEST.idx");
    cout << "QueryEngine Selective Output Test passed (4/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// QueryEngine Invalid Fields Test
// ----------------------------------------------------------------------
void testQueryEngineInvalidFields() {
    cout << "Running QueryEngine Invalid Fields Test..." << endl;
    
    // Create a temporary snapshot file for symbol TEST.
    const char* filename = "TEST.snap";
    {
        std::ofstream ofs(filename, std::ios::binary);
        assert(ofs.is_open());
        Snapshot snap;
        std::memset(snap.symbol, 0, sizeof(snap.symbol));
        std::strncpy(snap.symbol, "TEST", sizeof(snap.symbol) - 1);
        snap.epoch = 1000;
        snap.lastTradePrice = 10.0;
        snap.lastTradeQuantity = 20;
        snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 100;
        for (int i = 1; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.5; snap.askQuantities[0] = 200;
        for (int i = 1; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        ofs.close();
    }
    // Create corresponding index file.
    createIndexFile("TEST");
    
    vector<string> symbols = {"TEST"};
    QueryEngine engine(symbols);
    QueryCriteria criteria;
    criteria.startEpoch = 0;
    criteria.endEpoch = 2000;
    criteria.symbols = symbols;
    // Provide an invalid field name.
    unordered_set<string> selectFields = {"symbol", "epoch", "bid6p"};
    criteria.selectedFields = selectFields;
    
    cout << "Expect error message for invalid field:" << endl;
    engine.printSnapshots(engine.query(criteria), criteria);
    
    std::remove("TEST.snap");
    std::remove("TEST.idx");
    cout << "QueryEngine Invalid Fields Test passed (5/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// QueryEngine Multi-Symbol Test
// ----------------------------------------------------------------------
void testQueryEngineMultiSymbol() {
    cout << "Running QueryEngine Multi-Symbol Test..." << endl;
    
    // Create temporary snapshot files for two symbols: TEST1 and TEST2.
    const char* filename1 = "TEST1.snap";
    const char* filename2 = "TEST2.snap";
    {
        std::ofstream ofs(filename1, std::ios::binary);
        assert(ofs.is_open());
        Snapshot snap;
        std::memset(snap.symbol, 0, sizeof(snap.symbol));
        std::strncpy(snap.symbol, "TEST1", sizeof(snap.symbol) - 1);
        snap.epoch = 1000;
        snap.lastTradePrice = 10.0;
        snap.lastTradeQuantity = 20;
        snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 100;
        for (int i = 1; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.5; snap.askQuantities[0] = 200;
        for (int i = 1; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        ofs.close();
    }
    {
        std::ofstream ofs(filename2, std::ios::binary);
        assert(ofs.is_open());
        Snapshot snap;
        std::memset(snap.symbol, 0, sizeof(snap.symbol));
        std::strncpy(snap.symbol, "TEST2", sizeof(snap.symbol) - 1);
        snap.epoch = 1500;
        snap.lastTradePrice = 11.0;
        snap.lastTradeQuantity = 25;
        snap.bidPrices[0] = 9.8; snap.bidQuantities[0] = 90;
        for (int i = 1; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.3; snap.askQuantities[0] = 180;
        for (int i = 1; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        ofs.close();
    }
    // Create corresponding index files.
    createIndexFile("TEST1");
    createIndexFile("TEST2");
    
    vector<string> symbols = {"TEST1", "TEST2"};
    QueryEngine engine(symbols);
    QueryCriteria criteria;
    criteria.startEpoch = 0;
    criteria.endEpoch = 2000;
    criteria.selectedFields = {};
    criteria.symbols = symbols;
    
    vector<Snapshot> results = engine.query(criteria);
    // Expect two snapshots (one from each symbol).
    assert(results.size() == 2);
    
    cout << "Multi-Symbol grouped output:" << endl;
    engine.printSnapshots(results, criteria);
    
    // Clean up temporary files.
    std::remove("TEST1.snap");
    std::remove("TEST1.idx");
    std::remove("TEST2.snap");
    std::remove("TEST2.idx");
    
    cout << "QueryEngine Multi-Symbol Test passed (6/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// QueryEngine No Results Test
// ----------------------------------------------------------------------
void testQueryEngineNoResults() {
    cout << "Running QueryEngine No Results Test..." << endl;
    
    // Create a temporary snapshot file for symbol "TEST".
    const char* filename = "TEST.snap";
    {
        std::ofstream ofs(filename, std::ios::binary);
        assert(ofs.is_open());
        Snapshot snap;
        std::memset(snap.symbol, 0, sizeof(snap.symbol));
        std::strncpy(snap.symbol, "TEST", sizeof(snap.symbol) - 1);
        snap.epoch = 1000;
        snap.lastTradePrice = 10.0;
        snap.lastTradeQuantity = 20;
        snap.bidPrices[0] = 10.0; snap.bidQuantities[0] = 100;
        for (int i = 1; i < 5; ++i) { snap.bidPrices[i] = -1.0; snap.bidQuantities[i] = 0; }
        snap.askPrices[0] = 10.5; snap.askQuantities[0] = 200;
        for (int i = 1; i < 5; ++i) { snap.askPrices[i] = -1.0; snap.askQuantities[i] = 0; }
        writeBinarySnapshot(ofs, snap);
        ofs.close();
    }
    // Create corresponding index file.
    createIndexFile("TEST");
    
    vector<string> symbols = {"TEST"};
    QueryEngine engine(symbols);
    QueryCriteria criteria;
    criteria.startEpoch = 2000;
    criteria.endEpoch = 3000;
    criteria.symbols = symbols;
    criteria.selectedFields = {};
    
    vector<Snapshot> results = engine.query(criteria);
    assert(results.size() == 0);
    
    std::remove("TEST.snap");
    std::remove("TEST.idx");
    cout << "QueryEngine No Results Test passed (7/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// Test: Index File Content Test
// ----------------------------------------------------------------------
void testIndexFileContent() {
    cout << "Running Index File Content Test..." << endl;
    
    // Create a temporary snapshot file with 3 snapshots.
    const char* filename = "IDXTEST.snap";
    {
        std::ofstream ofs(filename, std::ios::binary);
        assert(ofs.is_open());
        for (int i = 0; i < 3; ++i) {
            Snapshot snap;
            std::memset(snap.symbol, 0, sizeof(snap.symbol));
            std::strncpy(snap.symbol, "IDXTEST", sizeof(snap.symbol));
            snap.symbol[sizeof(snap.symbol) - 1] = '\0';
            snap.epoch = 1000 + i * 1000;
            snap.lastTradePrice = 10.0 + i;
            snap.lastTradeQuantity = 20 + i;
            // Set first level; others default.
            snap.bidPrices[0] = 10.0 + i; snap.bidQuantities[0] = 100 + i;
            for (int j = 1; j < 5; ++j) { snap.bidPrices[j] = -1.0; snap.bidQuantities[j] = 0; }
            snap.askPrices[0] = 10.5 + i; snap.askQuantities[0] = 200 + i;
            for (int j = 1; j < 5; ++j) { snap.askPrices[j] = -1.0; snap.askQuantities[j] = 0; }
            writeBinarySnapshot(ofs, snap);
        }
        ofs.close();
    }
    // Create corresponding index file.
    createIndexFile("IDXTEST");
    
    // Open the index file and count entries.
    std::ifstream idxIfs("IDXTEST.idx", std::ios::binary);
    assert(idxIfs.is_open());
    idxIfs.seekg(0, std::ios::end);
    std::streampos size = idxIfs.tellg();
    idxIfs.close();
    // Each index entry is sizeof(int64_t) * 2.
    size_t entrySize = sizeof(int64_t) * 2;
    size_t count = size / entrySize;
    assert(count == 3);
    
    std::remove("IDXTEST.snap");
    std::remove("IDXTEST.idx");
    cout << "Index File Content Test passed (8/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// BookProcessor Tests
// ----------------------------------------------------------------------

// Test: BookProcessor with an empty log file.
void testBookProcessorEmptyFile() {
    cout << "Running BookProcessor Empty File Test..." << endl;
    
    string filename = "empty.log";
    {
        std::ofstream ofs(filename);
        // Write nothing.
        ofs.close();
    }
    vector<string> files = { filename };
    BookProcessor processor(files);
    processor.process();
    // For an empty file, no snapshot file should be created.
    std::ifstream snapIfs("EMPTY.snap", std::ios::binary);
    // The file should not exist.
    assert(!snapIfs.is_open());
    std::remove(filename.c_str());
    cout << "BookProcessor Empty File Test passed (9/12)!" << endl << endl;
}

// Test: BookProcessor with a single valid order.
void testBookProcessorSingleOrder() {
    cout << "Running BookProcessor Single Order Test..." << endl;
    
    string filename = "single.log";
    {
        // One valid order line.
        vector<string> lines = {
            "1609722840017828773 7374421476721609001 SINGLE BUY NEW 106.50 10"
        };
        writeToFile(filename, lines);
    }
    // Remove any previous snapshot/index files.
    std::remove("SINGLE.snap");
    std::remove("SINGLE.idx");
    
    vector<string> files = { filename };
    BookProcessor processor(files);
    processor.process();
    
    // The snapshot file for symbol "SINGLE" should exist and have exactly one snapshot.
    std::ifstream snapIfs("SINGLE.snap", std::ios::binary);
    assert(snapIfs.is_open());
    snapIfs.seekg(0, std::ios::end);
    std::streampos size = snapIfs.tellg();
    snapIfs.close();
    assert(size == sizeof(Snapshot));
    
    // Create the index file.
    createIndexFile("SINGLE");
    std::ifstream idxIfs("SINGLE.idx", std::ios::binary);
    assert(idxIfs.is_open());
    idxIfs.seekg(0, std::ios::end);
    std::streampos idxSize = idxIfs.tellg();
    idxIfs.close();
    size_t entrySize = sizeof(int64_t) * 2;
    assert(static_cast<size_t>(idxSize) == entrySize);
    
    std::remove(filename.c_str());
    std::remove("SINGLE.snap");
    std::remove("SINGLE.idx");
    cout << "BookProcessor Single Order Test passed (10/12)!" << endl << endl;
}

// Test: BookProcessor with invalid input lines.
void testBookProcessorInvalidInput() {
    cout << "Running BookProcessor Invalid Input Test..." << endl;
    
    string filename = "invalid.log";
    {
        // Three lines: first valid, second invalid (missing fields), third valid.
        vector<string> lines = {
            "1609722840017828773 7374421476721609001 INVALID BUY NEW 106.50 10",
            "this is not a valid line",
            "1609722840017829773 7374421476721609002 INVALID SELL NEW 107.00 5"
        };
        writeToFile(filename, lines);
    }
    // Remove any previous snapshot/index files.
    std::remove("INVALID.snap");
    std::remove("INVALID.idx");
    
    vector<string> files = { filename };
    BookProcessor processor(files);
    processor.process();
    
    // The processor should have processed only the two valid lines.
    std::ifstream snapIfs("INVALID.snap", std::ios::binary);
    assert(snapIfs.is_open());
    snapIfs.seekg(0, std::ios::end);
    std::streampos size = snapIfs.tellg();
    snapIfs.close();
    // Expect 2 snapshots.
    assert(size == 2 * sizeof(Snapshot));
    
    // Create index file.
    createIndexFile("INVALID");
    std::ifstream idxIfs("INVALID.idx", std::ios::binary);
    assert(idxIfs.is_open());
    idxIfs.seekg(0, std::ios::end);
    std::streampos idxSize = idxIfs.tellg();
    idxIfs.close();
    size_t entrySize = sizeof(int64_t) * 2;
    assert(static_cast<size_t>(idxSize) == 2 * entrySize);
    
    std::remove(filename.c_str());
    std::remove("INVALID.snap");
    std::remove("INVALID.idx");
    cout << "BookProcessor Invalid Input Test passed (11/12)!" << endl << endl;
}

// ----------------------------------------------------------------------
// Integration Test: Process and Query for ABB and CDD Logs
// ----------------------------------------------------------------------
void testProcessAndQueryABB_CDD() {
    cout << "Running process and query test for ABB and CDD..." << endl;
    
    // Create temporary log files with sample data.
    vector<string> abbLines = {
        "1609722840017828773 7374421476721609001 ABB SELL NEW 107.01 20",
        "1609722840017829773 7374421476721609002 ABB BUY NEW 106.50 10",
        "1609722840017830773 7374421476721609003 ABB SELL NEW 108.00 5",
        "1609722840017831773 7374421476721609004 ABB BUY NEW 106.80 15",
        "1609722840017832773 7374421476721609005 ABB SELL NEW 107.90 7",
        "1609722840017833773 7374421476721609006 ABB BUY CANCEL 106.50 10",
        "1609722840017834773 7374421476721609007 ABB SELL CANCEL 107.01 20",
        "1609722840017835773 7374421476721609008 ABB BUY NEW 106.70 8",
        "1609722840017836773 7374421476721609009 ABB SELL TRADE 108.00 5",
        "1609722840017837773 7374421476721609010 ABB BUY TRADE 106.80 15"
    };
    vector<string> cddLines = {
        "1609722840027798182 7374421476721609501 CDD SELL NEW 272.10 5",
        "1609722840027799282 7374421476721609502 CDD BUY NEW 271.50 10",
        "1609722840027800382 7374421476721609503 CDD SELL NEW 273.00 8",
        "1609722840027801482 7374421476721609504 CDD BUY NEW 271.80 12",
        "1609722840027802582 7374421476721609505 CDD SELL NEW 272.90 4",
        "1609722840027803682 7374421476721609506 CDD BUY CANCEL 271.50 10",
        "1609722840027804782 7374421476721609507 CDD SELL CANCEL 272.10 5",
        "1609722840027805882 7374421476721609508 CDD BUY NEW 271.70 9",
        "1609722840027806982 7374421476721609509 CDD SELL TRADE 273.00 8",
        "1609722840027808082 7374421476721609510 CDD BUY TRADE 271.80 12"
    };
    writeToFile("ABB.log", abbLines);
    writeToFile("CDD.log", cddLines);
    
    // Remove any existing snapshot/index files.
    std::remove("ABB.snap");
    std::remove("CDD.snap");
    std::remove("ABB.idx");
    std::remove("CDD.idx");
    
    vector<string> logFiles = {"ABB.log", "CDD.log"};
    BookProcessor processor(logFiles);
    processor.process();
    
    // Query ABB snapshots.
    vector<string> symbols1 = {"ABB"};
    QueryEngine engine1(symbols1);
    QueryCriteria criteria1;
    criteria1.startEpoch = 1609722840017828773;
    criteria1.endEpoch = 1609722840017837773;
    criteria1.symbols = symbols1;
    criteria1.selectedFields = {};
    vector<Snapshot> results1 = engine1.query(criteria1);
    assert(results1.size() == 10);
    cout << "\nABB snapshots (default grouped output):" << endl;
    engine1.printSnapshots(results1, criteria1);
    
    // Query CDD snapshots.
    vector<string> symbols2 = {"CDD"};
    QueryEngine engine2(symbols2);
    QueryCriteria criteria2;
    criteria2.startEpoch = 1609722840027798182;
    criteria2.endEpoch = 1609722840027808082;
    criteria2.symbols = symbols2;
    criteria2.selectedFields = {};
    vector<Snapshot> results2 = engine2.query(criteria2);
    assert(results2.size() == 10);
    cout << "\nCDD snapshots (default grouped output):" << endl;
    engine2.printSnapshots(results2, criteria2);
    
    // Clean up temporary files.
    std::remove("ABB.log");
    std::remove("CDD.log");
    std::remove("ABB.snap");
    std::remove("CDD.snap");
    std::remove("ABB.idx");
    std::remove("CDD.idx");
    
    cout << "Process and query test for ABB and CDD passed (12/12) (Integration Test)!" << endl << endl;
}

// ----------------------------------------------------------------------
// Main Test Runner
// ----------------------------------------------------------------------
int main() {
    testOrderBook();
    testSnapshotSerialization();
    testQueryEngineDefaultOutput();
    testQueryEngineSelectiveOutput();
    testQueryEngineInvalidFields();
    testQueryEngineMultiSymbol();
    testQueryEngineNoResults();
    testIndexFileContent();
    testBookProcessorEmptyFile();
    testBookProcessorSingleOrder();
    testBookProcessorInvalidInput();
    testProcessAndQueryABB_CDD();
    
    cout << "All tests (12/12) passed successfully :)" << endl;
    return 0;
}
