#ifdef LINUX_BUILD

#include "Editor/NodeTemplateSerializer.h"
#include "Editor/SceneSerializer.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace GameEngine {

static const char* kTemplateType = "NodeTemplate";
static const char* kTemplateVersion = "1.0";

bool NodeTemplateSerializer::isValidTemplateJson(const nlohmann::json& json) {
    if (json.contains("type") && json["type"] == kTemplateType) {
        return json.contains("rootNode") && json["rootNode"].is_object();
    }

    return json.contains("name") && json.contains("transform");
}

nlohmann::json NodeTemplateSerializer::extractRootNodeJson(const nlohmann::json& json) {
    if (json.contains("type") && json["type"] == kTemplateType) {
        return json["rootNode"];
    }

    return json;
}

std::string NodeTemplateSerializer::ensureTemplateExtension(const std::string& filepath) {
    const std::string suffix = ".template.json";
    if (filepath.size() >= suffix.size() &&
        filepath.compare(filepath.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return filepath;
    }

    if (filepath.size() >= 5 && filepath.compare(filepath.size() - 5, 5, ".json") == 0) {
        return filepath.substr(0, filepath.size() - 5) + suffix;
    }

    return filepath + suffix;
}

bool NodeTemplateSerializer::saveNodeTemplate(std::shared_ptr<SceneNode> node, const std::string& filepath) {
    if (!node) {
        std::cerr << "Cannot save null node as template" << std::endl;
        return false;
    }

    if (filepath.empty()) {
        return false;
    }

    try {
        const std::string outputPath = ensureTemplateExtension(filepath);

        nlohmann::json templateJson;
        templateJson["type"] = kTemplateType;
        templateJson["version"] = kTemplateVersion;
        templateJson["name"] = node->getName();
        templateJson["rootNode"] = SceneSerializer::serializeNodeToJson(node);

        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open template file for writing: " << outputPath << std::endl;
            return false;
        }

        file << templateJson.dump(2);
        file.close();

        std::cout << "Node template saved to: " << outputPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving node template: " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<SceneNode> NodeTemplateSerializer::loadNodeTemplate(const std::string& filepath) {
    if (filepath.empty()) {
        return nullptr;
    }

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open template file for reading: " << filepath << std::endl;
            return nullptr;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        nlohmann::json templateJson = nlohmann::json::parse(buffer.str());
        if (!isValidTemplateJson(templateJson)) {
            std::cerr << "Invalid node template file: " << filepath << std::endl;
            return nullptr;
        }

        nlohmann::json rootNodeJson = extractRootNodeJson(templateJson);
        return SceneSerializer::deserializeNodeFromJson(rootNodeJson);
    } catch (const std::exception& e) {
        std::cerr << "Error loading node template: " << e.what() << std::endl;
        return nullptr;
    }
}

}

#endif
