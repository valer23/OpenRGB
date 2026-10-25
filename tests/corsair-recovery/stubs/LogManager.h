#pragma once
#include <cstdio>
#define LOG_WARNING(...) do { std::fprintf(stderr,__VA_ARGS__); std::fprintf(stderr,"\n"); } while(0)
