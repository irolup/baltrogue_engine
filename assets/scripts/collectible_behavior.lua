local startTime = 0
local collected = false
local respawnTimer = 0
local respawnDelay = 6.0
local speedBoostDuration = 3.0
local timeWhenCollected = 0
local accumulatedPauseTime = 0
local pauseStartTime = 0

local playerArea3D = nil
local speedArea3D = nil

local originalSpeedPowerX = 0
local originalSpeedPowerY = 0
local originalSpeedPowerZ = 0
local respawnRadius = 3.0

local lastPausedState = false
local playerControllerNodeName = "Player_Controller"

local playerName = "Player"
local speedPowerUpRootName = "SpeedPowerUpRoot"
local SpeedModelName = "SpeedModel"
local playerAreaNodeName = "PlayerArea3D"
local speedAreaNodeName = "SpeedArea3D"

function start()
    startTime = getTime()

    originalSpeedPowerX, originalSpeedPowerY, originalSpeedPowerZ = 0.0, -1.5, -5.2

    playerArea3D = area3D.getComponent(playerAreaNodeName)
    speedArea3D = area3D.getComponent(speedAreaNodeName)
    
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
            pauseStartTime = getTime()
        else
            if pauseStartTime > 0 then
                accumulatedPauseTime = accumulatedPauseTime + (getTime() - pauseStartTime)
                pauseStartTime = 0
            end
        end
        lastPausedState = isCurrentlyPaused
    end
    
    if isCurrentlyPaused then
        return
    end
    
    local currentTime = getTime()
    
    if not playerArea3D or not speedArea3D then
        if not playerArea3D then
            playerArea3D = area3D.getComponent(playerAreaNodeName)
        end
        if not speedArea3D then
            speedArea3D = area3D.getComponent(speedAreaNodeName)
        end
        return
    end
    
    local playerBodies = playerArea3D.getBodiesInArea()
    local speedBodies = speedArea3D.getBodiesInArea()
    
    if type(playerBodies) ~= "table" then
        print("ERROR: playerBodies is not a table! Value: " .. tostring(playerBodies))
        return
    end
    if type(speedBodies) ~= "table" then
        print("ERROR: speedBodies is not a table! Value: " .. tostring(speedBodies))
        return
    end

    local playerHasSpeed = false
    local speedHasPlayer = false

    if playerArea3D.isBodyInArea(speedAreaNodeName) then
        playerHasSpeed = true
    end
    
    for i = 1, #playerBodies do
        if playerBodies[i] == speedAreaNodeName then
            playerHasSpeed = true
            break
        end
    end

    if speedArea3D.isBodyInArea(playerAreaNodeName) then
        speedHasPlayer = true
    end

    for i = 1, #speedBodies do
        if speedBodies[i] == playerAreaNodeName then
            speedHasPlayer = true
            break
        end
    end

    local collisionDetected = playerHasSpeed or speedHasPlayer
    
    if collisionDetected and not collected then
        collected = true
        
        if callNodeScriptFunctionWithParam then
            callNodeScriptFunctionWithParam(playerControllerNodeName, "activateSpeedBoost", speedBoostDuration)
            print("Speed boost activated for " .. speedBoostDuration .. " seconds!")
        else
            print("ERROR: Cannot call script function - callNodeScriptFunctionWithParam not available!")
        end

        if speedArea3D then
            speedArea3D.setMonitorMode(false)
        end

        setNodePosition(speedPowerUpRootName, 0, 1000, 0)

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

            local angle = math.random() * 2 * math.pi
            local distance = math.random() * respawnRadius
            local newX = originalSpeedPowerX + math.cos(angle) * distance
            local newZ = originalSpeedPowerZ + math.sin(angle) * distance
            local newY = originalSpeedPowerY
            
            setNodePosition(speedPowerUpRootName, newX, newY, newZ)
            print("Power-up respawned at (" .. newX .. ", " .. newY .. ", " .. newZ .. ")")

            if speedArea3D then
                speedArea3D.setMonitorMode(true)
            end
        end
    end
end

function render()
end

function destroy()
end
