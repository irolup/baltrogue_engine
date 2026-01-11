-- Player Behavior Script
-- The player follows the camera position for collision detection

local startTime = 0

function start()
    startTime = getTime()
end

function update(deltaTime)
    local currentTime = getTime()
    local elapsedTime = currentTime - startTime
    
    -- Player model stays in fixed position behind camera
    -- No need to move it to camera position anymore
end

function render()
    -- Optional: custom rendering logic
end

function destroy()
    -- Cleanup if needed
end
