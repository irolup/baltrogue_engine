#include "Navigation/NavGrid.h"
#include "Scene/SceneNode.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <functional>

namespace GameEngine {

namespace {

struct AStarNode {
    int ix, iz;
    float g, h, f;
    int parentIndex;
    AStarNode() : ix(0), iz(0), g(0), h(0), f(0), parentIndex(-1) {}
    AStarNode(int x, int z, float gVal, float hVal, int pIdx)
        : ix(x), iz(z), g(gVal), h(hVal), f(gVal + hVal), parentIndex(pIdx) {}
};

struct OpenEntry {
    float f;
    float h;
    float negAlignment;
    int index;
};
bool operator>(const OpenEntry& a, const OpenEntry& b) {
    if (a.f != b.f) return a.f > b.f;
    if (a.h != b.h) return a.h > b.h;
    return a.negAlignment > b.negAlignment;
}

uint64_t cellKey(int ix, int iz) {
    return (uint64_t)(unsigned)ix | ((uint64_t)(unsigned)iz << 32);
}

} // namespace

NavGrid::NavGrid()
    : sizeX_(32)
    , sizeZ_(32)
    , origin_(0.0f)
    , cellSize_(1.0f)
{
    obstacles_.resize((size_t)sizeX_ * sizeZ_, false);
}

void NavGrid::setGridSize(int sizeX, int sizeZ) {
    sizeX_ = std::max(1, sizeX);
    sizeZ_ = std::max(1, sizeZ);
    obstacles_.resize((size_t)sizeX_ * sizeZ_, false);
}

void NavGrid::setOrigin(const glm::vec3& origin) {
    origin_ = origin;
}

void NavGrid::setCellSize(float cellSize) {
    cellSize_ = std::max(0.01f, cellSize);
}

void NavGrid::setTileObstacle(int ix, int iz, bool obstacle) {
    if (!isValidCell(ix, iz)) return;
    obstacles_[(size_t)iz * sizeX_ + ix] = obstacle;
}

bool NavGrid::isTileObstacle(int ix, int iz) const {
    if (!isValidCell(ix, iz)) return true;
    return obstacles_[(size_t)iz * sizeX_ + ix];
}

void NavGrid::worldToCell(float wx, float wz, int& outIx, int& outIz) const {
    float fx = (wx - origin_.x) / cellSize_;
    float fz = (wz - origin_.z) / cellSize_;
    outIx = (int)std::floor(fx);
    outIz = (int)std::floor(fz);
    outIx = std::max(0, std::min(sizeX_ - 1, outIx));
    outIz = std::max(0, std::min(sizeZ_ - 1, outIz));
}

glm::vec3 NavGrid::cellToWorld(int ix, int iz) const {
    float wx = origin_.x + (ix + 0.5f) * cellSize_;
    float wz = origin_.z + (iz + 0.5f) * cellSize_;
    return glm::vec3(wx, origin_.y, wz);
}

void NavGrid::registerObstacleNode(SceneNode* node) {
    if (!node) return;
    for (SceneNode* n : obstacleNodes_) if (n == node) return;
    obstacleNodes_.push_back(node);
}

void NavGrid::unregisterObstacleNode(SceneNode* node) {
    obstacleNodes_.erase(
        std::remove(obstacleNodes_.begin(), obstacleNodes_.end(), node),
        obstacleNodes_.end());
}

void NavGrid::syncObstaclesFromNodes() {
    for (size_t i = 0; i < obstacles_.size(); ++i) obstacles_[i] = false;
    for (SceneNode* node : obstacleNodes_) {
        if (!node || !node->isActive()) continue;
        glm::mat4 world = node->getWorldMatrix();
        glm::vec3 pos(world[3]);
        glm::vec3 worldScale(
            glm::length(glm::vec3(world[0])),
            glm::length(glm::vec3(world[1])),
            glm::length(glm::vec3(world[2])));
        float halfX = worldScale.x * 0.5f;
        float halfZ = worldScale.z * 0.5f;
        float minX = pos.x - halfX, maxX = pos.x + halfX;
        float minZ = pos.z - halfZ, maxZ = pos.z + halfZ;
        int ixLo = (int)std::floor((minX - origin_.x) / cellSize_);
        int ixHi = (int)std::floor((maxX - origin_.x) / cellSize_);
        int izLo = (int)std::floor((minZ - origin_.z) / cellSize_);
        int izHi = (int)std::floor((maxZ - origin_.z) / cellSize_);
        ixLo = std::max(0, std::min(sizeX_ - 1, ixLo));
        ixHi = std::max(0, std::min(sizeX_ - 1, ixHi));
        izLo = std::max(0, std::min(sizeZ_ - 1, izLo));
        izHi = std::max(0, std::min(sizeZ_ - 1, izHi));
        if (ixLo > ixHi) std::swap(ixLo, ixHi);
        if (izLo > izHi) std::swap(izLo, izHi);
        for (int iz = izLo; iz <= izHi; ++iz)
            for (int ix = ixLo; ix <= ixHi; ++ix)
                setTileObstacle(ix, iz, true);
    }
}

bool NavGrid::isValidCell(int ix, int iz) const {
    return ix >= 0 && ix < sizeX_ && iz >= 0 && iz < sizeZ_;
}

//right now we are using A* algorithm to find the path, maybe we could add dijkstra's algorithm later
std::vector<glm::vec3> NavGrid::findPath(const glm::vec3& startWorld, const glm::vec3& endWorld, bool allowDiagonal) {
    std::vector<glm::vec3> result;
    syncObstaclesFromNodes();

    int startIx, startIz, endIx, endIz;
    worldToCell(startWorld.x, startWorld.z, startIx, startIz);
    worldToCell(endWorld.x, endWorld.z, endIx, endIz);

    if (isTileObstacle(startIx, startIz)) {
        int bestIx = -1, bestIz = -1;
        int bestDist = 999999;
        const int maxR = std::max(sizeX_, sizeZ_);
        for (int diz = -maxR; diz <= maxR; ++diz) {
            for (int dix = -maxR; dix <= maxR; ++dix) {
                if (dix == 0 && diz == 0) continue;
                int nx = startIx + dix, nz = startIz + diz;
                if (!isValidCell(nx, nz) || isTileObstacle(nx, nz)) continue;
                int dist = std::abs(dix) + std::abs(diz);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIx = nx;
                    bestIz = nz;
                }
            }
        }
        if (bestIx < 0) return result;
        startIx = bestIx;
        startIz = bestIz;
    }

