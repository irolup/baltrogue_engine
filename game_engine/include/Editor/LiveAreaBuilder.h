#ifndef LIVE_AREA_BUILDER_H
#define LIVE_AREA_BUILDER_H

#ifdef LINUX_BUILD

#include <string>

namespace GameEngine {

static const char* const kLiveAreaScriptPath = "scripts/build_livearea.sh";

class LiveAreaBuilder {
public:
    static bool generateAssets(std::string& output);

    static bool converterToolsAvailable();
};

}

#endif
#endif
