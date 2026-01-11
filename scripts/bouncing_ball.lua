-- Example Lua script: Bouncing Ball
-- Demonstrates physics interaction and input handling

-- Script variables
local bounceForce = 10.0
local jumpCooldown = 0.5
local lastJumpTime = 0
local ballRadius = 1.0

-- Called when the script starts
function start()
    print("Bouncing Ball script started!")
    lastJumpTime = getTime()
end

-- Called every frame
function update(deltaTime)
    local currentTime = getTime()
    
    -- Check for jump input (space key)
    if input.isKeyPressed("SPACE") then
        local timeSinceLastJump = currentTime - lastJumpTime
        
        if timeSinceLastJump >= jumpCooldown then
            -- Apply upward force
            if physics and physics.addForce then
                local jumpVector = math.vec3(0, bounceForce, 0)
                physics.addForce(jumpVector)
                print("Ball jumped!")
            end
            
            lastJumpTime = currentTime
        end
    end
    
    -- Check if ball fell below ground level (simplified check)
    -- For now, just reset periodically to demonstrate movement
    local currentTime = getTime()
    if math.floor(currentTime) % 5 == 0 and math.floor(currentTime * 10) % 10 == 0 then
        -- Reset ball position every 5 seconds
        transform.setPosition(0, 5, 0)
        print("Ball reset!")
    end
end

-- Called during rendering
function render()
    -- Draw debug text
    if renderer and renderer.drawText then
        renderer.drawText("Press SPACE to jump!", 10, 10)
        
        local cooldownText = "Jump cooldown: " .. math.max(0, jumpCooldown - (getTime() - lastJumpTime))
        renderer.drawText(cooldownText, 10, 30)
    end
end

-- Called when the script is destroyed
function destroy()
    print("Bouncing Ball script destroyed!")
end
