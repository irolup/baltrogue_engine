#ifndef NAV_GRID_H
#define NAV_GRID_H

#include <vector>
#include <glm/glm.hpp>

namespace GameEngine {

class SceneNode;
// Only 2D nav grid for now, we need to add 3D support later
class NavGrid {
public:
    NavGrid();
    ~NavGrid() = default;
    NavGrid(const NavGrid&) = delete;
    NavGrid& operator=(const NavGrid&) = delete;

    void setGridSize(int sizeX, int sizeZ);
    int getGridSizeX() const { return sizeX_; }
    int getGridSizeZ() const { return sizeZ_; }

    void setOrigin(const glm::vec3& origin);
    glm::vec3 getOrigin() const { return origin_; }

    void setCellSize(float cellSize);
    float getCellSize() const { return cellSize_; }

    void setTileObstacle(int ix, int iz, bool obstacle);
    bool isTileObstacle(int ix, int iz) const;

    void worldToCell(float wx, float wz, int& outIx, int& outIz) const;
    glm::vec3 cellToWorld(int ix, int iz) const;

    void registerObstacleNode(SceneNode* node);
    void unregisterObstacleNode(SceneNode* node);
    void syncObstaclesFromNodes();

    std::vector<glm::vec3> findPath(const glm::vec3& startWorld, const glm::vec3& endWorld, bool allowDiagonal = true);

    bool isValidCell(int ix, int iz) const;

private:
    int sizeX_;
    int sizeZ_;
    glm::vec3 origin_;
    float cellSize_;
    std::vector<bool> obstacles_;
    std::vector<SceneNode*> obstacleNodes_;
};

} // namespace GameEngine

#endif // NAV_GRID_H
