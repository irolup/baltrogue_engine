#ifndef BLOCK_STACK_GAME_H
#define BLOCK_STACK_GAME_H

#include "Minigame.h"
#include <vector>
#include <random>

struct StackBlock {
    glm::vec3 position;
    glm::vec3 size;
    glm::vec3 color;
    bool active;
    bool falling;
    float fallSpeed;
    
    StackBlock() : position(0.0f), size(1.0f), color(1.0f), active(false), falling(false), fallSpeed(0.0f) {}
};

class BlockStackGame : public Minigame {
public:
    BlockStackGame();
    ~BlockStackGame();
    
    // Minigame interface implementation
    bool init() override;
    void update(float deltaTime) override;
    void draw() override;
    void shutdown() override;
    
    void start() override;
    void pause() override;
    void resume() override;
    void reset() override;
    
    bool isGameOver() const override;
    int getScore() const override;
    float getTimeRemaining() const override;
    bool isInStartMenu() const;

    void drawHUD(BitmapFont& font) override;
    void drawStartMenu(BitmapFont& font);
    void drawPauseMenu(BitmapFont& font) override;    
    void drawEndMenu(BitmapFont& font) override;
    
    void processInput(const SceCtrlData& pad) override;
    MenuAction processStartMenuInput(const SceCtrlData& pad);
    MenuAction processPauseMenuInput(const SceCtrlData& pad) override;
    MenuAction processEndMenuInput(const SceCtrlData& pad) override;
    
private:
    // Game state
    int score;
    float timeRemaining;
    float gameDuration;
    bool gameOver;
    bool inStartMenu;
    int currentBlockIndex;
    int maxBlocks;
    
    // Game objects
    std::vector<StackBlock> blocks;
    StackBlock currentBlock;
    StackBlock nextBlock;
    
    // Game mechanics
    float blockMoveSpeed;
    float blockDropSpeed;
    bool blockMoving;
    bool blockDropping;
    float moveDirection;
    bool crossPressed;
    
    // Physics
    float gravity;
    float groundLevel;
    float windStrength;
    float windTimer;
    
    // Camera
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;
    
    // Helper methods
    void startGameplay();
    void spawnNewBlock();
    void updateCurrentBlock(float deltaTime);
    void updateFallingBlocks(float deltaTime);
    void checkCollisions();
    void drawBlock(const StackBlock& block);
    void drawGround();
    void drawUI();
    
    // Game logic
    bool canPlaceBlock(const StackBlock& block);
    void placeBlock();
    void calculateScore();
    void checkGameOver();
    void checkBlockStability();
    bool isBlockStable(const StackBlock& block);

    // UI properties
    size_t selectedIndexStartMenu = 0;
    size_t selectedIndexPauseMenu = 0;
    size_t selectedIndexEndMenu = 0;
    bool startMenuUpPressed = false;
    bool startMenuDownPressed = false;
    bool startMenuCrossPressed = false;
};

#endif // BLOCK_STACK_GAME_H
