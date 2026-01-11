-- Obstacle Behavior Script
-- Static obstacle that doesn't move much

local startTime = 0

function start()
    startTime = getTime()
end

function update(deltaTime)
    local currentTime = getTime()
    local elapsedTime = currentTime - startTime
    
    -- Obstacle: mostly static with slight movement
    local x = -2
    local y = 0.5 + math.sin(elapsedTime * 0.5) * 0.1  -- Very slight movement
    local z = -2
    
    setPosition(x, y, z)
    
    -- Very slow rotation
    setRotation(0, elapsedTime * 10, 0)
end

function render()
    -- Optional: custom rendering logic
end

function destroy()
    -- Cleanup if needed
end
