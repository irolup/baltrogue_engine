#include "Input/InputMapping.h"
#include "Input/InputManager.h"
#include "Core/Engine.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <vector>
#include <ctime>
#ifdef LINUX_BUILD
#include <filesystem>
#endif

#ifdef LINUX_BUILD
    #include <GLFW/glfw3.h>
#endif

namespace GameEngine {

InputMappingManager::InputMappingManager() 
    : hotReloadEnabled(false)
    , lastFileModificationTime(0)
{
}

InputMappingManager::~InputMappingManager() {
}

void InputMappingManager::initialize() {
#ifdef VITA_BUILD
    printf("InputMappingManager: Initializing...\n");
#endif
    
    // Check if saved mappings file exists
    std::ifstream savedFile("input_mappings.txt");
    bool hasSavedMappings = savedFile.is_open();
    savedFile.close();
    
    if (hasSavedMappings) {
        // If user has saved mappings, load only those (not defaults)
        loadMappingsFromFile("input_mappings.txt", true);
#ifdef VITA_BUILD
        printf("InputMappingManager: Loaded saved mappings, total mappings: %zu\n", mappings.size());
#endif
    } else {
        // If no saved mappings, load defaults
        loadMappingsFromFile("default_input_mappings.txt", true);
#ifdef VITA_BUILD
        printf("InputMappingManager: Loaded default mappings, total mappings: %zu\n", mappings.size());
#endif
    }
    
    // Enable hot-reload for the input mappings file
    enableHotReload("input_mappings.txt");
    
#ifdef VITA_BUILD
    printf("InputMappingManager: Final mappings count: %zu\n", mappings.size());
#endif
}

void InputMappingManager::shutdown() {
    // Auto-save mappings on shutdown

    //only save if we are in editor mode
    #ifdef EDITOR_BUILD
        saveMappingsToFile("input_mappings.txt");
    #endif
    
    mappings.clear();
    actionCallbacks.clear();
}

void InputMappingManager::addMapping(const InputMapping& mapping) {
    // Allow multiple mappings for the same action name (e.g., PC and VITA versions)
    // Only remove if it's an exact duplicate (same action name, input type, and input code)
    removeExactMapping(mapping);
    mappings.push_back(mapping);
}

void InputMappingManager::removeMapping(const std::string& actionName) {
    mappings.erase(
        std::remove_if(mappings.begin(), mappings.end(),
            [&actionName](const InputMapping& mapping) {
                return mapping.actionName == actionName;
            }),
        mappings.end()
    );
}

void InputMappingManager::removeExactMapping(const InputMapping& mapping) {
    mappings.erase(
        std::remove_if(mappings.begin(), mappings.end(),
            [&mapping](const InputMapping& existing) {
                return existing.actionName == mapping.actionName &&
                       existing.inputType == mapping.inputType &&
                       existing.inputCode == mapping.inputCode;
            }),
        mappings.end()
    );
}

void InputMappingManager::clearMappings() {
    mappings.clear();
}

bool InputMappingManager::isActionPressed(const std::string& actionName) const {
    const InputMapping* mapping = findMapping(actionName);
    if (!mapping || mapping->actionType != InputActionType::PRESSED) {
        return false;
    }
    return checkInputState(*mapping);
}

bool InputMappingManager::isActionHeld(const std::string& actionName) const {
    const InputMapping* mapping = findMapping(actionName);
    if (!mapping || mapping->actionType != InputActionType::HELD) {
        return false;
    }
    return checkInputState(*mapping);
}

bool InputMappingManager::isActionReleased(const std::string& actionName) const {
    const InputMapping* mapping = findMapping(actionName);
    if (!mapping || mapping->actionType != InputActionType::RELEASED) {
        return false;
    }
    return checkInputState(*mapping);
}

float InputMappingManager::getActionAxis(const std::string& actionName) const {
    const InputMapping* mapping = findMapping(actionName);
    if (!mapping) {
        return 0.0f;
    }
    return getAnalogValue(*mapping);
}

glm::vec2 InputMappingManager::getActionVector2(const std::string& actionName) const {
    const InputMapping* mapping = findMapping(actionName);
    if (!mapping) {
        return glm::vec2(0.0f);
    }
    
    // For analog sticks, return the stick value
    if (mapping->inputType == InputType::ANALOG_STICK) {
        auto& inputManager = GetEngine().getInputManager();
        if (mapping->inputCode == 0) { // Left stick
            return inputManager.getLeftStick();
        } else if (mapping->inputCode == 1) { // Right stick
            return inputManager.getRightStick();
        }
    }
    
    return glm::vec2(0.0f);
}

void InputMappingManager::setDefaultMappings() {
    mappings.clear();
    
#ifdef LINUX_BUILD
    // Keyboard mappings
    addMapping(InputMapping("MoveForward", InputType::KEYBOARD_KEY, GLFW_KEY_W));
    addMapping(InputMapping("MoveBackward", InputType::KEYBOARD_KEY, GLFW_KEY_S));
    addMapping(InputMapping("MoveLeft", InputType::KEYBOARD_KEY, GLFW_KEY_A));
    addMapping(InputMapping("MoveRight", InputType::KEYBOARD_KEY, GLFW_KEY_D));
    addMapping(InputMapping("MoveUp", InputType::KEYBOARD_KEY, GLFW_KEY_SPACE));
    addMapping(InputMapping("MoveDown", InputType::KEYBOARD_KEY, GLFW_KEY_LEFT_SHIFT));
    addMapping(InputMapping("Jump", InputType::KEYBOARD_KEY, GLFW_KEY_SPACE, InputActionType::PRESSED));
    addMapping(InputMapping("Run", InputType::KEYBOARD_KEY, GLFW_KEY_LEFT_CONTROL));
    addMapping(InputMapping("Interact", InputType::KEYBOARD_KEY, GLFW_KEY_E, InputActionType::PRESSED));
    addMapping(InputMapping("Menu", InputType::KEYBOARD_KEY, GLFW_KEY_ESCAPE, InputActionType::PRESSED));
    
    // Mouse mappings
    addMapping(InputMapping("LookHorizontal", InputType::MOUSE_AXIS, 0)); // X axis
    addMapping(InputMapping("LookVertical", InputType::MOUSE_AXIS, 1));   // Y axis
    addMapping(InputMapping("PrimaryFire", InputType::MOUSE_BUTTON, GLFW_MOUSE_BUTTON_LEFT, InputActionType::HELD));
    addMapping(InputMapping("SecondaryFire", InputType::MOUSE_BUTTON, GLFW_MOUSE_BUTTON_RIGHT, InputActionType::HELD));
    
    // Analog stick mappings (for gamepad support)
    addMapping(InputMapping("MoveHorizontal", InputType::ANALOG_STICK, 0, InputActionType::HELD, 0)); // Left stick X
    addMapping(InputMapping("MoveVertical", InputType::ANALOG_STICK, 0, InputActionType::HELD, 1));  // Left stick Y
    addMapping(InputMapping("LookHorizontal", InputType::ANALOG_STICK, 1, InputActionType::HELD, 0)); // Right stick X
    addMapping(InputMapping("LookVertical", InputType::ANALOG_STICK, 1, InputActionType::HELD, 1));  // Right stick Y
#endif

#ifdef VITA_BUILD
    // Vita button mappings
    addMapping(InputMapping("MoveHorizontal", InputType::ANALOG_STICK, 0, InputActionType::HELD, 0)); // Left stick X
    addMapping(InputMapping("MoveVertical", InputType::ANALOG_STICK, 0, InputActionType::HELD, 1));  // Left stick Y
    addMapping(InputMapping("LookHorizontal", InputType::ANALOG_STICK, 1, InputActionType::HELD, 0)); // Right stick X
    addMapping(InputMapping("LookVertical", InputType::ANALOG_STICK, 1, InputActionType::HELD, 1));  // Right stick Y
    
    addMapping(InputMapping("Jump", InputType::VITA_BUTTON, SCE_CTRL_CROSS, InputActionType::PRESSED));
    addMapping(InputMapping("Interact", InputType::VITA_BUTTON, SCE_CTRL_CIRCLE, InputActionType::PRESSED));
    addMapping(InputMapping("PrimaryFire", InputType::VITA_BUTTON, SCE_CTRL_SQUARE, InputActionType::HELD));
    addMapping(InputMapping("SecondaryFire", InputType::VITA_BUTTON, SCE_CTRL_TRIANGLE, InputActionType::HELD));
    addMapping(InputMapping("Run", InputType::VITA_BUTTON, SCE_CTRL_LTRIGGER));
    addMapping(InputMapping("Menu", InputType::VITA_BUTTON, SCE_CTRL_START, InputActionType::PRESSED));
    addMapping(InputMapping("Back", InputType::VITA_BUTTON, SCE_CTRL_SELECT, InputActionType::PRESSED));
    
    // D-pad mappings
    addMapping(InputMapping("DpadUp", InputType::VITA_BUTTON, SCE_CTRL_UP, InputActionType::PRESSED));
    addMapping(InputMapping("DpadDown", InputType::VITA_BUTTON, SCE_CTRL_DOWN, InputActionType::PRESSED));
    addMapping(InputMapping("DpadLeft", InputType::VITA_BUTTON, SCE_CTRL_LEFT, InputActionType::PRESSED));
    addMapping(InputMapping("DpadRight", InputType::VITA_BUTTON, SCE_CTRL_RIGHT, InputActionType::PRESSED));
#endif
}

void InputMappingManager::loadMappingsFromFile(const std::string& filePath, bool clearExisting) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // File doesn't exist, that's okay - we'll use defaults
        std::cout << "InputMappingManager: No saved mappings found at " << filePath << ", using defaults" << std::endl;
        return;
    }
    
    // Clear existing mappings if requested
    if (clearExisting) {
        mappings.clear();
    }
    
    // Enhanced format: action:name,platform,type,code,actionType,deadzone,sensitivity,axis
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        if (line.find("action:") != std::string::npos) {
            // Parse action line: action:name,platform,type,code,actionType,deadzone,sensitivity,axis
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (std::getline(iss, token, ',')) {
                tokens.push_back(token);
            }
            
            if (tokens.size() >= 5) {
                std::string name = tokens[0].substr(7); // Remove "action:" prefix
                std::string platform = tokens[1];
                InputType type = static_cast<InputType>(std::stoi(tokens[2]));
                int code = std::stoi(tokens[3]);
                InputActionType actionType = static_cast<InputActionType>(std::stoi(tokens[4]));
                
                float deadzone = 0.1f;
                float sensitivity = 1.0f;
                int axis = -1;
                bool invertAxis = false;
                
                if (tokens.size() > 5) deadzone = std::stof(tokens[5]);
                if (tokens.size() > 6) sensitivity = std::stof(tokens[6]);
                if (tokens.size() > 7) axis = std::stoi(tokens[7]);
                if (tokens.size() > 8) invertAxis = (std::stoi(tokens[8]) != 0);  // 0=false, 1=true
                
                // Load all mappings regardless of platform
                // User wants to see all mappings in the editor
                bool shouldLoad = true;
                
                if (shouldLoad) {
                    InputMapping mapping(name, type, code, actionType, axis);
                    mapping.deadzone = deadzone;
                    mapping.sensitivity = sensitivity;
                    mapping.invertAxis = invertAxis;
                    addMapping(mapping);
                }
            }
        }
    }
    
    file.close();
    std::cout << "InputMappingManager: Loaded mappings from " << filePath << std::endl;
}

