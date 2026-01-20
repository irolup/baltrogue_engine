local characterNodeName = "PlayerModel"
local currentAnimation = "Idle"
local idleAnimName = "Idle"
local walkAnimName = "Walk"
local walkBackAnimName = "Walk_back"
local walkLeftAnimName = "Walk_left"
local walkRightAnimName = "Walk_right"
local runAnimName = "Run"

function start()
    print("Animated Character script started!")
    
    local skeletons = animation.getAvailableSkeletons()
    print("Available skeletons: " .. #skeletons)
    for i = 1, #skeletons do
        print("  - " .. skeletons[i])
    end
    
    local actualSkeletonName = nil
    if #skeletons > 0 then
        actualSkeletonName = skeletons[1]
        print("Using skeleton: " .. actualSkeletonName)
        
        local animations = animation.getAnimationClips(actualSkeletonName)
        print("Available animations for " .. actualSkeletonName .. ": " .. #animations)
        for i = 1, #animations do
            print("  - " .. animations[i])
        end
        
        if animation.setSkeleton(characterNodeName, actualSkeletonName) then
            print("Set skeleton: " .. actualSkeletonName)
        else
            print("Skeleton may already be set: " .. actualSkeletonName)
        end
        
        local foundIdle = false
        for i = 1, #animations do
            if string.find(animations[i]:lower(), "idle") then
                idleAnimName = animations[i]
                foundIdle = true
                break
            end
        end
        
        for i = 1, #animations do
            local animLower = string.lower(animations[i])
            if string.find(animLower, "walk_back") or string.find(animLower, "walkback") then
                walkBackAnimName = animations[i]
            elseif string.find(animLower, "walk_left") or string.find(animLower, "walkleft") then
                walkLeftAnimName = animations[i]
            elseif string.find(animLower, "walk_right") or string.find(animLower, "walkright") then
                walkRightAnimName = animations[i]
            elseif string.find(animLower, "walk") and not string.find(animLower, "back") and not string.find(animLower, "left") and not string.find(animLower, "right") then
                walkAnimName = animations[i]
            elseif string.find(animLower, "run") then
                runAnimName = animations[i]
            end
        end
        
        if not foundIdle and #animations > 0 then
            idleAnimName = animations[1]
        end
        
        if animation.setAnimationClip(characterNodeName, idleAnimName) then
            print("Set initial animation clip: " .. idleAnimName)
            animation.setLoop(characterNodeName, true)
            animation.setSpeed(characterNodeName, 1.0)
            currentAnimation = idleAnimName
            
            if not animation.isPlaying(characterNodeName) then
                if animation.play(characterNodeName) then
                    print("Successfully started Idle animation")
                else
                    print("WARNING: play() returned false for animation: " .. idleAnimName)
                end
            else
                print("Animation already playing")
            end
        else
            print("Failed to set animation clip: " .. idleAnimName)
        end
    else
        print("ERROR: No skeletons available!")
    end
end

function update(deltaTime)
    if not input then
        return
    end
    
    local moveForwardAxis = input.getActionAxis("MoveForward")
    local moveBackwardAxis = input.getActionAxis("MoveBackward")
    local moveLeftAxis = input.getActionAxis("MoveLeft")
    local moveRightAxis = input.getActionAxis("MoveRight")
    local sprintAxis = input.getActionAxis("Sprint")
    
    local moveForwardHeld = moveForwardAxis > 0.1
    local moveBackwardHeld = moveBackwardAxis > 0.1
    local moveLeftHeld = moveLeftAxis > 0.1
    local moveRightHeld = moveRightAxis > 0.1
    local sprintHeld = sprintAxis > 0.1
    local isMoving = moveForwardHeld or moveBackwardHeld or moveLeftHeld or moveRightHeld
    
    local desiredAnimation = nil
    
    if isMoving and sprintHeld then
        desiredAnimation = runAnimName
    elseif moveBackwardHeld then
        desiredAnimation = walkBackAnimName
    elseif moveLeftHeld then
        desiredAnimation = walkLeftAnimName
    elseif moveRightHeld then
        desiredAnimation = walkRightAnimName
    elseif moveForwardHeld then
        desiredAnimation = walkAnimName
    else
        desiredAnimation = idleAnimName
    end
    
    if desiredAnimation ~= currentAnimation then
        local skeletons = animation.getAvailableSkeletons()
        if #skeletons > 0 then
            local animations = animation.getAnimationClips(skeletons[1])
            local found = false
            
            for i = 1, #animations do
                if animations[i] == desiredAnimation then
                    found = true
                    break
                end
            end
            
            if not found then
                for i = 1, #animations do
                    local animLower = string.lower(animations[i])
                    if desiredAnimation == runAnimName and string.find(animLower, "run") then
                        desiredAnimation = animations[i]
                        found = true
                        break
                    elseif desiredAnimation == walkAnimName and string.find(animLower, "walk") and not string.find(animLower, "back") and not string.find(animLower, "left") and not string.find(animLower, "right") then
                        desiredAnimation = animations[i]
                        found = true
                        break
                    elseif desiredAnimation == walkBackAnimName and (string.find(animLower, "walk_back") or string.find(animLower, "walkback")) then
                        desiredAnimation = animations[i]
                        found = true
                        break
                    elseif desiredAnimation == walkLeftAnimName and (string.find(animLower, "walk_left") or string.find(animLower, "walkleft")) then
                        desiredAnimation = animations[i]
                        found = true
                        break
                    elseif desiredAnimation == walkRightAnimName and (string.find(animLower, "walk_right") or string.find(animLower, "walkright")) then
                        desiredAnimation = animations[i]
                        found = true
                        break
                    elseif desiredAnimation == idleAnimName and string.find(animLower, "idle") then
                        desiredAnimation = animations[i]
                        found = true
                        break
                    end
                end
            end
            
            if found and animation.setAnimationClip(characterNodeName, desiredAnimation) then
                animation.setLoop(characterNodeName, true)
                animation.setSpeed(characterNodeName, 1.0)
                animation.play(characterNodeName)
                currentAnimation = desiredAnimation
                print("Switched to animation: " .. currentAnimation)
            end
        end
    end
end

function render()
    if renderer and renderer.drawText then
        renderer.drawText("Animation: " .. (currentAnimation or "None"), 10, 10)
    end
end

function destroy()
    print("Animated Character script destroyed!")
end

