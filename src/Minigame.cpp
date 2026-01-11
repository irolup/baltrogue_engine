#include "Minigame.h"

Minigame::Minigame() 
    : active(false)
    , paused(false)
    , name("Base Minigame")
    , camera(nullptr) {
    
    projection = glm::mat4(1.0f);
    view = glm::mat4(1.0f);
}

Minigame::~Minigame() {
    // Don't call virtual functions in destructor
    active = false;
    paused = false;
}

void Minigame::shutdown() {
    active = false;
    paused = false;
}
