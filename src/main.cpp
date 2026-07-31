#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

namespace archipelago {

enum class Resource { Iron, Steel };

std::string ToString(Resource r) {
    switch (r) {
        case Resource::Iron: return "Iron";
        case Resource::Steel: return "Steel";
    }
    return "Unknown";
}

class Warehouse {
public:
    void Deposit(Resource resource, int amount) {
        stock_[resource] += amount;
    }

    bool Withdraw(Resource resource, int amount) {
        int& current = stock_[resource];
        if (current < amount) return false;
        current -= amount;
        return true;
    }

    int Get(Resource resource) const {
        auto it = stock_.find(resource);
        return it == stock_.end() ? 0 : it->second;
    }

private:
    std::unordered_map<Resource, int> stock_;
};

class IronMine {
public:
    explicit IronMine(int rate) : rate_(rate) {}

    void Tick(Warehouse& warehouse, int hour) {
        warehouse.Deposit(Resource::Iron, rate_);
        totalProduced_ += rate_;
        std::cout << "[hour " << hour << "] Iron Mine produced " << rate_ << " Iron\n";
        std::cout << "[hour " << hour << "] Warehouse received " << rate_ << " Iron\n";
    }

    int totalProduced() const { return totalProduced_; }

private:
    int rate_;
    int totalProduced_ = 0;
};

class SteelMill {
public:
    SteelMill(int consumeRate, int outputRatioPercent)
        : consumeRate_(consumeRate), outputRatioPercent_(outputRatioPercent) {}

    void Tick(Warehouse& warehouse, int hour) {
        if (!warehouse.Withdraw(Resource::Iron, consumeRate_)) return;
        totalConsumed_ += consumeRate_;
        int produced = consumeRate_ * outputRatioPercent_ / 100;
        warehouse.Deposit(Resource::Steel, produced);
        totalProduced_ += produced;
        std::cout << "[hour " << hour << "] Steel Mill consumed " << consumeRate_ << " Iron\n";
        std::cout << "[hour " << hour << "] Steel Mill produced " << produced << " Steel\n";
    }

    int totalConsumed() const { return totalConsumed_; }
    int totalProduced() const { return totalProduced_; }

private:
    int consumeRate_;
    int outputRatioPercent_;
    int totalConsumed_ = 0;
    int totalProduced_ = 0;
};

class Port {
public:
    void Export(int amount, int hour) {
        totalExported_ += amount;
        std::cout << "[hour " << hour << "] Port exported " << amount << " Steel\n";
    }

    int totalExported() const { return totalExported_; }

private:
    int totalExported_ = 0;
};

class CargoShip {
public:
    explicit CargoShip(int capacity) : capacity_(capacity) {}

    void Tick(Warehouse& warehouse, Port& port, int hour) {
        switch (state_) {
            case State::AtMill: {
                int available = warehouse.Get(Resource::Steel);
                int amount = std::min(available, capacity_);
                if (amount <= 0) break;
                warehouse.Withdraw(Resource::Steel, amount);
                cargo_ = amount;
                std::cout << "[hour " << hour << "] Cargo Ship loaded " << amount << " Steel\n";
                state_ = State::ToPort;
                break;
            }
            case State::ToPort:
                state_ = State::AtPort;
                break;
            case State::AtPort: {
                if (cargo_ > 0) {
                    std::cout << "[hour " << hour << "] Cargo Ship unloaded " << cargo_ << " Steel\n";
                    port.Export(cargo_, hour);
                    cargo_ = 0;
                }
                state_ = State::ToMine;
                break;
            }
            case State::ToMine:
                state_ = State::AtMill;
                break;
        }
    }

    int cargo() const { return cargo_; }

private:
    enum class State { AtMill, ToPort, AtPort, ToMine };
    State state_ = State::AtMill;
    int capacity_;
    int cargo_ = 0;
};

// Invariant that must hold every tick: every unit of Steel ever produced is
// accounted for either sitting in the warehouse, riding the ship, or already
// exported through the port. If this ever breaks, resources were created or
// destroyed somewhere — that's the one failure mode Phase 0 exists to catch.
bool CheckMaterialBalance(const IronMine& mine, const SteelMill& mill, const Warehouse& warehouse,
                           const CargoShip& ship, const Port& port, int hour) {
    int ironBalance = mine.totalProduced() - mill.totalConsumed() - warehouse.Get(Resource::Iron);
    int steelBalance = mill.totalProduced() - warehouse.Get(Resource::Steel) - ship.cargo() - port.totalExported();

    if (ironBalance != 0 || steelBalance != 0) {
        std::cerr << "[hour " << hour << "] BALANCE VIOLATION: ironBalance=" << ironBalance
                  << " steelBalance=" << steelBalance << "\n";
        return false;
    }
    return true;
}

}  // namespace archipelago

int main(int argc, char** argv) {
    using namespace archipelago;

    int simulatedHours = 48;
    if (argc > 1) simulatedHours = std::atoi(argv[1]);

    Warehouse warehouse;
    IronMine mine(/*rate=*/10);
    SteelMill mill(/*consumeRate=*/10, /*outputRatioPercent=*/50);
    CargoShip ship(/*capacity=*/20);
    Port port;

    for (int hour = 1; hour <= simulatedHours; ++hour) {
        mine.Tick(warehouse, hour);
        mill.Tick(warehouse, hour);
        ship.Tick(warehouse, port, hour);

        if (!CheckMaterialBalance(mine, mill, warehouse, ship, port, hour)) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "\n=== Simulation finished: " << simulatedHours << " hours ===\n";
    std::cout << "Total Iron produced: " << mine.totalProduced() << "\n";
    std::cout << "Total Steel produced: " << mill.totalProduced() << "\n";
    std::cout << "Total Steel exported: " << port.totalExported() << "\n";
    std::cout << "Material balance: OK (no resources created or destroyed)\n";

    return EXIT_SUCCESS;
}
