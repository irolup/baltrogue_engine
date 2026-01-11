#include "GameController.h"
#ifdef LINUX_BUILD
#include <GL/glu.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <cstring>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#ifdef LINUX_BUILD
    #include <cstdlib>  // For exit()
#endif

GameController::GameController() 
    : running(false)
    , currentGameState(MENU_STATE)
    , currentMinigameIndex(-1)
    , currentMinigame(nullptr)
    , spinning(GL_TRUE)
    , wireframe(GL_FALSE)
    , depthTestEnabled(GL_TRUE)
    , projectionEnabled(GL_TRUE)
    , orientationY(0.0f)
    , spin_speed(30.0f)
    , deltaTime(0.0f)
    , lastFrame(0.0f)
    , old_buttons(0)
    , shader_idx(LAMBERTIAN)
    , lightPos0(5.0f, 10.0f, 10.0f)
    , Kd(0.5f) {
    
    // Initialize diffuse color
    diffuseColor[0] = 1.0f;
    diffuseColor[1] = 0.0f;
    diffuseColor[2] = 0.0f;
    
}

GameController::~GameController() {
    shutdown();
}

bool GameController::init() {
    // Initialize platform (graphics, input, etc.)
    if (!initPlatform()) {
        fatal_error("Failed to initialize platform");
        return false;
    }
    
    // Initialize graphics
    if (!initGraphics()) {
        fatal_error("Failed to initialize graphics");
        return false;
    }
    
    // Initialize shaders
    if (!initShaders()) {
        fatal_error("Failed to initialize shaders");
        return false;
    }
    
    // Set default shader BEFORE setting uniforms
    glUseProgram(programs[shader_idx]);
    
    // Check for errors after setting shader
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fatal_error("OpenGL error after setting shader: %d", error);
        return false;
    }
    
    // Initialize uniforms (after program is bound)
    if (!initUniforms()) {
        fatal_error("Failed to initialize uniforms");
        return false;
    }
    
    // Setup camera
    camera.update();
    
    // Initialize menu
    if (!menu.init()) {
        fatal_error("Failed to initialize menu");
        return false;
    }
    
    // Initialize minigames
    initMinigames();
    
    // Test basic OpenGL state
    error = glGetError();
    if (error != GL_NO_ERROR) {
        fatal_error("OpenGL error after initialization: %d", error);
        return false;
    }
    
    running = true;
    return true;
}

bool GameController::initPlatform() {
    return platformInit();
}

bool GameController::initGraphics() {
#ifdef LINUX_BUILD
    // Linux: Graphics already initialized by platform
    // Setting screen clear color
    glClearColor(0.26f, 0.46f, 0.98f, 1.0f);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fatal_error("OpenGL error after glClearColor: %d", error);
        return false;
    }
    
    // Enabling depth test
    glEnable(GL_DEPTH_TEST);
    
    // Check for OpenGL errors
    error = glGetError();
    if (error != GL_NO_ERROR) {
        fatal_error("OpenGL error after glEnable(GL_DEPTH_TEST): %d", error);
        return false;
    }
    
    // Projection matrix: FOV angle, aspect ratio, near and far planes
    projection = glm::perspective(45.0f, VITA_WIDTH / (float)VITA_HEIGHT, 0.1f, 10000.0f);
#else
    // Vita: Initialize graphics device
    if (vglInit(0x800000) != 0) {
        fatal_error("vglInit failed");
        return false;
    }
    
    // Enabling sampling for the analogs
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    
    // Setting screen clear color
    glClearColor(0.26f, 0.46f, 0.98f, 1.0f);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fatal_error("OpenGL error after glClearColor: %d", error);
        return false;
    }
    
    // Enabling depth test
    glEnable(GL_DEPTH_TEST);
    
    // Check for OpenGL errors
    error = glGetError();
    if (error != GL_NO_ERROR) {
        fatal_error("OpenGL error after glEnable(GL_DEPTH_TEST): %d", error);
        return false;
    }
    
    // Projection matrix: FOV angle, aspect ratio, near and far planes
    projection = glm::perspective(45.0f, 960.0f / 544.0f, 0.1f, 10000.0f);
#endif
    
    return true;
}

