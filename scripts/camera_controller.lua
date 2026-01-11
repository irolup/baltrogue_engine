-- Camera Controller Script (Enhanced with Action-Based Input)
-- Handles camera movement and rotation using configurable input actions
-- Works on both PC and Vita platforms through the input mapping system

-- Script variables
local moveSpeed = 5.0
local mouseSensitivity = 0.1
local cameraYaw = 0.0  -- Start looking forward
local cameraPitch = 0.0
local isFirstPerson = true

-- Called when the script starts
function start()
    print("Camera Controller (Enhanced) script started!")
    
    -- Initialize camera angles
    cameraYaw = 0.0
    cameraPitch = 0.0
    
    -- Scene structure: PlayerRoot (empty, move this) -> Player (PhysicsComponent) -> Main Camera (this script)
    print("Camera Controller: Scene uses PlayerRoot as the movement node")
    print("PlayerRoot is the empty parent node that should be moved (not the Player node with PhysicsComponent)")
    
    -- Set initial camera position relative to player
    camera.setPosition(0, 0, -4)  -- Behind player
    camera.setRotation(0, cameraYaw, 0)
    
    -- Print available input actions for debugging
    print("Available input actions:")
    print("- MoveForward/Backward/Left/Right: WASD keys or analog stick")
    print("- MoveUp/Down: Space/Shift keys")
    print("- LookHorizontal/Vertical: Mouse or right analog stick")
    print("- Jump: Space key or Cross button")
    print("- Run: Ctrl key or L trigger")
    print("- Interact: E key or Circle button")
    print("- Menu: Esc key or Start button")
end

