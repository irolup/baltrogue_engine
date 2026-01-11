#ifndef COLOR_MATCH_GAME_H
#define COLOR_MATCH_GAME_H

#include "Minigame.h"
#include <vector>
#include <random>
#include "BitmapFont.h"

struct ColorBlock {
    glm::vec3 position;
    glm::vec3 color;
    float size;
    bool active;
    float fallSpeed;
    
    ColorBlock() : position(0.0f), color(1.0f), size(0.5f), active(false), fallSpeed(50.0f) {}
};

struct ColorZone {
    glm::vec3 position;
    glm::vec3 color;
    float size;
    bool isCorrect;
    
    ColorZone() : position(0.0f), color(1.0f), size(1.0f), isCorrect(false) {}
};

class ColorMatchGame : public Minigame {
public:
    ColorMatchGame();
    ~ColorMatchGame();
    
    // Minigame interface implementation
    bool init() override;
    void update(float deltaTime) override;
    void draw() override;
    void shutdown() override;
    
    void start() override;
    void pause() override;
    void resume() override;
    void reset() override;
    
    void togglePause() override;
    bool isGameOver() const override;

    int getScore() const override;
    float getTimeRemaining() const override;

    void drawHUD(BitmapFont& font) override;
    void drawStartMenu(BitmapFont& font);
    void drawPauseMenu(BitmapFont& font) override;    
    void drawEndMenu(BitmapFont& font) override;

    void processInput(const SceCtrlData& pad) override;
    MenuAction processStartMenuInput(const SceCtrlData& pad);
    MenuAction processPauseMenuInput(const SceCtrlData& pad) override;
    MenuAction processEndMenuInput(const SceCtrlData& pad) override;
    
    bool isInStartMenu() const;
    
private:
    // Game state
    int score;
    float timeRemaining;
    float gameDuration;
    bool gameOver;
    bool inStartMenu;
    
    // Game objects
    std::vector<ColorBlock> blocks;
    std::vector<ColorZone> zones;
    
    // Game mechanics
    float blockSpawnTimer;
    float blockSpawnInterval;
    int maxBlocks;
    int currentBlockCount;
    
    // Random generation
    std::mt19937 rng;
    std::uniform_real_distribution<float> colorDist;
    std::uniform_real_distribution<float> positionDist;
    
    // Camera and rendering
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;
    
    // Timing for input processing
    float lastDeltaTime;
    
    // Helper methods
    void startGameplay();
    void spawnBlock();
    void updateBlocks(float deltaTime);
    void drawBlock(const ColorBlock& block);
    void drawZone(const ColorZone& zone);
    void drawSpawnMarkers();
    
    // Game logic
    void generateZones();
    glm::vec3 getRandomColor();
    bool isColorMatch(const glm::vec3& color1, const glm::vec3& color2);

    // UI properties
    size_t selectedIndexStartMenu = 0;
    size_t selectedIndexPauseMenu = 0;
    size_t selectedIndexEndMenu = 0;
    bool startMenuUpPressed = false;
    bool startMenuDownPressed = false;
    bool startMenuCrossPressed = false;   
};

#endif // COLOR_MATCH_GAME_H