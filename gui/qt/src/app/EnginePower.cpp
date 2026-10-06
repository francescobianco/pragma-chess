#include "EnginePower.h"

#include <QtGlobal>

EnginePower EnginePower::forLevel(int level, int cores)
{
    level = qBound(int(Minimum), level, int(Full));
    cores = qMax(1, cores);
    static const int shares[] = {3, 6, 10, 25, 0};
    EnginePower power;
    power.cpuPercent = shares[level - 1];
    // The threads the share covers, rounded up: a cap below one core (10%
    // of a four-core machine) is left to the cap.
    power.threads = power.cpuPercent > 0 ? qMax(1, (cores * power.cpuPercent + 99) / 100) : cores;
    power.lowPriority = level < Full;
    return power;
}

int EnginePower::quotaOfOneCore(int cores) const
{
    return cpuPercent > 0 ? cpuPercent * qMax(1, cores) : 0;
}