void InputMappingManager::saveMappingsToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cout << "InputMappingManager: Could not create file " << filePath << std::endl;
        return;
    }
    
    // Write header comments
    file << "# Input Mappings Configuration" << std::endl;
    file << "# This file defines how input actions are mapped to physical inputs" << std::endl;
    file << "# Format: action:name,platform,type,code,actionType,deadzone,sensitivity,axis" << std::endl;
    file << "# " << std::endl;
    file << "# Platforms:" << std::endl;
    file << "#   PC    - Linux/Windows builds" << std::endl;
    file << "#   VITA  - PlayStation Vita builds" << std::endl;
    file << "#   ALL   - Both platforms" << std::endl;
    file << "#" << std::endl;
    file << "# Input Types:" << std::endl;
    file << "#   0 = Keyboard Key (PC only)" << std::endl;
    file << "#   1 = Vita Button (VITA only)" << std::endl;
    file << "#   2 = Analog Stick (both platforms)" << std::endl;
    file << "#   3 = Mouse Button (PC only)" << std::endl;
    file << "#   4 = Mouse Axis (PC only)" << std::endl;
    file << "#" << std::endl;
    file << "# Action Types:" << std::endl;
    file << "#   0 = Pressed (triggered once when pressed)" << std::endl;
    file << "#   1 = Held (triggered continuously while held)" << std::endl;
    file << "#   2 = Released (triggered once when released)" << std::endl;
    file << "#" << std::endl;
    file << "# Axis (for analog sticks):" << std::endl;
    file << "#   0 = X axis" << std::endl;
    file << "#   1 = Y axis" << std::endl;
    file << "#   -1 = Magnitude (both axes combined)" << std::endl;
    file << "#" << std::endl;
    file << "# Invert Axis (optional, for analog sticks only):" << std::endl;
    file << "#   0 = Use raw axis value (default)" << std::endl;
    file << "#   1 = Invert axis value (negative becomes positive, used for MoveForward/MoveLeft)" << std::endl;
    file << std::endl;
    
    // Enhanced format: action:name,platform,type,code,actionType,deadzone,sensitivity,axis,invertAxis
    for (const auto& mapping : mappings) {
        // Determine platform based on input type
        std::string platform = "ALL";
        if (mapping.inputType == InputType::KEYBOARD_KEY || 
            mapping.inputType == InputType::MOUSE_BUTTON || 
            mapping.inputType == InputType::MOUSE_AXIS) {
            platform = "PC";
        } else if (mapping.inputType == InputType::VITA_BUTTON) {
            platform = "VITA";
        }
        
        file << "action:" << mapping.actionName << ","
             << platform << ","
             << static_cast<int>(mapping.inputType) << ","
             << mapping.inputCode << ","
             << static_cast<int>(mapping.actionType) << ","
             << mapping.deadzone << ","
             << mapping.sensitivity << ","
             << mapping.axis << ","
             << (mapping.invertAxis ? 1 : 0) << std::endl;
    }
    
    file.close();
    std::cout << "InputMappingManager: Saved mappings to " << filePath << std::endl;
}

