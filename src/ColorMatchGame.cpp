#include "ColorMatchGame.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#ifdef LINUX_BUILD
#include <GL/glu.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ColorMatchGame::ColorMatchGame() 
    : score(0)
    , timeRemaining(60.0f)
    , gameDuration(60.0f)
    , gameOver(false)
    , inStartMenu(true)
    , blockSpawnTimer(0.0f)
    , blockSpawnInterval(3.0f)
    , maxBlocks(6)
    , currentBlockCount(0)
    , cameraPosition(-27.0f, 8.5f, 0.0f)
    , cameraTarget(0.0f, 0.0f, 0.0f)
    , lastDeltaTime(0.0f)
    , startMenuUpPressed(false)
    , startMenuDownPressed(false)
    , startMenuCrossPressed(false){
    
    name = "Color Match";
    
    // Initialize random number generator
    std::random_device rd;
    rng.seed(rd());
    colorDist = std::uniform_real_distribution<float>(0.0f, 1.0f);
    positionDist = std::uniform_real_distribution<float>(-8.0f, 8.0f);
}

ColorMatchGame::~ColorMatchGame() {
    shutdown();
}

bool ColorMatchGame::init() {
    // Initialize blocks
    blocks.resize(maxBlocks);
    for (auto& block : blocks) {
        block.active = false;
    }
    
    // Generate color zones
    generateZones();
    
    // Set up camera for better view of the game area
    if (camera) {
        camera->setPosition(cameraPosition);
        camera->setOrientation(cameraTarget); 
    }
    
    return true;
}

void ColorMatchGame::update(float deltaTime) {
    if (!active || paused || gameOver || inStartMenu) {
        return;
    }
    // Store deltaTime for input processing
    lastDeltaTime = deltaTime;
    
    
    // Update game timer
    timeRemaining -= deltaTime;
    if (timeRemaining <= 0.0f) {
        timeRemaining = 0.0f;
        gameOver = true;
        return;
    }
    
    // Update block spawning
    blockSpawnTimer += deltaTime;
    if (blockSpawnTimer >= blockSpawnInterval && currentBlockCount < maxBlocks) {
        spawnBlock();
        blockSpawnTimer = 0.0f;
    }
    
    // Update blocks
    updateBlocks(deltaTime);
}

void ColorMatchGame::draw() {
    if (!active) return;

    if (camera) view = camera->getViewMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Only draw game scene if not in start menu
    if (!inStartMenu) {
        // Draw zones and blocks first
        for (const auto& zone : zones) drawZone(zone);
        for (const auto& block : blocks) if(block.active) drawBlock(block);

        // Draw pulsing spawn markers
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        drawSpawnMarkers();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    // For start menu, we'll just show a clean background (no zones/blocks)

}

void ColorMatchGame::shutdown() {
    active = false;
    blocks.clear();
    zones.clear();
}

void ColorMatchGame::start() {
    active = true;
    paused = false;
    gameOver = false;
    inStartMenu = true;
    selectedIndexStartMenu = 0;
    score = 0;
    timeRemaining = gameDuration;
    currentBlockCount = 0;
    blockSpawnTimer = 0.0f;
    cameraPosition = glm::vec3(-27.0f, 8.5f, 0.0f);
    camera->setPosition(cameraPosition);
    cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    camera->setOrientation(cameraTarget);
    
    // Clear all blocks
    for (auto& block : blocks) {
        block.active = false;
    }
    
    // Reset start menu button states - start as pressed to ignore menu selection button
    startMenuUpPressed = false;
    startMenuDownPressed = false;
    startMenuCrossPressed = true; // Start as true to ignore any button that might be held from main menu
}

void ColorMatchGame::startGameplay() {
    inStartMenu = false;
    // Generate initial zones when gameplay starts
    generateZones();
}

void ColorMatchGame::pause() {
    paused = true;
}

void ColorMatchGame::resume() {
    paused = false;
}

void ColorMatchGame::reset() {
    start();
}

void ColorMatchGame::togglePause() {
    paused = !paused;
}

bool ColorMatchGame::isGameOver() const {
    return gameOver;
}

int ColorMatchGame::getScore() const {
    return score;
}

float ColorMatchGame::getTimeRemaining() const {
    return timeRemaining;
}

bool ColorMatchGame::isInStartMenu() const {
    return inStartMenu;
}

void ColorMatchGame::drawHUD(BitmapFont& font) {
    // --- Save minimal state manually ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled     = glIsEnabled(GL_BLEND);

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

    // Draw time remaining
    font.setColor(1.0f, 1.0f, 1.0f); // white
    //we need to only show 2 decimal places
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
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled)     glEnable(GL_BLEND);     else glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);
}

