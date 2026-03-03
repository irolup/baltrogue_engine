-- Gravity gun: grab/move physics objects with a beam
local maxGrabDistance = 30.0
local beamOffsetRight, beamOffsetDown, beamOffsetForward = 0.55, -0.15, -1.7
local outlineVertPath = (_VITA_BUILD and "assets/shaders/outline.vert") or "assets/linux_shaders/outline.vert"
local outlineFragPath = (_VITA_BUILD and "assets/shaders/outline.frag") or "assets/linux_shaders/outline.frag"
local outlineParams = { u_OutlineColor = { 0.0, 0.9, 1.0 }, u_OutlinePower = 3.0, blendMode = "Alpha", depthWrite = false }

local heldNodeName = nil
local heldRootName = nil
local grabDistance = 5.0
local rotationX, rotationY = 0, 0
local lastBeamLength = 30.0
local lastGrabHeld = false

local function localToWorldOffset(nodeName, lx, ly, lz)
    local rx, ry, rz = getNodeRight(nodeName)
    local ux, uy, uz = getNodeUp(nodeName)
    local fx, fy, fz = getNodeForward(nodeName)
    return lx * rx + ly * ux - lz * fx, lx * ry + ly * uy - lz * fy, lx * rz + ly * uz - lz * fz
end

local function releaseHeld(opts)
    if not heldNodeName then return end
    if scene and scene.clearNodeMaterialOverride then
        scene.clearNodeMaterialOverride(heldNodeName)
        if heldRootName and heldRootName ~= heldNodeName then scene.clearNodeMaterialOverride(heldRootName) end
    end
    if setNodeBodyType then setNodeBodyType(heldNodeName, "dynamic") end
    if setNodeAngularVelocity then setNodeAngularVelocity(heldNodeName, 0, 0, 0) end
    if setNodeGravityEnabled then setNodeGravityEnabled(heldNodeName, true) end
    heldNodeName = nil
    heldRootName = nil
    rotationX, rotationY = 0, 0
end

