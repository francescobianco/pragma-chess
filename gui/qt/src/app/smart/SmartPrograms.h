#pragma once

#include <QString>

class SmartInterpreter;

/// The SMART programs of the smart/ folder, ready to run in this client.
///
/// They are built into the application; with PRAGMA_SMART_DIR set, they are
/// read from that folder instead (the smart/ folder of a checkout), so a fix
/// is tried without building anything.
namespace SmartPrograms {

/// The text of `fileName` ("TUTOR.smart"); empty, with `error`, if it cannot be read.
QString source(const QString &fileName, QString *error = nullptr);

/// The program `fileName` loaded, with the chess of SmartChess defined; one
/// per thread, loaded at its first use and kept. Null if it cannot be read
/// or loaded (the reason is logged once).
SmartInterpreter *program(const QString &fileName);

} // namespace SmartPrograms
