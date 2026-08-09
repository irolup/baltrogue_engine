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
local equippedTextNodeName = "equiped_objet"

local INTERACT_ACTION = "Interact"
local INTERACT_RAYCAST_NODE = "PlayerRaycast"

local WEAPON_PISTOL = 1
local WEAPON_MELEE = 2
local WEAPON_GRAVITY = 3
local WEAPON_PRISM = 4
local weaponNames = { [WEAPON_PISTOL] = "Impulse Pistol", [WEAPON_MELEE] = "Melee", [WEAPON_GRAVITY] = "Gravity Gun", [WEAPON_PRISM] = "Gravity Prism" }
local currentWeapon = WEAPON_PISTOL
local weaponConfig = {
    [WEAPON_PISTOL] = { range = 50.0, fireRate = 0.4, impulseStrength = 70.0 },
    [WEAPON_MELEE]  = { damage = 25, range = 3.0,  fireRate = 0.6 },
    [WEAPON_GRAVITY] = nil,
    [WEAPON_PRISM]  = { fireRate = 1.2, radius = 4.0, duration = 5.0, rotationSpeed = 90.0, strength = 2.5, invertGravity = false },
}
local lastFireTime = 0

local gravityGunOpts = { cameraName = cameraName, playerModelNodeName = playerModelNodeName, gunLaserNodeName = "GunLaser" }
local gravityGun = nil
local impulsePistol = nil
local meleeWeapon = nil
local gravityPrism = nil

--Player stats
local playerHealth = 100
local playerMaxHealth = 100
local playerArmor = 0
local playerMaxArmor = 100

_G.GameEnemies = _G.GameEnemies or { ["enemy"] = "CapsuleCollision" }

local function updateEquippedText()
    if renderer and renderer.setText and equippedTextNodeName then
        renderer.setText(equippedTextNodeName, "Equipped: " .. (weaponNames[currentWeapon] or "?"))
    end
end

local function getEnemyRootName(hitNodeName)
    if not hitNodeName or hitNodeName == "" then return nil end
    if getNodeParentName then
        local parent = getNodeParentName(hitNodeName)
        if parent then return parent end
    end
    return hitNodeName
end

local function tryFireWeapon()
    local now = getTime()
    local cfg = weaponConfig[currentWeapon]
    if not cfg or (now - lastFireTime) < cfg.fireRate then return end
    lastFireTime = now

    if currentWeapon == WEAPON_PISTOL and impulsePistol and impulsePistol.fire then
        impulsePistol.fire(cfg, cameraName)
        return
    end

    if currentWeapon == WEAPON_MELEE and meleeWeapon and meleeWeapon.fire then
        meleeWeapon.fire(cfg, cameraName, getEnemyRootName)
        return
    end

    if currentWeapon == WEAPON_PRISM and gravityPrism and gravityPrism.place then
        gravityPrism.place(cameraName, {
            radius = cfg.radius,
            duration = cfg.duration,
            rotationSpeed = cfg.rotationSpeed,
            strength = cfg.strength,
            invertGravity = cfg.invertGravity,
        }, { playerRootName = playerRootName })
        return
    end
end

local function handleWeaponInput()
    if input.isActionPressed("SecondaryFire") then
        if currentWeapon == WEAPON_PISTOL then currentWeapon = WEAPON_MELEE
        elseif currentWeapon == WEAPON_MELEE then currentWeapon = WEAPON_GRAVITY
        elseif currentWeapon == WEAPON_GRAVITY then currentWeapon = WEAPON_PRISM
        else currentWeapon = WEAPON_PISTOL end
        updateEquippedText()
    end
    if input.isActionPressed("WeaponSlot1") then currentWeapon = WEAPON_PISTOL; updateEquippedText() end
    if input.isActionPressed("WeaponSlot2") then currentWeapon = WEAPON_MELEE;  updateEquippedText() end
    if input.isActionPressed("WeaponSlot3") then currentWeapon = WEAPON_GRAVITY; updateEquippedText() end
    if input.isActionPressed("WeaponSlot4") then currentWeapon = WEAPON_PRISM;  updateEquippedText() end
    if input.isActionPressed("WeaponSlot5") then currentWeapon = WEAPON_MELEE;  updateEquippedText() end

    if currentWeapon ~= WEAPON_GRAVITY and (input.isActionPressed("PrimaryFire") or (input.isActionHeld and input.isActionHeld("PrimaryFire"))) then
        tryFireWeapon()
    end
end