local function update(deltaTime, isEquipped, opts)
    if not opts or not opts.cameraName or not opts.gunLaserNodeName then return end
    local cameraName = opts.cameraName
    local playerModelNodeName = opts.playerModelNodeName or "PlayerModel"
    local gunLaserNodeName = opts.gunLaserNodeName

    if not isEquipped then
        if setNodeVisible then setNodeVisible(gunLaserNodeName, false) end
        releaseHeld(opts)
        lastGrabHeld = false
        return
    end

    local firstPerson = not (isNodeVisible and isNodeVisible(playerModelNodeName))
    local grabHeld = input.isActionPressed("PhysicsGrab") or (input.getActionAxis and input.getActionAxis("PhysicsGrab") > 0.5)

    if not firstPerson or not grabHeld then
        if setNodeVisible then setNodeVisible(gunLaserNodeName, false) end
        releaseHeld(opts)
        lastGrabHeld = false
        return
    end

    lastGrabHeld = true
    local camX, camY, camZ = getActiveCameraPosition()
    if not camX then camX, camY, camZ = getNodePosition(cameraName) end
    local fx, fy, fz = getNodeForward(cameraName)
    local len = math.sqrt(fx*fx + fy*fy + fz*fz)
    if len < 0.0001 then fx, fy, fz = 0, 0, -1 else fx, fy, fz = fx/len, fy/len, fz/len end
    local rayOriginX, rayOriginY, rayOriginZ = camX, camY, camZ
    local rayDirX, rayDirY, rayDirZ = fx, fy, fz

    -- Try to grab
    if not heldNodeName and grabHeld then
        local hitNode, hitX, hitY, hitZ, hitDist = physicsRaycast(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance)
        if hitNode and type(hitNode) == "string" and hitNode ~= "" and hitDist then
            heldNodeName = hitNode
            heldRootName = (getNodeParentName and getNodeParentName(hitNode)) or hitNode
            if scene and scene.setNodeMaterialOverride then
                scene.setNodeMaterialOverride(heldNodeName, outlineVertPath, outlineFragPath, outlineParams)
                if heldRootName and heldRootName ~= heldNodeName then scene.setNodeMaterialOverride(heldRootName, outlineVertPath, outlineFragPath, outlineParams) end
            end
            if setNodeBodyType then setNodeBodyType(heldNodeName, "kinematic") end
            if setNodeGravityEnabled then setNodeGravityEnabled(heldNodeName, false) end
            if setNodeAngularVelocity then setNodeAngularVelocity(heldNodeName, 0, 0, 0) end
            if setNodeAngularFactor then setNodeAngularFactor(heldNodeName, 0, 0, 0) end
            grabDistance = hitDist
            if getNodeWorldPosition then
                local ox, oy, oz = getNodeWorldPosition(hitNode)
                if ox and oy and oz then
                    local d = math.sqrt((ox-rayOriginX)^2 + (oy-rayOriginY)^2 + (oz-rayOriginZ)^2)
                    if d > 0.01 and d <= maxGrabDistance then grabDistance = d end
                end
            end
        end
    end

    -- Rotate held object
    if heldNodeName and input.isActionHeld and input.isActionHeld("PhysicsRotate") then
        local mx, my = 0, 0
        if input.getMouseDelta then local dx, dy = input.getMouseDelta(); mx = (dx or 0) * 0.25; my = (dy or 0) * 0.25 end
        if input.getActionAxis then
            local dt = deltaTime or 1/60
            mx = mx + (input.getActionAxis("LookHorizontal") or 0) * 180 * dt
            my = my + (input.getActionAxis("LookVertical") or 0) * 180 * dt
        end
        rotationX, rotationY = rotationX + mx, rotationY + my
    end

    -- Move held object to ray
    if heldNodeName then
        if setNodeAngularVelocity then setNodeAngularVelocity(heldNodeName, 0, 0, 0) end
        local moveNode = heldRootName or heldNodeName
        local effectiveDist = grabDistance
        if physicsRaycastObstacle then
            local od = physicsRaycastObstacle(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance, heldNodeName)
            if od and od > 0 then effectiveDist = math.min(grabDistance, math.max(0.5, od - 0.2)) end
        end
        local holdX = rayOriginX + rayDirX * effectiveDist
        local holdY = rayOriginY + rayDirY * effectiveDist
        local holdZ = rayOriginZ + rayDirZ * effectiveDist
        if (rotationX ~= 0 or rotationY ~= 0) and getNodeRight and getNodeUp and applyNodeRotationAroundAxis then
            local rx, ry, rz = getNodeRight(cameraName)
            local ux, uy, uz = getNodeUp(cameraName)
            applyNodeRotationAroundAxis(moveNode, rx, ry, rz, rotationY)
            applyNodeRotationAroundAxis(moveNode, ux, uy, uz, -rotationX)
            rotationX, rotationY = 0, 0
        end
        if setNodePosition then setNodePosition(moveNode, holdX, holdY, holdZ) end
        setNodeVelocity(heldNodeName, 0, 0, 0)
        if syncNodeTransformToPhysics then syncNodeTransformToPhysics(heldNodeName) end
    end

    if setNodeVisible then setNodeVisible(gunLaserNodeName, grabHeld) end
    if grabHeld then
        if heldNodeName then
            local ed = grabDistance
            if physicsRaycastObstacle then
                local od = physicsRaycastObstacle(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance, heldNodeName)
                if od and od > 0 then ed = math.min(grabDistance, math.max(0.5, od - 0.2)) end
            end
            lastBeamLength = ed
        else
            lastBeamLength = (physicsRaycastObstacle and physicsRaycastObstacle(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance, "")) or maxGrabDistance
        end
    end
end

local function lateUpdate(opts)
    if not opts or not opts.cameraName or not opts.gunLaserNodeName then return end
    if not lastGrabHeld or not setBeamEndpoints then return end
    local cameraName = opts.cameraName
    local gunLaserNodeName = opts.gunLaserNodeName
    local camX, camY, camZ = getActiveCameraPosition()
    if not camX then camX, camY, camZ = getNodePosition(cameraName) end
    local fx, fy, fz = getNodeForward(cameraName)
    local len = math.sqrt(fx*fx + fy*fy + fz*fz)
    if len < 0.0001 then fx, fy, fz = 0, 0, -1 else fx, fy, fz = fx/len, fy/len, fz/len end
    local startX, startY, startZ = camX, camY, camZ
    local wx, wy, wz = localToWorldOffset(cameraName, beamOffsetRight, beamOffsetDown, beamOffsetForward)
    startX, startY, startZ = camX + wx, camY + wy, camZ + wz
    local endX = camX + fx * lastBeamLength
    local endY = camY + fy * lastBeamLength
    local endZ = camZ + fz * lastBeamLength
    setBeamEndpoints(gunLaserNodeName, startX, startY, startZ, endX, endY, endZ)
end

return { update = update, lateUpdate = lateUpdate }
