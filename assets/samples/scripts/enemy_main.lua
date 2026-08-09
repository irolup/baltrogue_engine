local health = 100
local maxHealth = 100
local isDead = false
local physicsNodeName = "CapsuleCollision"
local PLAYER_NODE_NAME = "Player"
local wasChasingByEnemy = {}
local rootNodeName = nil
local GROUND_Y_MIN = -2.5
local GROUND_Y_MAX = 1.0
local healthByEnemy = {}
local isDeadByEnemy = {}
local hiddenWhenDeadByEnemy = {}
local debugPosTick = 0

local enemy_chase = nil
local chase_load_failed = false
local function ensureChase()
  if enemy_chase then return enemy_chase end
  if chase_load_failed then return nil end
  local ok, mod = pcall(dofile, "assets/samples/scripts/enemy_chase.lua")
  if ok and mod and mod.updateChase then
    enemy_chase = mod
    return enemy_chase
  end
  chase_load_failed = true
  return nil
end

local function getRootName()
  local n = (type(node) == "function") and node() or node
  if n and n.getName then
    return n:getName()
  end
  return "enemy_" .. tostring(n)
end

function start()
    local n = (type(node) == "function") and node() or node
    if n and n.getName then
        rootNodeName = n:getName()
    else
        rootNodeName = "enemy_" .. tostring(n)
    end

    healthByEnemy[rootNodeName] = health
    isDeadByEnemy[rootNodeName] = isDead

    _G.Enemies = _G.Enemies or {}
    _G.Enemies[rootNodeName] = {
        health = health,
        maxHealth = maxHealth,
        physicsNode = (physicsNodeName and physicsNodeName ~= "") and physicsNodeName or nil,
        takeDamage = function(amount)
            if isDeadByEnemy[rootNodeName] then return end
            local h = (healthByEnemy[rootNodeName] or health) - (amount or 0)
            healthByEnemy[rootNodeName] = math.max(0, h)
            if h <= 0 then
                isDeadByEnemy[rootNodeName] = true
            end
        end,
        isDead = function() return isDeadByEnemy[rootNodeName] or false end,
    }

    _G.GameEnemies = _G.GameEnemies or {}
    _G.GameEnemies[rootNodeName] =
        (physicsNodeName and physicsNodeName ~= "") and physicsNodeName or rootNodeName
end

function update(deltaTime)
    local ename = rootNodeName or getRootName()
    if not ename then return end
    local myDead = isDeadByEnemy[ename]
    if myDead == nil then
        myDead = false
        healthByEnemy[ename] = health
        isDeadByEnemy[ename] = false
    end
    if myDead then
        if _G.Enemies and _G.Enemies[ename] then
            _G.Enemies[ename] = nil
        end
        if _G.GameEnemies then
            _G.GameEnemies[ename] = nil
        end
        if not hiddenWhenDeadByEnemy[ename] and setNodeVisible and ename then
            setNodeVisible(ename, false)
            hiddenWhenDeadByEnemy[ename] = true
        end
        return
    end
    if setOwnerFirstPhysicsChildAngularFactor then
        setOwnerFirstPhysicsChildAngularFactor(0, 0, 0)
    end
    if _G.Enemies and _G.Enemies[ename] then
        _G.Enemies[ename].health = healthByEnemy[ename] or health
    end
end

function fixedUpdate(deltaTime)
    local ename = rootNodeName or getRootName()
    if not ename or isDeadByEnemy[ename] then return end
    local chase = ensureChase()
    local hasNav = type(Nav) == "table" and type(Nav.get_agent) == "function"
    local agent = hasNav and Nav.get_agent(ename) or nil
    if chase and chase.updateChase and agent then
        wasChasingByEnemy[ename] = (chase.updateChase(ename, PLAYER_NODE_NAME, deltaTime, { maxDistance = 30.0 }) == true)
    else
        wasChasingByEnemy[ename] = false
    end
end

function lateUpdate(deltaTime)
    local ename = rootNodeName or getRootName()
    if not ename then return end
    if getPosition and setPosition and (isDeadByEnemy[ename] ~= true) then
        local lx, ly, lz = getPosition()
        if ly and (ly < GROUND_Y_MIN or ly > GROUND_Y_MAX) then
            local newY = math.max(GROUND_Y_MIN, math.min(ly, GROUND_Y_MAX))
            setPosition(lx, newY, lz)
        end
    end
end

function render(renderer)
end

function takeDamage(amount)
    local ename = rootNodeName or getRootName()
    if not ename then return end
    if isDeadByEnemy[ename] then return end
    local h = (healthByEnemy[ename] or health) - (amount or 0)
    healthByEnemy[ename] = math.max(0, h)
    if h <= 0 then
        isDeadByEnemy[ename] = true
    end
end