local function handleMovement(deltaTime, currentTime)
    local moveH = input.getActionAxis("MoveHorizontal")
    local moveV = input.getActionAxis("MoveVertical")
    if moveH == 0 then moveH = input.getActionAxis("MoveRight") - input.getActionAxis("MoveLeft") end
    if moveV == 0 then moveV = input.getActionAxis("MoveBackward") - input.getActionAxis("MoveForward") end

    local lookH = 0.0
    local lookV = 0.0
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

    if setNodeRotation then setNodeRotation(playerRootName, 0, cameraYaw, 0) end

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
            setNodePosition(playerRootName, px + desiredVelX * deltaTime, py + moveY * currentMoveSpeed * deltaTime, pz + desiredVelZ * deltaTime)
        end
    end

    if setNodeLocalPosition then
        setNodeLocalPosition(cameraName, firstPersonCamLocalPos[1], firstPersonCamLocalPos[2], firstPersonCamLocalPos[3])
    end
    if setNodeRotation then setNodeRotation(cameraName, -cameraPitch, 0.0, 0.0) end
end

local function tryInteract()
    if not input or not input.isActionPressed or not input.isActionPressed(INTERACT_ACTION) then return end
    if type(raycastFromNode) ~= "function" then return end
    if type(callNodeScriptFunction) ~= "function" then return end

    local hitNodeName = select(1, raycastFromNode(INTERACT_RAYCAST_NODE))
    if not hitNodeName or type(hitNodeName) ~= "string" then return end

    --draw a little sphere at the hit point for debugging:
    -- local base = "Ball_"
    -- local ballName = scene.createNode(base, "Root")
    -- if ballName then
    --     setNodePosition(ballName, hitX, hitY, hitZ)
    --     local meshChild = scene.createNode(base .. "_Mesh", ballName)
    --     if meshChild then
    --         scene.addMeshToNode(meshChild, "sphere", 0.1)
    --     end
    --     local collisionChild = scene.createNode(base .. "_Collision", ballName)
    --     if collisionChild then
    --         scene.addPhysicsToNode(collisionChild, "dynamic", "sphere", 0.1, 0.0)
    --     end
    --     table.insert(spawnedBalls, ballName)
    --     print("Spawned debug ball at: " .. hitX .. ", " .. hitY .. ", " .. hitZ)
    -- end


    -- Convention for modularity:
    -- raycast hits the collision node; the interactable's ScriptComponent is on the parent node.
    local nodeToTry = hitNodeName
    for _ = 1, 2 do
        callNodeScriptFunction(nodeToTry, "interact")
        local parent = getNodeParentName and getNodeParentName(nodeToTry) or nil
        if not parent then break end
        nodeToTry = parent
    end
end

function die()
    -- if we cal this function the health is 0 or below, for the moment we will respwan the player and or reset the scene

end

function start()
    lastJumpTime = getTime()
    lastFireTime = getTime()
    angularFactorSet = false
    if setNodeVisible then
        setNodeVisible(playerModelNodeName, false)
        setNodeVisible(gunMeshNodeName, true)
    end
    if input and input.setMouseCapture then input.setMouseCapture(true) end
    updateEquippedText()

    local ok, mod = pcall(dofile, "assets/samples/scripts/gravity_gun.lua")
    if ok and mod and mod.update then gravityGun = mod end
    ok, mod = pcall(dofile, "assets/samples/scripts/impulse_pistol.lua")
    if ok and mod and mod.fire then impulsePistol = mod end
    ok, mod = pcall(dofile, "assets/samples/scripts/melee.lua")
    if ok and mod and mod.fire then meleeWeapon = mod end
    ok, mod = pcall(dofile, "assets/samples/scripts/gravity_prism.lua")
    if ok and mod and mod.place then gravityPrism = mod end
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then return end
    local currentTime = getTime()

    tryInteract()
    handleWeaponInput()

    if gravityGun then
        gravityGun.update(deltaTime, currentWeapon == WEAPON_GRAVITY, gravityGunOpts)
    end
    if gravityPrism and gravityPrism.update then
        gravityPrism.update(deltaTime)
    end

    if not angularFactorSet and setNodeAngularFactor then
        setNodeAngularFactor("PlayerCollision", 0, 0, 0)
        angularFactorSet = true
    end
    handleMovement(deltaTime, currentTime)
end

function lateUpdate(deltaTime)
    if isGamePaused and isGamePaused() then return end
    if gravityGun then gravityGun.lateUpdate(gravityGunOpts) end
    if impulsePistol and impulsePistol.lateUpdate then
        impulsePistol.lateUpdate(gravityGunOpts, currentWeapon == WEAPON_PISTOL)
    end
end
