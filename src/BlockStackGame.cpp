#include "BlockStackGame.h"
#include <algorithm>

BlockStackGame::BlockStackGame()
    : score(0), timeRemaining(120.0f), gameDuration(120.0f), gameOver(false), inStartMenu(true), currentBlockIndex(0), maxBlocks(20), blockMoveSpeed(8.0f) // Increased from 5.0f for more challenge
      ,
      blockDropSpeed(12.0f) // Increased from 8.0f for faster pace
      ,
      blockMoving(false), blockDropping(false), moveDirection(1.0f), crossPressed(false), gravity(20.0f), groundLevel(0.0f), windStrength(0.0f), windTimer(0.0f), cameraPosition(0.0f, 6.0f, 25.0f), cameraTarget(-90.0f, 0.0f, 0.0f), startMenuUpPressed(false), startMenuDownPressed(false), startMenuCrossPressed(false)
{

    name = "Block Stack";
}

BlockStackGame::~BlockStackGame()
{
    shutdown();
}

bool BlockStackGame::init()
{
    // Initialize blocks vector
    blocks.resize(maxBlocks);

    // Set up camera
    if (camera)
    {
        camera->setPosition(cameraPosition);
        camera->setOrientation(cameraTarget);
    }

    return true;
}

void BlockStackGame::update(float deltaTime)
{
    if (!active || paused || gameOver || inStartMenu)
    {
        return;
    }

    // Update game timer
    timeRemaining -= deltaTime;
    if (timeRemaining <= 0.0f)
    {
        timeRemaining = 0.0f;
        gameOver = true;
        return;
    }

    // Update current block
    updateCurrentBlock(deltaTime);

    // Update falling blocks
    updateFallingBlocks(deltaTime);

    // Check collisions
    checkCollisions();

    // Update wind effects
    windTimer += deltaTime;
    windStrength = sin(windTimer * 2.0f) * 0.3f; // oscillating wind

    // Apply wind to tall blocks
    for (auto &block : blocks)
    {
        if (block.active && !block.falling && block.position.y > groundLevel + 5.0f)
        {
            float windEffect = windStrength * (block.position.y - groundLevel) * 0.001f;
            block.position.x += windEffect * deltaTime;
        }
    }

    // Check stability periodically (every few frames to avoid performance issues)
    static float stabilityCheckTimer = 0.0f;
    stabilityCheckTimer += deltaTime;
    if (stabilityCheckTimer >= 0.1f)
    { // Check every 100ms
        checkBlockStability();
        stabilityCheckTimer = 0.0f;
    }
    // Check game over conditions
    checkGameOver();
}

void BlockStackGame::draw()
{
    if (!active)
        return;

    // Set up view and projection matrices
    if (camera)
    {
        view = camera->getViewMatrix();
    }

    // Set up OpenGL state like ColorMatchGame does
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Only draw game scene if not in start menu
    if (!inStartMenu)
    {
        // Draw ground
        drawGround();

        // Draw placed blocks
        for (const auto &block : blocks)
        {
            if (block.active)
            {
                drawBlock(block);
            }
        }

        // Draw current block
        if (currentBlock.active)
        {
            drawBlock(currentBlock);
        }
    }
    else
    {
        // Draw a simple background for start menu
        drawGround(); // Just draw the ground as background
    }

    // Draw UI
    // drawUI();
}

void BlockStackGame::shutdown()
{
    active = false;
    blocks.clear();
}

void BlockStackGame::start()
{
    active = true;
    paused = false;
    gameOver = false;
    inStartMenu = true;
    selectedIndexStartMenu = 0;
    score = 0;
    timeRemaining = gameDuration;
    currentBlockIndex = 0;
    cameraPosition = glm::vec3(0.0f, 6.0f, 25.0f);
    camera->setPosition(cameraPosition);
    cameraTarget = glm::vec3(-90.0f, 0.0f, 0.0f);
    camera->setOrientation(cameraTarget);

    // Clear all blocks
    for (auto &block : blocks)
    {
        block.active = false;
        block.falling = false;
    }
    
    // Reset button states
    blockDropping = false;
    crossPressed = true; // Start as true to ignore any button that might be held from menu
    
    // Reset start menu button states - start as pressed to ignore menu selection button
    startMenuUpPressed = false;
    startMenuDownPressed = false;
    startMenuCrossPressed = true; // Start as true to ignore any button that might be held from main menu
}

