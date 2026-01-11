#pragma once
#include <string>
#include "Platform.h"

class BitmapFont {
public:
    BitmapFont();
    ~BitmapFont();

    // Always returns true since we don't need to load anything
    bool isLoaded() const { return true; }
    
    void drawText(const std::string& text, float x, float y, float scale = 1.0f);
    void setColor(float r, float g, float b, float a = 1.0f);

private:
    void drawCharacter(char c, float& x, float y, float scale);
    void drawPixel(float x, float y, float width, float height);
    
    float color[4];
    static const int FONT_CHAR_WIDTH = 8;
    static const int FONT_CHAR_HEIGHT = 12;
};
