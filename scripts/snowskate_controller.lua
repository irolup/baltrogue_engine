-- Snowskate: on foot (walk/run/jump) and on board (ride, faster). E to embark/disembark.

local playerRootName = "Player"
local playerCollisionName = "PlayerCollision"
local playerModelNodeName = "PlayerModel"
local snowskateRootName = "SnowskateRoot"
local snowskateCollisionName = "SnowskateCollision"
local cameraName = "PlayerCamera"

local moveSpeed = 5.0
local sprintSpeed = 10.0
local boardSpeed = 12.0
local boardSprintMultiplier = 1.5
local mouseSensitivity = 5.0
local stickSensitivity = 50.0
local cameraYaw = 0.0
local cameraPitch = -25.0
local cameraDistance = 6.0
local cameraHeight = 1.0
local turnSpeed = 5.0
local currentPlayerYaw = 0.0
local minPitch = -55.0
local maxPitch = 30.0
local minCameraHeight = 0.5
local jumpForce = 8.0
local jumpCooldown = 0.1
local lastJumpTime = 0
local boardHeightCarry = 0.45
local boardHeightRiding = -0.7
local boardGlideDamping = 0.992
local boardFollowLerp = 1.0
local boardHeightLerp = 0.2

local snowskateHUDNodeName = "SnowskateHUD"

local isRiding = false
local currentBoardHeightOffset = 0.45
local embarkCooldown = 0.0
local currentDisplaySpeed = 0.0