void BlockStackGame::startGameplay()
{
    inStartMenu = false;
    // Spawn first block
    spawnNewBlock();
}

void BlockStackGame::pause()
{
    paused = true;
}

void BlockStackGame::resume()
{
    paused = false;
}

void BlockStackGame::reset()
{
    start();
}

bool BlockStackGame::isGameOver() const
{
    return gameOver;
}

int BlockStackGame::getScore() const
{
    return score;
}

float BlockStackGame::getTimeRemaining() const
{
    return timeRemaining;
}

bool BlockStackGame::isInStartMenu() const
{
    return inStartMenu;
}

void BlockStackGame::drawHUD(BitmapFont &font)
{
    // --- Save minimal state manually ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);

    GLint prevSrcBlend, prevDstBlend;
    glGetIntegerv(GL_BLEND_SRC, &prevSrcBlend);
    glGetIntegerv(GL_BLEND_DST, &prevDstBlend);

    // --- Setup HUD state ---
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Projection -> 2D ortho
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    // Modelview -> Identity
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // --- Draw HUD text ---
    font.setColor(1.0f, 1.0f, 1.0f); // white
    // we need to only show 2 decimal places
    std::string timeString = std::to_string(timeRemaining);
    timeString = timeString.substr(0, timeString.find(".") + 3);
    font.drawText("Time: " + timeString, 1.95f, 2.0f, 4.0f);

    // Draw score
    font.setColor(1.0f, 1.0f, 1.0f); // white
    font.drawText("Score: " + std::to_string(score), 1.95f, 52.0f, 4.0f);

    // --- Restore matrices ---
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // --- Restore state manually ---
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);
}

void BlockStackGame::drawStartMenu(BitmapFont &font)
{
    // --- Save minimal OpenGL state ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);

    GLint prevSrcBlend, prevDstBlend;
    glGetIntegerv(GL_BLEND_SRC, &prevSrcBlend);
    glGetIntegerv(GL_BLEND_DST, &prevDstBlend);

    // --- Setup start menu state ---
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Projection for 2D HUD ---
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // --- Draw start menu text ---
    font.setColor(1.0f, 1.0f, 0.0f); // Yellow title
    font.drawText("Block Stack", 960.0f, 150.0f, 4.0f);

    font.setColor(1.0f, 1.0f, 1.0f); // White description
    font.drawText("Stack blocks as high as you can!", 960.0f, 250.0f, 2.0f);
    font.drawText("Use Left/Right to move, X to drop", 960.0f, 300.0f, 1.8f);

    std::vector<std::string> options = {"Start Game", "Return to Menu"};
    for (size_t i = 0; i < options.size(); ++i)
    {
        bool isSelected = (i == selectedIndexStartMenu);
        float scale = isSelected ? 1.5f : 1.2f;
        font.setColor(isSelected ? 1.0f : 0.7f, 1.0f, 1.0f);
        font.drawText(options[i], 960.0f, 400.0f + i * 100.0f, scale);

#ifndef LINUX_BUILD
        // Highlight background for selection
        if (isSelected)
        {
            float highlightWidth = 400.0f;
            float highlightHeight = 60.0f;
            float highlightX = 960.0f - highlightWidth / 2.0f;
            float highlightY = 400.0f + i * 100.0f - 25.0f;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_QUADS);
            glColor4f(1.0f, 1.0f, 0.0f, 0.6f);
            glVertex2f(highlightX, highlightY);
            glVertex2f(highlightX + highlightWidth, highlightY);
            glVertex2f(highlightX + highlightWidth, highlightY + highlightHeight);
            glVertex2f(highlightX, highlightY + highlightHeight);
            glEnd();
            glDisable(GL_BLEND);
        }
#endif
    }

    // --- Restore matrices ---
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // --- Restore OpenGL state ---
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);
}