bool GameController::initShaders() {
    loadShader("lambertian", LAMBERTIAN);
    return true;
}

bool GameController::initUniforms() {
    // Setting constant uniform values
    for (int i = 0; i < SHADERS_NUM; i++) {
        glUniform3fv(diffuseColorLoc[i], 1, diffuseColor);
        glUniform1f(KdLoc[i], Kd);
        if (projectionEnabled) {
            glUniformMatrix4fv(projectionMatrixLoc[i], 1, GL_FALSE, glm::value_ptr(projection));
        }
        glUniform3fv(pointLightPositionLoc[i], 1, glm::value_ptr(lightPos0));
    }
    return true;
}

void GameController::update() {
    if (!running) return;
    
    updateTiming();
    processInput();
    
    // Debug: Show we're in the main loop
    // Removed debug sleep that was causing freezes every 60 frames
    
    // Update based on current game state
    switch (currentGameState) {
        case MENU_STATE:
        case GAME_SELECT_STATE:
            menu.update(deltaTime);
            break;
        case PLAYING_STATE:
            if (currentMinigame) {
                updateCurrentMinigame(deltaTime);
            }
            camera.update();
            break;
        case PAUSED_STATE:
            // Paused - no updates
            break;
    }
}

void GameController::updateTiming() {
    // Calculating delta time in seconds
    GLfloat currentFrame = platformGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
}

void GameController::processInput() {
    // Reading inputs
    SceCtrlData pad;
#ifdef LINUX_BUILD
    platformPollInput(pad);
    
    // Check for exit signal
    if (pad.buttons & 0x80000000) {
        running = false;
        return;
    }
#else
    sceCtrlPeekBufferPositive(0, &pad, 1);
#endif
    
    // Handle input based on current game state
    switch (currentGameState) {
        case MENU_STATE:
        case GAME_SELECT_STATE:
            {
                MenuAction action = menu.processInput(pad);
                if (action != NO_ACTION) {
                    handleMenuAction(action);
                }
            }
            break;
        case PLAYING_STATE:
        if (currentMinigame) {
            if (!currentMinigame) break;

            // Check if this is BlockStackGame and it's in start menu
            BlockStackGame* blockStackGame = dynamic_cast<BlockStackGame*>(currentMinigame);
            ColorMatchGame* colorMatchGame = dynamic_cast<ColorMatchGame*>(currentMinigame);
            
            if (blockStackGame && blockStackGame->isInStartMenu()) {
                // BlockStackGame start menu input
                MenuAction action = blockStackGame->processStartMenuInput(pad);
                if (action == BACK_TO_MENU) {
                    stopCurrentMinigame();
                }
            } else if (colorMatchGame && colorMatchGame->isInStartMenu()) {
                // ColorMatchGame start menu input
                MenuAction action = colorMatchGame->processStartMenuInput(pad);
                if (action == BACK_TO_MENU) {
                    stopCurrentMinigame();
                }
            } else if (currentMinigame->isPaused()) {
                // Pause menu input
                MenuAction action = currentMinigame->processPauseMenuInput(pad);
                if (action == RESUME_GAME) {
                    resumeCurrentMinigame();
                } else if (action == RESTART_GAME) {
                    currentMinigame->reset();
                } else if (action == BACK_TO_MENU) {
                    stopCurrentMinigame();
                }
            } else if (!currentMinigame->isGameOver()) {
                // Normal gameplay input
                currentMinigame->processInput(pad);
                handleAnalogInput(pad);
            } else {
                // End menu input
                MenuAction action = currentMinigame->processEndMenuInput(pad);
                if (action == RESTART_GAME) {
                    currentMinigame->reset();
                    setGameState(PLAYING_STATE); // continue playing
                } else if (action == BACK_TO_MENU) {
                    stopCurrentMinigame();
                }
            }
        }
        break;
        case PAUSED_STATE:
            if (CHECK_BTN(SCE_CTRL_CROSS)) {
                resumeCurrentMinigame();
            }
            break;
    }
    
    old_buttons = pad.buttons;
}