void InputMappingManager::updateMapping(const std::string& actionName, const InputMapping& newMapping) {
    // Remove all mappings with this action name, then add the new one
    removeMapping(actionName);
    addMapping(newMapping);
}

void InputMappingManager::setActionCallback(const std::string& actionName, ActionCallback callback) {
    actionCallbacks[actionName] = callback;
}

const InputMapping* InputMappingManager::findMapping(const std::string& actionName) const {
    for (const auto& mapping : mappings) {
        if (mapping.actionName == actionName) {
            // Check if this mapping is appropriate for the current platform
            bool isCurrentPlatform = false;
            
#ifdef VITA_BUILD
            // For Vita builds, prefer Vita-specific mappings, but also accept analog stick mappings
            isCurrentPlatform = (mapping.inputType == InputType::VITA_BUTTON || 
                               mapping.inputType == InputType::ANALOG_STICK);
#else
            // For PC builds, prefer PC-specific mappings, but also accept analog stick mappings
            isCurrentPlatform = (mapping.inputType == InputType::KEYBOARD_KEY || 
                               mapping.inputType == InputType::MOUSE_BUTTON || 
                               mapping.inputType == InputType::MOUSE_AXIS ||
                               mapping.inputType == InputType::ANALOG_STICK);
#endif
            
            if (isCurrentPlatform) {
                return &mapping;
            }
        }
    }
    
    // If no platform-specific mapping found, return the first one (fallback)
    for (const auto& mapping : mappings) {
        if (mapping.actionName == actionName) {
            return &mapping;
        }
    }
    
    return nullptr;
}

