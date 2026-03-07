#include "Navigation/NavGridRegistry.h"
#include "Navigation/NavGrid.h"
#include <algorithm>

namespace GameEngine {

NavGridRegistry& NavGridRegistry::get() {
    static NavGridRegistry instance;
    return instance;
}

void NavGridRegistry::addGrid(NavGrid* grid) {
    if (!grid) return;
    if (std::find(grids_.begin(), grids_.end(), grid) != grids_.end()) return;
    grids_.push_back(grid);
}

void NavGridRegistry::removeGrid(NavGrid* grid) {
    grids_.erase(
        std::remove(grids_.begin(), grids_.end(), grid),
        grids_.end());
}

void NavGridRegistry::forEachGrid(std::function<void(NavGrid*)> f) {
    for (NavGrid* g : grids_)
        f(g);
}

NavGrid* NavGridRegistry::getFirstGrid() const {
    return grids_.empty() ? nullptr : grids_.front();
}

} // namespace GameEngine
