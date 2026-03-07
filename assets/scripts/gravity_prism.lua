-- Gravity Prism: rotating gravity volume. Affects enemies + objects.
local WORLD_GRAVITY = 9.81
local DEFAULT_RADIUS = 4.0
local DEFAULT_DURATION = 5.0
local DEFAULT_ROTATION_SPEED = 90.0
local DEFAULT_STRENGTH = 2.5
local PLACE_RANGE = 30.0
local DEFAULT_BLEND_MODE = "Additive"

local prismVisualCounter = 0
local lastDebugTime = -999
local DEBUG_INTERVAL = 1.5

local prisms = {}

local function getAffectedNodeList()
    local list = {}
    local gameEnemies = _G.GameEnemies
    if gameEnemies and type(gameEnemies) == "table" then
        for rootName, physicsNode in pairs(gameEnemies) do
            if type(rootName) == "string" and rootName ~= "" then
                local nodeName = (type(physicsNode) == "string" and physicsNode ~= "") and physicsNode or rootName
                list[nodeName] = true
            end
        end
    end
    local extra = _G.GravityPrismObjects
    if type(extra) == "table" then
        for _, name in ipairs(extra) do
            if type(name) == "string" and name ~= "" then list[name] = true end
        end
    end
    return list
end

local function pointInSphere(px, py, pz, cx, cy, cz, r)
    if not px or not cx then return false end
    local dx, dy, dz = px - cx, py - cy, pz - cz
    return dx*dx + dy*dy + dz*dz <= r*r
end

local function releasePrismAffected(affectedNodes)
    if not setNodeGravityEnabled then return end
    for nodeName, _ in pairs(affectedNodes) do
        setNodeGravityEnabled(nodeName, true)
    end
end

