-- Pause Menu Script
-- Simple pause menu using text nodes from the scene tree
-- Shows/hides menu and handles navigation with arrow keys

local controlsPanel = dofile("assets/scripts/ui/controls_panel.lua")

local isPaused = false
local lastEscapePressed = false
local selectedIndex = 0
local menuInitialized = false
local inControlsPanel = false
local lastCancelPressed = false

local inputState = {
    lastUpPressed = false,
    lastDownPressed = false,
    lastConfirmPressed = false
}

local menuPositions = {
    {x = 0.0, y = 5.0, z = 0.0},
    {x = 0.0, y = 1.0, z = 0.0},
    {x = 0.0, y = -3.0, z = 0.0},
    {x = 0.0, y = -7.0, z = 0.0}
}

local menuNodeNames = {
    "PauseMenuResume",
    "PauseMenuOptions",
    "PauseMenuControls",
    "PauseMenuReturn"
}
local selectorNodeName = "PauseMenuSelector"
local tireHudNodeName = "TireHud"

function start()
    for i = 1, #menuNodeNames do
        if isNodeVisible(menuNodeNames[i]) then
            setNodeVisible(menuNodeNames[i], false)
        end
    end
    if isNodeVisible(selectorNodeName) then
        setNodeVisible(selectorNodeName, false)
    end
    
    isPaused = false
    menuInitialized = false
    inControlsPanel = false
    controlsPanel.hide()
end

function showMenu()
    if isPaused then
        return
    end
    
    isPaused = true
    
    if input and input.setMouseCapture then
        input.setMouseCapture(false)
    end
    
    if setGamePaused then
        setGamePaused(true)
    end
    
    for i = 1, #menuNodeNames do
        local nodeName = menuNodeNames[i]
        setNodeVisible(nodeName, true)
        if renderer and renderer.setTextRenderMode then
            renderer.setTextRenderMode(nodeName, 1)
        end
    end
    
    setNodeVisible(selectorNodeName, true)
    if renderer and renderer.setTextRenderMode then
        renderer.setTextRenderMode(selectorNodeName, 1)
    end
    updateSelectorPosition()

    setNodeVisible(tireHudNodeName, false)
end

function hideMenu()
    if not isPaused then
        return
    end
    
    isPaused = false
    inControlsPanel = false
    controlsPanel.hide()
    
    if setGamePaused then
        setGamePaused(false)
    end
    
    if input and input.setMouseCapture then
        input.setMouseCapture(true)
    end
    
    for i = 1, #menuNodeNames do
        setNodeVisible(menuNodeNames[i], false)
    end
    
    setNodeVisible(selectorNodeName, false)
    setNodeVisible(tireHudNodeName, true)
end

local function showControlsPanel()
    inControlsPanel = true
    for i = 1, #menuNodeNames do
        setNodeVisible(menuNodeNames[i], false)
    end
    setNodeVisible(selectorNodeName, false)
    controlsPanel.show()
end

local function hideControlsPanel()
    inControlsPanel = false
    controlsPanel.hide()
    for i = 1, #menuNodeNames do
        local nodeName = menuNodeNames[i]
        setNodeVisible(nodeName, true)
        if renderer and renderer.setTextRenderMode then
            renderer.setTextRenderMode(nodeName, 1)
        end
    end
    setNodeVisible(selectorNodeName, true)
    updateSelectorPosition()
end

function updateSelectorPosition()
    if not isPaused then
        return
    end
    
    local menuPos = menuPositions[selectedIndex + 1]
    
    if setNodeLocalPosition then
        setNodeLocalPosition(selectorNodeName, -10.0, menuPos.y, 0.0)
    end
    
    if setNodeRotation then
        setNodeRotation(selectorNodeName, 0, 0, 0)
    end
    
    if renderer and renderer.setTextRenderMode then
        renderer.setTextRenderMode(selectorNodeName, 1)
    end
end

function selectCurrentOption()
    if selectedIndex == 0 then
        hideMenu()
    elseif selectedIndex == 1 then
        -- TODO: Show options menu
    elseif selectedIndex == 2 then
        showControlsPanel()
    elseif selectedIndex == 3 then
        hideMenu()
        if setGamePaused then
            setGamePaused(false)
        end
        if scene and scene.loadSceneFromFile then
            scene.loadSceneFromFile("Main Menu", "assets/scenes/main_menu.json")
        end
    end
end

function update(deltaTime)
    if _G.tireGameUiBlocking then
        return
    end

    if not menuInitialized then
        for i = 1, #menuNodeNames do
            if isNodeVisible(menuNodeNames[i]) then
                setNodeVisible(menuNodeNames[i], false)
            end
        end
        if isNodeVisible(selectorNodeName) then
            setNodeVisible(selectorNodeName, false)
        end
        menuInitialized = true
    end
    
    local cancelPressed = input.isActionPressed("menu_cancel")

    if isPaused and inControlsPanel then
        if cancelPressed and not lastCancelPressed then
            hideControlsPanel()
        end
        lastCancelPressed = cancelPressed
        lastEscapePressed = cancelPressed
        return
    end
    lastCancelPressed = cancelPressed

    local escapePressed = cancelPressed
    
    if escapePressed and not lastEscapePressed then
        if isPaused then
            hideMenu()
        else
            showMenu()
        end
    end
    
    lastEscapePressed = escapePressed
    
    if isPaused then
        local upPressed = input.isActionPressed("menu_up")
        local downPressed = input.isActionPressed("menu_down")
        local confirmPressed = input.isActionPressed("menu_confirm")
        
        if not inputState.lastUpPressed and upPressed then
            selectedIndex = (selectedIndex - 1) % #menuNodeNames
            updateSelectorPosition()
        end
        
        if not inputState.lastDownPressed and downPressed then
            selectedIndex = (selectedIndex + 1) % #menuNodeNames
            updateSelectorPosition()
        end
        
        if not inputState.lastConfirmPressed and confirmPressed then
            selectCurrentOption()
        end
        
        inputState.lastUpPressed = upPressed
        inputState.lastDownPressed = downPressed
        inputState.lastConfirmPressed = confirmPressed
    else
        inputState.lastUpPressed = false
        inputState.lastDownPressed = false
        inputState.lastConfirmPressed = false
    end
end

function render()
end

function destroy()
end
