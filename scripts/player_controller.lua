local moveSpeed = 5.0
local mouseSensitivity = 5.0
local stickSensitivity = 50.0
local cameraYaw = 0.0
local cameraPitch = 0.0
local isFirstPerson = true
local isMouseCaptured = false
local cameraName = "PlayerCamera"
local playerRootName = "Player"
local arrowLookStrength = 1.0
local jumpForce = 8.0
local jumpCooldown = 0.1
local lastJumpTime = 0

function start()
	cameraYaw = 0.0
	cameraPitch = -25.0
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
	
	-- Lock rotation to prevent capsule from rolling
	if setNodeAngularFactor then
		setNodeAngularFactor("PlayerCollision", 0, 0, 0)
	end
	
	local moveH = input.getActionAxis("MoveHorizontal")
	local moveV = input.getActionAxis("MoveVertical")
	
	if not isMouseCaptured then
		if input and input.setMouseCapture then
			input.setMouseCapture(true)
			isMouseCaptured = true
		else
			isMouseCaptured = true
		end
	end
	
	if moveH == 0 then
		moveH = input.getActionAxis("MoveRight") - input.getActionAxis("MoveLeft")
	end
	if moveV == 0 then
		moveV = input.getActionAxis("MoveBackward") - input.getActionAxis("MoveForward")
	end
	
	local moveUp = input.getActionAxis("MoveUp")
	local moveDown = input.getActionAxis("MoveDown")
	
	local lookH = 0.0
	local lookV = 0.0
	
	if input and input.getMouseDelta then
		local mouseDeltaX, mouseDeltaY = input.getMouseDelta()
		lookH = mouseDeltaX * mouseSensitivity * 0.005
		lookV = mouseDeltaY * mouseSensitivity * 0.005
	end
	
	local stickH = input.getActionAxis("LookHorizontal")
	local stickV = input.getActionAxis("LookVertical")
	lookH = lookH + stickH * stickSensitivity * deltaTime
	lookV = lookV + stickV * stickSensitivity * deltaTime
	
	local arrowH = input.getActionAxis("LookRight") - input.getActionAxis("LookLeft")
	local arrowV = input.getActionAxis("LookUp") - input.getActionAxis("LookDown")
	lookH = lookH + arrowH * arrowLookStrength * mouseSensitivity * deltaTime
	lookV = lookV + arrowV * arrowLookStrength * mouseSensitivity * deltaTime
	
	if lookH ~= 0 then
		cameraYaw = cameraYaw - lookH
	end
	if lookV ~= 0 then
		cameraPitch = cameraPitch - lookV
		cameraPitch = math.max(-75, math.min(75, cameraPitch))
	end
	
	if setNodeRotation then
		setNodeRotation(playerRootName, 0, cameraYaw, 0)
	end
	
	if setNodeRotation then
		setNodeRotation(cameraName, cameraPitch, 0, 0)
	end
	
	local moveY = moveUp - moveDown
	
	-- Calculate movement direction based on camera orientation
	local forwardX = math.sin(math.rad(cameraYaw))
	local forwardZ = math.cos(math.rad(cameraYaw))
	local rightX = math.cos(math.rad(cameraYaw))
	local rightZ = -math.sin(math.rad(cameraYaw))
	
	local desiredVelX = rightX * moveH * moveSpeed + forwardX * moveV * moveSpeed
	local desiredVelZ = rightZ * moveH * moveSpeed + forwardZ * moveV * moveSpeed
	
	if setNodeVelocity then
		local currentVelX, currentVelY, currentVelZ = 0, 0, 0
		if getNodeVelocity then
			local vx, vy, vz = getNodeVelocity("PlayerCollision")
			if vx ~= nil and vy ~= nil and vz ~= nil then
				currentVelX = vx
				currentVelY = vy
				currentVelZ = vz
			end
		end
		
		if moveH ~= 0 or moveV ~= 0 then
			currentVelX = desiredVelX
			currentVelZ = desiredVelZ
		else
			currentVelX = 0
			currentVelZ = 0
		end
		
		if moveY ~= 0 then
			currentVelY = moveY * moveSpeed
		end
		
		-- Handle jump input
		local currentTime = getTime()
		if input.isActionPressed("Jump") then
			local timeSinceLastJump = currentTime - lastJumpTime
			if timeSinceLastJump >= jumpCooldown then
				-- Check if player is on ground (vertical velocity is small)
				if math.abs(currentVelY) < 0.5 then
					currentVelY = jumpForce
					lastJumpTime = currentTime
				end
			end
		end
		
		setNodeVelocity("PlayerCollision", currentVelX, currentVelY, currentVelZ)
	else
		local worldMoveX = desiredVelX * deltaTime
		local worldMoveZ = desiredVelZ * deltaTime
		local worldMoveY = moveY * moveSpeed * deltaTime
		
		local px, py, pz = getNodePosition("Player")
		if px ~= nil and py ~= nil and pz ~= nil then
			local newX = px + worldMoveX
			local newY = py + worldMoveY
			local newZ = pz + worldMoveZ
			
			setNodePosition("Player", newX, newY, newZ)
		end
	end
	
	if input.isActionPressed("Interact") then
		isFirstPerson = not isFirstPerson
		print("Camera mode: " .. (isFirstPerson and "First Person" or "Third Person"))
	end
end

function render()
	if renderer and renderer.drawText then
		renderer.drawText("WASD/Left Stick: Move | Arrows/Right Stick: Look", 10, 10)
		renderer.drawText("Space/Cross: Jump | E/Circle: Toggle Camera", 10, 30)
		renderer.drawText("Camera: " .. (isFirstPerson and "First Person" or "Third Person"), 10, 50)
	end
end

function destroy()
end
