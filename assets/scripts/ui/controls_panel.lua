
local M = {}

M.NODE_TITLE = "ControlsPanelTitle"
M.NODE_LINES = {
    "ControlsPanelLine1",
    "ControlsPanelLine2",
    "ControlsPanelLine3",
    "ControlsPanelLine4",
    "ControlsPanelLine5",
}
M.NODE_BACK = "ControlsPanelBack"

local PC_LINES = {
    "Move Forward / Backward   -   W / S  or  Up / Down",
    "Steer Left / Right   -   A / D  or  Left / Right",
    "Drift   -   Left Shift / Left Ctrl",
    "Camera   -   Mouse",
    "Pause   -   Esc",
}

local VITA_LINES = {
    "Move Forward / Backward   -   Left Stick (Up / Down)",
    "Steer Left / Right   -   Left Stick (Left / Right)",
    "Drift   -   L Button",
    "Camera   -   Right Stick",
    "Pause   -   START",
}

local function setText(nodeName, text)
    if renderer and renderer.setText and nodeName then
        renderer.setText(nodeName, text)
    end
end

local function setScreenSpace(nodeName)
    if renderer and renderer.setTextRenderMode and nodeName then
        renderer.setTextRenderMode(nodeName, 1)
    end
end

local function setVisible(nodeName, visible)
    if setNodeVisible and nodeName then
        setNodeVisible(nodeName, visible)
    end
    if visible then
        setScreenSpace(nodeName)
    end
end

function M.show()
    local lines = _VITA_BUILD and VITA_LINES or PC_LINES

    setText(M.NODE_TITLE, "Controls")
    setVisible(M.NODE_TITLE, true)

    for i = 1, #M.NODE_LINES do
        setText(M.NODE_LINES[i], lines[i])
        setVisible(M.NODE_LINES[i], true)
    end

    setText(M.NODE_BACK, _VITA_BUILD and "Press START to go back" or "Press Esc to go back")
    setVisible(M.NODE_BACK, true)
end

function M.hide()
    setVisible(M.NODE_TITLE, false)
    for i = 1, #M.NODE_LINES do
        setVisible(M.NODE_LINES[i], false)
    end
    setVisible(M.NODE_BACK, false)
end

return M
