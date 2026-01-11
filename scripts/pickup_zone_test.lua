-- Pickup Zone Test Script
-- Demonstrates collision detection without physics interference

local startTime = 0
local playerZone = nil
local lemonZone = nil
local score = 0
local collectedLemons = 0

function start()
    startTime = getTime()
    print("Pickup Zone Test Script Started")
    
    -- Create pickup zones
    playerZone = pickupZone.createZone("player_zone", 0, 1, 0, 2, 2, 2)
    lemonZone = pickupZone.createZone("lemon_zone", 0, 2, 0, 1, 1, 1)
    
    print("Created pickup zones:")
    print("  Player zone: (0, 1, 0) size (2, 2, 2)")
    print("  Lemon zone: (0, 2, 0) size (1, 1, 1)")
end

function update(deltaTime)
    local currentTime = getTime()
    local elapsedTime = currentTime - startTime
    
    -- Simulate player movement (simple sine wave pattern)
    local playerX = math.sin(elapsedTime * 0.5) * 3
    local playerY = 1 + math.sin(elapsedTime * 2) * 0.5
    local playerZ = math.cos(elapsedTime * 0.3) * 2
    
    -- Update player zone position
    if playerZone then
        playerZone.x = playerX
        playerZone.y = playerY
        playerZone.z = playerZ
    end
    
    -- Check for collisions between player and lemon
    if playerZone and lemonZone then
        -- Simple distance-based collision detection
        local distance = math.sqrt(
            (playerZone.x - lemonZone.x)^2 + 
            (playerZone.y - lemonZone.y)^2 + 
            (playerZone.z - lemonZone.z)^2
        )
        
        -- If player is close enough to lemon, collect it
        if distance < 1.5 and collectedLemons == 0 then
            collectedLemons = 1
            score = score + 10
            print("LEMON COLLECTED! Score: " .. score)
            
            -- Move lemon to a new random position
            lemonZone.x = math.random(-5, 5)
            lemonZone.y = 2 + math.random(-1, 1)
            lemonZone.z = math.random(-5, 5)
            
            print("New lemon position: (" .. lemonZone.x .. ", " .. lemonZone.y .. ", " .. lemonZone.z .. ")")
        end
    end
    
    -- Print status every 3 seconds
    if math.floor(elapsedTime) % 3 == 0 and math.floor(elapsedTime * 10) % 10 == 0 then
        print("Status - Player: (" .. string.format("%.1f", playerX) .. ", " .. string.format("%.1f", playerY) .. ", " .. string.format("%.1f", playerZ) .. ") Score: " .. score)
    end
end

function render()
    -- Optional: custom rendering logic
end

function destroy()
    print("Pickup Zone Test Script Ended")
end
