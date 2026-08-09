local BUTTON_ROOT = "piston_button"
local PRESS_DEPTH = 0.5
local DOWN_DURATION = 0.08
local UP_DURATION = 0.12

local WALL_ROOT = "pushing_wall"
local WALL_DELTA_X = -6.0
local WALL_MOVE_DURATION = 1.5
-- Time to stay in extended position before moving back
local WALL_HOLD_AT_EXTENDED = 0.75

local baseX, baseY, baseZ = 0, 0, 0
local btnState = "idle" -- "idle", "pressing", "releasing"
local btnT = 0 -- time to move the button

local wallBaseX, wallBaseY, wallBaseZ = 0, 0, 0
-- "idle", "moving_out", "wait_extended", "moving_in"
local wallState = "idle"
local wallT = 0 -- time to move the wall

local function clamp01(t)
    if t < 0 then return 0 end
    if t > 1 then return 1 end
    return t
end

function start()
    if getNodeLocalPosition then
        baseX, baseY, baseZ = getNodeLocalPosition(BUTTON_ROOT)
        wallBaseX, wallBaseY, wallBaseZ = getNodeLocalPosition(WALL_ROOT)
    end
end

function interact()
    if btnState ~= "idle" then return end
    btnState = "pressing"
    btnT = 0

    -- Only trigger the wall once per cycle: only when it's at home
    if wallState == "idle" then
        wallState = "moving_out"
        wallT = 0
    end
end

function update(deltaTime)
    if not setNodeLocalPosition then return end

    -- Button piston
    if btnState == "pressing" then
        btnT = btnT + deltaTime
        local p = clamp01(btnT / DOWN_DURATION)
        local z = baseZ - PRESS_DEPTH * p
        setNodeLocalPosition(BUTTON_ROOT, baseX, baseY, z)
        if p >= 1 then
            btnState = "releasing"
            btnT = 0
        end
    elseif btnState == "releasing" then
        btnT = btnT + deltaTime
        local p = clamp01(btnT / UP_DURATION)
        local z = baseZ - PRESS_DEPTH * (1 - p)
        setNodeLocalPosition(BUTTON_ROOT, baseX, baseY, z)
        if p >= 1 then
            setNodeLocalPosition(BUTTON_ROOT, baseX, baseY, baseZ)
            btnState = "idle"
        end
    end

    -- Wall: out -> hold -> auto in
    if wallState == "moving_out" then
        wallT = wallT + deltaTime
        local p = clamp01(wallT / WALL_MOVE_DURATION)
        local x = wallBaseX + WALL_DELTA_X * p -- move the wall in the x axis
        setNodeLocalPosition(WALL_ROOT, x, wallBaseY, wallBaseZ) -- set the new position of the wall
        if p >= 1 then
            setNodeLocalPosition(WALL_ROOT, wallBaseX + WALL_DELTA_X, wallBaseY, wallBaseZ) -- set the wall to the extended position
            wallState = "wait_extended"
            wallT = 0 -- reset the time to move the wall
        end
    elseif wallState == "wait_extended" then
        wallT = wallT + deltaTime
        if wallT >= WALL_HOLD_AT_EXTENDED then
            wallState = "moving_in"
            wallT = 0 -- reset the time to move the wall
        end
    elseif wallState == "moving_in" then
        wallT = wallT + deltaTime
        local p = clamp01(wallT / WALL_MOVE_DURATION)
        local x = (wallBaseX + WALL_DELTA_X) + (-WALL_DELTA_X) * p -- move the wall in the x axis
        setNodeLocalPosition(WALL_ROOT, x, wallBaseY, wallBaseZ) -- set the new position of the wall
        if p >= 1 then
            setNodeLocalPosition(WALL_ROOT, wallBaseX, wallBaseY, wallBaseZ) -- set the wall to the home position
            wallState = "idle"
        end
    end
end
