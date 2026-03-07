#ifndef NAV_VOLUME_COMPONENT_H
#define NAV_VOLUME_COMPONENT_H

#include "Components/Component.h"
#include <glm/glm.hpp>
#include <memory>

namespace GameEngine {

class NavGrid;

class NavVolumeComponent : public Component {
public:
    NavVolumeComponent();
    virtual ~NavVolumeComponent();

    COMPONENT_TYPE(NavVolumeComponent)

    virtual void start() override;
    virtual void destroy() override;

    void setGridSizeX(int n) { gridSizeX_ = n; }
    void setGridSizeZ(int n) { gridSizeZ_ = n; }
    int getGridSizeX() const { return gridSizeX_; }
    int getGridSizeZ() const { return gridSizeZ_; }
    void setCellSize(float s) { cellSize_ = s; }
    float getCellSize() const { return cellSize_; }

    void syncToNavGrid();

    NavGrid* getGrid() const { return grid_.get(); }

    virtual void drawInspector() override;

private:
    int gridSizeX_;
    int gridSizeZ_;
    float cellSize_;
    std::unique_ptr<NavGrid> grid_;
};

} // namespace GameEngine

#endif // NAV_VOLUME_COMPONENT_H
