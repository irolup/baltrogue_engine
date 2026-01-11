#include "Core/MenuManager.h"
#include "Rendering/Renderer.h"
#include "Components/TextComponent.h"
#include "Core/Engine.h"
#include "Core/Time.h"
#include "Core/ScriptManager.h"
#include "Input/InputManager.h"
#include <iostream>

// Lua includes
extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace GameEngine {

std::unique_ptr<MenuManager> MenuManager::instance = nullptr;

MenuManager& MenuManager::getInstance() {
    if (!instance) {
        instance = std::unique_ptr<MenuManager>(new MenuManager());
    }
    return *instance;
}

MenuManager::MenuManager()
    : gamePaused(false)
    , savedTimeScale(1.0f)
    , luaState(nullptr)
    , initialized(false)
{
}

MenuManager::~MenuManager() {
    shutdown();
}

bool MenuManager::initialize() {
    if (initialized) {
        return true;
    }
    
    std::cout << "MenuManager: Initializing..." << std::endl;
    
    initialized = true;
    std::cout << "MenuManager: Successfully initialized" << std::endl;
    return true;
}

void MenuManager::shutdown() {
    if (!initialized) {
        return;
    }
    
    std::cout << "MenuManager: Shutting down..." << std::endl;
    
    for (auto& pair : menus) {
        pair.second.state = MenuState::HIDDEN;
    }
    visibleMenuStack.clear();
    
    gamePaused = false;
    
    menus.clear();
    luaState = nullptr;
    initialized = false;
    
    std::cout << "MenuManager: Shutdown complete" << std::endl;
}

std::string MenuManager::createMenu(const std::string& menuId, MenuType type) {
    if (menus.find(menuId) != menus.end()) {
        std::cerr << "MenuManager: Menu '" << menuId << "' already exists" << std::endl;
        return menuId;
    }
    
    Menu menu;
    menu.id = menuId;
    menu.type = type;
    menu.state = MenuState::HIDDEN;
    menu.selectedIndex = 0;
    
    if (type == MenuType::PAUSE_MENU || type == MenuType::MAIN_MENU) {
        menu.pauseGame = true;
    } else {
        menu.pauseGame = false;
    }
    
    menus[menuId] = menu;
    std::cout << "MenuManager: Created menu '" << menuId << "'" << std::endl;
    
    return menuId;
}

bool MenuManager::removeMenu(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return false;
    }
    
    if (it->second.state == MenuState::VISIBLE) {
        hideMenu(menuId);
    }
    
    menus.erase(it);
    std::cout << "MenuManager: Removed menu '" << menuId << "'" << std::endl;
    
    return true;
}

Menu* MenuManager::getMenu(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return nullptr;
    }
    return &it->second;
}

void MenuManager::showMenu(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        std::cerr << "MenuManager: Menu '" << menuId << "' not found" << std::endl;
        return;
    }
    
    Menu& menu = it->second;
    
    if (menu.state == MenuState::VISIBLE) {
        return;  // Already visible
    }
    
    menu.state = MenuState::VISIBLE;
    menu.selectedIndex = 0;
    
    auto stackIt = std::find(visibleMenuStack.begin(), visibleMenuStack.end(), menuId);
    if (stackIt == visibleMenuStack.end()) {
        visibleMenuStack.push_back(menuId);
    }
    
    if (menu.pauseGame && !gamePaused) {
        auto& engine = GetEngine();
        savedTimeScale = engine.getTime().getTimeScale();
        engine.getTime().setTimeScale(0.0f);
        gamePaused = true;
    }
    
    if (!menu.onShowCallback.empty()) {
        callMenuCallback(menu.onShowCallback, menuId);
    }
    
    std::cout << "MenuManager: Showing menu '" << menuId << "'" << std::endl;
}

void MenuManager::hideMenu(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return;
    }
    
    Menu& menu = it->second;
    
    if (menu.state == MenuState::HIDDEN) {
        return;  // Already hidden
    }
    
    menu.state = MenuState::HIDDEN;
    
    visibleMenuStack.erase(
        std::remove(visibleMenuStack.begin(), visibleMenuStack.end(), menuId),
        visibleMenuStack.end()
    );
    
    if (gamePaused) {
        bool shouldPause = false;
        for (const auto& visibleId : visibleMenuStack) {
            auto menuIt = menus.find(visibleId);
            if (menuIt != menus.end() && menuIt->second.pauseGame) {
                shouldPause = true;
                break;
            }
        }
        
        if (!shouldPause) {
            auto& engine = GetEngine();
            engine.getTime().setTimeScale(savedTimeScale);
            gamePaused = false;
        }
    }
    
    if (!menu.onHideCallback.empty()) {
        callMenuCallback(menu.onHideCallback, menuId);
    }
    
    std::cout << "MenuManager: Hiding menu '" << menuId << "'" << std::endl;
}

void MenuManager::toggleMenu(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return;
    }
    
    if (it->second.state == MenuState::VISIBLE) {
        hideMenu(menuId);
    } else {
        showMenu(menuId);
    }
}

