#ifndef EXAMPLE_MINIGAME_H
#define EXAMPLE_MINIGAME_H

#include "Minigame.h"
#include <vector>

// This is an example of how to create a new minigame
// Copy this structure and modify it for your own game!

struct ExampleObject {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float size;
    bool active;
    
    ExampleObject() : position(0.0f), velocity(0.0f), color(1.0f), size(1.0f), active(false) {}
};

class ExampleMinigame : public Minigame {
public:
    ExampleMinigame();
    ~ExampleMinigame();
    
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
    
    void processInput(const SceCtrlData& pad) override;
    
private:
    // Game state
    int score;
    float timeRemaining;
    float gameDuration;
    bool gameOver;
    
    // Game objects
    std::vector<ExampleObject> objects;
    
    // Game mechanics
    float spawnTimer;
    float spawnInterval;
    
    // Helper methods
    void spawnObject();
    void updateObjects(float deltaTime);
    void drawObject(const ExampleObject& obj);
    void drawUI();
    
    // Game logic
    void checkGameOver();
};

#endif // EXAMPLE_MINIGAME_H
