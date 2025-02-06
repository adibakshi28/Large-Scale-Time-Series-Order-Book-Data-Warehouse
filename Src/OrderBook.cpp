#include "OrderBook.h"
#include <algorithm>
#include <iostream>
#include <cstring>

OrderBook::OrderBook(const std::string &symbol)
    : symbol_(symbol), lastTradePrice_(-1.0), lastTradeQuantity_(0) {}

void OrderBook::processOrder(const Order &order) {
    try {
        if (order.category == OrderCategory::NEW) {
            addOrderToBook(order);
        } else if (order.category == OrderCategory::CANCEL) {
            removeOrderFromBook(order, order.quantity);
        } else if (order.category == OrderCategory::TRADE) {
            removeOrderFromBook(order, order.quantity);
            lastTradePrice_ = order.price;
            lastTradeQuantity_ = order.quantity;
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error processing order " << order.orderId << ": " << ex.what() << std::endl;
    }
}

void OrderBook::addOrderToBook(const Order &order) {
    if (order.side == OrderSide::BUY) {
        buyOrders_[order.orderId] = order;
        buyLevels_[order.price] += order.quantity;
    } else {
        sellOrders_[order.orderId] = order;
        sellLevels_[order.price] += order.quantity;
    }
}

void OrderBook::removeOrderFromBook(const Order &order, int quantityToRemove) {
    if (order.side == OrderSide::BUY) {
        auto it = buyOrders_.find(order.orderId);
        if (it != buyOrders_.end()) {
            Order &existing = it->second;
            int removeQty = std::min(existing.quantity, quantityToRemove);
            existing.quantity -= removeQty;
            buyLevels_[existing.price] -= removeQty;
            if (buyLevels_[existing.price] <= 0)
                buyLevels_.erase(existing.price);
            if (existing.quantity <= 0)
                buyOrders_.erase(it);
        }
    } else {
        auto it = sellOrders_.find(order.orderId);
        if (it != sellOrders_.end()) {
            Order &existing = it->second;
            int removeQty = std::min(existing.quantity, quantityToRemove);
            existing.quantity -= removeQty;
            sellLevels_[existing.price] -= removeQty;
            if (sellLevels_[existing.price] <= 0)
                sellLevels_.erase(existing.price);
            if (existing.quantity <= 0)
                sellOrders_.erase(it);
        }
    }
}

Snapshot OrderBook::getSnapshot(int64_t epoch) const {
    Snapshot snap;
    // Fill symbol: use fixed size char array
    std::memset(snap.symbol, 0, sizeof(snap.symbol));
    std::strncpy(snap.symbol, symbol_.c_str(), sizeof(snap.symbol)-1);
    snap.epoch = epoch;
    snap.lastTradePrice = lastTradePrice_;
    snap.lastTradeQuantity = lastTradeQuantity_;

    // Fill bid levels: iterate over buyLevels_ (descending order)
    int count = 0;
    for (const auto &level : buyLevels_) {
        if (count >= 5)
            break;
        snap.bidPrices[count] = level.first;
        snap.bidQuantities[count] = level.second;
        count++;
    }
    // Fill remaining bid levels with N.A (price -1 and quantity 0)
    for (int i = count; i < 5; ++i) {
        snap.bidPrices[i] = -1.0;
        snap.bidQuantities[i] = 0;
    }

    // Fill ask levels: iterate over sellLevels_ (ascending order)
    count = 0;
    for (const auto &level : sellLevels_) {
        if (count >= 5)
            break;
        snap.askPrices[count] = level.first;
        snap.askQuantities[count] = level.second;
        count++;
    }
    // Fill remaining ask levels with N.A
    for (int i = count; i < 5; ++i) {
        snap.askPrices[i] = -1.0;
        snap.askQuantities[i] = 0;
    }

    return snap;
}
