#include "Platform.h"
#include <cstring>
#include <iostream>

#ifdef LINUX_BUILD
    #include <unistd.h>  // For usleep
#endif

#ifdef LINUX_BUILD
    GLFWwindow* window = nullptr;
    
    // Input state tracking
    static uint32_t currentButtons = 0;
    static uint32_t previousButtons = 0;
    
    // Key callback for GLFW
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            // Map GLFW keys to Vita buttons
            switch (key) {
                case GLFW_KEY_UP:
                    currentButtons |= SCE_CTRL_UP;
                    break;
                case GLFW_KEY_DOWN:
                    currentButtons |= SCE_CTRL_DOWN;
                    break;
                case GLFW_KEY_LEFT:
                    currentButtons |= SCE_CTRL_LEFT;
                    break;
                case GLFW_KEY_RIGHT:
                    currentButtons |= SCE_CTRL_RIGHT;
                    break;
                case GLFW_KEY_S:
                    currentButtons |= SCE_CTRL_CROSS;
                    break;
                case GLFW_KEY_D:
                    currentButtons |= SCE_CTRL_CIRCLE;
                    break;
                case GLFW_KEY_A:
                    currentButtons |= SCE_CTRL_SQUARE;
                    break;
                case GLFW_KEY_W:
                    currentButtons |= SCE_CTRL_TRIANGLE;
                    break;
                case GLFW_KEY_Q:
                    currentButtons |= SCE_CTRL_LTRIGGER;
                    break;
                case GLFW_KEY_E:
                    currentButtons |= SCE_CTRL_RTRIGGER;
                    break;
                case GLFW_KEY_ENTER:
                    currentButtons |= SCE_CTRL_START;
                    break;
                case GLFW_KEY_TAB:
                    currentButtons |= SCE_CTRL_SELECT;
                    break;
            }
        } else if (action == GLFW_RELEASE) {
            // Clear buttons on release
            switch (key) {
                case GLFW_KEY_UP:
                    currentButtons &= ~SCE_CTRL_UP;
                    break;
                case GLFW_KEY_DOWN:
                    currentButtons &= ~SCE_CTRL_DOWN;
                    break;
                case GLFW_KEY_LEFT:
                    currentButtons &= ~SCE_CTRL_LEFT;
                    break;
                case GLFW_KEY_RIGHT:
                    currentButtons &= ~SCE_CTRL_RIGHT;
                    break;
                case GLFW_KEY_S:
                    currentButtons &= ~SCE_CTRL_CROSS;
                    break;
                case GLFW_KEY_D:
                    currentButtons &= ~SCE_CTRL_CIRCLE;
                    break;
                case GLFW_KEY_A:
                    currentButtons &= ~SCE_CTRL_SQUARE;
                    break;
                case GLFW_KEY_W:
                    currentButtons &= ~SCE_CTRL_TRIANGLE;
                    break;
                case GLFW_KEY_Q:
                    currentButtons &= ~SCE_CTRL_LTRIGGER;
                    break;
                case GLFW_KEY_E:
                    currentButtons &= ~SCE_CTRL_RTRIGGER;
                    break;
                case GLFW_KEY_ENTER:
                    currentButtons &= ~SCE_CTRL_START;
                    break;
                case GLFW_KEY_TAB:
                    currentButtons &= ~SCE_CTRL_SELECT;
                    break;
            }
        }
    }
    
    bool platformInit() {
        // Initialize GLFW
        if (!glfwInit()) {
            return false;
        }
        
        // Set OpenGL version (use 2.1 for maximum VitaGL compatibility)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        
        // Create window with Vita resolution
        window = glfwCreateWindow(VITA_WIDTH, VITA_HEIGHT, "First Game - Linux Build", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            return false;
        }
        
        // Make context current
        glfwMakeContextCurrent(window);
        
        // Set key callback
        glfwSetKeyCallback(window, keyCallback);
        
        // Initialize GLEW
        if (glewInit() != GLEW_OK) {
            glfwTerminate();
            return false;
        }
        
        // Set viewport
        glViewport(0, 0, VITA_WIDTH, VITA_HEIGHT);
        
        return true;
    }
    
    void platformShutdown() {
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }
    
    void platformPollInput(SceCtrlData& pad) {
        // Check if window is valid
        if (!window) {
            return;
        }
        
        // Update button states
        previousButtons = currentButtons;
        
        // Copy current button state
        pad.buttons = currentButtons;
        
        // Set analog sticks to center (128)
        pad.lx = 128;
        pad.ly = 128;
        pad.rx = 128;
        pad.ry = 128;
        
        // Process GLFW events
        glfwPollEvents();
        
        // Check if window should close
        if (glfwWindowShouldClose(window)) {
            // Signal exit by setting a special button combination
            pad.buttons |= 0x80000000; // Use high bit to signal exit
        }
    }
    
    void platformSwapBuffers() {
        if (!window) {
            return;
        }
        glfwSwapBuffers(window);
    }
    
    void platformSetVSync(bool enabled) {
        glfwSwapInterval(enabled ? 1 : 0);
    }
    
    float platformGetTime() {
        return (float)glfwGetTime();
    }
    
    void platformSleep(int microseconds) {
        usleep(microseconds);
    }
    
#else
    // Vita implementations
    bool platformInit() { 
        // VitaGL is initialized in main(), so just return success
        return true; 
    }
    void platformShutdown() {}
    void platformPollInput(SceCtrlData& pad) {
        // Read Vita controller input
        SceCtrlData ctrlData;
        sceCtrlPeekBufferPositive(0, &ctrlData, 1);
        pad = ctrlData;
    }
    void platformSwapBuffers() {
        vglSwapBuffers(GL_TRUE);
    }
    void platformSetVSync(bool enabled) {
        // VitaGL doesn't have vglSwapInterval, just ignore
        (void)enabled;
    }
    
    float platformGetTime() {
        return (float)sceKernelGetProcessTimeWide() / 1000000.0f;
    }
    
    void platformSleep(int microseconds) {
        sceKernelDelayThread(microseconds);
    }
#endif
