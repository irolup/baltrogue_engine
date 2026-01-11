local selectedIndex = 0

local inputState = {
    lastUpPressed = false,
    lastDownPressed = false,
    lastConfirmPressed = false
}

local menuPositions = {
    {x = 0.0, y = 3.0, z = 0.0},
    {x = 0.0, y = 1.0, z = 0.0},
    {x = 0.0, y = -1.0, z = 0.0},
    {x = 0.0, y = -3.0, z = 0.0}
}

local menuNodeNames = {
    "MainMenuStart",
    "MainMenuLoad",
    "MainMenuOptions",
    "MainMenuQuit"
}
local selectorNodeName = "MainMenuSelector"

function start()
    for i = 1, #menuNodeNames do
        setNodeVisible(menuNodeNames[i], true)
        if renderer and renderer.setTextRenderMode then
            renderer.setTextRenderMode(menuNodeNames[i], 1)
        end
    end
    
    updateSelectorPosition()
    setNodeVisible(selectorNodeName, true)
    if renderer and renderer.setTextRenderMode then
        renderer.setTextRenderMode(selectorNodeName, 1)
    end
end

function updateSelectorPosition()
    local selPos = menuPositions[selectedIndex + 1]
    setNodePosition(selectorNodeName, selPos.x - 10.0, selPos.y, selPos.z)
end

function update(deltaTime)
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
            selectedIndex = #menuNodeNames - 1
        end
        updateSelectorPosition()
    end
    
    if downPressed and not inputState.lastDownPressed then
        selectedIndex = selectedIndex + 1
        if selectedIndex >= #menuNodeNames then
            selectedIndex = 0
        end
        updateSelectorPosition()
    end
    
    if confirmPressed and not inputState.lastConfirmPressed then
        handleMenuSelection()
    end
    
    inputState.lastUpPressed = upPressed
    inputState.lastDownPressed = downPressed
    inputState.lastConfirmPressed = confirmPressed
end

function handleMenuSelection()
    if selectedIndex == 0 then
        print("Main Menu: Starting new game...")
        if scene and scene.loadSceneFromFile then
            if scene.loadSceneFromFile("Game Scene", "assets/scenes/first_game_demo.json") then
                print("Main Menu: Game scene loaded from JSON")
            else
                print("Main Menu: ERROR - Failed to load game scene from JSON!")
            end
        elseif scene and scene.loadScene then
            scene.loadScene("Game Scene")
        else
            print("Main Menu: ERROR - scene loading not available!")
        end
    elseif selectedIndex == 1 then
        print("Main Menu: Load Game (not implemented yet)")
    elseif selectedIndex == 2 then
        print("Main Menu: Options (not implemented yet)")
    elseif selectedIndex == 3 then
        print("Main Menu: Quitting game...")
        if quitGame then
            quitGame()
        else
            print("Main Menu: ERROR - quitGame not available!")
        end
    end
end

