#ifndef NAV_GRID_REGISTRY_H
#define NAV_GRID_REGISTRY_H

#include <vector>
#include <functional>

namespace GameEngine {

class NavGrid;


class NavGridRegistry {
public:
    static NavGridRegistry& get();

    void addGrid(NavGrid* grid);
    void removeGrid(NavGrid* grid);
    void forEachGrid(std::function<void(NavGrid*)> f);
    NavGrid* getFirstGrid() const;

private:
    NavGridRegistry() = default;
    std::vector<NavGrid*> grids_;
};

} // namespace GameEngine

#endif // NAV_GRID_REGISTRY_H
