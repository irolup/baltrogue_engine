#ifndef NODE_TEMPLATE_SERIALIZER_H
#define NODE_TEMPLATE_SERIALIZER_H

#ifdef LINUX_BUILD

#include <memory>
#include <string>
#include "Scene/SceneNode.h"
#include "../../vendor/json/single_include/nlohmann/json.hpp"

namespace GameEngine {

class NodeTemplateSerializer {
public:
    static bool saveNodeTemplate(std::shared_ptr<SceneNode> node, const std::string& filepath);
    static std::shared_ptr<SceneNode> loadNodeTemplate(const std::string& filepath);

private:
    static bool isValidTemplateJson(const nlohmann::json& json);
    static nlohmann::json extractRootNodeJson(const nlohmann::json& json);
    static std::string ensureTemplateExtension(const std::string& filepath);
};

}

#endif
#endif
