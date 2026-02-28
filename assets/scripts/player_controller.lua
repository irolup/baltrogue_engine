local moveSpeed = 5.0
local sprintSpeed = 10.0
local mouseSensitivity = 5.0
local stickSensitivity = 50.0
local cameraYaw = 0.0
local cameraPitch = -25.0
local cameraName = "PlayerCamera"
local playerRootName = "Player"
local arrowLookStrength = 1.0
local jumpForce = 8.0
local jumpCooldown = 0.1
local lastJumpTime = 0

local minPitch = -55.0
local maxPitch = 30.0
local firstPersonCamLocalPos = { 0.0, 0.7, 0.0 }
local playerModelNodeName = "PlayerModel"
local gunMeshNodeName = "GunMesh"

local SHOW_DEBUG_RAYS = false
local raycast1Name = "Raycast1"
local raycast2Name = "Raycast2"

function start()
    lastJumpTime = getTime()
    if setNodeVisible then
        setNodeVisible(playerModelNodeName, false)
        setNodeVisible(gunMeshNodeName, true)
    end
    if input and input.setMouseCapture then
        input.setMouseCapture(true)
    end
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then
        return
    end

    local currentTime = getTime()

    if setNodeAngularFactor then
        setNodeAngularFactor("PlayerCollision", 0, 0, 0)
    end

    local moveH = input.getActionAxis("MoveHorizontal")
    local moveV = input.getActionAxis("MoveVertical")
    if moveH == 0 then
        moveH = input.getActionAxis("MoveRight") - input.getActionAxis("MoveLeft")
    end
    if moveV == 0 then
        moveV = input.getActionAxis("MoveBackward") - input.getActionAxis("MoveForward")
    end

    local lookH, lookV = 0.0, 0.0
    if input and input.getMouseDelta then
        local dx, dy = input.getMouseDelta()
        lookH = dx * mouseSensitivity * 0.005
        lookV = dy * mouseSensitivity * 0.005
    end
    lookH = lookH + input.getActionAxis("LookHorizontal") * stickSensitivity * deltaTime
    lookV = lookV + input.getActionAxis("LookVertical") * stickSensitivity * deltaTime
    lookH = lookH + (input.getActionAxis("LookRight") - input.getActionAxis("LookLeft")) * arrowLookStrength * mouseSensitivity * deltaTime
    lookV = lookV + (input.getActionAxis("LookUp") - input.getActionAxis("LookDown")) * arrowLookStrength * mouseSensitivity * deltaTime

    cameraYaw = cameraYaw - lookH
    cameraPitch = math.max(minPitch, math.min(maxPitch, cameraPitch + lookV))

    local sprintAxis = input.getActionAxis("Sprint")
    if sprintAxis == 0 then sprintAxis = input.getActionAxis("Run") end
    local currentMoveSpeed = (sprintAxis > 0.1) and sprintSpeed or moveSpeed

    local forwardX = math.sin(math.rad(cameraYaw))
    local forwardZ = math.cos(math.rad(cameraYaw))
    local rightX = math.cos(math.rad(cameraYaw))
    local rightZ = -math.sin(math.rad(cameraYaw))
    local desiredVelX = rightX * moveH * currentMoveSpeed + forwardX * moveV * currentMoveSpeed
    local desiredVelZ = rightZ * moveH * currentMoveSpeed + forwardZ * moveV * currentMoveSpeed

    if setNodeRotation then
        setNodeRotation(playerRootName, 0, cameraYaw, 0)
    end

    if setNodeVelocity then
        local vx, vy, vz = 0, 0, 0
        if getNodeVelocity then
            local gvx, gvy, gvz = getNodeVelocity("PlayerCollision")
            if gvx and gvy and gvz then vx, vy, vz = gvx, gvy, gvz end
        end
        vx = (moveH ~= 0 or moveV ~= 0) and desiredVelX or 0
        vz = (moveH ~= 0 or moveV ~= 0) and desiredVelZ or 0
        if input.isActionPressed("Jump") and (currentTime - lastJumpTime >= jumpCooldown) and math.abs(vy) < 0.5 then
            vy = jumpForce
            lastJumpTime = currentTime
        end
        setNodeVelocity("PlayerCollision", vx, vy, vz)
    else
        local px, py, pz = getNodePosition(playerRootName)
        if px and py and pz then
            local moveY = input.getActionAxis("MoveUp") - input.getActionAxis("MoveDown")
            setNodePosition(playerRootName,
                px + desiredVelX * deltaTime,
                py + moveY * currentMoveSpeed * deltaTime,
                pz + desiredVelZ * deltaTime)
        end
    end


    -- First-person camera
    if setNodeLocalPosition then
        setNodeLocalPosition(cameraName, firstPersonCamLocalPos[1], firstPersonCamLocalPos[2], firstPersonCamLocalPos[3])
    end
    if setNodeRotation then
        setNodeRotation(cameraName, -cameraPitch, 0.0, 0.0)
    end
end
