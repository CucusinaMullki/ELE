#pragma once
#include "iostream"

#ifdef NDEBUG
    #define LOG(msg)
#else
    #define LOG(msg) std::cout << "[ELE] " << msg << std::endl;
#endif