local function place(cameraName, cfg, opts)
    if not getNodePosition then return false end
    if not physicsRaycastGround and not physicsRaycast then return false end
    opts = opts or {}
    cfg = cfg or {}
    local radius = (type(cfg.radius) == "number") and cfg.radius or DEFAULT_RADIUS
    local duration = (type(cfg.duration) == "number") and cfg.duration or DEFAULT_DURATION
    local rotationSpeed = (type(cfg.rotationSpeed) == "number") and cfg.rotationSpeed or DEFAULT_ROTATION_SPEED
    local strength = (type(cfg.strength) == "number" and cfg.strength > 0) and cfg.strength or DEFAULT_STRENGTH
    local invertGravity = cfg.invertGravity == true
    local blendMode = (type(cfg.blendMode) == "string" and (cfg.blendMode == "Opaque" or cfg.blendMode == "Alpha" or cfg.blendMode == "Additive")) and cfg.blendMode or DEFAULT_BLEND_MODE

    local camX, camY, camZ
    if getActiveCameraPosition then camX, camY, camZ = getActiveCameraPosition() end
    if not camX and cameraName then camX, camY, camZ = getNodePosition(cameraName) end
    if not camX and opts.playerRootName then camX, camY, camZ = getNodePosition(opts.playerRootName) end
    if not camX then return false end

    local fx, fy, fz = 0, 0, -1
    if getNodeForward and cameraName then
        local a, b, c = getNodeForward(cameraName)
        if a and b and c then
            local len = math.sqrt(a*a + b*b + c*c)
            if len >= 0.0001 then fx, fy, fz = a/len, b/len, c/len end
        end
    elseif getNodeForward and opts.playerRootName then
        local a, b, c = getNodeForward(opts.playerRootName)
        if a and b and c then
            local len = math.sqrt(a*a + b*b + c*c)
            if len >= 0.0001 then fx, fy, fz = a/len, b/len, c/len end
        end
    end

    local px, py, pz
    local exclude1 = (opts and type(opts.playerRootName) == "string") and opts.playerRootName or ""
    local exclude2 = (opts and type(opts.playerModelNodeName) == "string") and opts.playerModelNodeName or ""
    if physicsRaycastGround then
        local hx, hy, hz, nx, ny, nz, hitDist = physicsRaycastGround(camX, camY, camZ, fx, fy, fz, PLACE_RANGE, exclude1, exclude2)
        if hx and hitDist and hitDist <= PLACE_RANGE then
            px, py, pz = hx, hy, hz
        else
            px = camX + fx * PLACE_RANGE
            py = camY + fy * PLACE_RANGE
            pz = camZ + fz * PLACE_RANGE
        end
    else
        local hitNode, hitX, hitY, hitZ, hitDist = physicsRaycast(camX, camY, camZ, fx, fy, fz, PLACE_RANGE)
        if hitNode and type(hitNode) == "string" and hitNode ~= "" and hitDist and hitDist <= PLACE_RANGE then
            px, py, pz = hitX, hitY, hitZ
        else
            px = camX + fx * PLACE_RANGE
            py = camY + fy * PLACE_RANGE
            pz = camZ + fz * PLACE_RANGE
        end
    end

    local now = getTime and getTime() or 0
    prismVisualCounter = prismVisualCounter + 1
    local visualName = "GravityPrism_" .. prismVisualCounter
    local prismData = {
        x = px, y = py, z = pz,
        radius = radius,
        spawnTime = now,
        duration = duration,
        rotationSpeed = rotationSpeed,
        strength = strength,
        invertGravity = invertGravity,
        affectedNodes = {},
        visualNodeName = nil,
    }
    if scene and scene.createNode and scene.addMeshToNode and setNodePosition then
        local rootName = "Root"
        local created = scene.createNode(visualName, rootName)
        if created then
            setNodePosition(visualName, px, py, pz)
            if scene.addMeshToNode(visualName, "sphere", radius) then
                if scene.setNodeBlendMode then
                    scene.setNodeBlendMode(visualName, blendMode)
                end
                prismData.visualNodeName = visualName
            else
                if destroyNode then destroyNode(visualName) end
            end
        end
    end
    table.insert(prisms, prismData)
    if type(print) == "function" then
        local list = getAffectedNodeList()
        local names = {}
        for nodeName, _ in pairs(list) do names[#names + 1] = nodeName end
        local listStr = #names > 0 and table.concat(names, ", ") or "(none)"
        print("[GravityPrism] placed at " .. string.format("%.2f, %.2f, %.2f", px, py, pz) .. " radius=" .. radius .. " affectedList=" .. listStr)
    end
    return true
end

local function update(deltaTime)
    local now = getTime and getTime() or 0
    if not getNodePosition or not getNodeVelocity or not setNodeVelocity or not setNodeGravityEnabled then return end

    local nodeList = getAffectedNodeList()

    if type(print) == "function" and #prisms > 0 and now - lastDebugTime >= DEBUG_INTERVAL then
        lastDebugTime = now
        local names = {}
        for nodeName, _ in pairs(nodeList) do names[#names + 1] = nodeName end
        local listStr = #names > 0 and table.concat(names, ", ") or "(none)"
        local insideStr = ""
        for _, pr in ipairs(prisms) do
            local n = 0
            for _ in pairs(pr.affectedNodes) do n = n + 1 end
            insideStr = insideStr .. n .. " "
        end
        print("[GravityPrism] prisms=" .. #prisms .. " affectedNodes=" .. listStr .. " inside=" .. insideStr)
    end

    local i = 1
    while i <= #prisms do
        local p = prisms[i]
        local endTime = p.spawnTime + p.duration
        if now >= endTime then
            releasePrismAffected(p.affectedNodes)
            if p.visualNodeName and destroyNode then destroyNode(p.visualNodeName) end
            table.remove(prisms, i)
        else
            local angleRad = math.rad(p.rotationSpeed * (now - p.spawnTime))
            local g = (p.strength or DEFAULT_STRENGTH) * WORLD_GRAVITY
            local gx = g * math.sin(angleRad)
            local gy = -g * math.cos(angleRad)
            local gz = 0
            if p.invertGravity then gx, gy, gz = -gx, -gy, -gz end

            local stillInside = {}
            for nodeName, _ in pairs(p.affectedNodes) do
                local nx, ny, nz = getNodePosition(nodeName)
                if pointInSphere(nx, ny, nz, p.x, p.y, p.z, p.radius) then
                    stillInside[nodeName] = true
                    local vx, vy, vz = 0, 0, 0
                    if getNodeVelocity then
                        local a, b, c = getNodeVelocity(nodeName)
                        if a and b and c then vx, vy, vz = a, b, c end
                    end
                    setNodeVelocity(nodeName, vx + gx * deltaTime, vy + gy * deltaTime, vz + gz * deltaTime)
                else
                    setNodeGravityEnabled(nodeName, true)
                end
            end

            for nodeName, _ in pairs(nodeList) do
                if not stillInside[nodeName] and not p.affectedNodes[nodeName] then
                    local nx, ny, nz = getNodePosition(nodeName)
                    if not nx and type(print) == "function" then
                        print("[GravityPrism] getNodePosition('" .. tostring(nodeName) .. "')=nil (node missing?)")
                    end
                    if pointInSphere(nx, ny, nz, p.x, p.y, p.z, p.radius) then
                        if type(print) == "function" then
                            print("[GravityPrism] node '" .. tostring(nodeName) .. "' ENTERED prism at " .. tostring(nx) .. "," .. tostring(ny) .. "," .. tostring(nz))
                        end
                        setNodeGravityEnabled(nodeName, false)
                        stillInside[nodeName] = true
                        local vx, vy, vz = 0, 0, 0
                        if getNodeVelocity then
                            local a, b, c = getNodeVelocity(nodeName)
                            if a and b and c then vx, vy, vz = a, b, c end
                        end
                        setNodeVelocity(nodeName, vx + gx * deltaTime, vy + gy * deltaTime, vz + gz * deltaTime)
                    end
                end
            end
            p.affectedNodes = stillInside
            i = i + 1
        end
    end
end

return {
    place = place,
    update = update,
    defaults = {
        radius = DEFAULT_RADIUS,
        duration = DEFAULT_DURATION,
        rotationSpeed = DEFAULT_ROTATION_SPEED,
        strength = DEFAULT_STRENGTH,
        invertGravity = false,
        blendMode = DEFAULT_BLEND_MODE,
    }
}
