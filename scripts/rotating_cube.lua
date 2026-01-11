-- Example Lua script: Rotating Cube
-- This script demonstrates basic scripting functionality

-- Script variables
local rotationSpeed = 45.0  -- degrees per second
local startTime = 0

-- Called when the script starts
function start()
    print("Rotating Cube script started!")
    startTime = getTime()
end

-- Called every frame
function update(deltaTime)
    -- Rotate the object around Y axis
    local currentTime = getTime()
    local elapsedTime = currentTime - startTime
    
    -- Calculate rotation angle
    local rotationAngle = rotationSpeed * elapsedTime
    
    -- Apply rotation to the transform
    if transform and transform.setRotation then
        transform.setRotation(0, rotationAngle, 0)
    end
end

-- Called during rendering (optional)
function render()
    -- This function is called during the render phase
    -- You can use this for custom rendering logic
end

-- Called when the script is destroyed
function destroy()
    print("Rotating Cube script destroyed!")
end
