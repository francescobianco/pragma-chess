#include "Extension.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace ThisComputer {

QString system()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

bool hasBmi2()
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int registers[4] = {};
    __cpuidex(registers, 7, 0);
    return (registers[1] & (1 << 8)) != 0; // EBX bit 8
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("bmi2");
#else
    return false; // Not an x86 processor: the catalogs' "bmi2" builds are x86's.
#endif
}

} // namespace ThisComputer