void ColorMatchGame::drawStartMenu(BitmapFont& font) {
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
    font.drawText("Color Match", 960.0f, 150.0f, 4.0f);

    font.setColor(1.0f, 1.0f, 1.0f); // White description
    font.drawText("Match falling blocks to colored zones!", 960.0f, 250.0f, 2.0f);
    font.drawText("Use Left/Right to move blocks", 960.0f, 300.0f, 1.8f);

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


void ColorMatchGame::drawPauseMenu(BitmapFont& font) {
    // --- Save minimal OpenGL state ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled     = glIsEnabled(GL_BLEND);

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
    std::vector<std::string> options = { "Resume", "Restart", "Return to Menu" };
    for (size_t i = 0; i < options.size(); ++i) {
        bool isSelected = (i == selectedIndexPauseMenu);
        float scale = isSelected ? 1.5f : 1.2f;
        font.setColor(isSelected ? 1.0f : 0.7f, 1.0f, 1.0f);
        font.drawText(options[i], 960.0f, 400.0f + i * 100.0f, scale);

    #ifndef LINUX_BUILD
        // Highlight background for selection
        if (isSelected) {
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
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled)     glEnable(GL_BLEND);     else glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);

}

void ColorMatchGame::drawEndMenu(BitmapFont& font) {
    // --- Save minimal OpenGL state ---
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled     = glIsEnabled(GL_BLEND);

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

    std::vector<std::string> options = { "Restart", "Return to Main Menu" };
    for (size_t i = 0; i < options.size(); ++i) {
        bool isSelected = (i == selectedIndexEndMenu);
        float scale = isSelected ? 1.5f : 1.2f;
        font.drawText(options[i], 960.0f, 400.0f + i * 100.0f, scale);

    #ifndef LINUX_BUILD
        if (isSelected) {
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
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled)     glEnable(GL_BLEND);     else glDisable(GL_BLEND);
    glBlendFunc(prevSrcBlend, prevDstBlend);
}


void ColorMatchGame::processInput(const SceCtrlData& pad) {
    static bool startPressed = false;
    if (pad.buttons & SCE_CTRL_START) {
        if (!startPressed) {
            togglePause();
            startPressed = true;
        }
    } else {
        startPressed = false;
    }
    
    if (!active || paused) return;
    // Handle block control - guide the falling blocks
    for (auto& block : blocks) {
        if (block.active && block.position.y > 2.0f) { // Only control blocks that are still falling
            // Cross button - move block backwards - PC X
            if (pad.buttons & SCE_CTRL_CROSS) {
                block.position.x -= 10.0f * lastDeltaTime;
                // Clamp to playfield boundaries
                if (block.position.x < -8.0f) block.position.x = -8.0f;
            }
            
            // Triangle button - move block forwards  - PC T
            if (pad.buttons & SCE_CTRL_TRIANGLE) {
                block.position.x += 10.0f * lastDeltaTime;
                // Clamp to playfield boundaries
                if (block.position.x > 8.0f) block.position.x = 8.0f;
            }

            // Square button - move left - PC S
            if (pad.buttons & SCE_CTRL_SQUARE) {
                block.position.z -= 10.0f * lastDeltaTime;
                // Clamp to playfield boundaries
                if (block.position.z > 8.0f) block.position.z = 8.0f;
            }

            // Circle button - move right - PC C
            if (pad.buttons & SCE_CTRL_CIRCLE) {
                block.position.z += 10.0f * lastDeltaTime;
                // Clamp to playfield boundaries
                if (block.position.z < -8.0f) block.position.z = -8.0f;
            }
            
            // Only control the first active block (most recent one)
            break;
        }
    }
    
    // Enhanced camera controls for better view
    bool cameraChanged = false;
    
    float moveSpeed = 5.0f;

    // Shoulder buttons: Move camera up/down (Y-axis)
    if (pad.buttons & SCE_CTRL_LTRIGGER) {
        cameraPosition.y += moveSpeed * lastDeltaTime;
        cameraChanged = true;
    }
    if (pad.buttons & SCE_CTRL_RTRIGGER) {
        cameraPosition.y -= moveSpeed * lastDeltaTime;
        cameraChanged = true;
    }

    // Apply changes
    if (cameraChanged && camera) {
        camera->setPosition(cameraPosition);
    }
    
}

MenuAction ColorMatchGame::processStartMenuInput(const SceCtrlData& pad) {
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

MenuAction ColorMatchGame::processPauseMenuInput(const SceCtrlData& pad) {
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

MenuAction ColorMatchGame::processEndMenuInput(const SceCtrlData& pad) {
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

void ColorMatchGame::spawnBlock() {
    for (auto& block : blocks) {
        if (!block.active) {
            block.active = true;
            block.position = glm::vec3(positionDist(rng), 15.0f, positionDist(rng));
            block.color = getRandomColor();
            //block.fallSpeed = 5.0f + (rand() % 20);
            block.fallSpeed = 5.0f;
            currentBlockCount++;
            break;
        }
    }
}

void ColorMatchGame::updateBlocks(float deltaTime) {
    for (auto& block : blocks) {
        if (block.active) {
            // Update position
            block.position.y -= block.fallSpeed * deltaTime;
            
            // Check if block hit the ground level (zones are at Y=0)
            float groundLevel = 0.0f;
            float blockBottomY = block.position.y - (block.size * 0.5f);
            
            if (blockBottomY <= groundLevel) {
                // Block hit the ground - stop it and check for zone collision
                block.position.y = groundLevel + (block.size * 0.5f);
                
                // Find the closest zone for more efficient collision detection
                bool landedOnZone = false;
                float closestDistance = 1000.0f;
                const ColorZone* closestZone = nullptr;
                
                for (const auto& zone : zones) {
                    // Quick distance check before detailed collision
                    float dx = block.position.x - zone.position.x;
                    float dz = block.position.z - zone.position.z;
                    float distance = dx * dx + dz * dz; // Squared distance (faster than sqrt)
                    
                    if (distance < closestDistance) {
                        float zoneRadius = zone.size * 0.5f;
                        // Check if block center is within zone boundaries (X and Z only)
                        if (std::abs(dx) < zoneRadius && std::abs(dz) < zoneRadius) {
                            landedOnZone = true;
                            closestZone = &zone;
                            closestDistance = distance;
                        }
                    }
                }
                
                if (landedOnZone && closestZone) {
                    // Check color match with the closest zone
                    if (isColorMatch(block.color, closestZone->color)) {
                        // Correct color match
                        score += 50;
                    } else {
                        // Wrong color - penalty
                        score = std::max(0, score - 25);
                    }
                } else {
                    // If block didn't land on any zone, it misses - penalty
                    score = std::max(0, score - 10);
                }
                #ifdef LINUX_BUILD
                    printf("[DEBUG] Score: %d\n", score);
                #endif
                // Remove block after scoring
                block.active = false;
                currentBlockCount--;
            }
        }
    }
}

void ColorMatchGame::drawBlock(const ColorBlock& block) {
    // Static unit cube (36 vertices for 12 triangles)
    static const float unitCubeVertices[36 * 3] = {
        // Front face
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        // Back face
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
        // Top face
        -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
        // Bottom face
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
        // Right face
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
        // Left face
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f
    };

    // Colors (all vertices same color)
    float colors[36 * 3];
    for(int i = 0; i < 36; i++) {
        colors[i*3 + 0] = block.color.r;
        colors[i*3 + 1] = block.color.g;
        colors[i*3 + 2] = block.color.b;
    }

    glPushMatrix();

    // Translate to block position
    glTranslatef(block.position.x, block.position.y, block.position.z);

    // Scale to block size
    glScalef(block.size, block.size, block.size);

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

void ColorMatchGame::drawZone(const ColorZone& zone) {
    float size = zone.size * 0.5f;

    // Quad as 2 triangles (6 vertices)
    float vertices[6 * 3] = {
        -size, 0.0f, -size,  size, 0.0f, -size,  size, 0.0f, size,
        -size, 0.0f, -size,  size, 0.0f, size, -size, 0.0f, size
    };

    float colors[6 * 3];
    for (int i = 0; i < 6; i++) {
        colors[i*3+0] = zone.color.r;
        colors[i*3+1] = zone.color.g;
        colors[i*3+2] = zone.color.b;
    }

    glPushMatrix();
    glTranslatef(zone.position.x, zone.position.y, zone.position.z);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glColorPointer(3, GL_FLOAT, 0, colors);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glPopMatrix();
}

void ColorMatchGame::drawSpawnMarkers() {
    const float spawnHeight = 15.0f;
    const float markerSize = 0.3f;

    float pulseTime = blockSpawnTimer / blockSpawnInterval;
    float pulse = 0.5f + 0.5f * sin(pulseTime * 3.14159f * 4);

    std::vector<glm::vec3> spawnPositions = {
        glm::vec3(-6.0f, spawnHeight, -6.0f),
        glm::vec3(6.0f, spawnHeight, -6.0f),
        glm::vec3(-6.0f, spawnHeight, 6.0f),
        glm::vec3(6.0f, spawnHeight, 6.0f)
    };

    for (const auto& pos : spawnPositions) {
        float scale = markerSize * (0.8f + 0.4f * pulse);

        // Cube vertices centered at origin
        float s = 0.5f;
        float vertices[36*3] = {
            -s,-s,-s,  s,-s,-s,  s, s,-s,
            -s,-s,-s,  s, s,-s, -s, s,-s,
            s,-s,s,  -s,-s,s,  -s, s,s,
            s,-s,s,  -s, s,s,  s, s,s,
            -s, s,-s,  s, s,-s,  s, s,s,
            -s, s,-s,  s, s,s,  -s, s,s,
            -s,-s,-s,  s,-s,-s,  s,-s,s,
            -s,-s,-s,  s,-s,s,  -s,-s,s,
            s,-s,s,  s,-s,-s,  s,s,-s,
            s,-s,s,  s,s,-s,  s,s,s,
            -s,-s,-s,  -s,-s,s,  -s,s,s,
            -s,-s,-s,  -s,s,s,  -s,s,-s
        };

        float colors[36*3];
        for(int i=0;i<36;i++){
            colors[i*3+0] = 1.0f;          // Yellowish color
            colors[i*3+1] = 1.0f;
            colors[i*3+2] = 0.8f;
        }

        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glScalef(scale, scale, scale);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices);
        glColorPointer(3, GL_FLOAT, 0, colors);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);

        glPopMatrix();
    }
}

void ColorMatchGame::generateZones() {
    zones.clear();
    
    // Create 4 color zones in a square pattern
    std::vector<glm::vec3> zonePositions = {
        glm::vec3(-4.0f, 0.0f, -4.0f),
        glm::vec3(4.0f, 0.0f, -4.0f),
        glm::vec3(-4.0f, 0.0f, 4.0f),
        glm::vec3(4.0f, 0.0f, 4.0f)
    };
    
    std::vector<glm::vec3> zoneColors = {
        glm::vec3(1.0f, 0.0f, 0.0f), // Red
        glm::vec3(0.0f, 1.0f, 0.0f), // Green
        glm::vec3(0.0f, 0.0f, 1.0f), // Blue
        glm::vec3(1.0f, 1.0f, 0.0f)  // Yellow
    };
    
    for (size_t i = 0; i < zonePositions.size(); ++i) {
        ColorZone zone;
        zone.position = zonePositions[i];
        zone.color = zoneColors[i];
        zone.size = 2.0f;
        zones.push_back(zone);
    }
}

glm::vec3 ColorMatchGame::getRandomColor() {
    // Return one of the zone colors randomly
    std::vector<glm::vec3> colors = {
        glm::vec3(1.0f, 0.0f, 0.0f), // Red
        glm::vec3(0.0f, 1.0f, 0.0f), // Green
        glm::vec3(0.0f, 0.0f, 1.0f), // Blue
        glm::vec3(1.0f, 1.0f, 0.0f)  // Yellow
    };
    
    return colors[rand() % colors.size()];
}

bool ColorMatchGame::isColorMatch(const glm::vec3& color1, const glm::vec3& color2) {
    // Simple color matching with some tolerance
    float tolerance = 0.1f;
    return (std::abs(color1.r - color2.r) < tolerance &&
            std::abs(color1.g - color2.g) < tolerance &&
            std::abs(color1.b - color2.b) < tolerance);
}


