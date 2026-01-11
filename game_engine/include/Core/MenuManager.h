#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <algorithm>

namespace GameEngine {
    class Renderer;
    class SceneNode;
    class TextComponent;
}

extern "C" {
    #include <lua.h>
}

namespace GameEngine {

enum class MenuType {
    MAIN_MENU,
    PAUSE_MENU,
    SETTINGS_MENU,
    CUSTOM
};

enum class MenuState {
    HIDDEN,
    VISIBLE,
    TRANSITIONING
};

struct MenuItem {
    std::string id;
    std::string text;
    std::string action;
    bool enabled;
    glm::vec2 position;
    float fontSize;
    glm::vec4 color;
    
    MenuItem() : enabled(true), position(0.0f), fontSize(24.0f), color(1.0f, 1.0f, 1.0f, 1.0f) {}
};

struct Menu {
    std::string id;
    MenuType type;
    MenuState state;
    std::vector<MenuItem> items;
    int selectedIndex;
    bool pauseGame;
    std::string onShowCallback;
    std::string onHideCallback;
    std::string onUpdateCallback;
    
    bool showBackground;
    glm::vec4 backgroundColor;
    
    Menu() 
        : type(MenuType::CUSTOM)
        , state(MenuState::HIDDEN)
        , selectedIndex(0)
        , pauseGame(true)
        , showBackground(true)
        , backgroundColor(0.0f, 0.0f, 0.0f, 0.7f)
    {}
};

class MenuManager {
public:
    static MenuManager& getInstance();
    
    bool initialize();
    void shutdown();
    ~MenuManager();
    
    std::string createMenu(const std::string& menuId, MenuType type = MenuType::CUSTOM);
    bool removeMenu(const std::string& menuId);
    Menu* getMenu(const std::string& menuId);
    
    void showMenu(const std::string& menuId);
    void hideMenu(const std::string& menuId);
    void toggleMenu(const std::string& menuId);
    bool isMenuVisible(const std::string& menuId) const;
    
    void addMenuItem(const std::string& menuId, const MenuItem& item);
    void removeMenuItem(const std::string& menuId, const std::string& itemId);
    void clearMenuItems(const std::string& menuId);
    
    void selectNextItem(const std::string& menuId);
    void selectPreviousItem(const std::string& menuId);
    void selectItem(const std::string& menuId, int index);
    void activateSelectedItem(const std::string& menuId);
    
    void setPauseOnMenu(const std::string& menuId, bool pause);
    bool isGamePaused() const { return gamePaused; }
    
    void update(float deltaTime);
    void render(Renderer& renderer);
    
    void bindToLua(lua_State* L);
    
    bool isAnyMenuVisible() const;
    std::vector<std::string> getVisibleMenus() const;
    
private:
    MenuManager();
    
    static std::unique_ptr<MenuManager> instance;
    
    std::unordered_map<std::string, Menu> menus;
    std::vector<std::string> visibleMenuStack;
    
    bool gamePaused;
    float savedTimeScale;
    
    void handleMenuInput();
    
    void callMenuCallback(const std::string& callbackName, const std::string& menuId);
    
    void renderMenuBackground(Renderer& renderer, const Menu& menu);
    void renderMenuItems(Renderer& renderer, const Menu& menu);
    
    lua_State* luaState;
    bool initialized;
};

} // namespace GameEngine

#endif // MENU_MANAGER_H

