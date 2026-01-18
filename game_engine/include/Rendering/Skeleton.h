#ifndef SKELETON_H
#define SKELETON_H

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace GameEngine {

struct Bone {
    std::string name;
    int parentIndex;  // -1 for root bone
    glm::mat4 bindPose;      // Transform in bind pose (T-pose)
    glm::mat4 inverseBindPose;  // Inverse bind pose matrix for skinning
    int gltfNodeIndex;       // Original glTF node index
    
    Bone() : parentIndex(-1), bindPose(1.0f), inverseBindPose(1.0f), gltfNodeIndex(-1) {}
};

class Skeleton {
public:
    Skeleton();
    ~Skeleton();
    
    bool loadFromGLTF(const std::string& filepath, int skinIndex = 0);
    void addBone(const Bone& bone);
    
    const std::vector<Bone>& getBones() const { return bones; }
    const Bone* getBone(const std::string& name) const;
    const Bone* getBone(int index) const;
    int getBoneIndex(const std::string& name) const;
    int getBoneCount() const { return static_cast<int>(bones.size()); }
    
    const std::string& getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    
    const std::string& getFilePath() const { return filePath; }
    void setFilePath(const std::string& path) { filePath = path; }
    
    // Get the root bone index (first bone with parentIndex == -1)
    int getRootBoneIndex() const;
    
    // Get bone hierarchy as a tree structure
    std::vector<int> getChildBones(int boneIndex) const;
    
private:
    std::string name;
    std::string filePath;
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> boneNameToIndex;
    
    void buildNameIndex();
};

} // namespace GameEngine

#endif // SKELETON_H

