-- Physics Gun


local cameraName = "PlayerCamera"
local playerModelNodeName = "PlayerModel"
local gunLaserNodeName = "GunLaser"  -- Line from side of camera to raycast target
local maxGrabDistance = 30.0
local ballRootName = "Ball"
local ballPhysicsNodeName = "SphereCollision"
local ballSpawnX, ballSpawnY, ballSpawnZ = 0, 0, 0

local beamOffsetRight, beamOffsetDown, beamOffsetForward = 0.55, -0.15, -1.7

local outlineVertPath = (_VITA_BUILD and "assets/shaders/outline.vert") or "assets/linux_shaders/outline.vert"
local outlineFragPath = (_VITA_BUILD and "assets/shaders/outline.frag") or "assets/linux_shaders/outline.frag"
local outlineParams = {
    u_OutlineColor = { 0.0, 0.9, 1.0 },
    u_OutlinePower = 3.0,
    blendMode = "Alpha",
    depthWrite = false
}

-- Currently held object
local heldNodeName = nil
-- Offset from object center to grab point
local hitOffsetLocalX, hitOffsetLocalY, hitOffsetLocalZ = 0, 0, 0
local currentGrabDistance = 5.0
local rotationDiffX, rotationDiffY, rotationDiffZ = 0, 0, 0
local rotationInputX, rotationInputY = 0, 0

local fixedDeltaTime = 1.0 / 60.0

local function localToWorldOffset(nodeName, lx, ly, lz)
    local rx, ry, rz = getNodeRight(nodeName)
    local ux, uy, uz = getNodeUp(nodeName)
    local fx, fy, fz = getNodeForward(nodeName)
    local wx = lx * rx + ly * ux - lz * fx
    local wy = lx * ry + ly * uy - lz * fy
    local wz = lx * rz + ly * uz - lz * fz
    return wx, wy, wz
end

local function rotateVectorAroundAxis(vx, vy, vz, ax, ay, az, angleDeg)
    local angle = math.rad(angleDeg)
    local len = math.sqrt(ax*ax + ay*ay + az*az)
    if len < 0.0001 then return vx, vy, vz end
    ax, ay, az = ax/len, ay/len, az/len
    local cos = math.cos(angle)
    local sin = math.sin(angle)
    local nx = vx*(cos + ax*ax*(1-cos)) + vy*(ax*ay*(1-cos) - az*sin) + vz*(ax*az*(1-cos) + ay*sin)
    local ny = vx*(ay*ax*(1-cos) + az*sin) + vy*(cos + ay*ay*(1-cos)) + vz*(ay*az*(1-cos) - ax*sin)
    local nz = vx*(az*ax*(1-cos) - ay*sin) + vy*(az*ay*(1-cos) + ax*sin) + vz*(cos + az*az*(1-cos))
    return nx, ny, nz
end


local function computeHitOffsetLocal(objPosX, objPosY, objPosZ, hitX, hitY, hitZ, nodeName)
    local dx = hitX - objPosX
    local dy = hitY - objPosY
    local dz = hitZ - objPosZ
    local rx, ry, rz = getNodeRight(nodeName)
    local ux, uy, uz = getNodeUp(nodeName)
    local fx, fy, fz = getNodeForward(nodeName)
    local lx = dx * rx + dy * ry + dz * rz
    local ly = dx * ux + dy * uy + dz * uz
    local lz = -(dx * fx + dy * fy + dz * fz)
    return lx, ly, lz
end

