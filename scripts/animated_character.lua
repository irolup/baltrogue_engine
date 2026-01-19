-- Animation script for Jogging.glb character
-- Uses the skeleton and animation loaded from the model

local characterNodeName = "Model"  -- Name of the node with the animated model (has ModelRenderer and AnimationComponent)
local skeletonName = "Armature" 
local currentAnimation = "Armature|mixamo.com|Layer0"  -- Exact animation name from console

function start()
    print("Animated Character script started!")
    
    -- Wait a moment for auto-detection to work, then use whatever skeleton/clip was found
    -- Or manually set if auto-detection didn't work
    
    -- Get available skeletons
    local skeletons = animation.getAvailableSkeletons()
    print("Available skeletons: " .. #skeletons)
    for i = 1, #skeletons do
        print("  - " .. skeletons[i])
    end
    
    -- Try to use the first available skeleton (auto-detection should have set it)
    local actualSkeletonName = nil
    if #skeletons > 0 then
        actualSkeletonName = skeletons[1]
        print("Using skeleton: " .. actualSkeletonName)
        
        -- Get animations for this skeleton
        local animations = animation.getAnimationClips(actualSkeletonName)
        print("Available animations for " .. actualSkeletonName .. ": " .. #animations)
        for i = 1, #animations do
            print("  - " .. animations[i])
        end
        
        -- Try to set skeleton (might already be set by auto-detection)
        if animation.setSkeleton(characterNodeName, actualSkeletonName) then
            print("Set skeleton: " .. actualSkeletonName)
        else
            print("Skeleton may already be set: " .. actualSkeletonName)
        end
        
        -- Try to set and play the first available animation
        if #animations > 0 then
            local animName = animations[1]
            if animation.setAnimationClip(characterNodeName, animName) then
                print("Set animation clip: " .. animName)
                animation.setLoop(characterNodeName, true)
                animation.setSpeed(characterNodeName, 1.0)
                
                if not animation.isPlaying(characterNodeName) then
                    if animation.play(characterNodeName) then
                        print("Successfully called play() for animation: " .. animName)
                    else
                        print("WARNING: play() returned false for animation: " .. animName)
                    end
                else
                    print("Animation already playing (autoPlay may have started it)")
                end
            else
                print("Failed to set animation clip: " .. animName)
            end
        else
            print("No animations found for skeleton: " .. actualSkeletonName)
        end
    else
        print("ERROR: No skeletons available!")
    end
end

function update(deltaTime)
    -- Animation is playing and updating bone transforms each frame
    -- The AnimationComponent.update() is called automatically by the engine
    
    -- Example: Switch animations based on input
    if input and input.isActionPressed then
        if input.isActionPressed("Jump") then
            -- Switch to jump animation if available
            local animations = animation.getAnimationClips(skeletonName)
            for i = 1, #animations do
                if string.find(animations[i]:lower(), "jump") then
                    animation.setAnimationClip(characterNodeName, animations[i])
                    animation.play(characterNodeName)
                    currentAnimation = animations[i]
                    print("Switched to: " .. currentAnimation)
                    break
                end
            end
        end
    end
end

function render()
    if renderer and renderer.drawText then
        renderer.drawText("Animation: " .. (currentAnimation or "None"), 10, 10)
        renderer.drawText("Skeleton: " .. (skeletonName or "None"), 10, 30)
        renderer.drawText("Press Jump to switch animation", 10, 50)
    end
end

function destroy()
    print("Animated Character script destroyed!")
end

