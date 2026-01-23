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
    print("Speed boost activated for " .. duration .. " seconds!")
end

local cameraDistance = 6.0
local cameraHeight = 1.0
local turnSpeed = 5.0
local currentPlayerYaw = 0.0
local minPitch = -55.0
local maxPitch = 30.0
local minCameraHeight = 0.5

function start()
    lastJumpTime = getTime()
    
    if input and input.setMouseCapture then
        input.setMouseCapture(true)
        isMouseCaptured = true
    end
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then
        return
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
        print("Camera mode: " .. (isFirstPerson and "First Person" or "Third Person"))
    end
end