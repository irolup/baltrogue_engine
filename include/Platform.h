#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef LINUX_BUILD
    // Linux includes
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
    #include <GL/glu.h>
    
    // Linux input mapping (using bit flags matching Vita SDK values)
    // These values match the official PlayStation Vita SDK (psp2/ctrl.h)
    // to ensure consistency between Linux and Vita builds
    #define SCE_CTRL_SELECT    0x0001  // 1
    #define SCE_CTRL_START     0x0008  // 8
    #define SCE_CTRL_UP        0x0010  // 16
    #define SCE_CTRL_RIGHT     0x0020  // 32
    #define SCE_CTRL_DOWN      0x0040  // 64
    #define SCE_CTRL_LEFT      0x0080  // 128
    #define SCE_CTRL_LTRIGGER  0x0100  // 256
    #define SCE_CTRL_RTRIGGER  0x0200  // 512
    #define SCE_CTRL_TRIANGLE  0x1000  // 4096
    #define SCE_CTRL_SQUARE    0x2000  // 8192
    #define SCE_CTRL_CROSS     0x4000  // 16384
    #define SCE_CTRL_CIRCLE    0x8000  // 32768
    
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