void BlockStackGame::drawPauseMenu(BitmapFont &font)
{
    // --- Save minimal OpenGL state ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);

    GLint prevSrcBlend, prevDstBlend;
    glGetIntegerv(GL_BLEND_SRC, &prevSrcBlend);
    glGetIntegerv(GL_BLEND_DST, &prevDstBlend);

    // --- Setup for 2D overlay ---
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // --- Title ---
    font.setColor(1.0f, 1.0f, 0.0f);
    font.drawText("Paused", 960.0f, 200.0f, 4.0f);

    // --- Options ---
    std::vector<std::string> options = {"Resume", "Restart", "Return to Menu"};
    for (size_t i = 0; i < options.size(); ++i)
    {
        bool isSelected = (i == selectedIndexPauseMenu);
        float scale = isSelected ? 1.5f : 1.2f;
        font.setColor(isSelected ? 1.0f : 0.7f, 1.0f, 1.0f);
        font.drawText(options[i], 960.0f, 400.0f + i * 100.0f, scale);

#ifndef LINUX_BUILD
        // Highlight background for selection
        if (isSelected)
        {
            float highlightWidth = 400.0f;
            float highlightHeight = 60.0f;
            float highlightX = 960.0f - highlightWidth / 2.0f;
            float highlightY = 400.0f + i * 100.0f - 25.0f;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_QUADS);
            glColor4f(1.0f, 1.0f, 0.0f, 0.6f);
            glVertex2f(highlightX, highlightY);
            glVertex2f(highlightX + highlightWidth, highlightY);
            glVertex2f(highlightX + highlightWidth, highlightY + highlightHeight);
            glVertex2f(highlightX, highlightY + highlightHeight);
            glEnd();
            glDisable(GL_BLEND);
        }
#endif
    }

    // --- Restore matrices ---
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // --- Restore OpenGL state ---
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);
}

void BlockStackGame::drawEndMenu(BitmapFont &font)
{
    // --- Save minimal OpenGL state ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);

    GLint prevSrcBlend, prevDstBlend;
    glGetIntegerv(GL_BLEND_SRC, &prevSrcBlend);
    glGetIntegerv(GL_BLEND_DST, &prevDstBlend);

    // --- Setup HUD/end menu state ---
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Projection for 2D HUD ---
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // --- Draw end menu text ---
    font.setColor(1.0f, 0.0f, 0.0f);
    font.drawText("Game Over!", 960.0f, 150.0f, 4.0f);

    font.setColor(1.0f, 1.0f, 1.0f);
    font.drawText("Score: " + std::to_string(score), 960.0f, 300.0f, 2.5f);

    std::vector<std::string> options = {"Restart", "Return to Main Menu"};
    for (size_t i = 0; i < options.size(); ++i)
    {
        bool isSelected = (i == selectedIndexEndMenu);
        float scale = isSelected ? 1.5f : 1.2f;
        font.drawText(options[i], 960.0f, 400.0f + i * 100.0f, scale);

#ifndef LINUX_BUILD
        if (isSelected)
        {
            float highlightWidth = 400.0f;
            float highlightHeight = 60.0f;
            float highlightX = 960.0f - highlightWidth / 2.0f;
            float highlightY = 400.0f + i * 100.0f - 25.0f;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_QUADS);
            glColor4f(1.0f, 1.0f, 0.0f, 0.6f);
            glVertex2f(highlightX, highlightY);
            glVertex2f(highlightX + highlightWidth, highlightY);
            glVertex2f(highlightX + highlightWidth, highlightY + highlightHeight);
            glVertex2f(highlightX, highlightY + highlightHeight);
            glEnd();
            glDisable(GL_BLEND);
        }
#endif
    }

    // --- Restore matrices ---
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // --- Restore OpenGL state ---
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);
}