bool InputMappingManager::checkInputState(const InputMapping& mapping) const {
    auto& inputManager = GetEngine().getInputManager();
    
    switch (mapping.inputType) {
        case InputType::KEYBOARD_KEY:
#ifdef LINUX_BUILD
            if (mapping.actionType == InputActionType::PRESSED) {
                return inputManager.isKeyPressed(mapping.inputCode);
            } else if (mapping.actionType == InputActionType::HELD) {
                return inputManager.isKeyHeld(mapping.inputCode);
            } else if (mapping.actionType == InputActionType::RELEASED) {
                return inputManager.isKeyReleased(mapping.inputCode);
            }
#endif
            break;
            
        case InputType::VITA_BUTTON:
            if (mapping.actionType == InputActionType::PRESSED) {
                return inputManager.isButtonPressed(mapping.inputCode);
            } else if (mapping.actionType == InputActionType::HELD) {
                return inputManager.isButtonHeld(mapping.inputCode);
            } else if (mapping.actionType == InputActionType::RELEASED) {
                return inputManager.isButtonReleased(mapping.inputCode);
            }
            break;
            
        case InputType::MOUSE_BUTTON:
#ifdef LINUX_BUILD
            if (mapping.actionType == InputActionType::PRESSED) {
                return inputManager.isMouseButtonPressed(mapping.inputCode);
            } else if (mapping.actionType == InputActionType::HELD) {
                return inputManager.isMouseButtonHeld(mapping.inputCode);
            } else if (mapping.actionType == InputActionType::RELEASED) {
                return inputManager.isMouseButtonReleased(mapping.inputCode);
            }
#endif
            break;
            
        case InputType::ANALOG_STICK:
            // For analog sticks, check based on axis
            {
                glm::vec2 stickValue = (mapping.inputCode == 0) ? inputManager.getLeftStick() : inputManager.getRightStick();
                
                if (mapping.axis == -1) {
                    // Check magnitude for both axes combined
                    float magnitude = glm::length(stickValue);
                    return magnitude > mapping.deadzone;
                } else if (mapping.axis == 0) {
                    // Check X axis
                    return glm::abs(stickValue.x) > mapping.deadzone;
                } else if (mapping.axis == 1) {
                    // Check Y axis
                    return glm::abs(stickValue.y) > mapping.deadzone;
                }
            }
            break;
            
        case InputType::MOUSE_AXIS:
#ifdef LINUX_BUILD
            // For mouse axes, check if delta exceeds threshold
            {
                glm::vec2 mouseDelta = inputManager.getMouseDelta();
                float axisValue = (mapping.inputCode == 0) ? mouseDelta.x : mouseDelta.y;
                return std::abs(axisValue) > mapping.deadzone;
            }
#endif
            break;
    }
    
    return false;
}