function start()
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then
        return
    end

    local dt = (deltaTime and deltaTime > 0) and deltaTime or fixedDeltaTime

    -- Respawn ball (PC: M key)
    if input.isActionPressed("RespawnBall") then
        if heldNodeName == ballPhysicsNodeName then
            heldNodeName = nil
        end
        if setNodePosition then
            setNodePosition(ballRootName, ballSpawnX, ballSpawnY, ballSpawnZ)
        end
        if setNodeVelocity then
            setNodeVelocity(ballPhysicsNodeName, 0, 0, 0)
        end
        if setNodeAngularVelocity then
            setNodeAngularVelocity(ballPhysicsNodeName, 0, 0, 0)
        end
    end

    -- Physics gun only works in first person
    local firstPerson = not (isNodeVisible and isNodeVisible(playerModelNodeName))
    if not firstPerson then
        if setNodeVisible and gunLaserNodeName then
            setNodeVisible(gunLaserNodeName, false)
        end
        if heldNodeName then
            if scene and scene.clearNodeMaterialOverride then
                scene.clearNodeMaterialOverride(heldNodeName)
            end
            if setNodeAngularVelocity then
                setNodeAngularVelocity(heldNodeName, 0, 0, 0)
            end
            if setNodeGravityEnabled then
                setNodeGravityEnabled(heldNodeName, true)
            end
            heldNodeName = nil
        end
        rotationInputX, rotationInputY = 0, 0
        return
    end

    local grabHeld = input.isActionPressed("PhysicsGrab") or (input.getActionAxis and input.getActionAxis("PhysicsGrab") > 0.5)

    if not grabHeld then
        if setNodeVisible and gunLaserNodeName then
            setNodeVisible(gunLaserNodeName, false)
        end
        if heldNodeName then
            if scene and scene.clearNodeMaterialOverride then
                scene.clearNodeMaterialOverride(heldNodeName)
            end
            if setNodeAngularVelocity then
                setNodeAngularVelocity(heldNodeName, 0, 0, 0)
            end
            if setNodeGravityEnabled then
                setNodeGravityEnabled(heldNodeName, true)
            end
            heldNodeName = nil
        end
        rotationInputX, rotationInputY = 0, 0
    end

    local camX, camY, camZ = getActiveCameraPosition()
    if not camX then
        camX, camY, camZ = getNodePosition(cameraName)
    end
    local fx, fy, fz = getNodeForward(cameraName)
    local rayOriginX, rayOriginY, rayOriginZ = camX, camY, camZ
    local rayDirX, rayDirY, rayDirZ = fx, fy, fz
    local len = math.sqrt(rayDirX*rayDirX + rayDirY*rayDirY + rayDirZ*rayDirZ)
    if len < 0.0001 then
        rayDirX, rayDirY, rayDirZ = 0, 0, -1
    else
        rayDirX, rayDirY, rayDirZ = rayDirX/len, rayDirY/len, rayDirZ/len
    end

    if not heldNodeName and grabHeld then
        local hitNode, hitX, hitY, hitZ, hitDist = physicsRaycast(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance)
        if hitNode and type(hitNode) == "string" and hitNode ~= "" and hitX and hitY and hitZ and hitDist then
            heldNodeName = hitNode
            if scene and scene.setNodeMaterialOverride then
                scene.setNodeMaterialOverride(heldNodeName, outlineVertPath, outlineFragPath, outlineParams)
            end
            if setNodeGravityEnabled then
                setNodeGravityEnabled(heldNodeName, false)
            end
            if setNodeAngularVelocity then
                setNodeAngularVelocity(heldNodeName, 0, 0, 0)
            end
            hitOffsetLocalX, hitOffsetLocalY, hitOffsetLocalZ = 0, 0, 0
            currentGrabDistance = hitDist
            if getNodeWorldPosition then
                local ox, oy, oz = getNodeWorldPosition(hitNode)
                if ox ~= nil and oy ~= nil and oz ~= nil then
                    local dx = ox - rayOriginX
                    local dy = oy - rayOriginY
                    local dz = oz - rayOriginZ
                    local d = math.sqrt(dx*dx + dy*dy + dz*dz)
                    if d > 0.01 and d <= maxGrabDistance then
                        currentGrabDistance = d
                    end
                end
            end
            local camPitch, camYaw, camRoll = getNodeWorldEuler(cameraName)
            local objPitch, objYaw, objRoll = getNodeWorldEuler(heldNodeName)
            if camPitch and objPitch then
                rotationDiffX = objPitch - camPitch
                rotationDiffY = objYaw - camYaw
                rotationDiffZ = objRoll - camRoll
            end
        end
    end

    if heldNodeName and input.isActionHeld and input.isActionHeld("PhysicsRotate") then
        local mx, my = 0, 0
        -- Mouse (PC)
        if input.getMouseDelta then
            local dx, dy = input.getMouseDelta()
            mx = (dx or 0) * 0.25
            my = (dy or 0) * 0.25
        end
        -- Right stick / look axes (Vita)
        if input.getActionAxis then
            local stickDegPerSec = 180.0
            mx = mx + (input.getActionAxis("LookHorizontal") or 0) * stickDegPerSec * dt
            my = my + (input.getActionAxis("LookVertical") or 0) * stickDegPerSec * dt
        end
        rotationInputX = rotationInputX + mx
        rotationInputY = rotationInputY + my
    end

    if heldNodeName then
        if setNodeAngularVelocity then
            setNodeAngularVelocity(heldNodeName, 0, 0, 0)
        end

        local camPitch, camYaw, camRoll = getNodeWorldEuler(cameraName)
        if camPitch then
            setNodeWorldRotation(heldNodeName, camPitch + rotationDiffX, camYaw + rotationDiffY, camRoll + rotationDiffZ)
        end

        local effectiveGrabDistance = currentGrabDistance
        if physicsRaycastObstacle then
            local obstacleDist = physicsRaycastObstacle(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance, heldNodeName)
            if obstacleDist and obstacleDist > 0 then
                local margin = 0.2
                effectiveGrabDistance = math.min(currentGrabDistance, math.max(0.5, obstacleDist - margin))
            end
        end

        local holdX = rayOriginX + rayDirX * effectiveGrabDistance
        local holdY = rayOriginY + rayDirY * effectiveGrabDistance
        local holdZ = rayOriginZ + rayDirZ * effectiveGrabDistance

        if (rotationInputX ~= 0 or rotationInputY ~= 0) and getNodeRight and getNodeUp and applyNodeRotationAroundAxis then
            local rx, ry, rz = getNodeRight(cameraName)
            local ux, uy, uz = getNodeUp(cameraName)
            applyNodeRotationAroundAxis(heldNodeName, rx, ry, rz, rotationInputY)
            applyNodeRotationAroundAxis(heldNodeName, ux, uy, uz, -rotationInputX)
            local objPitch, objYaw, objRoll = getNodeWorldEuler(heldNodeName)
            if objPitch and camPitch then
                rotationDiffX = objPitch - camPitch
                rotationDiffY = objYaw - camYaw
                rotationDiffZ = objRoll - camRoll
            end
            rotationInputX, rotationInputY = 0, 0
        end

        local woX, woY, woZ = localToWorldOffset(heldNodeName, hitOffsetLocalX, hitOffsetLocalY, hitOffsetLocalZ)
        local centerDestX = holdX - woX
        local centerDestY = holdY - woY
        local centerDestZ = holdZ - woZ

        if setNodePosition then
            setNodePosition(heldNodeName, centerDestX, centerDestY, centerDestZ)
        end
        setNodeVelocity(heldNodeName, 0, 0, 0)
        if syncNodeTransformToPhysics then
            syncNodeTransformToPhysics(heldNodeName)
        end
    end

    if setNodeVisible and gunLaserNodeName then
        setNodeVisible(gunLaserNodeName, grabHeld)
    end

    
    if grabHeld and setBeamEndpoints and gunLaserNodeName then
        local startX = rayOriginX
        local startY = rayOriginY
        local startZ = rayOriginZ
        if localToWorldOffset then
            local wx, wy, wz = localToWorldOffset(cameraName, beamOffsetRight, beamOffsetDown, beamOffsetForward)
            startX = rayOriginX + wx
            startY = rayOriginY + wy
            startZ = rayOriginZ + wz
        end
        local endX, endY, endZ
        local beamLength = maxGrabDistance
        if heldNodeName then
            local effectiveGrabDistance = currentGrabDistance
            if physicsRaycastObstacle then
                local obstacleDist = physicsRaycastObstacle(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance, heldNodeName)
                if obstacleDist and obstacleDist > 0 then
                    local margin = 0.2
                    effectiveGrabDistance = math.min(currentGrabDistance, math.max(0.5, obstacleDist - margin))
                end
            end
            beamLength = effectiveGrabDistance
        else
            if physicsRaycastObstacle then
                local obstacleDist = physicsRaycastObstacle(rayOriginX, rayOriginY, rayOriginZ, rayDirX, rayDirY, rayDirZ, maxGrabDistance, "")
                if obstacleDist and obstacleDist > 0 then
                    beamLength = obstacleDist
                end
            end
        end
        endX = rayOriginX + rayDirX * beamLength
        endY = rayOriginY + rayDirY * beamLength
        endZ = rayOriginZ + rayDirZ * beamLength
        setBeamEndpoints(gunLaserNodeName, startX, startY, startZ, endX, endY, endZ)
    end
end