void BlockStackGame::processInput(const SceCtrlData &pad)
{
    static bool startPressed = false;
    if (pad.buttons & SCE_CTRL_START) {
        if (!startPressed) {
            togglePause();
            startPressed = true;
        }
    } else {
        startPressed = false;
    }

    if (!active || paused)
        return;

    // Handle block movement
    if (pad.buttons & SCE_CTRL_LEFT)
    {
        moveDirection = -1.0f;
        blockMoving = true;
    }
    else if (pad.buttons & SCE_CTRL_RIGHT)
    {
        moveDirection = 1.0f;
        blockMoving = true;
    }
    else
    {
        blockMoving = false;
    }

    // Handle block dropping
    if (pad.buttons & SCE_CTRL_CROSS)
    {
        if (!crossPressed && !blockDropping && currentBlock.active)
        {
            blockDropping = true;
            crossPressed = true;
        }
    }
    else
    {
        crossPressed = false;
    }

    // Handle camera movement
    if (pad.buttons & SCE_CTRL_UP)
    {
        cameraPosition.z -= 0.5f;
        if (camera)
        {
            camera->setPosition(cameraPosition);
        }
    }
    if (pad.buttons & SCE_CTRL_DOWN)
    {
        cameraPosition.z += 0.5f;
        if (camera)
        {
            camera->setPosition(cameraPosition);
        }
    }
}

MenuAction BlockStackGame::processStartMenuInput(const SceCtrlData& pad) {
    // Handle D-pad navigation
    size_t optionsCount = 2;

    if (pad.buttons & SCE_CTRL_UP) {
        if (!startMenuUpPressed && selectedIndexStartMenu > 0) {
            selectedIndexStartMenu--;
            startMenuUpPressed = true;
        }
    } else { startMenuUpPressed = false; }

    if (pad.buttons & SCE_CTRL_DOWN) {
        if (!startMenuDownPressed && selectedIndexStartMenu < optionsCount - 1) {
            selectedIndexStartMenu++;
            startMenuDownPressed = true;
        }
    } else { startMenuDownPressed = false; }

    if (pad.buttons & SCE_CTRL_CROSS) {
        if (!startMenuCrossPressed) {
            startMenuCrossPressed = true;
            if (selectedIndexStartMenu == 0) {
                // Start Game selected
                startGameplay();
                return NO_ACTION;
            } else {
                // Return to Menu selected
                return BACK_TO_MENU;
            }
        }
    } else { startMenuCrossPressed = false; }

    return NO_ACTION;
}

MenuAction BlockStackGame::processPauseMenuInput(const SceCtrlData& pad) {
    static bool upPressed = false;
    static bool downPressed = false;
    static bool crossPressed = false;

    size_t optionsCount = 3;

    if (pad.buttons & SCE_CTRL_UP) {
        if (!upPressed && selectedIndexPauseMenu > 0) {
            selectedIndexPauseMenu--;
            upPressed = true;
        }
    } else { upPressed = false; }

    if (pad.buttons & SCE_CTRL_DOWN) {
        if (!downPressed && selectedIndexPauseMenu < optionsCount - 1) {
            selectedIndexPauseMenu++;
            downPressed = true;
        }
    } else { downPressed = false; }

    if (pad.buttons & SCE_CTRL_CROSS) {
        if (!crossPressed) {
            crossPressed = true;
            switch (selectedIndexPauseMenu) {
                case 0: return RESUME_GAME;     // Resume - let GameController handle this
                case 1: return RESTART_GAME;
                case 2: return BACK_TO_MENU;
            }
        }
    } else { crossPressed = false; }

    return NO_ACTION;
}

