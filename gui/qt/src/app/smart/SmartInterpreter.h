#pragma once

#include "SmartScript.h"
#include "SmartValue.h"

#include <QHash>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

/// A mistake found while a SMART program runs: the line of the program
/// (0 until the interpreter knows it) and what went wrong.
struct SmartError {
    int line = 0;
    QString message;
};

/// Runs a SmartScript (smart/README.md). The client defines its functions —
/// the chess and the commands that collect what the program shows —, loads
/// the program (its top-level statements run once) and then calls its entry
/// functions as often as it needs; the globals live in between, as the
/// program's memory.
class SmartInterpreter {
public:
    /// A function of the client: gets the evaluated arguments, may throw a
    /// SmartError (SmartInterpreter::fail) for a wrong use.
    using Builtin = std::function<SmartValue(const std::vector<SmartValue> &args)>;

    explicit SmartInterpreter(SmartScript script);

    /// Defines (or replaces) a function of the client, by a case-insensitive name.
    void define(const QString &name, Builtin builtin);
    /// A constant of the client (WHITE, BLACK…), before load().
    void defineConstant(const QString &name, const SmartValue &value);
    /// Runs the top-level statements, once: constants and memory. False,
    /// with the line in `error`, if the program stops on a mistake.
    bool load(QString *error = nullptr);
    /// Calls a function of the program; nothing, with `error`, if it fails.
    std::optional<SmartValue> call(const QString &function, const std::vector<SmartValue> &args = {},
                                   QString *error = nullptr);
    bool hasFunction(const QString &name) const;

    SmartValue global(const QString &name) const;
    void setGlobal(const QString &name, const SmartValue &value);

    /// Statements one call may run before it is stopped.
    void setStepLimit(qint64 steps) { m_stepLimit = steps; }

    /// For the client's functions: stops the program with `message`.
    [[noreturn]] static void fail(const QString &message);
    /// Fails unless there are `count` arguments.
    static void expectArguments(const QString &function, const std::vector<SmartValue> &args, int count);
    /// The argument as a number, or fails saying what it is.
    static double numberArgument(const QString &function, const std::vector<SmartValue> &args, int index);
    static int intArgument(const QString &function, const std::vector<SmartValue> &args, int index);
    static QString textArgument(const QString &function, const std::vector<SmartValue> &args, int index);
    static const std::vector<SmartValue> &listArgument(const QString &function, const std::vector<SmartValue> &args,
                                                       int index);

private:
    enum class Flow { Normal, Return, ExitFor, ExitWhile };
    struct Frame {
        QHash<QString, SmartValue> locals;
        QSet<QString> shared;
        SmartValue returned;
    };

    Flow run(const SmartScript::Block &block);
    Flow run(const SmartScript::Stmt &stmt);
    SmartValue evaluate(const SmartScript::Expr &expr);
    SmartValue binary(const SmartScript::Expr &expr);
    SmartValue invoke(const QString &name, const std::vector<SmartValue> &args, int line);
    bool truth(const SmartValue &value) const;
    SmartValue read(const QString &name) const;
    void write(const QString &name, const SmartValue &value);
    void defineCore();

    SmartScript m_script;
    QHash<QString, Builtin> m_builtins;
    QHash<QString, SmartValue> m_globals;
    QSet<QString> m_constants;
    std::vector<Frame> m_frames;
    qint64 m_steps = 0;
    qint64 m_stepLimit = 2'000'000;
};
