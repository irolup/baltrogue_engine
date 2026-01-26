local moveSpeed = 5.0
local sprintSpeed = 10.0
local mouseSensitivity = 5.0
local stickSensitivity = 50.0
local cameraYaw = 0.0
local cameraPitch = -25.0
local isFirstPerson = false
local isMouseCaptured = false
local cameraName = "PlayerCamera"
local playerRootName = "Player"
local arrowLookStrength = 1.0
local jumpForce = 8.0
local jumpCooldown = 0.1
local lastJumpTime = 0

local speedBoostEndTime = 0
local speedBoostMultiplier = 2.0

function activateSpeedBoost(duration)
    local currentTime = getTime()
    speedBoostEndTime = currentTime + duration
end

local cameraDistance = 6.0
local cameraHeight = 1.0
local turnSpeed = 5.0
local currentPlayerYaw = 0.0
local minPitch = -55.0
local maxPitch = 30.0
local minCameraHeight = 0.5

local walkSoundNodeName = "Walk_Sound"
local walkSound = nil
local isMoving = false
local isSprinting = false
local walkPitch = 1.0
local runPitch = 1.5
local lastMovementState = false
local stopSoundFrameCount = 0

function start()
    lastJumpTime = getTime()
    
    if input and input.setMouseCapture then
        input.setMouseCapture(true)
        isMouseCaptured = true
    end
    
    if sound and sound.getComponent then
        walkSound = sound.getComponent(walkSoundNodeName)
        if walkSound and type(walkSound) == "table" and walkSound.setLoop then
            if walkSound.stop and type(walkSound.stop) == "function" then
                pcall(function() walkSound:stop() end)
            end
            if walkSound.setLoop and type(walkSound.setLoop) == "function" then
                pcall(function() walkSound:setLoop(true) end)
            end
            if walkSound.stop and type(walkSound.stop) == "function" then
                for i = 1, 5 do
                    pcall(function() walkSound:stop() end)
                end
                stopSoundFrameCount = 10
            end
            lastMovementState = false
        else
            walkSound = nil
        end
    else
        walkSound = nil
    end
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then
        return
    end

    if walkSound == nil and sound and sound.getComponent then
        walkSound = sound.getComponent(walkSoundNodeName)
        if walkSound and walkSound.setLoop then
            if walkSound.stop then
                pcall(function() walkSound:stop() end)
            end
            pcall(function() walkSound:setLoop(true) end)
            if walkSound.stop then
                pcall(function() walkSound:stop() end)
                pcall(function() walkSound:stop() end)
                stopSoundFrameCount = 5
            end
            lastMovementState = false
        end
    end
    
    if stopSoundFrameCount > 0 and walkSound and walkSound.stop then
        pcall(function() walkSound.stop() end)
        stopSoundFrameCount = stopSoundFrameCount - 1
    end

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

    local moveUp = input.getActionAxis("MoveUp")
    local moveDown = input.getActionAxis("MoveDown")
    local moveY = moveUp - moveDown

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

    local currentTime = getTime()
    local speedBoostActive = (currentTime < speedBoostEndTime)
    
    local sprintAxis = input.getActionAxis("Sprint")
    if sprintAxis == 0 then sprintAxis = input.getActionAxis("Run") end
    local baseSpeed = (sprintAxis > 0.1) and sprintSpeed or moveSpeed
    local currentMoveSpeed = baseSpeed
    if speedBoostActive then
        currentMoveSpeed = baseSpeed * speedBoostMultiplier
    end
    
    local wasMoving = isMoving
    local wasSprinting = isSprinting
    isMoving = (moveH ~= 0 or moveV ~= 0)
    isSprinting = (sprintAxis > 0.1)
    
    if walkSound ~= nil and type(walkSound) == "table" then
        local soundIsPlaying = false
        if walkSound.isPlaying and type(walkSound.isPlaying) == "function" then
            local success, result = pcall(function() return walkSound:isPlaying() end)
            if success then
                soundIsPlaying = result
            end
        end
        
        if isMoving ~= lastMovementState then
            if isMoving then
                if walkSound and type(walkSound) == "table" and walkSound.play and type(walkSound.play) == "function" then
                    pcall(function() walkSound:play() end)
                end
            else
                if walkSound and type(walkSound) == "table" and walkSound.stop and type(walkSound.stop) == "function" then
                    pcall(function() walkSound:stop() end)
                    stopSoundFrameCount = 5
                end
            end
            lastMovementState = isMoving
        end
        
        if not isMoving and walkSound and type(walkSound) == "table" and walkSound.stop and type(walkSound.stop) == "function" then
            if stopSoundFrameCount > 0 then
                pcall(function() walkSound:stop() end)
                stopSoundFrameCount = stopSoundFrameCount - 1
            elseif soundIsPlaying then
                pcall(function() walkSound:stop() end)
                stopSoundFrameCount = 3
            end
        end
        
        if isMoving and walkSound and type(walkSound) == "table" and walkSound.setPitch and type(walkSound.setPitch) == "function" then
            if isSprinting and not wasSprinting then
                pcall(function() walkSound:setPitch(runPitch) end)
            elseif not isSprinting and wasSprinting then
                pcall(function() walkSound:setPitch(walkPitch) end)
            end
        end
    end

    -- Calculate movement direction relative to camera
    local forwardX = math.sin(math.rad(cameraYaw))
    local forwardZ = math.cos(math.rad(cameraYaw))
    local rightX = math.cos(math.rad(cameraYaw))
    local rightZ = -math.sin(math.rad(cameraYaw))

    local desiredVelX = rightX * moveH * currentMoveSpeed + forwardX * moveV * currentMoveSpeed
    local desiredVelZ = rightZ * moveH * currentMoveSpeed + forwardZ * moveV * currentMoveSpeed

	if (moveH ~= 0 or moveV ~= 0) then
		local angle = 0
		if desiredVelZ ~= 0 then
			angle = math.atan(desiredVelX / desiredVelZ)
			if desiredVelZ < 0 then
				angle = angle + math.pi
			end
		else
			if desiredVelX > 0 then
				angle = math.pi / 2
			else
				angle = -math.pi / 2
			end
		end
		local targetYaw = math.deg(angle) + 180

		local diff = targetYaw - currentPlayerYaw
		diff = (diff + 180) % 360 - 180

		currentPlayerYaw = currentPlayerYaw + diff * math.min(turnSpeed * deltaTime, 1)

		if setNodeRotation then
			setNodeRotation(playerRootName, 0, currentPlayerYaw, 0)
		end
	end

    if setNodeVelocity then
        local vx, vy, vz = 0, 0, 0
        if getNodeVelocity then
            local gvx, gvy, gvz = getNodeVelocity("PlayerCollision")
            if gvx and gvy and gvz then vx, vy, vz = gvx, gvy, gvz end
        end

        vx = (moveH ~= 0 or moveV ~= 0) and desiredVelX or 0
        vz = (moveH ~= 0 or moveV ~= 0) and desiredVelZ or 0

        local currentTime = getTime()
        if input.isActionPressed("Jump") and (currentTime - lastJumpTime >= jumpCooldown) then
            if math.abs(vy) < 0.5 then
                vy = jumpForce
                lastJumpTime = currentTime
            end
        end

        setNodeVelocity("PlayerCollision", vx, vy, vz)
    else
        local px, py, pz = getNodePosition(playerRootName)
        if px and py and pz then
            setNodePosition(playerRootName,
                px + desiredVelX * deltaTime,
                py + moveY * currentMoveSpeed * deltaTime,
                pz + desiredVelZ * deltaTime)
        end
    end

    local px, py, pz = getNodePosition(playerRootName)
    if px and py and pz then
        local radYaw = math.rad(cameraYaw)
        local radPitch = math.rad(cameraPitch)

        local camX = px + cameraDistance * math.cos(radPitch) * math.sin(radYaw)
        local camY = py + cameraDistance * math.sin(radPitch) + cameraHeight
        local camZ = pz + cameraDistance * math.cos(radPitch) * math.cos(radYaw)

        local minY = py + minCameraHeight
        if camY < minY then
            camY = minY
        end

        setNodePosition(cameraName, camX, camY, camZ)

        if setNodeLookAt then
            setNodeLookAt(cameraName, px, py + cameraHeight, pz)
        end
    end

    if input.isActionPressed("Interact") then
        isFirstPerson = not isFirstPerson
    end
end