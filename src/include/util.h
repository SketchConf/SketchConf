#pragma once
#include <deque>
#include "defs.hpp"
#include "streamgen.h"

int poisson_trunc(int l, const Constraint& constraint, StreamGen& stream);

int Open(const char* file, int flag, int perm = NEW_FILE_PERM);

void Write(int fd, const void* buf, size_t len);

inline bool stop(int niter, double prev_avg, double cur_avg)
{
    if (std::fabs(cur_avg-prev_avg) > STOP_THRESHOLD*prev_avg)
        return false;
    else
        return true;
}

inline bool stop(int niter, double prev_avg, double cur_avg, double demand_prob)
{
    if (std::fabs(cur_avg-prev_avg) > STOP_THRESHOLD*prev_avg)
        return false;
    else
        return true;
}

inline TP now() { return std::chrono::high_resolution_clock::now(); }