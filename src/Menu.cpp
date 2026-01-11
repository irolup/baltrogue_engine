#include "Menu.h"
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Menu::Menu() 
    : currentState(MAIN_MENU)
    , selectedIndex(0)
    , lastAction(NO_ACTION)
    , menuScale(1.0f) {
}

Menu::~Menu() {
    shutdown();
}

bool Menu::init() {
    // Font is always loaded since we use the built-in fallback font
    initMainMenu();
    initGameSelect();
    initSettings();
    initCredits();
    return true;
}

void Menu::update(float deltaTime) {

}

void Menu::draw() {
    switch (currentState) {
        case MAIN_MENU:
            drawMainMenu();
            break;
        case GAME_SELECT:
            drawGameSelect();
            break;
        case SETTINGS:
            drawSettings();
            break;
        case CREDITS:
            drawCredits();
            break;
        default:
            break;
    }
}

void Menu::shutdown() {
    mainMenuItems.clear();
    gameSelectItems.clear();
    settingsItems.clear();
    creditsItems.clear();
}

MenuAction Menu::processInput(const SceCtrlData& pad) {
    lastAction = NO_ACTION;
    
    handleMenuNavigation(pad);
    handleMenuSelection(pad);
    
    return lastAction;
}

void Menu::setState(MenuState state) {
    currentState = state;
    selectedIndex = 0;
}

void Menu::addMinigame(const std::string& name) {
    gameSelectItems.push_back(MenuItem(name, SELECT_MINIGAME, gameSelectItems.size()));
}

void Menu::clearMinigames() {
    gameSelectItems.clear();
}

void Menu::initMainMenu() {
    mainMenuItems.clear();
    mainMenuItems.push_back(MenuItem("Play Game", START_GAME));
    mainMenuItems.push_back(MenuItem("Settings", OPEN_SETTINGS));
    mainMenuItems.push_back(MenuItem("Credits", OPEN_CREDITS));
    mainMenuItems.push_back(MenuItem("Quit", QUIT_GAME));
}

void Menu::initGameSelect() {
    gameSelectItems.clear();
    gameSelectItems.push_back(MenuItem("Back to Menu", BACK_TO_MENU));
}

void Menu::initSettings() {
    settingsItems.clear();
    settingsItems.push_back(MenuItem("Back to Menu", BACK_TO_MENU));
}

void Menu::initCredits() {
    creditsItems.clear();
    creditsItems.push_back(MenuItem("Back to Menu", BACK_TO_MENU));
}

void Menu::drawMainMenu() {
    drawText("3D Minigame Collection", 960.0f, 200.0f, 3.0f, true);
    
    // Safety check to prevent crash
    if (mainMenuItems.empty()) {
        return;
    }
    
    // Ensure selectedIndex is within bounds
    if (selectedIndex >= mainMenuItems.size()) {
        selectedIndex = 0;
    }
    
    for (size_t i = 0; i < mainMenuItems.size(); ++i) {
        bool isSelected = (i == selectedIndex);
        drawMenuItem(mainMenuItems[i], i, isSelected);
    }
}

void Menu::drawGameSelect() {
    drawText("Select Minigame", 960.0f, 150.0f, 2.2f, true);
    
    for (size_t i = 0; i < gameSelectItems.size(); ++i) {
        bool isSelected = (i == selectedIndex);
        drawMenuItem(gameSelectItems[i], i, isSelected);
    }
}

void Menu::drawSettings() {
    drawText("Settings", 960.0f, 200.0f, 2.2f, true);
    drawText("Settings menu coming soon...", 960.0f, 400.0f, 1.0f, true);
    
    for (size_t i = 0; i < settingsItems.size(); ++i) {
        bool isSelected = (i == selectedIndex);
        drawMenuItem(settingsItems[i], i, isSelected);
    }
}

void Menu::drawCredits() {
    drawText("Credits", 960.0f, 200.0f, 2.2f, true);
    drawText("Created with VitaGL and C++", 960.0f, 350.0f, 1.0f, true);
    drawText("A collection of fun 3D minigames", 960.0f, 400.0f, 1.0f, true);
    
    // Safety check to prevent crash
    if (creditsItems.empty()) {
        return;
    }
    
    // Ensure selectedIndex is within bounds
    if (selectedIndex >= creditsItems.size()) {
        selectedIndex = 0;
    }
    
    for (size_t i = 0; i < creditsItems.size(); ++i) {
        bool isSelected = (i == selectedIndex);
        drawMenuItem(creditsItems[i], i, isSelected);
    }
}

