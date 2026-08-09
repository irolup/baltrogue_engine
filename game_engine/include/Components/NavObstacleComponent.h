#ifndef NAV_OBSTACLE_COMPONENT_H
#define NAV_OBSTACLE_COMPONENT_H

#include "Components/Component.h"
#include <vector>

namespace GameEngine {

class NavGrid;

class NavObstacleComponent : public Component {
public:
    NavObstacleComponent();
    virtual ~NavObstacleComponent();

    COMPONENT_TYPE(NavObstacleComponent)

    virtual void start() override;
    virtual void destroy() override;
    virtual void suspend() override;
    virtual void resume() override;

    virtual void drawInspector() override;

private:
};

} // namespace GameEngine

#endif // NAV_OBSTACLE_COMPONENT_H
