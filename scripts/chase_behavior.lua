-- Chase Enemy Behavior Script
-- This enemy tries to chase the player (simplified version)

local startTime = 0
local lastPlayerX = 0
local lastPlayerZ = 0

function start()
    startTime = getTime()
end

function update(deltaTime)
    local currentTime = getTime()
    local elapsedTime = currentTime - startTime
    
    -- Simple chase behavior - move towards center where player often is
    local centerX = math.sin(elapsedTime * 0.5) * 3  -- Approximate player position
    local centerZ = math.cos(elapsedTime * 0.3) * 2
    
    -- Move towards the center
    local currentX = 5 - elapsedTime * 0.5  -- Start at x=5, move left
    local currentY = 1 + math.sin(elapsedTime * 2) * 0.2
    local currentZ = math.sin(elapsedTime * 0.6) * 1.5
    
    -- Keep within bounds
    if currentX < -3 then
        currentX = 5  -- Reset position
    end
    
    setPosition(currentX, currentY, currentZ)
end

function render()
    -- Optional: custom rendering logic
end

function destroy()
    -- Cleanup if needed
end