bool MenuManager::isMenuVisible(const std::string& menuId) const {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return false;
    }
    return it->second.state == MenuState::VISIBLE;
}

void MenuManager::addMenuItem(const std::string& menuId, const MenuItem& item) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        std::cerr << "MenuManager: Menu '" << menuId << "' not found" << std::endl;
        return;
    }
    
    Menu& menu = it->second;
    menu.items.push_back(item);
    std::cout << "MenuManager: Added item '" << item.id << "' to menu '" << menuId << "'" << std::endl;
}

void MenuManager::removeMenuItem(const std::string& menuId, const std::string& itemId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return;
    }
    
    Menu& menu = it->second;
    menu.items.erase(
        std::remove_if(menu.items.begin(), menu.items.end(),
            [&itemId](const MenuItem& item) { return item.id == itemId; }),
        menu.items.end()
    );
}

void MenuManager::clearMenuItems(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return;
    }
    
    it->second.items.clear();
    it->second.selectedIndex = 0;
}

void MenuManager::selectNextItem(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end() || it->second.items.empty()) {
        return;
    }
    
    Menu& menu = it->second;
    menu.selectedIndex = (menu.selectedIndex + 1) % menu.items.size();
}

void MenuManager::selectPreviousItem(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end() || it->second.items.empty()) {
        return;
    }
    
    Menu& menu = it->second;
    menu.selectedIndex = (menu.selectedIndex - 1 + menu.items.size()) % menu.items.size();
}

void MenuManager::selectItem(const std::string& menuId, int index) {
    auto it = menus.find(menuId);
    if (it == menus.end() || it->second.items.empty()) {
        return;
    }
    
    Menu& menu = it->second;
    if (index >= 0 && index < static_cast<int>(menu.items.size())) {
        menu.selectedIndex = index;
    }
}

void MenuManager::activateSelectedItem(const std::string& menuId) {
    auto it = menus.find(menuId);
    if (it == menus.end() || it->second.items.empty()) {
        return;
    }
    
    Menu& menu = it->second;
    if (menu.selectedIndex >= 0 && menu.selectedIndex < static_cast<int>(menu.items.size())) {
        MenuItem& item = menu.items[menu.selectedIndex];
        
        if (!item.enabled) {
        return;
    }
    
    if (!item.action.empty()) {
        if (!luaState) {
            luaState = ScriptManager::getInstance().getGlobalLuaState();
        }
        
        if (luaState) {
            lua_getglobal(luaState, item.action.c_str());
            if (lua_isfunction(luaState, -1)) {
                lua_pushstring(luaState, menuId.c_str());
                lua_pushstring(luaState, item.id.c_str());
                
                if (lua_pcall(luaState, 2, 0, 0) != LUA_OK) {
                    std::cerr << "MenuManager: Error calling menu item action: " << lua_tostring(luaState, -1) << std::endl;
                    lua_pop(luaState, 1);
                }
            } else {
                lua_pop(luaState, 1);
            }
            }
        }
    }
}

void MenuManager::setPauseOnMenu(const std::string& menuId, bool pause) {
    auto it = menus.find(menuId);
    if (it == menus.end()) {
        return;
    }
    
    it->second.pauseGame = pause;
}

void MenuManager::update(float deltaTime) {
    if (!initialized) {
        return;
    }
    
    handleMenuInput();
    
    for (const auto& menuId : visibleMenuStack) {
        auto it = menus.find(menuId);
        if (it != menus.end() && !it->second.onUpdateCallback.empty()) {
            callMenuCallback(it->second.onUpdateCallback, menuId);
        }
    }
}

void MenuManager::render(Renderer& renderer) {
    if (!initialized) {
        return;
    }
    
    for (const auto& menuId : visibleMenuStack) {
        auto it = menus.find(menuId);
        if (it != menus.end() && it->second.state == MenuState::VISIBLE) {
            const Menu& menu = it->second;
            
            if (menu.showBackground) {
                renderMenuBackground(renderer, menu);
            }
            
            renderMenuItems(renderer, menu);
        }
    }
}

