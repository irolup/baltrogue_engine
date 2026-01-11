-- Patrol Enemy Behavior Script
-- Moves back and forth in a patrol pattern

local startTime = 0

function start()
    startTime = getTime()
end

function update(deltaTime)
    local currentTime = getTime()
    local elapsedTime = currentTime - startTime
    
    -- Patrol movement: back and forth
    local x = -5 + math.sin(elapsedTime * 0.8) * 4
    local y = 1 + math.sin(elapsedTime * 3) * 0.3
    local z = 0
    
    setPosition(x, y, z)
end

function render()
    -- Optional: custom rendering logic
end

function destroy()
    -- Cleanup if needed
end
