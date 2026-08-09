local characterNodeName = "PlayerModel"
local currentAnimation = "Idle"

-- Only keep these
local idleAnimName = "Idle"
local walkAnimName = "Walk"
local runAnimName = "Run"

function start()
    print("Animated Character script started!")

    local skeletons = animation.getAvailableSkeletons()
    if #skeletons == 0 then
        print("ERROR: No skeletons available!")
        return
    end

    local skeletonName = skeletons[1]
    print("Using skeleton: " .. skeletonName)

    local animations = animation.getAnimationClips(skeletonName)
    print("Available animations for " .. skeletonName .. ": " .. #animations)
    for i = 1, #animations do
        print("  - " .. animations[i])
    end

    if not animation.setSkeleton(characterNodeName, skeletonName) then
        print("Skeleton may already be set: " .. skeletonName)
    end

    -- Ensure the animations exist
    local function findAnimation(name)
        for i = 1, #animations do
            if string.lower(animations[i]) == string.lower(name) then
                return animations[i]
            end
        end
        return nil
    end

    idleAnimName = findAnimation(idleAnimName) or animations[1]
    walkAnimName = findAnimation(walkAnimName) or idleAnimName
    runAnimName = findAnimation(runAnimName) or walkAnimName

    -- Set initial animation
    currentAnimation = idleAnimName
    animation.setAnimationClip(characterNodeName, currentAnimation)
    animation.setLoop(characterNodeName, true)
    animation.setSpeed(characterNodeName, 1.0)
    animation.play(characterNodeName)
    print("Set initial animation: " .. currentAnimation)
end

function update(deltaTime)
    if not input then return end

    local moveForward = input.getActionAxis("MoveForward") > 0.1
    local moveBackward = input.getActionAxis("MoveBackward") > 0.1
    local moveLeft = input.getActionAxis("MoveLeft") > 0.1
    local moveRight = input.getActionAxis("MoveRight") > 0.1
    local sprint = input.getActionAxis("Sprint") > 0.1

    local isMoving = moveForward or moveBackward or moveLeft or moveRight

    local desiredAnimation
    if isMoving and sprint then
        desiredAnimation = runAnimName
    elseif isMoving then
        desiredAnimation = walkAnimName
    else
        desiredAnimation = idleAnimName
    end

    if desiredAnimation ~= currentAnimation then
        animation.setAnimationClip(characterNodeName, desiredAnimation)
        animation.setLoop(characterNodeName, true)
        animation.setSpeed(characterNodeName, 1.0)
        animation.play(characterNodeName)
        currentAnimation = desiredAnimation
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