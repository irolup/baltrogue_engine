-- Apply impulse: shoots a visible vector line, applies directional impulse (push)
local beamEndX, beamEndY, beamEndZ = 0, 0, 0
local beamVisibleUntil = 0
local beamOffsetRight, beamOffsetDown, beamOffsetForward = 0.55, -0.15, -1.7
local gunLaserNodeName = "GunLaser"

local function localToWorldOffset(nodeName, lx, ly, lz)
    local rx, ry, rz = getNodeRight(nodeName)
    local ux, uy, uz = getNodeUp(nodeName)
    local fx, fy, fz = getNodeForward(nodeName)
    return lx * rx + ly * ux - lz * fx, lx * ry + ly * uy - lz * fy, lx * rz + ly * uz - lz * fz
end

return {
    -- Fire a ray, apply impulse to hit body store beam end for drawing
    -- cfg: { range, impulseStrength }
    fire = function(cfg, cameraName)
        if not cfg or not cameraName then return end
        local range = cfg.range or 50.0
        local strength = cfg.impulseStrength or 14.0
        local camX, camY, camZ = getActiveCameraPosition()
        if not camX then camX, camY, camZ = getNodePosition(cameraName) end
        if not camX then return end
        local fx, fy, fz = getNodeForward(cameraName)
        local len = math.sqrt(fx * fx + fy * fy + fz * fz)
        if len < 0.0001 then fx, fy, fz = 0, 0, -1 else fx, fy, fz = fx / len, fy / len, fz / len end

        local hitNode, hitX, hitY, hitZ, hitDist = physicsRaycast(camX, camY, camZ, fx, fy, fz, range)
        local endX, endY, endZ

        if hitNode and type(hitNode) == "string" and hitNode ~= "" and hitDist and hitDist <= range then
            endX, endY, endZ = hitX, hitY, hitZ
            local vx, vy, vz = 0, 0, 0
            if getNodeVelocity then
                local gx, gy, gz = getNodeVelocity(hitNode)
                if gx and gy and gz then vx, vy, vz = gx, gy, gz end
            end
            setNodeVelocity(hitNode, vx + fx * strength, vy + fy * strength, vz + fz * strength)
        else
            endX = camX + fx * range
            endY = camY + fy * range
            endZ = camZ + fz * range
        end

        local duration = 0.12
        beamEndX, beamEndY, beamEndZ = endX, endY, endZ
        beamVisibleUntil = getTime() + duration
        return endX, endY, endZ
    end,

    -- Draw or hide the impulse vector line
    -- opts: { cameraName, gunLaserNodeName }
    lateUpdate = function(opts, isEquipped)
        if not opts or not opts.cameraName then return end
        local cameraName = opts.cameraName
        local nodeName = (opts.gunLaserNodeName and opts.gunLaserNodeName ~= "") and opts.gunLaserNodeName or gunLaserNodeName
        local now = getTime()
        if isEquipped and now < beamVisibleUntil and setBeamEndpoints then
            local camX, camY, camZ = getActiveCameraPosition()
            if not camX then camX, camY, camZ = getNodePosition(cameraName) end
            if camX then
                local wx, wy, wz = localToWorldOffset(cameraName, beamOffsetRight, beamOffsetDown, beamOffsetForward)
                local startX = camX + wx
                local startY = camY + wy
                local startZ = camZ + wz
                setBeamEndpoints(nodeName, startX, startY, startZ, beamEndX, beamEndY, beamEndZ)
                if setNodeVisible then setNodeVisible(nodeName, true) end
            end
        elseif isEquipped then
            if setNodeVisible then setNodeVisible(nodeName, false) end
        end
    end
}
