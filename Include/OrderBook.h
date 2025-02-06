#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include "Order.h"
#include "Snapshot.h"
#include <map>
#include <unordered_map>
#include <string>

/**
 * @brief Comparator for sorting prices in descending order.
 * 
 * Used for the buy side to ensure the highest price appears first.
 */
struct DescendingComparator {
    bool operator()(double lhs, double rhs) const {
        return lhs > rhs;
    }
};

/**
 * @brief The OrderBook class.
 *
 * Maintains the current order book for a specific symbol by processing orders
 * (NEW, CANCEL, and TRADE) and aggregates orders into bid and ask levels.
 * Provides snapshots representing the order book state at a given epoch.
 */
class OrderBook {
public:
    /**
     * @brief Construct a new OrderBook object for a given symbol.
     * 
     * @param symbol The symbol associated with this order book.
     */
    explicit OrderBook(const std::string& symbol);

    /**
     * @brief Process an order update.
     * 
     * Updates the order book state based on the order type (NEW, CANCEL, or TRADE).
     * 
     * @param order The order to process.
     */
    void processOrder(const Order& order);

    /**
     * @brief Get a snapshot of the current order book state.
     * 
     * The snapshot reflects the order book status after processing all orders
     * from time 0 up to the provided epoch.
     * 
     * @param epoch The snapshot time in nanoseconds.
     * @return Snapshot The order book snapshot in fixed format.
     */
    Snapshot getSnapshot(int64_t epoch) const;

private:
    std::string symbol_; ///< The symbol for this order book.

    // Maps to track individual orders.
    std::unordered_map<std::string, Order> buyOrders_;
    std::unordered_map<std::string, Order> sellOrders_;

    // Aggregated bid levels (sorted in descending order) and ask levels (sorted in ascending order).
    std::map<double, int, DescendingComparator> buyLevels_;
    std::map<double, int> sellLevels_;

    double lastTradePrice_; ///< Last trade price (if any).
    int lastTradeQuantity_; ///< Last trade quantity (if any).

    /**
     * @brief Add a new order to the order book.
     * 
     * Updates the aggregated levels for BUY or SELL orders.
     * 
     * @param order The new order to add.
     */
    void addOrderToBook(const Order& order);

    /**
     * @brief Remove a quantity from an existing order in the order book.
     * 
     * Used for CANCEL and TRADE orders to decrease the quantity at a given price level.
     * 
     * @param order The order to update.
     * @param quantityToRemove The quantity to remove.
     */
    void removeOrderFromBook(const Order& order, int quantityToRemove);
};

#endif
