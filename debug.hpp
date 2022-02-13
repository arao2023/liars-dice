#pragma once

#ifdef ENABLE_LOGGING
    #include <cstdio>
    namespace Debug {
        auto& log {std::printf};
    }
#else
    namespace Debug {
        inline void log(...) {}
    }
#endif
