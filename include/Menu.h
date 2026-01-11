#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>
#include <functional>
#include "Platform.h"
#include "BitmapFont.h"

enum MenuState {
    MAIN_MENU,
    GAME_SELECT,
    SETTINGS,
    CREDITS,
    IN_GAME
};

enum MenuAction {
    NO_ACTION,
    START_GAME,
    SELECT_MINIGAME,
    OPEN_SETTINGS,
    OPEN_CREDITS,
    BACK_TO_MENU,
    RESTART_GAME,
    RESUME_GAME,
    QUIT_GAME
};

struct MenuItem {
    std::string text;
    MenuAction action;
    int minigameIndex; // -1 if not a minigame selection
    bool isSelectable;
    
    MenuItem(const std::string& t, MenuAction a, int mgIndex = -1, bool selectable = true)
        : text(t), action(a), minigameIndex(mgIndex), isSelectable(selectable) {}
};

class Menu {
public:
    Menu();
    ~Menu();
    
    // Core methods
    bool init();
    void update(float deltaTime);
    void draw();
    void shutdown();
    
    // Input handling
    MenuAction processInput(const SceCtrlData& pad);
    
    // Menu navigation
    void setState(MenuState state);
    MenuState getState() const { return currentState; }
    
    // Menu management
    void addMinigame(const std::string& name);
    void clearMinigames();
    
    // Getters
    int getSelectedIndex() const { return selectedIndex; }
    MenuAction getLastAction() const { return lastAction; }
    BitmapFont& getFont() { return font; }
    
private:
    // Menu state
    MenuState currentState;
    int selectedIndex;
    MenuAction lastAction;
    
    // Menu items for different states
    std::vector<MenuItem> mainMenuItems;
    std::vector<MenuItem> gameSelectItems;
    std::vector<MenuItem> settingsItems;
    std::vector<MenuItem> creditsItems;
    
    // UI properties
    float menuScale;
    
    // Font system
    BitmapFont font;
    
    // Helper methods
    void initMainMenu();
    void initGameSelect();
    void initSettings();
    void initCredits();
    
    void drawMainMenu();
    void drawGameSelect();
    void drawSettings();
    void drawCredits();
    
    void drawMenuItem(const MenuItem& item, int index, bool isSelected);
    void drawText(const std::string& text, float x, float y, float scale, bool centered = true);
    
    // Input helpers
    void handleMenuNavigation(const SceCtrlData& pad);
    void handleMenuSelection(const SceCtrlData& pad);
};

#endif // MENU_H
