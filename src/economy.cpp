#include "economy.h"

#include <algorithm>
#include <iostream>

namespace archipelago {

void Warehouse::Deposit(Resource resource, int amount) { stock_[resource] += amount; }

bool Warehouse::Withdraw(Resource resource, int amount) {
    int& current = stock_[resource];
    if (current < amount) return false;
    current -= amount;
    return true;
}

int Warehouse::Get(Resource resource) const {
    auto it = stock_.find(resource);
    return it == stock_.end() ? 0 : it->second;
}

void Warehouse::SetStock(Resource resource, int amount) { stock_[resource] = amount; }

void IronMine::Tick(Warehouse& warehouse, int hour) {
    warehouse.Deposit(Resource::Iron, rate_);
    totalProduced_ += rate_;
    std::cout << "[hour " << hour << "] Iron Mine produced " << rate_ << " Iron\n";
}

void SteelMill::Tick(Warehouse& warehouse, int hour) {
    if (!warehouse.Withdraw(Resource::Iron, consumeRate_)) {
        idle_ = true;
        std::cout << "[hour " << hour << "] Steel Mill idle (not enough Iron)\n";
        return;
    }
    idle_ = false;
    totalConsumed_ += consumeRate_;
    int produced = consumeRate_ * outputRatioPercent_ / 100;
    warehouse.Deposit(Resource::Steel, produced);
    totalProduced_ += produced;
    std::cout << "[hour " << hour << "] Steel Mill consumed " << consumeRate_ << " Iron, produced " << produced
               << " Steel\n";
}

void Port::Export(int amount) {
    totalExported_ += amount;
    std::cout << "Port exported " << amount << " Steel\n";
}

double Market::CurrentPrice() const {
    double price = basePrice_ / (1.0 + stock_ * sensitivity_);
    return std::max(price, basePrice_ * kMinPriceFraction);
}

double Market::Sell(int amount) {
    double revenue = CurrentPrice() * amount;
    stock_ += amount;
    totalRevenue_ += revenue;
    totalSold_ += amount;
    return revenue;
}

void Market::Tick(int hour) {
    float absorbed = std::min(stock_, demandPerHour_);
    stock_ -= absorbed;
    std::cout << "[hour " << hour << "] Market absorbed " << absorbed << " Steel (stock=" << stock_
               << ", price=$" << CurrentPrice() << "/unit)\n";
}

}  // namespace archipelago
