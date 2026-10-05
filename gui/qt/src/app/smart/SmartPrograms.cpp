#include "SmartPrograms.h"

#include <QDir>
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
    thread_local QHash<QString, std::shared_ptr<SmartProgram>> loaded;
    if (const auto found = loaded.constFind(fileName); found != loaded.constEnd())
        return found->get();
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
    loaded.insert(fileName, smart); // A program that failed is not tried again.
    return smart.get();
}

} // namespace SmartPrograms
