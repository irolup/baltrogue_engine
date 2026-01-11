#pragma once

#ifdef VITA_BUILD
// Vita-specific math function implementations
// Only provide these if they're not already available
#include <cmath>

// Check if the functions are available at link time
// If they're not, we'll provide our own implementations
namespace VitaMath {
    // These are just wrappers around std:: functions
    // The actual issue is likely linking, not missing declarations
    inline float powf_wrapper(float x, float y) {
        return std::pow(x, y);
    }
    
    inline float fmodf_wrapper(float x, float y) {
        return std::fmod(x, y);
    }
    
    inline float ceilf_wrapper(float x) {
        return std::ceil(x);
    }
}

// Don't use the global namespace - let the linker find the real functions
// If linking fails, we can provide actual implementations
#endif