void GameController::handleAnalogInput(const SceCtrlData& pad) {
    // Camera orientation (right analog)
    int rx = pad.rx - 127, ry = pad.ry - 127;
    if (rx < -ANALOGS_DEADZONE || rx > ANALOGS_DEADZONE) {
        camera.processMouseMovement(rx, 0, GL_TRUE);
    }
    if (ry < -ANALOGS_DEADZONE || ry > ANALOGS_DEADZONE) {
        camera.processMouseMovement(0, ry, GL_TRUE);
    }
    
    // Camera movement (left analog)
    int lx = pad.lx - 127, ly = pad.ly - 127;
    if (lx < -ANALOGS_DEADZONE) {
        camera.processKeyboard(LEFT, deltaTime);
    } else if (lx > ANALOGS_DEADZONE) {
        camera.processKeyboard(RIGHT, deltaTime);
    }
    if (ly < -ANALOGS_DEADZONE) {
        camera.processKeyboard(FORWARD, deltaTime);
    } else if (ly > ANALOGS_DEADZONE) {
        camera.processKeyboard(BACKWARD, deltaTime);
    }
}

void GameController::updateGameState() {
    // Properly altering rotation angle if spinning mode is enabled
    if (spinning) {
        orientationY += deltaTime * spin_speed;
    }
}

void GameController::draw() {
    if (!running) return;

    // Clear buffers once
    if (depthTestEnabled)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
        glClear(GL_COLOR_BUFFER_BIT);

    // Always draw menu if in menu state
    if (currentGameState == MENU_STATE || currentGameState == GAME_SELECT_STATE) {
    #ifdef LINUX_BUILD
        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(1.0f, 1.0f, 1.0f);
        menu.draw();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(programs[shader_idx]);
    #else
        // Vita path
        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glColor3f(1.0f, 1.0f, 1.0f);
        menu.draw();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(programs[shader_idx]);
    #endif
    }
    // Draw minigame if active and not in menu state
    else if (currentMinigame) {
    #ifdef LINUX_BUILD
        glUseProgram(0);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(45.0f, 960.0f / 544.0f, 0.1f, 1000.0f);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        Camera& cam = getCamera();
        glm::vec3 pos = cam.getPosition();
        glm::vec3 target = pos + cam.getFront();
        glm::vec3 up = cam.getUp();
        gluLookAt(pos.x, pos.y, pos.z, target.x, target.y, target.z, up.x, up.y, up.z);

        drawCurrentMinigame();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glUseProgram(programs[shader_idx]);
    #else
        vglWaitVblankStart(GL_TRUE);
        glUseProgram(0);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0f, 960.0f / 544.0f, 0.1f, 1000.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        Camera& cam = getCamera();
        glm::vec3 pos = cam.getPosition();
        glm::vec3 target = pos + cam.getFront();
        glm::vec3 up = cam.getUp();
        gluLookAt(pos.x, pos.y, pos.z, target.x, target.y, target.z, up.x, up.y, up.z);

        drawCurrentMinigame();

        glUseProgram(programs[shader_idx]);
    #endif
    }

    // Swap buffers once at the end
    #ifdef LINUX_BUILD
        platformSwapBuffers();
    #else
        vglSwapBuffers(GL_FALSE);
    #endif
}

void GameController::renderScene() {
    // Get view matrix from camera
    glm::mat4 view = camera.getViewMatrix();
    glUniformMatrix4fv(viewMatrixLoc[shader_idx], 1, GL_FALSE, glm::value_ptr(view));

}

void GameController::shutdown() {
    if (!running) return;
    
    running = false;
    
#ifdef LINUX_BUILD
    // Linux: Shutdown platform
    platformShutdown();
#else
    // Vita: Terminating graphics device
    vglEnd();
#endif
}