void Menu::drawMenuItem(const MenuItem& item, int index, bool isSelected) {
    float y = 400.0f + index * 100.0f; // Increased spacing for larger text
    float scale = isSelected ? 1.2f : 1.0f;
    
            // Draw the text first (behind the selector)
        drawText(item.text, 960.0f, y, scale * 2.0f, true); // Center the text with much larger size
        
#ifndef LINUX_BUILD
        // Draw selection indicator for selected items (on top of text) - Vita only
        if (isSelected) {
            // Draw a yellow highlight around selected item
            float highlightWidth = 300.0f * scale; // Wider to accommodate much larger text
            float highlightHeight = 60.0f * scale; // Taller to accommodate much larger text
            float highlightX = 960.0f - highlightWidth / 2.0f;
            float highlightY = y - 25.0f; // Center the highlight vertically with the text
            
            // Convert to OpenGL coordinates
            float glX = (highlightX - 960.0f) / 960.0f;
            float glY = (544.0f - highlightY) / 544.0f;
            float glWidth = highlightWidth / 960.0f;
            float glHeight = highlightHeight / 544.0f;
            
            // Enable blending for transparency
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glBegin(GL_QUADS);
            glColor4f(1.0f, 1.0f, 0.0f, 0.6f);
            glVertex2f(glX, glY);
            glVertex2f(glX + glWidth, glY);
            glVertex2f(glX + glWidth, glY + glHeight);
            glVertex2f(glX, glY + glHeight);
            glEnd();
            
            // Reset color and disable blending
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glDisable(GL_BLEND);
        }
#endif
}

void Menu::drawText(const std::string& text, float x, float y, float scale, bool centered) {
    // Set text color to white for better visibility
    font.setColor(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Center the text if requested
    if (centered) {
        // Estimate text width for centering using actual font character width (8 pixels)
        float estimatedWidth = text.length() * 8.0f * scale; // 8 pixels per character (CHAR_WIDTH)
        x -= estimatedWidth / 2.0f;
    }
    
    // Draw the text using the built-in font
    font.drawText(text, x, y, scale);
}

void Menu::handleMenuNavigation(const SceCtrlData& pad) {
    // Handle D-pad navigation with button repeat protection
    static bool upPressed = false;
    static bool downPressed = false;
    static int repeatDelay = 0;
    
    if (pad.buttons & SCE_CTRL_UP) {
        if (!upPressed && selectedIndex > 0) {
            selectedIndex--;
            upPressed = true;
            repeatDelay = 0;
        }
    } else {
        upPressed = false;
    }
    
    if (pad.buttons & SCE_CTRL_DOWN) {
        if (!downPressed) {
            size_t maxItems = 0;
            switch (currentState) {
                case MAIN_MENU:
                    maxItems = mainMenuItems.size();
                    break;
                case GAME_SELECT:
                    maxItems = gameSelectItems.size();
                    break;
                case SETTINGS:
                    maxItems = settingsItems.size();
                    break;
                case CREDITS:
                    maxItems = creditsItems.size();
                    break;
                default:
                    break;
            }
            if (selectedIndex < maxItems - 1) {
                selectedIndex++;
            }
            downPressed = true;
            repeatDelay = 0;
        }
    } else {
        downPressed = false;
    }
}

void Menu::handleMenuSelection(const SceCtrlData& pad) {
    static bool crossPressed = false;
    
    if (pad.buttons & SCE_CTRL_CROSS) {
        if (!crossPressed) { // Only process once per press
            MenuItem* currentItem = nullptr;
            
            switch (currentState) {
                case MAIN_MENU:
                    if (selectedIndex < mainMenuItems.size()) {
                        currentItem = &mainMenuItems[selectedIndex];
                    }
                    break;
                case GAME_SELECT:
                    if (selectedIndex < gameSelectItems.size()) {
                        currentItem = &gameSelectItems[selectedIndex];
                    }
                    break;
                case SETTINGS:
                    if (selectedIndex < settingsItems.size()) {
                        currentItem = &settingsItems[selectedIndex];
                    }
                    break;
                case CREDITS:
                    if (selectedIndex < creditsItems.size()) {
                        currentItem = &creditsItems[selectedIndex];
                    }
                    break;
                default:
                    break;
            }
            
            if (currentItem && currentItem->isSelectable) {
                lastAction = currentItem->action;
            }
            
            crossPressed = true;
        }
    } else {
        crossPressed = false; // Reset when button is released
    }
}
