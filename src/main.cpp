#include "GameController.h"

int main() {
    // Create and initialize the game controller
    GameController controller;
    
    if (!controller.init()) {
        // If initialization fails, the controller will show an error dialog
        return -1;
    }
    
    // Main game loop
    while (controller.isRunning()) {
        controller.update();
        controller.draw();
    }
    
    // Cleanup is handled by the controller destructor
    return 0;
}