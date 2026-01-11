#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef LINUX_BUILD
    // Linux includes
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
    #include <GL/glu.h>
    
    // Linux input mapping (using bit flags like Vita)
    #define SCE_CTRL_UP        0x0001
    #define SCE_CTRL_DOWN      0x0002
    #define SCE_CTRL_LEFT      0x0004
    #define SCE_CTRL_RIGHT     0x0008
    #define SCE_CTRL_CROSS     0x0010
    #define SCE_CTRL_CIRCLE    0x0020
    #define SCE_CTRL_SQUARE    0x0040
    #define SCE_CTRL_TRIANGLE  0x0080
    #define SCE_CTRL_LTRIGGER  0x0100
    #define SCE_CTRL_RTRIGGER  0x0200
    #define SCE_CTRL_START     0x0400
    #define SCE_CTRL_SELECT    0x0800
    
    // Linux input structure
    struct SceCtrlData {
        uint32_t buttons;
        uint8_t lx, ly, rx, ry;
    };
    
    // Linux graphics functions
    #define vglInit(x) (0)
    #define vglSwapBuffers(GL_TRUE) glfwSwapBuffers(window)
    #define vglSetMode(x, y, z) (0)
    #define vglSwapInterval(x) glfwSwapInterval(x)
    
    // Global window pointer for Linux
    extern GLFWwindow* window;
    
#else
    // Vita includes
    #include <vitasdk.h>
    #include <vitaGL.h>
    // SceCtrl functions are available through vitasdk.h
#endif

// Common constants
#define VITA_WIDTH 960
#define VITA_HEIGHT 544

// Platform-specific function declarations
bool platformInit();
void platformShutdown();
void platformPollInput(SceCtrlData& pad);
void platformSwapBuffers();
void platformSetVSync(bool enabled);
float platformGetTime(); // Returns time in seconds
void platformSleep(int microseconds);

#endif // PLATFORM_H
