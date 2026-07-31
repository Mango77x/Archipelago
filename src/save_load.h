#pragma once

// --- Save/Load: plain-text state dump. Debugging tool first, save format
// second — a human can open this file and see exactly what broke. ---

#include <fstream>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include "economy.h"
#include "ship.h"

namespace archipelago {

void SaveGame(int hour, double hourAccumulator, const IronMine& mine, const SteelMill& mill,
              const Warehouse& island1Warehouse, const Warehouse& island2Warehouse,
              const std::vector<CargoShip>& playerShips, const std::vector<CargoShip>& aiShips, const Port& port,
              const Market& market, const Economy& economy, const Economy& aiEconomy);

bool LoadGame(JPH::BodyInterface& bodyInterface, int& hour, double& hourAccumulator, IronMine& mine, SteelMill& mill,
              Warehouse& island1Warehouse, Warehouse& island2Warehouse, std::vector<CargoShip>& playerShips,
              std::vector<CargoShip>& aiShips, Port& port, Market& market, Economy& economy, Economy& aiEconomy,
              int shipCapacity);

}  // namespace archipelago
