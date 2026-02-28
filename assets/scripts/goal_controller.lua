local goalArea3D = nil
local ballArea3D = nil
local ballNodeName = "Ball"
local goalAreaNodeName = "Goal_1_Area3D"
local ballAreaNodeName = "Ball_Area3D"
local scoreNodeName = "Score"
local score = 0
local ballRespawnPosition = {0.0, 0.0, 0.0}
local hasScored = false

function start()
    goalArea3D = area3D.getComponent(goalAreaNodeName)
    ballArea3D = area3D.getComponent(ballAreaNodeName)
    
    local bx, by, bz = getNodePosition(ballNodeName)
    if bx and by and bz then
        ballRespawnPosition[1] = bx
        ballRespawnPosition[2] = by
        ballRespawnPosition[3] = bz
        print("Ball original position captured: (" .. bx .. ", " .. by .. ", " .. bz .. ")")
    else
        print("WARNING: Could not get ball's original position!")
    end
    
    renderer.setText(scoreNodeName, "Score : " .. score)
end

function update(deltaTime)
    if isGamePaused and isGamePaused() then
        return
    end
    
    if not goalArea3D then
        goalArea3D = area3D.getComponent(goalAreaNodeName)
    end
    if not ballArea3D then
        ballArea3D = area3D.getComponent(ballAreaNodeName)
    end
    
    if not goalArea3D or not ballArea3D then
        return
    end
    
    local ballInGoal = false
    
    if goalArea3D.isBodyInArea(ballAreaNodeName) then
        ballInGoal = true
    end
    
    local bodiesInGoal = goalArea3D.getBodiesInArea()
    if type(bodiesInGoal) == "table" then
        for i = 1, #bodiesInGoal do
            if bodiesInGoal[i] == ballAreaNodeName then
                ballInGoal = true
                break
            end
        end
    end
    
    if ballInGoal and not hasScored then
        hasScored = true
        score = score + 1
        
        renderer.setText(scoreNodeName, "Score : " .. score)
        
        if setNodeVelocity then
            setNodeVelocity("SphereCollision", 0, 0, 0)
        end
        
        if setNodeAngularVelocity then
            setNodeAngularVelocity("SphereCollision", 0, 0, 0)
        end
        
        setNodePosition(ballNodeName, ballRespawnPosition[1], ballRespawnPosition[2], ballRespawnPosition[3])
        
        if setNodeVelocity then
            setNodeVelocity("SphereCollision", 0, 0, 0)
        end
        if setNodeAngularVelocity then
            setNodeAngularVelocity("SphereCollision", 0, 0, 0)
        end
        
        print("Goal! Score: " .. score .. " - Respawned ball to (" .. ballRespawnPosition[1] .. ", " .. ballRespawnPosition[2] .. ", " .. ballRespawnPosition[3] .. ")")
        
        local checkX, checkY, checkZ = getNodePosition(ballNodeName)
        if checkX and checkY and checkZ then
            print("Ball position after respawn: (" .. checkX .. ", " .. checkY .. ", " .. checkZ .. ")")
        end
    elseif not ballInGoal and hasScored then
        hasScored = false
    end
end

function render()
end

function destroy()
end
