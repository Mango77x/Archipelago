#pragma once

// The production chain and money simulation (Fase 0-2): mine -> mill ->
// warehouse -> market -> player/AI cash. No rendering, no physics, no
// knowledge of ships beyond the Resource enum they carry.

#include <unordered_map>

#include "common.h"

namespace archipelago {

class Warehouse {
public:
    void Deposit(Resource resource, int amount);
    bool Withdraw(Resource resource, int amount);
    int Get(Resource resource) const;
    void SetStock(Resource resource, int amount);

private:
    std::unordered_map<Resource, int> stock_;
};

class IronMine {
public:
    explicit IronMine(int rate) : rate_(rate) {}

    void Tick(Warehouse& warehouse, int hour);

    int totalProduced() const { return totalProduced_; }
    void SetTotalProduced(int value) { totalProduced_ = value; }

private:
    int rate_;
    int totalProduced_ = 0;
};

class SteelMill {
public:
    SteelMill(int consumeRate, int outputRatioPercent)
        : consumeRate_(consumeRate), outputRatioPercent_(outputRatioPercent) {}

    void Tick(Warehouse& warehouse, int hour);

    int totalConsumed() const { return totalConsumed_; }
    int totalProduced() const { return totalProduced_; }
    bool isIdle() const { return idle_; }
    void SetTotals(int consumed, int produced, bool idle) {
        totalConsumed_ = consumed;
        totalProduced_ = produced;
        idle_ = idle;
    }

private:
    int consumeRate_;
    int outputRatioPercent_;
    int totalConsumed_ = 0;
    int totalProduced_ = 0;
    bool idle_ = false;
};

class Port {
public:
    void Export(int amount);

    int totalExported() const { return totalExported_; }
    void SetTotalExported(int value) { totalExported_ = value; }

private:
    int totalExported_ = 0;
};

// Single-commodity supply/demand market for Steel. Selling adds to stock_
// (a glut); Tick() absorbs stock at a fixed demand rate, and price recovers
// toward basePrice_ as stock drains. Flood the market faster than demand
// absorbs it and the price craters — the only lever Fase 2 gives the player
// to raise profit is pacing deliveries to match demand, not just moving more.
class Market {
public:
    Market(double basePrice, float demandPerHour, double sensitivity)
        : basePrice_(basePrice), demandPerHour_(demandPerHour), sensitivity_(sensitivity) {}

    double CurrentPrice() const;
    double Sell(int amount);
    void Tick(int hour);

    float stock() const { return stock_; }
    double totalRevenue() const { return totalRevenue_; }
    int totalSold() const { return totalSold_; }

    void SetState(float stock, double totalRevenue, int totalSold) {
        stock_ = stock;
        totalRevenue_ = totalRevenue;
        totalSold_ = totalSold;
    }

private:
    // Raised 0.2->0.35: with multiple ships (player + AI) selling into one
    // shared market, a glut was cratering price hard enough that a full
    // 20-unit hold barely covered a round trip's own maintenance. A higher
    // floor keeps selling worthwhile even when stock is high, without
    // removing the "don't flood the market" pressure entirely.
    static constexpr double kMinPriceFraction = 0.35;
    double basePrice_;
    float demandPerHour_;
    double sensitivity_;
    float stock_ = 0.0f;
    double totalRevenue_ = 0.0;
    int totalSold_ = 0;
};

// Player's cash balance. Revenue comes only from Market sales; expenses are
// the fixed hourly upkeep of mine + mill + fleet, charged whether or not the
// ships are moving — so profit only grows by moving more Steel, faster, and
// selling it without crashing the price. No scripted rewards, per Fase 2 DoD.
class Economy {
public:
    explicit Economy(double startingCash) : cash_(startingCash) {}

    void AddRevenue(double amount) {
        cash_ += amount;
        totalRevenue_ += amount;
    }
    void ChargeExpense(double amount) {
        cash_ -= amount;
        totalExpenses_ += amount;
    }

    double cash() const { return cash_; }
    double totalExpenses() const { return totalExpenses_; }
    double totalRevenue() const { return totalRevenue_; }

    void SetState(double cash, double totalExpenses, double totalRevenue) {
        cash_ = cash;
        totalExpenses_ = totalExpenses;
        totalRevenue_ = totalRevenue;
    }

private:
    double cash_;
    double totalExpenses_ = 0.0;
    double totalRevenue_ = 0.0;
};

}  // namespace archipelago