function start()
    lastJumpTime = getTime()
    embarkCooldown = 0.0
    currentBoardHeightOffset = boardHeightCarry
    if setNodeVisible then
        setNodeVisible(playerModelNodeName, true)
    end
    if setNodeAngularFactor then
        setNodeAngularFactor(playerCollisionName, 0, 0, 0)
    end
    if setNodeBodyType then
        setNodeBodyType(snowskateCollisionName, "KINEMATIC")
    end
    if input and input.setMouseCapture then
        input.setMouseCapture(true)
    end
    if animation then
        local skeletonName = "Player"
        if animation.setSkeleton then
            animation.setSkeleton(playerModelNodeName, skeletonName)
        end
        if animation.setAnimationClip and animation.setLoop and animation.setSpeed and animation.play then
            animation.setAnimationClip(playerModelNodeName, "Idle")
            animation.setLoop(playerModelNodeName, true)
            animation.setSpeed(playerModelNodeName, 1.0)
            animation.play(playerModelNodeName)
        end
    end
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then
        return
    end

    if setNodeAngularFactor then
        setNodeAngularFactor(playerCollisionName, 0, 0, 0)
    end

    local currentTime = getTime()
    if embarkCooldown > 0 then
        embarkCooldown = embarkCooldown - deltaTime
    end

    local embarkPressed = (input.isActionPressed and (input.isActionPressed("Embark") or input.isActionPressed("Interact")))
    if embarkPressed and embarkCooldown <= 0 then
        isRiding = not isRiding
        embarkCooldown = 0.5
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
    lookH = lookH + (input.getActionAxis("LookRight") - input.getActionAxis("LookLeft")) * mouseSensitivity * deltaTime
    lookV = lookV + (input.getActionAxis("LookUp") - input.getActionAxis("LookDown")) * mouseSensitivity * deltaTime

    cameraYaw = cameraYaw - lookH
    cameraPitch = math.max(minPitch, math.min(maxPitch, cameraPitch + lookV))

    local forwardX = math.sin(math.rad(cameraYaw))
    local forwardZ = math.cos(math.rad(cameraYaw))
    local rightX = math.cos(math.rad(cameraYaw))
    local rightZ = -math.sin(math.rad(cameraYaw))

    local sprintAxis = input.getActionAxis("Sprint")
    if sprintAxis == 0 then sprintAxis = input.getActionAxis("Run") end
    local speed
    if isRiding then
        speed = boardSpeed * (sprintAxis > 0.1 and boardSprintMultiplier or 1.0)
    else
        speed = (sprintAxis > 0.1) and sprintSpeed or moveSpeed
    end
    currentDisplaySpeed = speed

    local desiredVelX = rightX * moveH * speed + forwardX * moveV * speed
    local desiredVelZ = rightZ * moveH * speed + forwardZ * moveV * speed

    local px, py, pz = getNodePosition(playerRootName)

    if setNodeVelocity then
        local vx, vy, vz = 0, 0, 0
        if getNodeVelocity then
            local gvx, gvy, gvz = getNodeVelocity(playerCollisionName)
            if gvx and gvy and gvz then vx, vy, vz = gvx, gvy, gvz end
        end
        if isRiding then
            if moveH ~= 0 or moveV ~= 0 then
                vx = desiredVelX
                vz = desiredVelZ
            else
                vx = vx * boardGlideDamping
                vz = vz * boardGlideDamping
            end
        else
            vx = (moveH ~= 0 or moveV ~= 0) and desiredVelX or 0
            vz = (moveH ~= 0 or moveV ~= 0) and desiredVelZ or 0
            if input.isActionPressed("Jump") and (currentTime - lastJumpTime >= jumpCooldown) then
                if math.abs(vy) < 0.5 then
                    vy = jumpForce
                    lastJumpTime = currentTime
                end
            end
        end
        setNodeVelocity(playerCollisionName, vx, vy, vz)
    end

    if px and py and pz and setNodePosition then
        local targetOffset = isRiding and boardHeightRiding or boardHeightCarry
        currentBoardHeightOffset = currentBoardHeightOffset + (targetOffset - currentBoardHeightOffset) * boardHeightLerp
        local targetY = py + currentBoardHeightOffset
        local tx, ty, tz = px, targetY, pz
        if boardFollowLerp < 1.0 then
            local sx, sy, sz = getNodePosition(snowskateRootName)
            if sx and sy and sz then
                tx = sx + (px - sx) * boardFollowLerp
                ty = sy + (targetY - sy) * boardFollowLerp
                tz = sz + (pz - sz) * boardFollowLerp
            end
        end
        setNodePosition(snowskateRootName, tx, ty, tz)
    end

    if setNodeRotation then
        local dirX, dirZ = desiredVelX, desiredVelZ
        if isRiding and (moveH == 0 and moveV == 0) and getNodeVelocity then
            local gvx, gvy, gvz = getNodeVelocity(playerCollisionName)
            if gvx and gvz and (math.abs(gvx) > 0.1 or math.abs(gvz) > 0.1) then
                dirX, dirZ = gvx, gvz
            end
        end
        if math.abs(dirX) > 0.05 or math.abs(dirZ) > 0.05 then
            local angle = 0
            if dirZ ~= 0 then
                angle = math.atan(dirX / dirZ)
                if dirZ < 0 then angle = angle + math.pi end
            else
                angle = dirX > 0 and (math.pi / 2) or (-math.pi / 2)
            end
            local targetYaw = math.deg(angle) + 180
            local diff = (targetYaw - currentPlayerYaw + 180) % 360 - 180
            local turn = turnSpeed * deltaTime
            if isRiding then turn = turn * 1.5 end
            currentPlayerYaw = currentPlayerYaw + diff * math.min(turn, 1)
            setNodeRotation(playerRootName, 0, currentPlayerYaw, 0)
        end
    end

    if setNodeRotation then
        setNodeRotation(snowskateRootName, 0, currentPlayerYaw + 90, 0)
    end

    if renderer and renderer.setText and snowskateHUDNodeName then
        local mode = isRiding and "BOARD (glide, no jump)" or "ON FOOT (walk, run, jump)"
        renderer.setText(snowskateHUDNodeName, mode .. "  |  E: embark  |  speed: " .. string.format("%.1f", currentDisplaySpeed))
    end

    if px and py and pz then
        local radYaw = math.rad(cameraYaw)
        local radPitch = math.rad(cameraPitch)
        local camX = px + cameraDistance * math.cos(radPitch) * math.sin(radYaw)
        local camY = py + cameraDistance * math.sin(radPitch) + cameraHeight
        local camZ = pz + cameraDistance * math.cos(radPitch) * math.cos(radYaw)
        if camY < py + minCameraHeight then
            camY = py + minCameraHeight
        end
        setNodePosition(cameraName, camX, camY, camZ)
        if setNodeLookAt then
            setNodeLookAt(cameraName, px, py + cameraHeight, pz)
        end
    end
end

function render()
end