    // If goal cell is blocked, snap to nearest walkable cell
    if (isTileObstacle(endIx, endIz)) {
        int bestIx = -1, bestIz = -1;
        int bestDist = 999999;
        const int maxR = std::max(sizeX_, sizeZ_);
        for (int diz = -maxR; diz <= maxR; ++diz) {
            for (int dix = -maxR; dix <= maxR; ++dix) {
                if (dix == 0 && diz == 0) continue;
                int nx = endIx + dix, nz = endIz + diz;
                if (!isValidCell(nx, nz) || isTileObstacle(nx, nz)) continue;
                int dist = std::abs(dix) + std::abs(diz);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIx = nx;
                    bestIz = nz;
                }
            }
        }
        if (bestIx < 0) return result;
        endIx = bestIx;
        endIz = bestIz;
    }

    const float sqrt2 = 1.414213562f;
    std::vector<AStarNode> nodes;
    nodes.push_back(AStarNode(startIx, startIz, 0.0f,
        (float)(std::abs(endIx - startIx) + std::abs(endIz - startIz)), -1));

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> open;
    open.push({ nodes[0].f, nodes[0].h, 0.0f, 0 });
    std::unordered_set<uint64_t> closed;

    static const int dx[] = { -1, 1, 0, 0, -1, -1, 1, 1 };
    static const int dz[] = { 0, 0, -1, 1, -1, 1, -1, 1 };
    static const float cost[] = { 1.0f, 1.0f, 1.0f, 1.0f, sqrt2, sqrt2, sqrt2, sqrt2 };
    const int neibCount = allowDiagonal ? 8 : 4;

    int goalNodeIndex = -1;

    while (!open.empty()) {
        int curIndex = open.top().index;
        open.pop();
        const AStarNode& cur = nodes[(size_t)curIndex];
        uint64_t key = cellKey(cur.ix, cur.iz);
        if (closed.count(key)) continue;
        closed.insert(key);

        if (cur.ix == endIx && cur.iz == endIz) {
            goalNodeIndex = curIndex;
            break;
        }

        for (int i = 0; i < neibCount; ++i) {
            int nx = cur.ix + dx[i];
            int nz = cur.iz + dz[i];
        
            if (!isValidCell(nx, nz) || isTileObstacle(nx, nz))
                continue;
        
            // Prevent diagonal corner cutting
            if (i >= 4) { 
                int adj1x = cur.ix + dx[i];
                int adj1z = cur.iz;
                int adj2x = cur.ix;
                int adj2z = cur.iz + dz[i];
        
                if (isTileObstacle(adj1x, adj1z) ||
                    isTileObstacle(adj2x, adj2z))
                    continue;
            }
            uint64_t nkey = cellKey(nx, nz);
            if (closed.count(nkey)) continue;
            float g = cur.g + cost[i];
            int dxAbs = std::abs(endIx - nx);
            int dzAbs = std::abs(endIz - nz);

            float h = (float)(
                std::max(dxAbs, dzAbs) +
                (sqrt2 - 1.0f) * std::min(dxAbs, dzAbs)
            );
            int childIdx = (int)nodes.size();
            nodes.push_back(AStarNode(nx, nz, g, h, curIndex));

            int stepX = nx - cur.ix, stepZ = nz - cur.iz;
            int toGoalX = endIx - cur.ix, toGoalZ = endIz - cur.iz;
            float alignment = (float)(stepX * toGoalX + stepZ * toGoalZ);
            open.push({ nodes.back().f, nodes.back().h, -alignment, childIdx });
        }
    }

    if (goalNodeIndex >= 0) {
        std::vector<std::pair<int, int>> all;
        for (int idx = goalNodeIndex; idx >= 0; idx = nodes[(size_t)idx].parentIndex) {
            all.push_back({ nodes[(size_t)idx].ix, nodes[(size_t)idx].iz });
        }
        for (size_t i = all.size(); i > 0; --i){
            glm::vec3 p = cellToWorld(all[i - 1].first, all[i - 1].second);
            p.y = startWorld.y;
            result.push_back(p);
        }
    }
    return result;
}

} // namespace GameEngine