MenuAction BlockStackGame::processEndMenuInput(const SceCtrlData& pad) {
    // Handle D-pad navigation
    static bool upPressed = false;
    static bool downPressed = false;
    static bool crossPressed = false;

    size_t optionsCount = 2;

    if (pad.buttons & SCE_CTRL_UP) {
        if (!upPressed && selectedIndexEndMenu > 0) {
            selectedIndexEndMenu--;
            upPressed = true;
        }
    } else { upPressed = false; }

    if (pad.buttons & SCE_CTRL_DOWN) {
        if (!downPressed && selectedIndexEndMenu < optionsCount - 1) {
            selectedIndexEndMenu++;
            downPressed = true;
        }
    } else { downPressed = false; }

    if (pad.buttons & SCE_CTRL_CROSS) {
        if (!crossPressed) {
            crossPressed = true;
            return (selectedIndexEndMenu == 0) ? RESTART_GAME : BACK_TO_MENU;
        }
    } else { crossPressed = false; }

    return NO_ACTION;
}



void BlockStackGame::spawnNewBlock()
{
    if (currentBlockIndex >= maxBlocks)
    {
        gameOver = true;
        return;
    }

    // Set up current block
    currentBlock.active = true;
    currentBlock.falling = false;
    currentBlock.fallSpeed = 0.0f;

    // Position the block above the stack
    float highestY = groundLevel;
    for (const auto &block : blocks)
    {
        if (block.active && !block.falling)
        {
            highestY = std::max(highestY, block.position.y + block.size.y);
        }
    }

    currentBlock.position = glm::vec3(0.0f, highestY + 2.0f, 0.0f);
    currentBlock.size = glm::vec3(1.0f, 1.0f, 1.0f);

    // Random color
    currentBlock.color = glm::vec3(
        static_cast<float>(rand()) / RAND_MAX,
        static_cast<float>(rand()) / RAND_MAX,
        static_cast<float>(rand()) / RAND_MAX);

    // Random size variation - much more dramatic sizes for difficulty
    float widthVariation = 0.5f + (static_cast<float>(rand()) / RAND_MAX) * 1.0f;  // 0.5x to 1.5x width
    float heightVariation = 0.3f + (static_cast<float>(rand()) / RAND_MAX) * 0.7f; // 0.3x to 1.0x height
    float depthVariation = 0.5f + (static_cast<float>(rand()) / RAND_MAX) * 1.0f;  // 0.5x to 1.5x depth

    currentBlock.size.x *= widthVariation;
    currentBlock.size.y *= heightVariation;
    currentBlock.size.z *= depthVariation;
}

void BlockStackGame::updateCurrentBlock(float deltaTime)
{
    if (!currentBlock.active || blockDropping)
        return;

    // Automatic continuous movement for timing challenge
    static float autoMoveDirection = 1.0f;
    currentBlock.position.x += autoMoveDirection * blockMoveSpeed * deltaTime;

    // Bounce off boundaries
    float maxX = 8.0f - currentBlock.size.x * 0.5f;
    if (currentBlock.position.x >= maxX)
    {
        currentBlock.position.x = maxX;
        autoMoveDirection = -1.0f;
    }
    else if (currentBlock.position.x <= -maxX)
    {
        currentBlock.position.x = -maxX;
        autoMoveDirection = 1.0f;
    }

    // Manual movement (optional override)
    if (blockMoving)
    {
        currentBlock.position.x += moveDirection * blockMoveSpeed * 0.5f * deltaTime; // Slower manual control
        currentBlock.position.x = std::max(-maxX, std::min(maxX, currentBlock.position.x));
    }
}

void BlockStackGame::updateFallingBlocks(float deltaTime)
{
    // Update current block if dropping
    if (blockDropping && currentBlock.active)
    {
        currentBlock.position.y -= blockDropSpeed * deltaTime;

        // Check if block can be placed
        if (canPlaceBlock(currentBlock))
        {
            placeBlock();
            spawnNewBlock();
            blockDropping = false;
        }

        // Check if block fell below ground
        if (currentBlock.position.y < groundLevel)
        {
            placeBlock();
            spawnNewBlock();
            blockDropping = false;
        }
    }

    // Update falling placed blocks
    for (auto &block : blocks)
    {
        if (block.active && block.falling)
        {
            block.fallSpeed += gravity * deltaTime;
            block.position.y -= block.fallSpeed * deltaTime;

            // Check if block hit ground
            if (block.position.y <= groundLevel)
            {
                block.position.y = groundLevel;
                block.falling = false;
                block.fallSpeed = 0.0f;
            }
        }
    }
}

