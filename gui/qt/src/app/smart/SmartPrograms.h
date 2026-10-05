#pragma once

#include "SmartChess.h"
#include "SmartInterpreter.h"

#include <QString>

/// A SMART program ready to run in this client: its interpreter, with the
/// chess of SmartChess defined, and what its commands collect.
struct SmartProgram {
    explicit SmartProgram(SmartScript script)
        : interpreter(std::move(script))
    {
    }

    SmartInterpreter interpreter;
    SmartChess::Output output;
};

/// The SMART programs of the smart/ folder, ready to run in this client.
///
/// They are built into the application; with PRAGMA_SMART_DIR set, they are
/// read from that folder instead (the smart/ folder of a checkout), so a fix
/// is tried without building anything.
namespace SmartPrograms {

/// The text of `fileName` ("TUTOR.smart"); empty, with `error`, if it cannot be read.
QString source(const QString &fileName, QString *error = nullptr);

/// The program `fileName` loaded; one per thread, loaded at its first use
/// and kept, so its globals are its memory. Null if it cannot be read or
/// loaded (the reason is logged once).
SmartProgram *program(const QString &fileName);

} // namespace SmartPrograms
