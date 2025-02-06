#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <cstdint>

/**
 * @brief Enum for order side.
 */
enum class OrderSide {
    BUY,
    SELL
};

/**
 * @brief Enum for order category.
 */
enum class OrderCategory {
    NEW,
    CANCEL,
    TRADE
};

/**
 * @brief Represents a single order from the log file.
 *
 * Contains the order's timestamp (epoch in nanoseconds), unique order ID, 
 * associated symbol, side (BUY/SELL), category (NEW/CANCEL/TRADE), price, and quantity.
 */
struct Order {
    int64_t epoch;         
    std::string orderId;   
    std::string symbol; 
    OrderSide side;     
    OrderCategory category;
    double price;
    int quantity;
};

#endif