float InputMappingManager::getAnalogValue(const InputMapping& mapping) const {
    auto& inputManager = GetEngine().getInputManager();
    
    switch (mapping.inputType) {
        case InputType::ANALOG_STICK:
            {
                glm::vec2 stickValue = (mapping.inputCode == 0) ? inputManager.getLeftStick() : inputManager.getRightStick();
                
                if (mapping.axis == -1) {
                    // Return magnitude
                    float magnitude = glm::length(stickValue);
                    if (magnitude > mapping.deadzone) {
                        return magnitude * mapping.sensitivity;
                    }
                } else if (mapping.axis == 0) {
                    // Return X axis - return raw signed value (negative = left, positive = right)
                    if (glm::abs(stickValue.x) > mapping.deadzone) {
                        float value = stickValue.x;
                        if (mapping.invertAxis) {
                            value = -value;  // Invert if mapping specifies it
                        }
                        return value * mapping.sensitivity;
                    }
                } else if (mapping.axis == 1) {
                    // Return Y axis - return raw signed value (negative = forward/up, positive = backward/down)
                    if (glm::abs(stickValue.y) > mapping.deadzone) {
                        float value = stickValue.y;
                        if (mapping.invertAxis) {
                            value = -value;  // Invert if mapping specifies it
                        }
                        return value * mapping.sensitivity;
                    }
                }
            }
            break;
            
        case InputType::MOUSE_AXIS:
#ifdef LINUX_BUILD
            {
                glm::vec2 mouseDelta = inputManager.getMouseDelta();
                float axisValue = (mapping.inputCode == 0) ? mouseDelta.x : mouseDelta.y;
                return axisValue * mapping.sensitivity;
            }
#endif
            break;
            
        case InputType::KEYBOARD_KEY:
#ifdef LINUX_BUILD
            // For keyboard keys, return 1.0 when held, 0.0 when not held
            // This allows getActionAxis() to work for both keyboard (binary) and analog sticks (continuous)
            {
                auto& inputManager = GetEngine().getInputManager();
                return inputManager.isKeyHeld(mapping.inputCode) ? 1.0f : 0.0f;
            }
#else
            return 0.0f;
#endif
            break;
            
        case InputType::VITA_BUTTON:
            // For Vita buttons, return 1.0 when held, 0.0 when not held
            {
                auto& inputManager = GetEngine().getInputManager();
                return inputManager.isButtonHeld(mapping.inputCode) ? 1.0f : 0.0f;
            }
            break;
            
        case InputType::MOUSE_BUTTON:
#ifdef LINUX_BUILD
            // For mouse buttons, return 1.0 when held, 0.0 when not held
            {
                auto& inputManager = GetEngine().getInputManager();
                return inputManager.isMouseButtonHeld(mapping.inputCode) ? 1.0f : 0.0f;
            }
#else
            return 0.0f;
#endif
            break;
    }
    
    return 0.0f;
}