void MenuManager::handleMenuInput() {
    if (visibleMenuStack.empty()) {
        return;
    }
    
    const std::string& topMenuId = visibleMenuStack.back();
    auto it = menus.find(topMenuId);
    if (it == menus.end()) {
        return;
    }
    
    Menu& menu = it->second;
    
    if (topMenuId == "pause_state_control") {
        return;
    }
    
    if (menu.items.empty()) {
        return;
    }
    
    auto& inputManager = GetEngine().getInputManager();
    
    glm::vec2 leftStick = inputManager.getLeftStick();
    bool upPressed = inputManager.isActionPressed("menu_up") || 
#ifdef LINUX_BUILD
                     inputManager.isKeyPressed(265) ||  // GLFW_KEY_UP
#endif
                     (leftStick.y > 0.5f);
    bool downPressed = inputManager.isActionPressed("menu_down") || 
#ifdef LINUX_BUILD
                       inputManager.isKeyPressed(264) ||  // GLFW_KEY_DOWN
#endif
                       (leftStick.y < -0.5f);
    
    if (upPressed) {
        selectPreviousItem(topMenuId);
    } else if (downPressed) {
        selectNextItem(topMenuId);
    }
    
    bool activatePressed = inputManager.isActionPressed("menu_confirm") ||
#ifdef LINUX_BUILD
                           inputManager.isKeyPressed(257) ||  // GLFW_KEY_ENTER
                           inputManager.isKeyPressed(32) ||   // GLFW_KEY_SPACE
#endif
                           false;
    
    if (activatePressed) {
        activateSelectedItem(topMenuId);
    }
    
    bool cancelPressed = inputManager.isActionPressed("menu_cancel") ||
#ifdef LINUX_BUILD
                         inputManager.isKeyPressed(256) ||  // GLFW_KEY_ESCAPE
#endif
                         false;
    
    if (cancelPressed && menu.type == MenuType::PAUSE_MENU) {
        hideMenu(topMenuId);
    }
}

void MenuManager::callMenuCallback(const std::string& callbackName, const std::string& menuId) {
    if (!luaState) {
        luaState = ScriptManager::getInstance().getGlobalLuaState();
    }
    
    if (!luaState || callbackName.empty()) {
        return;
    }
    
    lua_getglobal(luaState, callbackName.c_str());
    if (lua_isfunction(luaState, -1)) {
        lua_pushstring(luaState, menuId.c_str());
        if (lua_pcall(luaState, 1, 0, 0) != LUA_OK) {
            std::cerr << "MenuManager: Error calling menu callback '" << callbackName << "': " 
                      << lua_tostring(luaState, -1) << std::endl;
            lua_pop(luaState, 1);
        }
    } else {
        lua_pop(luaState, 1);
    }
}

void MenuManager::renderMenuBackground(Renderer& renderer, const Menu& menu) {
}

void MenuManager::renderMenuItems(Renderer& renderer, const Menu& menu) {
}

bool MenuManager::isAnyMenuVisible() const {
    return !visibleMenuStack.empty();
}

std::vector<std::string> MenuManager::getVisibleMenus() const {
    return visibleMenuStack;
}

void MenuManager::bindToLua(lua_State* L) {
    if (!L) {
        return;
    }
    
    luaState = L;
    
    lua_newtable(L);
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        int type = luaL_optinteger(L, 2, static_cast<int>(MenuType::CUSTOM));
        MenuManager::getInstance().createMenu(menuId, static_cast<MenuType>(type));
        return 0;
    });
    lua_setfield(L, -2, "createMenu");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        MenuManager::getInstance().showMenu(menuId);
        return 0;
    });
    lua_setfield(L, -2, "showMenu");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        MenuManager::getInstance().hideMenu(menuId);
        return 0;
    });
    lua_setfield(L, -2, "hideMenu");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        MenuManager::getInstance().toggleMenu(menuId);
        return 0;
    });
    lua_setfield(L, -2, "toggleMenu");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        bool visible = MenuManager::getInstance().isMenuVisible(menuId);
        lua_pushboolean(L, visible);
        return 1;
    });
    lua_setfield(L, -2, "isMenuVisible");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        const char* itemId = luaL_checkstring(L, 2);
        const char* text = luaL_checkstring(L, 3);
        const char* action = luaL_optstring(L, 4, "");
        
        MenuItem item;
        item.id = itemId;
        item.text = text;
        item.action = action;
        
        if (lua_istable(L, 5)) {
            lua_getfield(L, 5, "x");
            lua_getfield(L, 5, "y");
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                item.position.x = lua_tonumber(L, -2);
                item.position.y = lua_tonumber(L, -1);
            }
            lua_pop(L, 2);
        }
        
        MenuManager::getInstance().addMenuItem(menuId, item);
        return 0;
    });
    lua_setfield(L, -2, "addMenuItem");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* menuId = luaL_checkstring(L, 1);
        Menu* menu = MenuManager::getInstance().getMenu(menuId);
        if (!menu) {
            return 0;
        }
        
        if (lua_isboolean(L, 2)) {
            menu->pauseGame = lua_toboolean(L, 2);
        }
        
        if (lua_isstring(L, 3)) {
            menu->onShowCallback = lua_tostring(L, 3);
        }
        if (lua_isstring(L, 4)) {
            menu->onHideCallback = lua_tostring(L, 4);
        }
        if (lua_isstring(L, 5)) {
            menu->onUpdateCallback = lua_tostring(L, 5);
        }
        
        return 0;
    });
    lua_setfield(L, -2, "setMenuProperties");
    
    lua_pushcfunction(L, [](lua_State* L) -> int {
        bool paused = MenuManager::getInstance().isGamePaused();
        lua_pushboolean(L, paused);
        return 1;
    });
    lua_setfield(L, -2, "isGamePaused");
    
    lua_setglobal(L, "MenuManager");
    
    std::cout << "MenuManager: Lua bindings registered" << std::endl;
}

} // namespace GameEngine

