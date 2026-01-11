#include "Input/InputManager.h"
#include "Input/InputMapping.h"
#include <cstring>
#include <iostream>

#ifdef LINUX_BUILD
    #include <GLFW/glfw3.h>
    extern GLFWwindow* window; // Access global window from Platform.h
#endif

namespace GameEngine {

InputManager::InputManager()
#ifdef LINUX_BUILD
    : editorMode(false)
    , mouseCaptured(false)
    , debugMouseInput(false)
    , mousePosition(0.0f)
    , previousMousePosition(0.0f)
    , mouseDelta(0.0f)
    , mouseWheel(0.0f)
#endif
{
    memset(&currentInput, 0, sizeof(currentInput));
    memset(&previousInput, 0, sizeof(previousInput));
    
    inputMappingManager = std::unique_ptr<InputMappingManager>(new InputMappingManager());
}

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::initialize() {
    inputMappingManager->initialize();
    return true;
}

void InputManager::shutdown() {
    if (inputMappingManager) {
        inputMappingManager->shutdown();
    }
}

void InputManager::update() {
    previousInput = currentInput;
    
    platformPollInput(currentInput);
    
#ifdef LINUX_BUILD
    if (mouseCaptured || editorMode) {
        updateMouseInput();
    }
    
    if (editorMode) {
        updateKeyboardInput();
    }
#endif
    
    if (inputMappingManager) {
        inputMappingManager->checkForFileChanges();
    }
    
    processInputEvents();
}

bool InputManager::isButtonPressed(uint32_t button) const {
    return (currentInput.buttons & button) && !(previousInput.buttons & button);
}

bool InputManager::isButtonHeld(uint32_t button) const {
    return (currentInput.buttons & button) != 0;
}

bool InputManager::isButtonReleased(uint32_t button) const {
    return !(currentInput.buttons & button) && (previousInput.buttons & button);
}

InputState InputManager::getButtonState(uint32_t button) const {
    bool current = (currentInput.buttons & button) != 0;
    bool previous = (previousInput.buttons & button) != 0;
    
    if (current && !previous) {
        return InputState::PRESSED;
    } else if (current && previous) {
        return InputState::HELD;
    } else if (!current && previous) {
        return InputState::RELEASED;
    }
    
    return InputState::RELEASED;
}

glm::vec2 InputManager::getLeftStick() const {
    return normalizeStick(currentInput.lx, currentInput.ly);
}

glm::vec2 InputManager::getRightStick() const {
    return normalizeStick(currentInput.rx, currentInput.ry);
}

#ifdef LINUX_BUILD
glm::vec2 InputManager::getMousePosition() const {
    return mousePosition;
}

glm::vec2 InputManager::getMouseDelta() const {
    return mouseDelta;
}

bool InputManager::isMouseButtonPressed(int button) const {
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second == InputState::PRESSED;
}

bool InputManager::isMouseButtonHeld(int button) const {
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second == InputState::HELD;
}

bool InputManager::isMouseButtonReleased(int button) const {
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second == InputState::RELEASED;
}

float InputManager::getMouseWheel() const {
    return mouseWheel;
}

bool InputManager::isKeyPressed(int key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::PRESSED;
}

bool InputManager::isKeyHeld(int key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::HELD;
}

bool InputManager::isKeyReleased(int key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::RELEASED;
}

void InputManager::updateMouseInput() {
    if (!window) {
        return;
    }
    
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    
    previousMousePosition = mousePosition;
    mousePosition = glm::vec2((float)xpos, (float)ypos);
    mouseDelta = mousePosition - previousMousePosition;
    
    for (int button = GLFW_MOUSE_BUTTON_1; button <= GLFW_MOUSE_BUTTON_LAST; ++button) {
        bool currentState = glfwGetMouseButton(window, button) == GLFW_PRESS;
        InputState prevState = InputState::RELEASED;
        
        auto it = mouseButtonStates.find(button);
        if (it != mouseButtonStates.end()) {
            prevState = it->second;
        }
        
        if (currentState && (prevState == InputState::RELEASED)) {
            mouseButtonStates[button] = InputState::PRESSED;
        } else if (currentState && (prevState == InputState::PRESSED || prevState == InputState::HELD)) {
            mouseButtonStates[button] = InputState::HELD;
        } else if (!currentState && (prevState == InputState::PRESSED || prevState == InputState::HELD)) {
            mouseButtonStates[button] = InputState::RELEASED;
        } else {
            mouseButtonStates[button] = InputState::RELEASED;
        }
    }
}

void InputManager::updateKeyboardInput() {
    if (!window) return;
    
    int keys[] = {
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
        GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_LEFT_CONTROL,
        GLFW_KEY_ESCAPE, GLFW_KEY_ENTER, GLFW_KEY_TAB,
        GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN
    };
    
    for (int key : keys) {
        bool currentState = glfwGetKey(window, key) == GLFW_PRESS;
        InputState prevState = InputState::RELEASED;
        
        auto it = keyStates.find(key);
        if (it != keyStates.end()) {
            prevState = it->second;
        }
        
        if (currentState && (prevState == InputState::RELEASED)) {
            keyStates[key] = InputState::PRESSED;
        } else if (currentState && (prevState == InputState::PRESSED || prevState == InputState::HELD)) {
            keyStates[key] = InputState::HELD;
        } else if (!currentState && (prevState == InputState::PRESSED || prevState == InputState::HELD)) {
            keyStates[key] = InputState::RELEASED;
        } else {
            keyStates[key] = InputState::RELEASED;
        }
    }
}
#endif

glm::vec2 InputManager::normalizeStick(uint8_t x, uint8_t y) const {
    float fx = (x - 128.0f) / 128.0f;
    float fy = (y - 128.0f) / 128.0f;
    
    const float deadzone = 0.1f;
    glm::vec2 stick(fx, fy);
    float magnitude = glm::length(stick);
    
    if (magnitude < deadzone) {
        return glm::vec2(0.0f);
    }
    
    // Normalize and remove deadzone
    return stick * ((magnitude - deadzone) / (1.0f - deadzone)) / magnitude;
}

void InputManager::processInputEvents() {
    // Call callbacks for button events
    if (onButtonPressed) {
        for (int i = 0; i < 32; ++i) {
            uint32_t button = 1 << i;
            if (isButtonPressed(button)) {
                onButtonPressed(button);
            }
        }
    }
    
    if (onButtonReleased) {
        for (int i = 0; i < 32; ++i) {
            uint32_t button = 1 << i;
            if (isButtonReleased(button)) {
                onButtonReleased(button);
            }
        }
    }
    
    // Call stick callbacks
    if (onLeftStick) {
        glm::vec2 leftStick = getLeftStick();
        if (glm::length(leftStick) > 0.0f) {
            onLeftStick(leftStick);
        }
    }
    
    if (onRightStick) {
        glm::vec2 rightStick = getRightStick();
        if (glm::length(rightStick) > 0.0f) {
            onRightStick(rightStick);
        }
    }
}

#ifdef LINUX_BUILD
bool InputManager::shouldExit() const {
    return (currentInput.buttons & 0x80000000) != 0;
}

void InputManager::setMouseCapture(bool enabled) {
    if (!window) {
        return;
    }
    
    mouseCaptured = enabled;
    
    if (enabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}
#endif

// Action-based input methods
bool InputManager::isActionPressed(const std::string& actionName) const {
    return inputMappingManager->isActionPressed(actionName);
}

bool InputManager::isActionHeld(const std::string& actionName) const {
    return inputMappingManager->isActionHeld(actionName);
}

bool InputManager::isActionReleased(const std::string& actionName) const {
    return inputMappingManager->isActionReleased(actionName);
}

float InputManager::getActionAxis(const std::string& actionName) const {
    return inputMappingManager->getActionAxis(actionName);
}

glm::vec2 InputManager::getActionVector2(const std::string& actionName) const {
    return inputMappingManager->getActionVector2(actionName);
}

} // namespace GameEngine
