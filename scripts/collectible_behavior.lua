local startTime = 0
local collected = false
local respawnTimer = 0
local respawnDelay = 3.0
local timeWhenCollected = 0
local accumulatedPauseTime = 0
local pauseStartTime = 0
local score = 0

local playerArea3D = nil
local lemonArea3D = nil

local originalLemonX = 0
local originalLemonY = 0
local originalLemonZ = 0

local originalScoreX = -8.2
local originalScoreY = 7.4
local originalScoreZ = 0.0
local lastPausedState = false
local scoreNodeName = "Score"

local playerName = "Player"
local lemonRootName = "LemonRoot"
local lemonNodeName = "Lemon"
local playerAreaNodeName = "PlayerArea3D"
local lemonAreaNodeName = "LemonArea3D"

function start()
    startTime = getTime()
    
    originalLemonX, originalLemonY, originalLemonZ = 0.0, -1.5, -5.2
    
    playerArea3D = area3D.getComponent(playerAreaNodeName)
    lemonArea3D = area3D.getComponent(lemonAreaNodeName)
    
    local scoreX, scoreY, scoreZ = getNodePosition(scoreNodeName)
    if scoreX ~= nil and not (scoreX == 0 and scoreY == 0 and scoreZ == 0) then
        originalScoreX = scoreX
        originalScoreY = scoreY
        originalScoreZ = scoreZ
    else
        originalScoreX = -8.2
        originalScoreY = 7.4
        originalScoreZ = 0.0
    end
    
    if isGamePaused and isGamePaused() then
        lastPausedState = true
    else
        lastPausedState = false
    end
end

function update(deltaTime)
    local isCurrentlyPaused = false
    if isGamePaused and isGamePaused() then
        isCurrentlyPaused = true
    end
    
    if isCurrentlyPaused ~= lastPausedState then
        if isCurrentlyPaused then
            setNodeVisible(scoreNodeName, false)
            pauseStartTime = getTime()
        else
            if pauseStartTime > 0 then
                accumulatedPauseTime = accumulatedPauseTime + (getTime() - pauseStartTime)
                pauseStartTime = 0
            end
            setNodeVisible(scoreNodeName, true)
        end
        lastPausedState = isCurrentlyPaused
    end
    
    if isCurrentlyPaused then
        return
    end
    
    local currentTime = getTime()
    
    if not playerArea3D or not lemonArea3D then
        if not playerArea3D then
            playerArea3D = area3D.getComponent(playerAreaNodeName)
        end
        if not lemonArea3D then
            lemonArea3D = area3D.getComponent(lemonAreaNodeName)
        end
        return
    end
    
    local playerBodies = playerArea3D.getBodiesInArea()
    local lemonBodies = lemonArea3D.getBodiesInArea()
    
    if type(playerBodies) ~= "table" then
        print("ERROR: playerBodies is not a table! Value: " .. tostring(playerBodies))
        return
    end
    if type(lemonBodies) ~= "table" then
        print("ERROR: lemonBodies is not a table! Value: " .. tostring(lemonBodies))
        return
    end
    
    local playerHasLemon = false
    local lemonHasPlayer = false
    
    if playerArea3D.isBodyInArea(lemonAreaNodeName) then
        playerHasLemon = true
    end
    
    for i = 1, #playerBodies do
        if playerBodies[i] == lemonAreaNodeName then
            playerHasLemon = true
            break
        end
    end
    
    if lemonArea3D.isBodyInArea(playerAreaNodeName) then
        lemonHasPlayer = true
    end
    
    for i = 1, #lemonBodies do
        if lemonBodies[i] == playerAreaNodeName then
            lemonHasPlayer = true
            break
        end
    end
    
    local collisionDetected = playerHasLemon or lemonHasPlayer
    
    if collisionDetected and not collected then
        collected = true
        score = score + 10

        local textNodeName = "Score"
        if _VITA_BUILD then
            textNodeName = "Score"
        end
        renderer.setText(textNodeName, "COLLECTED LEMONS: " .. score)

        if lemonArea3D then
            lemonArea3D.setMonitorMode(false)
        end
        
        setNodePosition(lemonRootName, 0, 1000, 0)
        
        timeWhenCollected = currentTime
        accumulatedPauseTime = 0
        pauseStartTime = 0
        respawnTimer = timeWhenCollected + respawnDelay
    end
    
    if collected then
        local elapsedSinceCollection = currentTime - timeWhenCollected - accumulatedPauseTime
        if elapsedSinceCollection >= respawnDelay then
            collected = false
            accumulatedPauseTime = 0
            pauseStartTime = 0
            
            setNodePosition(lemonRootName, originalLemonX, originalLemonY, originalLemonZ)
            
            if lemonArea3D then
                lemonArea3D.setMonitorMode(true)
            end
        end
    end
end

function render()
end

function destroy()
end