void InputMappingManager::enableHotReload(const std::string& filePath) {
    hotReloadFilePath = filePath;
    hotReloadEnabled = true;
    
#ifdef LINUX_BUILD
    // Get initial file modification time
    try {
        auto fileTime = std::filesystem::last_write_time(filePath);
        lastFileModificationTime = std::chrono::duration_cast<std::chrono::seconds>(
            fileTime.time_since_epoch()).count();
    } catch (const std::exception& e) {
        std::cout << "InputMappingManager: Could not get file modification time: " << e.what() << std::endl;
        hotReloadEnabled = false;
    }
#else
    // For Vita builds, use a simple time-based approach
    lastFileModificationTime = time(nullptr);
#endif
    
    std::cout << "InputMappingManager: Hot-reload enabled for " << filePath << std::endl;
}

void InputMappingManager::disableHotReload() {
    hotReloadEnabled = false;
    hotReloadFilePath.clear();
    std::cout << "InputMappingManager: Hot-reload disabled" << std::endl;
}

void InputMappingManager::checkForFileChanges() {
    if (!hotReloadEnabled || hotReloadFilePath.empty()) {
        return;
    }
    
#ifdef LINUX_BUILD
    try {
        auto fileTime = std::filesystem::last_write_time(hotReloadFilePath);
        auto currentModTime = std::chrono::duration_cast<std::chrono::seconds>(
            fileTime.time_since_epoch()).count();
        
        if (currentModTime > lastFileModificationTime) {
            std::cout << "InputMappingManager: Input mappings file changed, reloading..." << std::endl;
            reloadMappings();
            lastFileModificationTime = currentModTime;
        }
    } catch (const std::exception& e) {
        std::cout << "InputMappingManager: Error checking file changes: " << e.what() << std::endl;
    }
#else
    // For Vita builds, hot-reload is disabled
    // This is a simplified implementation that doesn't check file changes
#endif
}

void InputMappingManager::reloadMappings() {
    if (!hotReloadFilePath.empty()) {
        // Only reload the saved mappings file, not the defaults
        // This shows only what the user has saved, not the full default set
        loadMappingsFromFile(hotReloadFilePath, true);
        std::cout << "InputMappingManager: Mappings reloaded successfully" << std::endl;
    }
}

} // namespace GameEngine