-- Called every frame
function update(deltaTime)
    -- Handle movement input using configurable actions
    
    local moveVector = {x = 0, y = 0, z = 0}
    local inputDetected = false
    
    -- Use action-based input instead of direct key checking
    if input.isActionHeld("MoveForward") then
        moveVector.z = moveVector.z - moveSpeed * deltaTime
        inputDetected = true
    end
    if input.isActionHeld("MoveBackward") then
        moveVector.z = moveVector.z + moveSpeed * deltaTime
        inputDetected = true
    end
    if input.isActionHeld("MoveLeft") then
        moveVector.x = moveVector.x - moveSpeed * deltaTime
        inputDetected = true
    end
    if input.isActionHeld("MoveRight") then
        moveVector.x = moveVector.x + moveSpeed * deltaTime
        inputDetected = true
    end
    if input.isActionHeld("MoveUp") then
        moveVector.y = moveVector.y + moveSpeed * deltaTime
        inputDetected = true
    end
    if input.isActionHeld("MoveDown") then
        moveVector.y = moveVector.y - moveSpeed * deltaTime
        inputDetected = true
    end
    
    -- Debug: Print if input is detected
    if inputDetected then
        print(string.format("Camera: Input detected - moveVector(%.3f, %.3f, %.3f)", moveVector.x, moveVector.y, moveVector.z))
    end
    
    -- Apply movement to player (parent node) relative to camera orientation
    if moveVector.x ~= 0 or moveVector.z ~= 0 then
        -- Calculate forward and right vectors based on camera yaw
        local forwardX = math.sin(math.rad(cameraYaw))
        local forwardZ = math.cos(math.rad(cameraYaw))
        local rightX = math.cos(math.rad(cameraYaw))
        local rightZ = -math.sin(math.rad(cameraYaw))
        
        -- Apply movement to player
        local worldMoveX = rightX * moveVector.x + forwardX * moveVector.z
        local worldMoveZ = rightZ * moveVector.x + forwardZ * moveVector.z
        
        -- Debug: Always print when there's movement (for debugging)
        print(string.format("Camera: Movement detected - moveVector(%.3f, %.3f, %.3f), worldMove(%.3f, %.3f)", 
            moveVector.x, moveVector.y, moveVector.z, worldMoveX, worldMoveZ))
        
        -- Move the player root node (node without PhysicsComponent, for proper movement)
        -- Scene structure: PlayerRoot (empty, move this) -> Player (PhysicsComponent) -> Main Camera
        -- Use setNodePosition to move PlayerRoot explicitly
        local px, py, pz = getNodePosition("PlayerRoot")
        if px ~= nil and py ~= nil and pz ~= nil then
            local newX = px + worldMoveX
            local newY = py + moveVector.y
            local newZ = pz + worldMoveZ
            
            print(string.format("Camera: Moving PlayerRoot from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)", 
                px, py, pz, newX, newY, newZ))
            
            setNodePosition("PlayerRoot", newX, newY, newZ)
            
            -- Verify position was set immediately
            local verifyX, verifyY, verifyZ = getNodePosition("PlayerRoot")
            if verifyX ~= nil then
                print(string.format("Camera: After setNodePosition, PlayerRoot is at (%.2f, %.2f, %.2f)", 
                    verifyX, verifyY, verifyZ))
                if math.abs(verifyX - newX) > 0.1 or math.abs(verifyY - newY) > 0.1 or math.abs(verifyZ - newZ) > 0.1 then
                    print(string.format("Camera: WARNING - Position mismatch! Expected (%.2f, %.2f, %.2f) but got (%.2f, %.2f, %.2f)", 
                        newX, newY, newZ, verifyX, verifyY, verifyZ))
                end
            else
                print("Camera: ERROR - Could not verify PlayerRoot position after setting!")
            end
        else
            print("Camera: ERROR - Could not get PlayerRoot position!")
            -- Fallback: move the direct parent if PlayerRoot not found
            local playerNode = getParent()
            if playerNode then
                print("Camera: Trying fallback - moving parent node")
                playerNode.move(worldMoveX, moveVector.y, worldMoveZ)
            else
                print("Camera: ERROR - No parent node found either!")
            end
        end
    elseif moveVector.y ~= 0 then
        -- Move the player root node vertically
        local px, py, pz = getNodePosition("PlayerRoot")
        if px ~= nil and py ~= nil and pz ~= nil then
            setNodePosition("PlayerRoot", px, py + moveVector.y, pz)
        else
            -- Fallback: move the direct parent if PlayerRoot not found
            local playerNode = getParent()
            if playerNode then
                playerNode.move(0, moveVector.y, 0)
            end
        end
    end
    
    -- Handle mouse look using configurable actions
    local mouseX, mouseY = input.getActionAxis("LookHorizontal"), input.getActionAxis("LookVertical")
    
    if mouseX ~= 0 or mouseY ~= 0 then
        cameraYaw = cameraYaw + mouseX * mouseSensitivity
        cameraPitch = cameraPitch - mouseY * mouseSensitivity
        
        -- Clamp pitch to avoid flipping upside down
        cameraPitch = math.max(-89, math.min(89, cameraPitch))
        
        -- Apply camera rotation
        camera.setRotation(cameraPitch, cameraYaw, 0)
    end
    
    -- Handle keyboard rotation (alternative to mouse) using actions
    if input.isActionHeld("DpadLeft") then
        cameraYaw = cameraYaw + 50 * deltaTime
        camera.setRotation(cameraPitch, cameraYaw, 0)
    end
    if input.isActionHeld("DpadRight") then
        cameraYaw = cameraYaw - 50 * deltaTime
        camera.setRotation(cameraPitch, cameraYaw, 0)
    end
    if input.isActionHeld("DpadUp") then
        cameraPitch = cameraPitch + 50 * deltaTime
        cameraPitch = math.max(-89, math.min(89, cameraPitch))
        camera.setRotation(cameraPitch, cameraYaw, 0)
    end
    if input.isActionHeld("DpadDown") then
        cameraPitch = cameraPitch - 50 * deltaTime
        cameraPitch = math.max(-89, math.min(89, cameraPitch))
        camera.setRotation(cameraPitch, cameraYaw, 0)
    end
    
    -- Toggle first person view using configurable action
    if input.isActionPressed("Interact") then
        isFirstPerson = not isFirstPerson
        print("Camera mode: " .. (isFirstPerson and "First Person" or "Third Person"))
    end
    
    -- Reset player position using configurable action
    if input.isActionPressed("Menu") then
        -- Reset PlayerRoot position (the empty parent node that should be moved)
        setNodePosition("PlayerRoot", 0, 1, 0)
        camera.setPosition(0, 0, -4)  -- Reset camera relative to player
        cameraYaw = 0.0
        cameraPitch = 0.0
        camera.setRotation(cameraPitch, cameraYaw, 0)
        print("Player and camera reset to default position")
    end
    
    -- Jump action
    if input.isActionPressed("Jump") then
        print("Jump action triggered!")
    end
    
    -- Run action
    if input.isActionHeld("Run") then
        moveSpeed = 10.0  -- Double speed when running
    else
        moveSpeed = 5.0   -- Normal speed
    end
end

-- Called during rendering
function render()
    -- Draw UI
    if renderer and renderer.setText then
        renderer.setText("CameraInfo", "Actions: Move | Look | Interact: Toggle Mode | Menu: Reset | Jump: Test | Run: Speed")
    end
    
    -- Get current player and camera positions for display
    -- Get PlayerRoot position (the actual moving node)
    local playerX, playerY, playerZ = getNodePosition("PlayerRoot")
    if playerX == nil then
        -- Fallback to parent if PlayerRoot not found
        local playerNode = getParent()
        if playerNode then
            playerX, playerY, playerZ = playerNode.getPosition()
        end
    end
    
    local camX, camY, camZ = camera.getPosition()
    local rotX, rotY, rotZ = camera.getRotation()
    
    if renderer and renderer.setText then
        renderer.setText("PlayerPos", string.format("Player: %.1f, %.1f, %.1f", playerX, playerY, playerZ))
        renderer.setText("CameraPos", string.format("Camera: %.1f, %.1f, %.1f", camX, camY, camZ))
        renderer.setText("CameraRot", string.format("Rot: %.1f, %.1f, %.1f", rotX, rotY, rotZ))
        renderer.setText("CameraMode", "Mode: " .. (isFirstPerson and "First Person" or "Third Person"))
    end
end

-- Called when the script is destroyed
function destroy()
    print("Camera Controller script destroyed!")
end
