#pragma once
#include "iostream"
#include <sstream>
#include "UI/DebugConsole.h"

#ifdef NDEBUG
    #define LOG(msg) \
        do { \
            std::ostringstream oss; \
            oss << msg; \
            DebugConsole::instance().log(oss.str()); \
        } while(0)
#else
    #define LOG(msg) \
        do { \
            std::ostringstream oss; \
            oss << msg; \
            std::cout << "[ELE] " << oss.str() << std::endl; \
            DebugConsole::instance().log(oss.str()); \
        } while(0)
#endif