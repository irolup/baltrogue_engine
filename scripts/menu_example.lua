-- Example menu system implementation in Lua
-- This demonstrates how to create and control menus using the MenuManager

-- Menu type constants (for reference)
-- 0 = MAIN_MENU
-- 1 = PAUSE_MENU
-- 2 = SETTINGS_MENU
-- 3 = CUSTOM

-- Create main menu
function createMainMenu()
    -- Create the menu
    MenuManager.createMenu("main_menu", 0)  -- 0 = MAIN_MENU
    
    -- Add menu items
    MenuManager.addMenuItem("main_menu", "start_game", "Start Game", "onStartGame")
    MenuManager.addMenuItem("main_menu", "settings", "Settings", "onSettings")
    MenuManager.addMenuItem("main_menu", "quit", "Quit", "onQuit")
    
    -- Set menu properties
    MenuManager.setMenuProperties("main_menu", true, "onMainMenuShow", "onMainMenuHide", "onMainMenuUpdate")
    
    -- Position menu items (optional - uses default positions if not set)
    -- Menu items are positioned automatically, but you can customize this
end

-- Create pause menu
function createPauseMenu()
    MenuManager.createMenu("pause_menu", 1)  -- 1 = PAUSE_MENU
    
    MenuManager.addMenuItem("pause_menu", "resume", "Resume", "onResume")
    MenuManager.addMenuItem("pause_menu", "restart", "Restart", "onRestart")
    MenuManager.addMenuItem("pause_menu", "main_menu", "Main Menu", "onReturnToMainMenu")
    
    MenuManager.setMenuProperties("pause_menu", true, "", "", "")
end

-- Menu callbacks
function onStartGame(menuId, itemId)
    print("Starting game from menu: " .. menuId .. ", item: " .. itemId)
    MenuManager.hideMenu("main_menu")
    -- Start your game here
end

function onSettings(menuId, itemId)
    print("Opening settings from menu: " .. menuId .. ", item: " .. itemId)
    -- Show settings menu
end

function onQuit(menuId, itemId)
    print("Quitting from menu: " .. menuId .. ", item: " .. itemId)
    -- Exit game
end

function onResume(menuId, itemId)
    print("Resuming game from pause menu")
    MenuManager.hideMenu("pause_menu")
end

function onRestart(menuId, itemId)
    print("Restarting game")
    MenuManager.hideMenu("pause_menu")
    -- Restart game logic here
end

function onReturnToMainMenu(menuId, itemId)
    print("Returning to main menu")
    MenuManager.hideMenu("pause_menu")
    MenuManager.showMenu("main_menu")
end

-- Menu lifecycle callbacks
function onMainMenuShow(menuId)
    print("Main menu shown: " .. menuId)
end

function onMainMenuHide(menuId)
    print("Main menu hidden: " .. menuId)
end

function onMainMenuUpdate(menuId)
    -- Called every frame while menu is visible
    -- You can update menu animations, effects, etc. here
end

-- Initialize menus
function start()
    createMainMenu()
    createPauseMenu()
    
    -- Show main menu on start
    MenuManager.showMenu("main_menu")
end

-- Example: Toggle pause menu (call this from your game script)
function togglePauseMenu()
    if MenuManager.isMenuVisible("pause_menu") then
        MenuManager.hideMenu("pause_menu")
    else
        MenuManager.showMenu("pause_menu")
    end
end