void BlockStackGame::checkCollisions()
{
    // Check if current block collides with placed blocks
    if (currentBlock.active && !blockDropping)
    {
        for (const auto &block : blocks)
        {
            if (block.active && !block.falling)
            {
                // Simple AABB collision check
                if (currentBlock.position.x < block.position.x + block.size.x &&
                    currentBlock.position.x + currentBlock.size.x > block.position.x &&
                    currentBlock.position.y < block.position.y + block.size.y &&
                    currentBlock.position.y + currentBlock.size.y > block.position.y &&
                    currentBlock.position.z < block.position.z + block.size.z &&
                    currentBlock.position.z + currentBlock.size.z > block.position.z)
                {

                    // Collision detected, start falling
                    blockDropping = true;
                    break;
                }
            }
        }
    }
}

bool BlockStackGame::canPlaceBlock(const StackBlock &block)
{
    // Check if block is supported by another block or ground
    float blockBottom = block.position.y;

    // Check ground
    if (blockBottom <= groundLevel)
    {
        return true;
    }

    // Check other blocks
    for (const auto &otherBlock : blocks)
    {
        if (otherBlock.active && !otherBlock.falling)
        {
            float otherTop = otherBlock.position.y + otherBlock.size.y;
            if (std::abs(blockBottom - otherTop) < 0.1f)
            {
                // Check horizontal overlap
                if (block.position.x < otherBlock.position.x + otherBlock.size.x &&
                    block.position.x + block.size.x > otherBlock.position.x &&
                    block.position.z < otherBlock.position.z + otherBlock.size.z &&
                    block.position.z + block.size.z > otherBlock.position.z)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void BlockStackGame::placeBlock()
{
    if (currentBlockIndex < maxBlocks)
    {
        blocks[currentBlockIndex] = currentBlock;
        blocks[currentBlockIndex].active = true;
        currentBlockIndex++;

        // Calculate score
        calculateScore();

        // Check if any blocks need to fall using center-of-mass stability
        checkBlockStability();
    }

    currentBlock.active = false;
}

void BlockStackGame::checkBlockStability()
{
    // Check stability for all blocks using center-of-mass physics
    bool stabilityChanged = true;
    int iterations = 0;
    const int maxIterations = 10; // Prevent infinite loops

    while (stabilityChanged && iterations < maxIterations)
    {
        stabilityChanged = false;
        iterations++;

        for (auto &block : blocks)
        {
            if (block.active && !block.falling)
            {
                bool isStable = isBlockStable(block);

                if (!isStable)
                {
                    block.falling = true;
                    block.fallSpeed = 0.0f;
                    stabilityChanged = true;
                }
            }
        }
    }
}

bool BlockStackGame::isBlockStable(const StackBlock &block)
{
    // Check if block's center of mass is supported
    glm::vec3 centerOfMass = block.position + block.size * 0.5f;

    // Check if center of mass is over ground
    if (centerOfMass.y <= groundLevel + block.size.y * 0.5f)
    {
        return true;
    }

    // Check if center of mass is supported by other blocks
    for (const auto &otherBlock : blocks)
    {
        if (otherBlock.active && !otherBlock.falling && &otherBlock != &block)
        {
            // Check if this block is directly below our block
            float heightDifference = block.position.y - (otherBlock.position.y + otherBlock.size.y);
            if (heightDifference >= -0.1f && heightDifference <= 0.1f)
            {

                // Check if the center of mass of our block is over the supporting block
                if (centerOfMass.x >= otherBlock.position.x &&
                    centerOfMass.x <= otherBlock.position.x + otherBlock.size.x &&
                    centerOfMass.z >= otherBlock.position.z &&
                    centerOfMass.z <= otherBlock.position.z + otherBlock.size.z)
                {

                    // Additionally check if there's sufficient overlap (at least 30% of block width)
                    float overlapX = std::min(block.position.x + block.size.x, otherBlock.position.x + otherBlock.size.x) -
                                     std::max(block.position.x, otherBlock.position.x);
                    float overlapZ = std::min(block.position.z + block.size.z, otherBlock.position.z + otherBlock.size.z) -
                                     std::max(block.position.z, otherBlock.position.z);

                    float blockArea = block.size.x * block.size.z;
                    float overlapArea = std::max(0.0f, overlapX) * std::max(0.0f, overlapZ);
                    float overlapRatio = overlapArea / blockArea;

                    // Increase stability requirements based on tower height
                    float towerHeight = block.position.y - groundLevel;
                    float heightPenalty = towerHeight / 10.0f;           // Increase requirement by 10% per 10 units height
                    float requiredOverlap = 0.3f + heightPenalty * 0.2f; // Base 30%, increases with height
                    requiredOverlap = std::min(requiredOverlap, 0.8f);   // Cap at 80%

                    if (overlapRatio >= requiredOverlap)
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void BlockStackGame::calculateScore()
{
    // Score based on height and number of blocks
    float highestPoint = groundLevel;
    for (const auto &block : blocks)
    {
        if (block.active && !block.falling)
        {
            highestPoint = std::max(highestPoint, block.position.y + block.size.y);
        }
    }

    score = static_cast<int>(highestPoint * 10.0f) + currentBlockIndex * 5;
}

void BlockStackGame::checkGameOver()
{
    // Game over if too many blocks fell or time ran out
    int fallenBlocks = 0;
    for (const auto &block : blocks)
    {
        if (block.active && block.falling && block.position.y < groundLevel - 5.0f)
        {
            fallenBlocks++;
        }
    }

    if (fallenBlocks > 5)
    {
        gameOver = true;
    }
}

void BlockStackGame::drawBlock(const StackBlock &block)
{
    // Static unit cube (36 vertices for 12 triangles)
    static const float unitCubeVertices[36 * 3] = {
        // Front face
        -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        // Back face
        -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f,
        // Top face
        -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
        // Bottom face
        -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f,
        -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
        // Right face
        0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f,
        0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f,
        // Left face
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f};

    // Colors (all vertices same color)
    float colors[36 * 3];
    for (int i = 0; i < 36; i++)
    {
        colors[i * 3 + 0] = block.color.r;
        colors[i * 3 + 1] = block.color.g;
        colors[i * 3 + 2] = block.color.b;
    }

    glPushMatrix();

    // Translate to block position
    glTranslatef(block.position.x, block.position.y, block.position.z);

    // Scale to block size
    glScalef(block.size.x, block.size.y, block.size.z);

    // Draw cube using vertex arrays
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, unitCubeVertices);
    glColorPointer(3, GL_FLOAT, 0, colors);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glPopMatrix();
}

void BlockStackGame::drawGround()
{
    // Define the 4 vertices of the quad (2 triangles)
    static const float groundVertices[6 * 3] = {
        // First triangle
        -10.0f, groundLevel, -10.0f,
        10.0f, groundLevel, -10.0f,
        10.0f, groundLevel, 10.0f,
        // Second triangle
        -10.0f, groundLevel, -10.0f,
        10.0f, groundLevel, 10.0f,
        -10.0f, groundLevel, 10.0f};

    // Color for all vertices
    float colors[6 * 3];
    for (int i = 0; i < 6; i++)
    {
        colors[i * 3 + 0] = 0.3f;
        colors[i * 3 + 1] = 0.3f;
        colors[i * 3 + 2] = 0.3f;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, groundVertices);
    glColorPointer(3, GL_FLOAT, 0, colors);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

void BlockStackGame::drawUI()
{
    // Draw score and time
    glColor3f(1.0f, 1.0f, 1.0f);

    glColor3f(1.0f, 1.0f, 1.0f);
}
