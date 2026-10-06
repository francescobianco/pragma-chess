#pragma once

/// How much of the computer the engine may take: five levels, from Minimum
/// to Full. The engine is not made weaker — no depth, no time limit: it
/// searches as it would, with fewer of the machine's resources, so it is
/// slower. A level is a share of the machine's CPU, given three ways: the
/// threads the engine searches with, a hard cap on the CPU its process may
/// use (where the system has one: cgroups on Linux, a job object on
/// Windows), and a low priority, so that the rest of the desktop goes
/// first. Pure, unit-tested.
struct EnginePower {
    enum Level { Minimum = 1, Light, Medium, High, Full };
    /// Every engine starts here: a relaxed fifth of the computer, not all of it.
    static constexpr int kDefault = Medium;

    /// Share of the whole machine's CPU, in per cent; 0 for no cap (Full).
    int cpuPercent = 0;
    /// Search threads: as many cores as the share covers, at least one.
    int threads = 1;
    /// Run the engine below normal priority.
    bool lowPriority = false;

    /// The level `level` (clamped to 1–5) on a computer with `cores` logical cores.
    static EnginePower forLevel(int level, int cores);
    /// The cap as systemd's CPUQuota counts it: per cent of one core
    /// (50% of 8 cores is 400%); 0 for none.
    int quotaOfOneCore(int cores) const;
};
