#include "save_load.h"

#include <iostream>

namespace archipelago {

namespace {

constexpr const char* kSaveFile = "archipelago_save.txt";
constexpr const char* kSaveHeader = "ARCHIPELAGO_SAVE_V5";

void WriteFleet(std::ofstream& out, const std::vector<CargoShip>& ships) {
    out << ships.size() << "\n";
    for (const CargoShip& s : ships) {
        Vec3 pos = s.position();
        Vec3 vel = s.velocity();
        out << pos.x << " " << pos.y << " " << pos.z << " " << vel.x << " " << vel.y << " " << vel.z << " "
            << s.heading() << " " << s.angularVelocity() << " " << s.cargo() << " "
            << static_cast<int>(s.cargoResource()) << " " << static_cast<int>(s.routeKind()) << "\n";
    }
}

bool ReadFleet(JPH::BodyInterface& bodyInterface, std::ifstream& in, std::vector<CargoShip>& outShips,
               int shipCapacity) {
    size_t shipCount = 0;
    in >> shipCount;
    std::vector<CargoShip> loaded;
    for (size_t i = 0; i < shipCount && in; ++i) {
        Vec3 pos{}, vel{};
        float heading = 0.0f, angularVelocity = 0.0f;
        int cargo = 0;
        int cargoResourceInt = 0;
        int routeKindInt = 0;
        in >> pos.x >> pos.y >> pos.z >> vel.x >> vel.y >> vel.z >> heading >> angularVelocity >> cargo >>
            cargoResourceInt >> routeKindInt;

        RouteKind kind =
            (routeKindInt == static_cast<int>(RouteKind::IronRoute)) ? RouteKind::IronRoute : RouteKind::SteelRoute;
        Resource cargoResource =
            (cargoResourceInt == static_cast<int>(Resource::Iron)) ? Resource::Iron : Resource::Steel;

        CargoShip s(bodyInterface, CreateShipBody(bodyInterface, pos), shipCapacity, kind);
        s.SetState(pos, vel, heading, angularVelocity, cargo, cargoResource);
        loaded.push_back(s);
    }
    if (!in || loaded.size() != shipCount) return false;
    outShips = std::move(loaded);
    return true;
}

}  // namespace

void SaveGame(int hour, double hourAccumulator, const IronMine& mine, const SteelMill& mill,
              const Warehouse& island1Warehouse, const Warehouse& island2Warehouse,
              const std::vector<CargoShip>& playerShips, const std::vector<CargoShip>& aiShips, const Port& port,
              const Market& market, const Economy& economy, const Economy& aiEconomy) {
    std::ofstream out(kSaveFile);
    if (!out) {
        std::cerr << "SaveGame: could not open " << kSaveFile << " for writing\n";
        return;
    }
    out << kSaveHeader << "\n";
    out << hour << " " << hourAccumulator << "\n";
    out << mine.totalProduced() << "\n";
    out << mill.totalConsumed() << " " << mill.totalProduced() << " " << (mill.isIdle() ? 1 : 0) << "\n";
    out << island1Warehouse.Get(Resource::Iron) << " " << island1Warehouse.Get(Resource::Steel) << "\n";
    out << island2Warehouse.Get(Resource::Iron) << " " << island2Warehouse.Get(Resource::Steel) << "\n";
    out << port.totalExported() << "\n";
    out << market.stock() << " " << market.totalRevenue() << " " << market.totalSold() << "\n";
    out << economy.cash() << " " << economy.totalExpenses() << " " << economy.totalRevenue() << "\n";
    out << aiEconomy.cash() << " " << aiEconomy.totalExpenses() << " " << aiEconomy.totalRevenue() << "\n";
    WriteFleet(out, playerShips);
    WriteFleet(out, aiShips);
    std::cout << "Game saved to " << kSaveFile << " (hour " << hour << ", " << playerShips.size()
               << " player ships, " << aiShips.size() << " AI ships)\n";
}

bool LoadGame(JPH::BodyInterface& bodyInterface, int& hour, double& hourAccumulator, IronMine& mine, SteelMill& mill,
              Warehouse& island1Warehouse, Warehouse& island2Warehouse, std::vector<CargoShip>& playerShips,
              std::vector<CargoShip>& aiShips, Port& port, Market& market, Economy& economy, Economy& aiEconomy,
              int shipCapacity) {
    std::ifstream in(kSaveFile);
    if (!in) {
        std::cerr << "LoadGame: could not open " << kSaveFile << "\n";
        return false;
    }
    std::string header;
    std::getline(in, header);
    if (header != kSaveHeader) {
        std::cerr << "LoadGame: unrecognized save file header (old save from a previous phase?)\n";
        return false;
    }

    int mineProduced = 0;
    int millConsumed = 0, millProduced = 0, millIdleFlag = 0;
    int island1Iron = 0, island1Steel = 0;
    int island2Iron = 0, island2Steel = 0;
    int exported = 0;
    float marketStock = 0.0f;
    double marketRevenue = 0.0;
    int marketSold = 0;
    double cash = 0.0, totalExpenses = 0.0, totalRevenue = 0.0;
    double aiCash = 0.0, aiTotalExpenses = 0.0, aiTotalRevenue = 0.0;

    in >> hour >> hourAccumulator;
    in >> mineProduced;
    in >> millConsumed >> millProduced >> millIdleFlag;
    in >> island1Iron >> island1Steel;
    in >> island2Iron >> island2Steel;
    in >> exported;
    in >> marketStock >> marketRevenue >> marketSold;
    in >> cash >> totalExpenses >> totalRevenue;
    in >> aiCash >> aiTotalExpenses >> aiTotalRevenue;

    std::vector<CargoShip> loadedPlayerShips;
    std::vector<CargoShip> loadedAiShips;
    bool okPlayer = ReadFleet(bodyInterface, in, loadedPlayerShips, shipCapacity);
    bool okAi = ReadFleet(bodyInterface, in, loadedAiShips, shipCapacity);

    if (!in || !okPlayer || !okAi) {
        std::cerr << "LoadGame: save file is truncated or malformed\n";
        return false;
    }

    mine.SetTotalProduced(mineProduced);
    mill.SetTotals(millConsumed, millProduced, millIdleFlag != 0);
    island1Warehouse.SetStock(Resource::Iron, island1Iron);
    island1Warehouse.SetStock(Resource::Steel, island1Steel);
    island2Warehouse.SetStock(Resource::Iron, island2Iron);
    island2Warehouse.SetStock(Resource::Steel, island2Steel);
    port.SetTotalExported(exported);
    market.SetState(marketStock, marketRevenue, marketSold);
    economy.SetState(cash, totalExpenses, totalRevenue);
    aiEconomy.SetState(aiCash, aiTotalExpenses, aiTotalRevenue);
    playerShips = std::move(loadedPlayerShips);
    aiShips = std::move(loadedAiShips);

    std::cout << "Game loaded from " << kSaveFile << " (hour " << hour << ", " << playerShips.size()
               << " player ships, " << aiShips.size() << " AI ships)\n";
    return true;
}

}  // namespace archipelago
