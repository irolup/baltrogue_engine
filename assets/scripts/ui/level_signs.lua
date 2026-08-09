local M = {}

local SIGNS = {
    {
        sign = "ControlsSign",
        textNode = "Text",
        pc = "Move: W / S",
        vita = "Move: L Stick \n  Up / Down",
    },
    {
        sign = "ControlsLookSign",
        textNode = "Text (2)",
        pc = "Look: A / D \n  / Mouse",
        vita = "Look: L Stick \n  / R Stick",
    },
}

local SIGN_SCENE = "level_1"

local function currentSceneName()
    if scene and scene.getName then
        return scene.getName()
    end
    return nil
end

function M.apply()
    if currentSceneName() ~= SIGN_SCENE then
        return false
    end
    if not (renderer and renderer.setText) then
        return false
    end

    for i = 1, #SIGNS do
        local entry = SIGNS[i]
        renderer.setText(entry.textNode, _VITA_BUILD and entry.vita or entry.pc)
    end
    return true
end

return M
