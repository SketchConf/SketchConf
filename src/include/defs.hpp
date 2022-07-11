#pragma once
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <deque>
#include <cstdint>
#include <string>
#include <chrono>
#include <cmath>
#include "logger.hpp"

#define NEW_FILE_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

typedef uint32_t data_t;
typedef uint32_t count_t;
typedef uint64_t seed_t;
typedef std::chrono::high_resolution_clock::time_point TP;

static const double EPOCH = 1000;
static const int64_t MAX_MEMORY=20'000'000;
extern count_t TINY_THRESHOLD;
extern double BIG_PERCENT;
static int TINY_SIM_NUM=1000;
static const double STOP_THRESHOLD=1e-4;
static const double RELATIVE_ERROR=0.01;

enum class SketchType {
    CM = 0,
    COUNT,
    TOWER,
    NITROCM,
    NITROCOUNT
};

class Constraint
{
public:
    double err;
    double prob;
};

class Demand
{
public:
    std::string name;
    SketchType type;
    std::deque<Constraint> constraints;
    double sample_rate;
};

class Config
{
public:
    int type;
    int nrows;
    int ncols;
};
