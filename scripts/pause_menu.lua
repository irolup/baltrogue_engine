-- Pause Menu Script
-- Simple pause menu using text nodes from the scene tree
-- Shows/hides menu and handles navigation with arrow keys

local isPaused = false
local lastEscapePressed = false
local selectedIndex = 0
local menuInitialized = false

local inputState = {
    lastUpPressed = false,
    lastDownPressed = false,
    lastConfirmPressed = false
}

local menuPositions = {
    {x = 0.0, y = 2.0, z = 0.0},
    {x = 0.0, y = -2.0, z = 0.0},
    {x = 0.0, y = -4.0, z = 0.0}
}

local menuNodeNames = {
    "PauseMenuResume",
    "PauseMenuOptions",
    "PauseMenuReturn"
}
local selectorNodeName = "PauseMenuSelector"

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
end

function hideMenu()
    if not isPaused then
        return
    end
    
    isPaused = false
    
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
        -- TODO: Return to main menu
    end
end

function update(deltaTime)
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
    
    local escapePressed = input.isActionPressed("menu_cancel")
    
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
            selectedIndex = (selectedIndex - 1) % 3
            updateSelectorPosition()
        end
        
        if not inputState.lastDownPressed and downPressed then
            selectedIndex = (selectedIndex + 1) % 3
            updateSelectorPosition()
        end
        
        if not inputState.lastConfirmPressed and confirmPressed then
            selectCurrentOption()
        end
        
        inputState.lastUpPressed = upPressed
        inputState.lastDownPressed = downPressed
        inputState.lastConfirmPressed = confirmPressed
        
        updateSelectorPosition()
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
