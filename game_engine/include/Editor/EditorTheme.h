#ifndef EDITOR_THEME_H
#define EDITOR_THEME_H

#ifdef LINUX_BUILD

namespace GameEngine {

class EditorTheme {
public:
    enum class Id {
        Cyberpunk = 0,
        Dark,
        Light,
        Count
    };

    static void apply(Id theme);

    static Id getCurrent();

    static const char* name(Id theme);

private:
    static void applyCyberpunk();

    static void applyViewportCorrections();

    static Id currentTheme;
};

} 

#endif

#endif