void GameController::loadShader(const char* name, int type) {
    // Load vertex shader from filesystem
    char fname[256];
#ifdef LINUX_BUILD
    sprintf(fname, "assets/linux_shaders/%s.vert", name);
#else
    sprintf(fname, "app0:%s.vert", name);
#endif
    FILE* f = fopen(fname, "r");
    if (!f)
        fatal_error("Cannot open %s", fname);
    fseek(f, 0, SEEK_END);
    int32_t vsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* vshader = (char*)malloc(vsize);
    fread(vshader, 1, vsize, f);
    fclose(f);
    
    // Load fragment shader from filesystem
#ifdef LINUX_BUILD
    sprintf(fname, "assets/linux_shaders/%s.frag", name);
#else
    sprintf(fname, "app0:%s.frag", name);
#endif
    f = fopen(fname, "r");
    if (!f)
        fatal_error("Cannot open %s", fname);
    fseek(f, 0, SEEK_END);
    int32_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* fshader = (char*)malloc(fsize);
    fread(fshader, 1, fsize, f);
    fclose(f);
    
    // Create required shaders and program
    vshaders[type] = glCreateShader(GL_VERTEX_SHADER);
    fshaders[type] = glCreateShader(GL_FRAGMENT_SHADER);
    programs[type] = glCreateProgram();
    
    // Compiling vertex shader
    glShaderSource(vshaders[type], 1, &vshader, &vsize);
    glCompileShader(vshaders[type]);
    
    // Check vertex shader compilation
    GLint success;
    glGetShaderiv(vshaders[type], GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vshaders[type], 512, NULL, infoLog);
        fatal_error("Vertex shader compilation failed: %s", infoLog);
    }
    
    // Compiling fragment shader
    glShaderSource(fshaders[type], 1, &fshader, &fsize);
    glCompileShader(fshaders[type]);
    
    // Check fragment shader compilation
    glGetShaderiv(fshaders[type], GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fshaders[type], 512, NULL, infoLog);
        fatal_error("Fragment shader compilation failed: %s", infoLog);
    }
    
    // Attaching shaders to final program
    glAttachShader(programs[type], vshaders[type]);
    glAttachShader(programs[type], fshaders[type]);
    
    // Binding attrib locations for the given shaders
    glBindAttribLocation(programs[type], 0, "position");
    glBindAttribLocation(programs[type], 1, "normal");
    
    // Linking program
    glLinkProgram(programs[type]);
    
    // Check program linking
    glGetProgramiv(programs[type], GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(programs[type], 512, NULL, infoLog);
        fatal_error("Shader program linking failed: %s", infoLog);
    }
    
    // Getting uniforms locations for the given shaders
    modelMatrixLoc[type] = glGetUniformLocation(programs[type], "modelMatrix");
    viewMatrixLoc[type] = glGetUniformLocation(programs[type], "viewMatrix");
    projectionMatrixLoc[type] = glGetUniformLocation(programs[type], "projectionMatrix");
    normalMatrixLoc[type] = glGetUniformLocation(programs[type], "normalMatrix");
    pointLightPositionLoc[type] = glGetUniformLocation(programs[type], "pointLightPosition");
    KdLoc[type] = glGetUniformLocation(programs[type], "Kd");
    diffuseColorLoc[type] = glGetUniformLocation(programs[type], "diffuseColor");
    
    // Deleting temporary buffers
    free(fshader);
    free(vshader);
}

void GameController::drawModel(to_model* mdl) {
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, mdl->pos);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, mdl->normals);
    glDrawArrays(GL_TRIANGLES, 0, mdl->num_vertices);
}

void GameController::fatal_error(const char* fmt, ...) {
    va_list list;
    char string[512];

    va_start(list, fmt);
    vsnprintf(string, sizeof(string), fmt, list);
    va_end(list);
    
#ifdef LINUX_BUILD
    // Linux: Print error and exit
    fprintf(stderr, "Fatal Error: %s\n", string);
    exit(1);
#else
    // Vita: Initialize sceMsgDialog widget with a given message text
    SceMsgDialogUserMessageParam msg_param;
    memset(&msg_param, 0, sizeof(msg_param));
    msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
    msg_param.msg = (SceChar8*)string;

    SceMsgDialogParam param;
    sceMsgDialogParamInit(&param);
    _sceCommonDialogSetMagicNumber(&param.commonParam);
    param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &msg_param;

    sceMsgDialogInit(&param);

    // Wait for user input
    while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
        glClear(GL_COLOR_BUFFER_BIT);
        vglSwapBuffers(GL_TRUE);
    }
    sceMsgDialogTerm();

    sceKernelExitProcess(0);
    while (1);
#endif
}

