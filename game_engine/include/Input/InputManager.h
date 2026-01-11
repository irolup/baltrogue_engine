#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "Platform.h"
#include <glm/glm.hpp>
#include <functional>
#include <unordered_map>
#include <memory>

namespace GameEngine {

enum class InputState {
    RELEASED,
    PRESSED,
    HELD
};

class InputMappingManager;

class InputManager {
public:
    InputManager();
    ~InputManager();
    
    bool initialize();
    void shutdown();
    void update();
    
    bool isButtonPressed(uint32_t button) const;
    bool isButtonHeld(uint32_t button) const;
    bool isButtonReleased(uint32_t button) const;
    InputState getButtonState(uint32_t button) const;
    
    glm::vec2 getLeftStick() const;
    glm::vec2 getRightStick() const;
    
    const SceCtrlData& getCurrentInput() const { return currentInput; }
    const SceCtrlData& getPreviousInput() const { return previousInput; }
    
    using ButtonCallback = std::function<void(uint32_t button)>;
    using StickCallback = std::function<void(const glm::vec2& stick)>;
    
    void setButtonPressCallback(ButtonCallback callback) { onButtonPressed = callback; }
    void setButtonReleaseCallback(ButtonCallback callback) { onButtonReleased = callback; }
    void setLeftStickCallback(StickCallback callback) { onLeftStick = callback; }
    void setRightStickCallback(StickCallback callback) { onRightStick = callback; }
    
#ifdef LINUX_BUILD
    glm::vec2 getMousePosition() const;
    glm::vec2 getMouseDelta() const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonHeld(int button) const;
    bool isMouseButtonReleased(int button) const;
    float getMouseWheel() const;
    
    bool isKeyPressed(int key) const;
    bool isKeyHeld(int key) const;
    bool isKeyReleased(int key) const;
    
    bool isInEditorMode() const { return editorMode; }
    void setEditorMode(bool enabled) { editorMode = enabled; }
    
    void setMouseCapture(bool enabled);
    bool isMouseCaptured() const { return mouseCaptured; }
    void setDebugMouseInput(bool enabled) { debugMouseInput = enabled; }
    
    bool shouldExit() const;
#endif
    
    InputMappingManager& getInputMapping() { return *inputMappingManager; }
    const InputMappingManager& getInputMapping() const { return *inputMappingManager; }
    
    bool isActionPressed(const std::string& actionName) const;
    bool isActionHeld(const std::string& actionName) const;
    bool isActionReleased(const std::string& actionName) const;
    float getActionAxis(const std::string& actionName) const;
    glm::vec2 getActionVector2(const std::string& actionName) const;
    
private:
    SceCtrlData currentInput;
    SceCtrlData previousInput;
    
    std::unique_ptr<InputMappingManager> inputMappingManager;
    
    ButtonCallback onButtonPressed;
    ButtonCallback onButtonReleased;
    StickCallback onLeftStick;
    StickCallback onRightStick;
    
#ifdef LINUX_BUILD
    bool editorMode;
    bool mouseCaptured;
    bool debugMouseInput;
    glm::vec2 mousePosition;
    glm::vec2 previousMousePosition;
    glm::vec2 mouseDelta;
    float mouseWheel;
    
    std::unordered_map<int, InputState> mouseButtonStates;
    std::unordered_map<int, InputState> keyStates;
    
    void updateMouseInput();
    void updateKeyboardInput();
#endif
    
    glm::vec2 normalizeStick(uint8_t x, uint8_t y) const;
    void processInputEvents();
};

} // namespace GameEngine

#endif // INPUT_MANAGER_H
