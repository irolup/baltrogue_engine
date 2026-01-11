#ifndef GAME_CONTROLLER_H
#define GAME_CONTROLLER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>
#include "Platform.h"
#include "Camera.h"
#include "Menu.h"
#include "Minigame.h"
#include "ColorMatchGame.h"
#include "BlockStackGame.h"

#ifdef LINUX_BUILD
    // Linux: Define a simple model structure
    struct to_model {
        float* pos;
        float* normals;
        int num_vertices;
    };
#else
    // Vita: Include the actual library
    #include <libtoloader.h>
#endif

// Number of shader sets available
#define SHADERS_NUM 1

// Analogs deadzone
#define ANALOGS_DEADZONE 30

// Macro to check if a button has been pressed
#define CHECK_BTN(x) ((pad.buttons & x) && (!(old_buttons & x)))

// Game states
enum GameState {
    MENU_STATE,
    GAME_SELECT_STATE,
    PLAYING_STATE,
    PAUSED_STATE
};

// Available illumination models
enum {
    LAMBERTIAN
};

// Movement directions for camera
enum CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class GameController {
public:
    GameController();
    ~GameController();
    
    // Main lifecycle methods
    bool init();
    void update();
    void draw();
    void shutdown();
    
    // Platform-specific methods
    bool initPlatform();
    void shutdownPlatform();
    
    // Input handling
    void processInput();
    
    // Shader management
    void loadShader(const char* name, int type);
    
    // Model drawing
    void drawModel(to_model* mdl);
    
    // Error handling
    void fatal_error(const char* fmt, ...);
    
    // Getters
    bool isRunning() const { return running; }
    Camera& getCamera() { return camera; }
    
    // Game state management
    void setGameState(GameState state);
    GameState getGameState() const { return currentGameState; }
    
    // Minigame management
    void addMinigame(std::unique_ptr<Minigame> minigame);
    void selectMinigame(int index);
    void startCurrentMinigame();
    void pauseCurrentMinigame();
    void resumeCurrentMinigame();
    void stopCurrentMinigame();

private:
    // Core state
    bool running;
    GameState currentGameState;
    
    // Menu system
    Menu menu;
    
    // Minigames
    std::vector<std::unique_ptr<Minigame>> minigames;
    int currentMinigameIndex;
    Minigame* currentMinigame;
    
    // Camera
    Camera camera;
    
    // Graphics
    GLuint vshaders[SHADERS_NUM];
    GLuint fshaders[SHADERS_NUM];
    GLuint programs[SHADERS_NUM];
    
    // Uniforms locations
    GLint modelMatrixLoc[SHADERS_NUM];
    GLint viewMatrixLoc[SHADERS_NUM];
    GLint projectionMatrixLoc[SHADERS_NUM];
    GLint normalMatrixLoc[SHADERS_NUM];
    GLint pointLightPositionLoc[SHADERS_NUM];
    GLint KdLoc[SHADERS_NUM];
    GLint diffuseColorLoc[SHADERS_NUM];
    
    
    // Matrices
    glm::mat4 projection;
    
    // Game state
    uint32_t old_buttons;
    GLboolean spinning;
    GLboolean wireframe;
    GLboolean depthTestEnabled;
    GLboolean projectionEnabled;
    GLfloat orientationY;
    GLfloat spin_speed;
    
    // Timing
    GLfloat deltaTime;
    GLfloat lastFrame;
    
    // Lighting
    glm::vec3 lightPos0;
    GLfloat diffuseColor[3];
    GLfloat Kd;
    
    // Shader index
    int shader_idx;
    
    // Helper methods
    bool initGraphics();
    bool initShaders();
    bool initUniforms();
    void updateTiming();
    void handleButtonInput(const SceCtrlData& pad);
    void handleAnalogInput(const SceCtrlData& pad);
    void updateGameState();
    void renderScene();
    
    // Game management
    void initMinigames();
    void handleMenuAction(MenuAction action);
    void updateCurrentMinigame(float deltaTime);
    void drawCurrentMinigame();
};

#endif // GAME_CONTROLLER_H