void GameController::initMinigames() {
    // Add minigames to the collection
    addMinigame(std::unique_ptr<ColorMatchGame>(new ColorMatchGame()));
    addMinigame(std::unique_ptr<BlockStackGame>(new BlockStackGame()));
    
    // Add minigames to menu
    for (const auto& minigame : minigames) {
        menu.addMinigame(minigame->getName());
    }
}

void GameController::addMinigame(std::unique_ptr<Minigame> minigame) {
    if (minigame) {
        // Set camera reference
        minigame->setCamera(&camera);
        minigame->setProjection(projection);
        
        minigames.push_back(std::move(minigame));
    }
}

void GameController::selectMinigame(int index) {
    if (index >= 0 && index < static_cast<int>(minigames.size())) {
        currentMinigameIndex = index;
        currentMinigame = minigames[index].get();
        
        if (currentMinigame) {
            currentMinigame->init();
        }
    }
}

void GameController::startCurrentMinigame() {
    if (currentMinigame) {
        currentMinigame->start();
        setGameState(PLAYING_STATE);
    }
}

void GameController::pauseCurrentMinigame() {
    if (currentMinigame) {
        currentMinigame->pause();
        setGameState(PAUSED_STATE);
    }
}

void GameController::resumeCurrentMinigame() {
    if (currentMinigame) {
        currentMinigame->resume();
        setGameState(PLAYING_STATE);
    }
}

void GameController::stopCurrentMinigame() {
    if (currentMinigame) {
        currentMinigame->shutdown();
        currentMinigame = nullptr;
        currentMinigameIndex = -1;
        setGameState(MENU_STATE);
    }
}

void GameController::setGameState(GameState state) {
    currentGameState = state;
    
    switch (state) {
        case MENU_STATE:
            menu.setState(MAIN_MENU);
            break;
        case GAME_SELECT_STATE:
            menu.setState(GAME_SELECT);
            break;
        case PLAYING_STATE:
            // Already set by startCurrentMinigame
            break;
        case PAUSED_STATE:
            // Already set by pauseCurrentMinigame
            break;
    }
}

void GameController::handleMenuAction(MenuAction action) {
    switch (action) {
        case START_GAME:
            setGameState(GAME_SELECT_STATE);
            break;
        case SELECT_MINIGAME:
            {
                int selectedIndex = menu.getSelectedIndex() - 1; // -1 for "Back to Menu"
                if (selectedIndex >= 0) {
                    selectMinigame(selectedIndex);
                    startCurrentMinigame();
                }
            }
            break;
        case OPEN_SETTINGS:
            setGameState(MENU_STATE);
            // TODO: Implement settings
            break;
        case OPEN_CREDITS:
            setGameState(MENU_STATE);
            // TODO: Implement credits
            break;
        case BACK_TO_MENU:
            setGameState(MENU_STATE);
            break;
        case QUIT_GAME:
            running = false;
            break;
        default:
            break;
    }
}

void GameController::updateCurrentMinigame(float deltaTime) {
    if (!currentMinigame) return;

    if (currentMinigame->isPaused()) {
        // Don’t update gameplay while paused
        return;
    }

    if (!currentMinigame->isGameOver()) {
        // Normal gameplay update
        currentMinigame->update(deltaTime);
    } else {
        // Game over state - menu handled inside minigame
    }
}

void GameController::drawCurrentMinigame() {
    if (!currentMinigame) return;

    currentMinigame->draw();

    // Check if this is BlockStackGame and it's in start menu
    BlockStackGame* blockStackGame = dynamic_cast<BlockStackGame*>(currentMinigame);
    ColorMatchGame* colorMatchGame = dynamic_cast<ColorMatchGame*>(currentMinigame);
    
    if (blockStackGame && blockStackGame->isInStartMenu()) {
        blockStackGame->drawStartMenu(menu.getFont());
    } else if (colorMatchGame && colorMatchGame->isInStartMenu()) {
        colorMatchGame->drawStartMenu(menu.getFont());
    } else if (currentMinigame->isPaused()) {
        currentMinigame->drawPauseMenu(menu.getFont());
    } 
    else if (currentMinigame->isGameOver()) {
        currentMinigame->drawEndMenu(menu.getFont());
    } 
    else {
        currentMinigame->drawHUD(menu.getFont());
    }
}