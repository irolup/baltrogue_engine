#ifndef INPUT_MAPPING_H
#define INPUT_MAPPING_H

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace GameEngine {

enum class InputType {
    KEYBOARD_KEY,
    VITA_BUTTON,
    ANALOG_STICK,
    MOUSE_BUTTON,
    MOUSE_AXIS
};

enum class InputActionType {
    PRESSED,
    HELD,
    RELEASED
};

struct InputMapping {
    std::string actionName;
    InputType inputType;
    int inputCode;
    InputActionType actionType;
    float deadzone;
    float sensitivity;
    int axis;
    bool invertAxis;
    
    InputMapping() : invertAxis(false) {}
    InputMapping(const std::string& name, InputType type, int code, InputActionType action = InputActionType::HELD, int ax = -1)
        : actionName(name), inputType(type), inputCode(code), actionType(action), deadzone(0.1f), sensitivity(1.0f), axis(ax), invertAxis(false) {}
};

class InputMappingManager {
public:
    InputMappingManager();
    ~InputMappingManager();
    
    void initialize();
    void shutdown();
    
    void addMapping(const InputMapping& mapping);
    void removeMapping(const std::string& actionName);
    void removeExactMapping(const InputMapping& mapping);
    void clearMappings();
    
    bool isActionPressed(const std::string& actionName) const;
    bool isActionHeld(const std::string& actionName) const;
    bool isActionReleased(const std::string& actionName) const;
    
    float getActionAxis(const std::string& actionName) const;
    glm::vec2 getActionVector2(const std::string& actionName) const;
    
    void setDefaultMappings();
    void loadMappingsFromFile(const std::string& filePath, bool clearExisting = true);
    void saveMappingsToFile(const std::string& filePath) const;
    
    void enableHotReload(const std::string& filePath);
    void disableHotReload();
    void checkForFileChanges();
    void reloadMappings();
    
    const std::vector<InputMapping>& getAllMappings() const { return mappings; }
    void updateMapping(const std::string& actionName, const InputMapping& newMapping);
    
    using ActionCallback = std::function<void(const std::string& actionName)>;
    void setActionCallback(const std::string& actionName, ActionCallback callback);
    
private:
    std::vector<InputMapping> mappings;
    std::unordered_map<std::string, ActionCallback> actionCallbacks;
    
    std::string hotReloadFilePath;
    bool hotReloadEnabled;
    time_t lastFileModificationTime;
    
    const InputMapping* findMapping(const std::string& actionName) const;
    bool checkInputState(const InputMapping& mapping) const;
    float getAnalogValue(const InputMapping& mapping) const;
};

} // namespace GameEngine

#endif // INPUT_MAPPING_H
