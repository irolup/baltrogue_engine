-- Reusable menu option selector.
local M = {}

local function hasFn(f)
    return type(f) == "function"
end

function M.create(def)
    return {
        selectorNode = def.selectorNode,
        optionNodes = def.optionNodes,
        optionYs = def.optionYs,
        selectorX = def.selectorX or -8.5,
        selectedIndex = 0,
        visible = false,
        inputState = {
            lastUp = false,
            lastDown = false,
            lastConfirm = false,
        },
    }
end

local function setScreenSpace(nodeName)
    if renderer and renderer.setTextRenderMode and nodeName then
        renderer.setTextRenderMode(nodeName, 1)
    end
end

function M.show(menu)
    menu.visible = true
    menu.selectedIndex = 0

    for i = 1, #menu.optionNodes do
        local nodeName = menu.optionNodes[i]
        if hasFn(setNodeVisible) then
            setNodeVisible(nodeName, true)
        end
        setScreenSpace(nodeName)
    end

    if hasFn(setNodeVisible) then
        setNodeVisible(menu.selectorNode, true)
    end
    setScreenSpace(menu.selectorNode)
    M.updateSelectorPosition(menu)
end

function M.hide(menu)
    menu.visible = false

    if hasFn(setNodeVisible) then
        setNodeVisible(menu.selectorNode, false)
        for i = 1, #menu.optionNodes do
            setNodeVisible(menu.optionNodes[i], false)
        end
    end
end

function M.updateSelectorPosition(menu)
    if not menu.visible then return end

    local y = menu.optionYs[menu.selectedIndex + 1] or 0.0
    if hasFn(setNodeLocalPosition) then
        setNodeLocalPosition(menu.selectorNode, menu.selectorX, y, 0.0)
    end
    if hasFn(setNodeRotation) then
        setNodeRotation(menu.selectorNode, 0, 0, 0)
    end
    setScreenSpace(menu.selectorNode)
end

function M.update(menu, onConfirm)
    if not menu.visible then return end

    local upPressed = input and input.isActionPressed and input.isActionPressed("menu_up") or false
    local downPressed = input and input.isActionPressed and input.isActionPressed("menu_down") or false
    local confirmPressed = input and input.isActionPressed and input.isActionPressed("menu_confirm") or false

    local count = #menu.optionNodes
    if count == 0 then return end

    if upPressed and not menu.inputState.lastUp then
        menu.selectedIndex = menu.selectedIndex - 1
        if menu.selectedIndex < 0 then
            menu.selectedIndex = count - 1
        end
        M.updateSelectorPosition(menu)
    end

    if downPressed and not menu.inputState.lastDown then
        menu.selectedIndex = (menu.selectedIndex + 1) % count
        M.updateSelectorPosition(menu)
    end

    if confirmPressed and not menu.inputState.lastConfirm then
        if onConfirm then
            onConfirm(menu.selectedIndex)
        end
    end

    menu.inputState.lastUp = upPressed
    menu.inputState.lastDown = downPressed
    menu.inputState.lastConfirm = confirmPressed
end

return M
