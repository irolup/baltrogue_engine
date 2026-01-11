#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#ifdef LINUX_BUILD

#include <string>
#include <vector>

namespace GameEngine {

class FileDialog {
public:
    // Open file dialog for loading scenes
    static std::string openFileDialog(const std::string& title = "Open Scene", 
                                     const std::string& filter = "*.json");
    
    // Save file dialog for saving scenes
    static std::string saveFileDialog(const std::string& title = "Save Scene", 
                                     const std::string& filter = "*.json",
                                     const std::string& defaultName = "scene.json");
    
    // Check if a file dialog result is valid (not cancelled)
    static bool isValidResult(const std::string& result);
    
    // Get the default scenes directory
    static std::string getDefaultScenesDirectory();
    
    // Create the scenes directory if it doesn't exist
    static void ensureScenesDirectoryExists();
};

} // namespace GameEngine

#endif // LINUX_BUILD
#endif // FILE_DIALOG_H
