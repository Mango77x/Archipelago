#include "simulation.h"

#include <iostream>

namespace archipelago {

namespace {

void AccumulateAfloat(const std::vector<CargoShip>& ships, int& ironAfloat, int& steelAfloat) {
    for (const CargoShip& s : ships) {
        if (s.cargo() <= 0) continue;
        if (s.cargoResource() == Resource::Iron) {
            ironAfloat += s.cargo();
        } else {
            steelAfloat += s.cargo();
        }
    }
}

}  // namespace

bool CheckMaterialBalance(const IronMine& mine, const SteelMill& mill, const Warehouse& island1Warehouse,
                           const Warehouse& island2Warehouse, const std::vector<CargoShip>& playerShips,
                           const std::vector<CargoShip>& aiShips, const Port& port, int hour) {
    int ironAfloat = 0;
    int steelAfloat = 0;
    AccumulateAfloat(playerShips, ironAfloat, steelAfloat);
    AccumulateAfloat(aiShips, ironAfloat, steelAfloat);

    int ironBalance = mine.totalProduced() - mill.totalConsumed() - island1Warehouse.Get(Resource::Iron) -
                       island2Warehouse.Get(Resource::Iron) - ironAfloat;
    int steelBalance =
        mill.totalProduced() - island2Warehouse.Get(Resource::Steel) - steelAfloat - port.totalExported();

    if (ironBalance != 0 || steelBalance != 0) {
        std::cerr << "[hour " << hour << "] BALANCE VIOLATION: ironBalance=" << ironBalance
                   << " steelBalance=" << steelBalance << "\n";
        return false;
    }
    return true;
}

void RunAiDecisionLogic(JPH::BodyInterface& bodyInterface, Economy& aiEconomy, std::vector<CargoShip>& aiShips,
                         int shipCapacity, double shipCost, Vec3 island1Dock, Vec3 island2Dock, int maxShips) {
    if (static_cast<int>(aiShips.size()) >= maxShips) return;
    if (aiEconomy.cash() < shipCost * 2.0) return;

    int ironShips = 0, steelShips = 0;
    for (const CargoShip& s : aiShips) {
        if (s.routeKind() == RouteKind::IronRoute) {
            ++ironShips;
        } else {
            ++steelShips;
        }
    }

    if (ironShips <= steelShips) {
        aiShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island1Dock), shipCapacity,
                              RouteKind::IronRoute);
    } else {
        aiShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island2Dock), shipCapacity,
                              RouteKind::SteelRoute);
    }
    aiEconomy.ChargeExpense(shipCost);
    std::cout << "AI company bought a ship (fleet size now " << aiShips.size() << ")\n";
}

}  // namespace archipelago
