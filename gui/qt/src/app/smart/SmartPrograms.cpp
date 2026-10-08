#include "SmartPrograms.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QtGlobal>

#include <memory>

namespace SmartPrograms {

QString source(const QString &fileName, QString *error)
{
    const QString folder = qEnvironmentVariable("PRAGMA_SMART_DIR");
    const QString path = folder.isEmpty() ? QStringLiteral(":/smart/") + fileName : QDir(folder).filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot read %1: %2").arg(path, file.errorString());
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

SmartProgram *program(const QString &fileName)
{
    // Interpreters keep their memory and are not shared between threads.
    struct Loaded {
        std::shared_ptr<SmartProgram> program;
        QDateTime modified;
    };
    thread_local QHash<QString, Loaded> loaded;
    // Read from disk (PRAGMA_SMART_DIR, tuning): a program saved again is read
    // again at its next use, so a fix shows without restarting.
    const QString folder = qEnvironmentVariable("PRAGMA_SMART_DIR");
    const QDateTime modified = folder.isEmpty() ? QDateTime() : QFileInfo(QDir(folder).filePath(fileName)).lastModified();
    if (const auto found = loaded.constFind(fileName); found != loaded.constEnd() && found->modified == modified)
        return found->program.get();
    QString error;
    std::shared_ptr<SmartProgram> smart;
    const QString text = source(fileName, &error);
    if (!text.isEmpty()) {
        if (std::optional<SmartScript> script = SmartScript::parse(text, &error)) {
            smart = std::make_shared<SmartProgram>(*script);
            SmartChess::define(smart->interpreter, smart->output);
            if (!smart->interpreter.load(&error))
                smart.reset();
        }
    }
    if (!smart)
        qWarning("SMART %s: %s", qPrintable(fileName), qPrintable(error));
    else if (!folder.isEmpty())
        qInfo("SMART %s read from %s", qPrintable(fileName), qPrintable(folder));
    // A program that failed is not tried again until its file changes.
    loaded.insert(fileName, {smart, modified});
    return smart.get();
}

} // namespace SmartPrograms
