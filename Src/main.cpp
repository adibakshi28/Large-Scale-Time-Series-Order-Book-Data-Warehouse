#include "BookProcessor.h"
#include "QueryEngine.h"
#include "Progress.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>

using namespace std;
using namespace std::chrono;

vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    stringstream ss(s);
    string token;
    while (getline(ss, token, delimiter)) {
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

uint64_t getFileSize(const string &filePath) {
    ifstream ifs(filePath, ios::binary | ios::ate);
    if (!ifs.is_open()) return 0;
    uint64_t size = ifs.tellg();
    ifs.close();
    return size;
}

int main(int argc, char* argv[]) {
    try {
        // Process raw data mode if no command-line arguments are given.
        if (argc == 1) {
            // List of raw order log files.
            vector<string> files = {"Data/SCH.log", "Data/SCS.log"};
            uint64_t total = 0;
            for (auto &f : files) {
                total += getFileSize(f);
            }
            g_totalBytes = total;  // Set total bytes for progress tracking.

            // Process the raw data files.
            BookProcessor processor(files);

            // Start a loading bar thread to display progress.
            atomic<bool> done(false);
            thread loader([&done]() {
                const int barWidth = 50;
                while (!done.load()) {
                    double progress = (g_totalBytes > 0) ? static_cast<double>(g_bytesProcessed.load()) / g_totalBytes.load() : 0.0;
                    int pos = static_cast<int>(barWidth * progress);
                    cout << "\r[";
                    for (int i = 0; i < barWidth; ++i) {
                        if (i < pos) cout << "#";
                        else if (i == pos) cout << ">";
                        else cout << " ";
                    }
                    cout << "] " << fixed << setprecision(1) << (progress * 100) << " %";
                    cout.flush();
                    this_thread::sleep_for(milliseconds(200));
                }
                // Print complete progress bar.
                cout << "\r[";
                for (int i = 0; i < barWidth; ++i)
                    cout << "#";
                cout << "] 100.0 %" << endl;
            });

            auto startTime = steady_clock::now();
            processor.process();
            done.store(true);
            loader.join();
            auto endTime = steady_clock::now();
            auto duration = duration_cast<seconds>(endTime - startTime).count();
            cout << "Total processing time: " << duration << " seconds." << endl;
        }
        // Query mode: the first argument is "query".
        else if (argc >= 5 && string(argv[1]) == "query") {
            // Parse symbols.
            string symbolsArg = argv[2];
            vector<string> symbols;
            if (symbolsArg == "ALL")
                symbols = {"SCH", "SCS"};
            else
                symbols = split(symbolsArg, ',');

            // Parse epoch values with error checking.
            int64_t startEpoch = 0, endEpoch = 0;
            try {
                startEpoch = stoll(argv[3]);
                endEpoch = stoll(argv[4]);
            } catch (const std::exception &e) {
                cerr << "Error: Invalid epoch value. " << e.what() << endl;
                return 1;
            }

            // Parse optional selective fields.
            unordered_set<string> selectedFields;
            if (argc >= 6) {
                vector<string> fields = split(argv[5], ',');
                for (const auto &f : fields)
                    selectedFields.insert(f);
            }

            // Set up query criteria.
            QueryCriteria criteria;
            criteria.startEpoch = startEpoch;
            criteria.endEpoch = endEpoch;
            criteria.symbols = symbols;
            criteria.selectedFields = selectedFields;

            // Execute query and print results.
            QueryEngine engine(symbols);
            vector<Snapshot> results = engine.query(criteria);
            engine.printSnapshots(results, criteria);
        }
        // Print usage information if arguments are incorrect.
        else {
            cout << "Error:\n"
                 << "Correct Usage:\n"
                 << "  " << argv[0] << "                   // Process raw data\n"
                 << "  " << argv[0] << " query <symbols> <startEpoch> <endEpoch> [<fields>]\n"
                 << "     <symbols>: comma-separated list (or ALL)\n"
                 << "     <fields>: comma-separated list from:\n"
                 << "         symbol, epoch, bid1p, bid1q, bid2p, bid2q, bid3p, bid3q,\n"
                 << "         bid4p, bid4q, bid5p, bid5q, ask1p, ask1q, ask2p, ask2q,\n"
                 << "         ask3p, ask3q, ask4p, ask4q, ask5p, ask5q, lastTradePrice, lastTradeQuantity\n";
        }
    } catch (const std::exception &ex) {
        cerr << "Unexpected error: " << ex.what() << endl;
        return 1;
    }
    return 0;
}
