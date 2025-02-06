#ifndef QUERYENGINE_H
#define QUERYENGINE_H

#include "Snapshot.h"
#include <string>
#include <vector>
#include <unordered_set>

/**
 * @brief Structure representing query criteria.
 *
 * Specifies the time range, symbols to query, and (optionally) a set of selected fields.
 */
struct QueryCriteria {
    int64_t startEpoch;                          ///< Start of the epoch range (inclusive).
    int64_t endEpoch;                            ///< End of the epoch range (inclusive).
    std::vector<std::string> symbols;            ///< List of symbols to query. If empty, use all known symbols.
    std::unordered_set<std::string> selectedFields;  ///< Set of fields to output. If empty, output default grouped view.
};

/**
 * @brief The QueryEngine class.
 *
 * Provides an optimized query processing engine to access time-series order book snapshots.
 * Supports querying by time range, symbol(s), and outputting all fields or a selective set.
 */
class QueryEngine {
public:
    /**
     * @brief Constructs a QueryEngine with a list of symbols.
     *
     * @param symbolList List of symbols for which snapshot files exist.
     */
    QueryEngine(const std::vector<std::string>& symbolList);

    /**
     * @brief Queries snapshots based on the given criteria.
     *
     * Uses an index file for each symbol to perform a binary search for fast retrieval.
     *
     * @param criteria Query criteria.
     * @return std::vector<Snapshot> Filtered and sorted snapshots.
     */
    std::vector<Snapshot> query(const QueryCriteria &criteria);

    /**
     * @brief Prints the snapshots based on the query criteria.
     *
     * If no selective fields are provided, prints the default grouped view.
     * Otherwise, prints only the exact fields requested (with prices rounded to two decimals).
     *
     * @param snapshots The snapshots to print.
     * @param criteria The query criteria (including any selected fields).
     */
    void printSnapshots(const std::vector<Snapshot>& snapshots, const QueryCriteria &criteria) const;

private:
    std::vector<std::string> symbolList_;  ///< List of symbols for which snapshot files exist.

    /**
     * @brief Reads snapshots for a given symbol from the corresponding binary file using an index file for fast lookup.
     *
     * @param symbol The symbol for which to read the snapshots.
     * @param startEpoch The start epoch for filtering.
     * @param endEpoch The end epoch for filtering.
     * @return std::vector<Snapshot> All snapshots for the symbol within the epoch range.
     */
    std::vector<Snapshot> readSnapshotsForSymbol(const std::string& symbol, int64_t startEpoch, int64_t endEpoch);
};

#endif
