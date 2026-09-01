local levels = dofile("assets/scripts/tire_game/levels.lua")
local tireSave = dofile("assets/scripts/tire_game/tire_save.lua")
local controlsPanel = dofile("assets/scripts/ui/controls_panel.lua")

local selectedIndex = 0
local inControlsPanel = false

local inputState = {
    lastUpPressed = false,
    lastDownPressed = false,
    lastConfirmPressed = false,
    lastCancelPressed = false,
}

-- Options is hidden until there is real settings UI (G8).
local menuEntries = {
    { node = "MainMenuStart", y = 3.0, action = "start" },
    { node = "MainMenuLoad", y = -0.5, action = "load" },
    { node = "MainMenuControls", y = -4.0, action = "controls" },
    { node = "MainMenuQuit", y = -7.0, action = "quit" },
}

local hiddenMenuNodes = {
    "MainMenuOptions",
}

local selectorNodeName = "MainMenuSelector"

local function updateSelectorPosition()
    local entry = menuEntries[selectedIndex + 1]
    if not entry then
        return
    end
    setNodePosition(selectorNodeName, -10.0, entry.y, 0.0)
end

local function entryIndexForNode(nodeName)
    if not nodeName then
        return nil
    end
    for i = 1, #menuEntries do
        if menuEntries[i].node == nodeName then
            return i - 1
        end
    end
    return nil
end

local function updatePointerSelection()
    if not (ui and ui.hitTest and input and input.isPointerActive) then
        return
    end

    if not input.isPointerActive() then
        return
    end

    local hoveredIndex = entryIndexForNode(ui.hitTest())
    if not hoveredIndex then
        return
    end

    if hoveredIndex ~= selectedIndex then
        selectedIndex = hoveredIndex
        updateSelectorPosition()
    end

    if input.isPointerPressed() then
        handleMenuSelection()
    end
end

local function loadLevelSafe(levelId)
    if not levelId or not levels.loadLevel(levelId) then
        return levels.loadLevel("level_1")
    end
    return true
end

function start()
    for i = 1, #menuEntries do
        local entry = menuEntries[i]
        setNodeVisible(entry.node, true)
        setNodePosition(entry.node, 0.0, entry.y, 0.0)
        if renderer and renderer.setTextRenderMode then
            renderer.setTextRenderMode(entry.node, 1)
        end
    end

    for i = 1, #hiddenMenuNodes do
        setNodeVisible(hiddenMenuNodes[i], false)
    end

    updateSelectorPosition()
    setNodeVisible(selectorNodeName, true)
    if renderer and renderer.setTextRenderMode then
        renderer.setTextRenderMode(selectorNodeName, 1)
    end

    if scene and scene.preloadSceneFromFile then
        scene.preloadSceneFromFile("level_1", "assets/scenes/level_1.json")
    end
end

local function showControlsPanel()
    inControlsPanel = true
    for i = 1, #menuEntries do
        setNodeVisible(menuEntries[i].node, false)
    end
    setNodeVisible(selectorNodeName, false)
    controlsPanel.show()
end

local function hideControlsPanel()
    inControlsPanel = false
    for i = 1, #menuEntries do
        local entry = menuEntries[i]
        setNodeVisible(entry.node, true)
        setNodePosition(entry.node, 0.0, entry.y, 0.0)
    end
    setNodeVisible(selectorNodeName, true)
    updateSelectorPosition()
    controlsPanel.hide()
end

function update(deltaTime)
    if inControlsPanel then
        local cancelPressed = input and input.isActionPressed and input.isActionPressed("menu_cancel") or false
        if cancelPressed and not inputState.lastCancelPressed then
            hideControlsPanel()
        end
        inputState.lastCancelPressed = cancelPressed
        return
    end

    local upPressed = false
    local downPressed = false
    local confirmPressed = false

    if input and input.isActionPressed then
        upPressed = input.isActionPressed("menu_up")
        downPressed = input.isActionPressed("menu_down")
        confirmPressed = input.isActionPressed("menu_confirm")
    else
        if isKeyDown then
            upPressed = isKeyDown("W") or isKeyDown("UP")
            downPressed = isKeyDown("S") or isKeyDown("DOWN")
            confirmPressed = isKeyDown("ENTER") or isKeyDown("SPACE")
        end
    end

    if upPressed and not inputState.lastUpPressed then
        selectedIndex = selectedIndex - 1
        if selectedIndex < 0 then
            selectedIndex = #menuEntries - 1
        end
        updateSelectorPosition()
    end

    if downPressed and not inputState.lastDownPressed then
        selectedIndex = selectedIndex + 1
        if selectedIndex >= #menuEntries then
            selectedIndex = 0
        end
        updateSelectorPosition()
    end

    if confirmPressed and not inputState.lastConfirmPressed then
        handleMenuSelection()
    end

    updatePointerSelection()

    inputState.lastUpPressed = upPressed
    inputState.lastDownPressed = downPressed
    inputState.lastConfirmPressed = confirmPressed
end

function handleMenuSelection()
    local entry = menuEntries[selectedIndex + 1]
    if not entry then
        return
    end

    if entry.action == "start" then
        print("Main Menu: Starting new game...")
        if loadLevelSafe("level_1") then
            print("Main Menu: Game scene load requested")
        else
            print("Main Menu: ERROR - Failed to load game scene!")
        end
    elseif entry.action == "load" then
        local levelId = tireSave.getLastLevelId() or "level_1"
        print("Main Menu: Load Game -> " .. tostring(levelId))
        if loadLevelSafe(levelId) then
            print("Main Menu: Continue scene load requested")
        else
            print("Main Menu: ERROR - Failed to load saved level, falling back failed")
        end
    elseif entry.action == "controls" then
        showControlsPanel()
    elseif entry.action == "quit" then
        print("Main Menu: Quitting game...")
        if quitGame then
            quitGame()
        else
            print("Main Menu: ERROR - quitGame not available!")
        end
    end
end
