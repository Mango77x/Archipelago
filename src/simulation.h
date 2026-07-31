#pragma once

// Ties the economy and the fleets together: the material-balance invariant
// check, and the rival AI company's expansion logic.

#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include "common.h"
#include "economy.h"
#include "ship.h"

namespace archipelago {

// Invariant that must hold every simulated hour: every unit of Iron/Steel
// ever produced is accounted for either in an island's warehouse, riding a
// ship (player's or the AI company's — they draw from the same two
// warehouses, see Fase 5.1), or already exported.
bool CheckMaterialBalance(const IronMine& mine, const SteelMill& mill, const Warehouse& island1Warehouse,
                           const Warehouse& island2Warehouse, const std::vector<CargoShip>& playerShips,
                           const std::vector<CargoShip>& aiShips, const Port& port, int hour);

// Simple rule-based expansion, not a learned/adaptive AI — honest for a
// first sub-phase. Buys on whichever leg has fewer of its own ships, so it
// doesn't lopsidedly pile onto one route. Same $500 cost, same capacity as
// the player pays — no hidden bonuses (principios 16-18).
void RunAiDecisionLogic(JPH::BodyInterface& bodyInterface, Economy& aiEconomy, std::vector<CargoShip>& aiShips,
                         int shipCapacity, double shipCost, Vec3 island1Dock, Vec3 island2Dock, int maxShips);

}  // namespace archipelago
