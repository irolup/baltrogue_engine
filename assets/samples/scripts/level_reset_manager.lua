-- assets/samples/scripts/level_reset_manager.lua
-- Template: soft reset for Vita-friendly death handling.

local M = {}

local PLAYER_ROOT = "Player"
local PLAYER_COLLISION = "PlayerCollision"
local PLAYER_CAMERA = "PlayerCamera"

local TRACKED_NODES = {
    "dynamic_cube",
    "dynamic_cube_mesh",
    "dynamic_cube_collision",
    "enemy",
    "enemy2",
    "Wall_Test",
    "pushing_wall",
    "platform",
    "ramp",
}

local HOOK_NODES = {
    "Player", -- player_controller.lua
    "enemy", -- enemy_main.lua
    "enemy2",
    "Pause_Menu_Controller",
}

local initial = {
    captured = false,
    node = {},
    player = {},
}

local runtime = {
    isResetting = false,
    transientNodes = {}, -- names created/spawned at runtime
}

local function hasFn(f)
    return type(f) == "function"
end

local function safeGetTransform(nodeName)
    if not hasFn(getNodePosition) or not hasFn(getNodeRotation) or not hasFn(getNodeScale) then
        return nil
    end
    local px, py, pz = getNodePosition(nodeName)
    local rx, ry, rz = getNodeRotation(nodeName)
    local sx, sy, sz = getNodeScale(nodeName)
    if not px or not py or not pz then return nil end
    if not rx or not ry or not rz then return nil end
    if not sx or not sy or not sz then return nil end
    return {
        p = { px, py, pz },
        r = { rx, ry, rz },
        s = { sx, sy, sz },
    }
end

local function safeApplyTransform(nodeName, t)
    if not t then return end
    if hasFn(setNodePosition) then
        setNodePosition(nodeName, t.p[1], t.p[2], t.p[3])
    end
    if hasFn(setNodeRotation) then
        setNodeRotation(nodeName, t.r[1], t.r[2], t.r[3])
    end
    if hasFn(setNodeScale) then
        setNodeScale(nodeName, t.s[1], t.s[2], t.s[3])
    end
end

local function captureNodeState(nodeName)
    local snap = {}
    snap.transform = safeGetTransform(nodeName)

    if hasFn(isNodeVisible) then
        snap.visible = isNodeVisible(nodeName)
    end
    if hasFn(isNodeActive) then
        snap.active = isNodeActive(nodeName)
    end
    return snap
end

local function applyNodeState(nodeName, snap)
    if not snap then return end

    safeApplyTransform(nodeName, snap.transform)

    if snap.visible ~= nil and hasFn(setNodeVisible) then
        setNodeVisible(nodeName, snap.visible)
    end
    if snap.active ~= nil and hasFn(setNodeActive) then
        setNodeActive(nodeName, snap.active)
    end
end

local function callHook(nodeName, fnName)
    if hasFn(callNodeScriptFunction) then
        callNodeScriptFunction(nodeName, fnName)
    end
end

function M.registerTransientNode(nodeName)
    if nodeName and nodeName ~= "" then
        runtime.transientNodes[nodeName] = true
    end
end

function M.captureInitialState()
    if initial.captured then return end

    -- Capture tracked nodes
    for _, n in ipairs(TRACKED_NODES) do
        initial.node[n] = captureNodeState(n)
    end

    -- Player spawn/camera baseline
    initial.player.root = captureNodeState(PLAYER_ROOT)
    initial.player.collision = captureNodeState(PLAYER_COLLISION)
    initial.player.camera = captureNodeState(PLAYER_CAMERA)

    initial.captured = true
end

local function clearTransients()
    if not hasFn(scene) and not hasFn(removeNode) then
        runtime.transientNodes = {}
        return
    end

    for nodeName, _ in pairs(runtime.transientNodes) do
        if hasFn(removeNode) then
            removeNode(nodeName)
        elseif scene and type(scene.destroyNode) == "function" then
            scene.destroyNode(nodeName)
        end
    end

    runtime.transientNodes = {}
end

local function resetPhysicsFor(nodeName)
    if hasFn(setNodeVelocity) then
        setNodeVelocity(nodeName, 0, 0, 0)
    end
    if hasFn(setNodeAngularVelocity) then
        setNodeAngularVelocity(nodeName, 0, 0, 0)
    end
end

function M.softReset(reason)
    if runtime.isResetting then return end
    runtime.isResetting = true

    if not initial.captured then
        M.captureInitialState()
    end

    for _, n in ipairs(HOOK_NODES) do
        callHook(n, "onPreReset")
    end

    clearTransients()

    -- Restore all tracked scene nodes
    for nodeName, snap in pairs(initial.node) do
        applyNodeState(nodeName, snap)
        resetPhysicsFor(nodeName)
    end

    applyNodeState(PLAYER_ROOT, initial.player.root)
    applyNodeState(PLAYER_COLLISION, initial.player.collision)
    applyNodeState(PLAYER_CAMERA, initial.player.camera)
    resetPhysicsFor(PLAYER_COLLISION)

    --Optional post-reset hooks (for health, ammo...)
    for _, n in ipairs(HOOK_NODES) do
        callHook(n, "onPostReset")
    end

    runtime.isResetting = false
end

return M