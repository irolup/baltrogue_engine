local enemyRootName = nil
local health = 100
local maxHealth = 100
local isDead = false
local physicsNodeName = "CapsuleCollision"

local function getRootName()
    local n = (type(node) == "function") and node() or node
    if n and n.getName then
        return n.getName()
    end
    return "enemy"
end

function start()
    enemyRootName = getRootName()
    _G.Enemies = _G.Enemies or {}
    _G.Enemies[enemyRootName] = {
        health = health,
        maxHealth = maxHealth,
        physicsNode = (physicsNodeName and physicsNodeName ~= "") and physicsNodeName or nil,
        takeDamage = function(amount)
            if isDead then return end
            health = health - (amount or 0)
            if health <= 0 then
                health = 0
                isDead = true
            end
        end,
        isDead = function() return isDead end,
    }
end

function update(deltaTime)
    if isDead then
        if _G.Enemies then
            _G.Enemies[enemyRootName] = nil
        end
        if setNodeVisible and enemyRootName then
            setNodeVisible(enemyRootName, false)
        end
        return
    end
    -- Keep upright like the player no rotation when moved 
    local physNode = (physicsNodeName and physicsNodeName ~= "") and physicsNodeName or enemyRootName
    if setNodeAngularFactor and physNode then
        setNodeAngularFactor(physNode, 0, 0, 0)
    end
    -- Keep _G.Enemies in sync
    if _G.Enemies and _G.Enemies[enemyRootName] then
        _G.Enemies[enemyRootName].health = health
    end
end

function fixedUpdate(deltaTime)
end

function lateUpdate(deltaTime)
end

function render(renderer)
end

-- Global so callNodeScriptFunctionWithParam("enemy", "takeDamage", amount) from player/melee
function takeDamage(amount)
    if isDead then return end
    health = health - (amount or 0)
    print("Enemy took damage: " .. amount .. " health: " .. health)
    if health <= 0 then
        health = 0
        print("Enemy is dead")
        isDead = true
    end
end
