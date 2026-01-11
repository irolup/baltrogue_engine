-- Vita Camera Controller Script (Enhanced)
-- Uses configurable input actions for cross-platform compatibility

-- Script variables
local moveSpeed = 5.0
local rotateSpeed = 2.0
local cameraYaw = 0.0
local cameraPitch = 0.0
local debugCounter = 0

-- Called when the script starts
function start()
    print("Vita Camera Controller (Enhanced) script started!")
    
    -- Initialize camera angles
    cameraYaw = 0.0
    cameraPitch = 0.0
    
    -- Find the player node (parent of this camera)
    local playerNode = getParent()
    if playerNode then
        print("Found player node: " .. playerNode.getName())
    else
        print("Warning: No parent node found for camera!")
    end
    
    -- Set initial camera position relative to player
    camera.setPosition(0, 0, -4)  -- Behind player
    
    -- Print available input actions for debugging
    print("Available input actions:")
    print("- MoveHorizontal: Left stick X axis")
    print("- MoveVertical: Left stick Y axis") 
    print("- LookHorizontal: Right stick X axis")
    print("- LookVertical: Right stick Y axis")
    print("- SecondaryFire: Triangle button (reset player)")
end

-- Called every frame
function update(deltaTime)
    debugCounter = debugCounter + 1
    
    -- Handle movement with configurable analog stick actions
    local moveH = input.getActionAxis("MoveHorizontal")
    local moveV = input.getActionAxis("MoveVertical")
    
    -- Debug: Print raw input values every second
    if debugCounter % 60 == 0 then
        print(string.format("Raw input - MoveH: %.3f, MoveV: %.3f", moveH, moveV))
    end
    
    if moveH ~= 0 or moveV ~= 0 then
        -- Calculate forward and right vectors based on camera yaw
        local forwardX = math.sin(math.rad(cameraYaw))
        local forwardZ = math.cos(math.rad(cameraYaw))
        local rightX = math.cos(math.rad(cameraYaw))
        local rightZ = -math.sin(math.rad(cameraYaw))
        
        -- Apply movement to player (parent node)
        local worldMoveX = rightX * moveH * moveSpeed * deltaTime + forwardX * moveV * moveSpeed * deltaTime
        local worldMoveZ = rightZ * moveH * moveSpeed * deltaTime + forwardZ * moveV * moveSpeed * deltaTime
        
        -- Move the player node (parent of camera)
        local playerNode = getParent()
        if playerNode then
            playerNode.move(worldMoveX, 0, worldMoveZ)
        end
        
        if debugCounter % 60 == 0 then
            print(string.format("Player moved by: X=%.3f, Z=%.3f", worldMoveX, worldMoveZ))
            print(string.format("Camera yaw: %.3f, forwardX: %.3f, forwardZ: %.3f", cameraYaw, forwardX, forwardZ))
        end
    end
    
    -- Handle camera rotation with configurable analog stick actions
    local lookH = input.getActionAxis("LookHorizontal")
    local lookV = input.getActionAxis("LookVertical")
    
    if lookH ~= 0 or lookV ~= 0 then
        cameraYaw = cameraYaw + lookH * rotateSpeed * deltaTime
        cameraPitch = cameraPitch - lookV * rotateSpeed * deltaTime
        
        -- Clamp pitch to avoid flipping upside down
        cameraPitch = math.max(-89, math.min(89, cameraPitch))
        
        -- Apply camera rotation
        camera.setRotation(cameraPitch, cameraYaw, 0)
        
        if debugCounter % 60 == 0 then
            print(string.format("Camera rotation: Pitch=%.3f, Yaw=%.3f", cameraPitch, cameraYaw))
        end
    end
    
    -- Reset player position with configurable action
    if input.isActionHeld("SecondaryFire") then
        local playerNode = getParent()
        if playerNode then
            playerNode.setPosition(0, 2, 12)  -- Reset player to original position
        end
        camera.setPosition(0, 0, -4)  -- Reset camera relative to player
        cameraYaw = 0.0
        cameraPitch = 0.0
        camera.setRotation(cameraPitch, cameraYaw, 0)
        print("Player and camera reset to default position")
    end
    
    -- Additional actions for testing
    if input.isActionPressed("Jump") then
        print("Jump action triggered!")
    end
    
    if input.isActionPressed("Interact") then
        print("Interact action triggered!")
    end
end

-- Called during rendering
function render()
    -- Draw UI
    if renderer and renderer.setText then
        renderer.setText("VitaCameraInfo", "Left Stick: Move | Right Stick: Look | Triangle: Reset")
    end
    
    -- Get current player and camera positions for display
    local playerNode = getParent()
    local playerX, playerY, playerZ = 0, 0, 0
    if playerNode then
        playerX, playerY, playerZ = playerNode.getPosition()
    end
    
    local camX, camY, camZ = camera.getPosition()
    local rotX, rotY, rotZ = camera.getRotation()
    
    if renderer and renderer.setText then
        renderer.setText("VitaPlayerPos", string.format("Player: %.1f, %.1f, %.1f", playerX, playerY, playerZ))
        renderer.setText("VitaCameraPos", string.format("Camera: %.1f, %.1f, %.1f", camX, camY, camZ))
        renderer.setText("VitaCameraRot", string.format("Rot: %.1f, %.1f, %.1f", rotX, rotY, rotZ))
    end
end

-- Called when the script is destroyed
function destroy()
    print("Vita Camera Controller (Fixed) script destroyed!")
end
