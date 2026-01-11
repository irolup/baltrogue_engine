#ifndef MINIGAME_H
#define MINIGAME_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include "Platform.h"
#include "Camera.h"
#include "BitmapFont.h"
#include "Menu.h"

class Minigame {
public:
    Minigame();
    virtual ~Minigame();
    
    // Core lifecycle methods
    virtual bool init() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;
    virtual void shutdown() = 0;
    
    // Game state management
    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void reset() = 0;
    
    // Game logic
    virtual void togglePause() { paused = !paused; }
    virtual bool isGameOver() const = 0;
    virtual int getScore() const = 0;
    virtual float getTimeRemaining() const = 0;
    
    // Input handling
    virtual void processInput(const SceCtrlData& pad) = 0;

    //Optional HUD
    virtual void drawHUD(BitmapFont& font) {}

    //Pause menu
    virtual void drawPauseMenu(BitmapFont& font) {}
    virtual MenuAction processPauseMenuInput(const SceCtrlData& pad) { return NO_ACTION; }

    //End game
    virtual void drawEndMenu(BitmapFont& font) {}
    virtual MenuAction processEndMenuInput(const SceCtrlData& pad) { return NO_ACTION; }
    
    // Getters
    bool isActive() const { return active; }
    bool isPaused() const { return paused; }
    const std::string& getName() const { return name; }
    
    // Helper methods
    void setCamera(Camera* cam) { camera = cam; }
    void setProjection(const glm::mat4& proj) { projection = proj; }
    void setView(const glm::mat4& v) { view = v; }
    
protected:
    // Common properties
    bool active;
    bool paused;
    std::string name;
    
    // Camera reference (can be used by minigames)
    Camera* camera;
    
    // Common matrices
    glm::mat4 projection;
    glm::mat4 view;
};

#endif // MINIGAME_H
