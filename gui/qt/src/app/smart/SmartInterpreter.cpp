#include "SmartInterpreter.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kMaxDepth = 200;

} // namespace

SmartInterpreter::SmartInterpreter(SmartScript script)
    : m_script(std::move(script))
{
    defineCore();
}

void SmartInterpreter::define(const QString &name, Builtin builtin)
{
    m_builtins.insert(name.toUpper(), std::move(builtin));
}

void SmartInterpreter::defineConstant(const QString &name, const SmartValue &value)
{
    m_globals.insert(name.toUpper(), value);
    m_constants.insert(name.toUpper());
}

bool SmartInterpreter::load(QString *error)
{
    m_frames.clear();
    m_steps = 0;
    try {
        run(m_script.main());
        return true;
    } catch (const SmartError &failure) {
        if (error)
            *error = QStringLiteral("line %1: %2").arg(failure.line).arg(failure.message);
        return false;
    }
}

std::optional<SmartValue> SmartInterpreter::call(const QString &function, const std::vector<SmartValue> &args,
                                                 QString *error)
{
    m_frames.clear();
    m_steps = 0;
    try {
        if (!m_script.functions().contains(function.toUpper()))
            fail(QStringLiteral("the program has no function %1").arg(function.toUpper()));
        return invoke(function.toUpper(), args, 0);
    } catch (const SmartError &failure) {
        m_frames.clear();
        if (error)
            *error = QStringLiteral("line %1: %2").arg(failure.line).arg(failure.message);
        return std::nullopt;
    }
}

bool SmartInterpreter::hasFunction(const QString &name) const
{
    return m_script.functions().contains(name.toUpper());
}

SmartValue SmartInterpreter::global(const QString &name) const
{
    return m_globals.value(name.toUpper());
}

void SmartInterpreter::setGlobal(const QString &name, const SmartValue &value)
{
    m_globals.insert(name.toUpper(), value);
}

void SmartInterpreter::fail(const QString &message)
{
    throw SmartError{0, message};
}

void SmartInterpreter::expectArguments(const QString &function, const std::vector<SmartValue> &args, int count)
{
    if (int(args.size()) != count)
        fail(QStringLiteral("%1 takes %2 argument(s), not %3").arg(function).arg(count).arg(args.size()));
}

double SmartInterpreter::numberArgument(const QString &function, const std::vector<SmartValue> &args, int index)
{
    if (index >= int(args.size()) || !args.at(index).isNumber())
        fail(QStringLiteral("%1 needs a number as argument %2, not %3")
                 .arg(function).arg(index + 1)
                 .arg(index < int(args.size()) ? args.at(index).typeName() : QStringLiteral("nothing")));
    return args.at(index).number();
}

int SmartInterpreter::intArgument(const QString &function, const std::vector<SmartValue> &args, int index)
{
    return int(std::trunc(numberArgument(function, args, index)));
}

QString SmartInterpreter::textArgument(const QString &function, const std::vector<SmartValue> &args, int index)
{
    if (index >= int(args.size()) || !args.at(index).isText())
        fail(QStringLiteral("%1 needs a text as argument %2, not %3")
                 .arg(function).arg(index + 1)
                 .arg(index < int(args.size()) ? args.at(index).typeName() : QStringLiteral("nothing")));
    return args.at(index).text();
}

const std::vector<SmartValue> &SmartInterpreter::listArgument(const QString &function,
                                                              const std::vector<SmartValue> &args, int index)
{
    if (index >= int(args.size()) || !args.at(index).isList())
        fail(QStringLiteral("%1 needs a list as argument %2, not %3")
                 .arg(function).arg(index + 1)
                 .arg(index < int(args.size()) ? args.at(index).typeName() : QStringLiteral("nothing")));
    return args.at(index).items();
}

SmartInterpreter::Flow SmartInterpreter::run(const SmartScript::Block &block)
{
    for (const SmartScript::StmtPtr &stmt : block) {
        const Flow flow = run(*stmt);
        if (flow != Flow::Normal)
            return flow;
    }
    return Flow::Normal;
}

SmartInterpreter::Flow SmartInterpreter::run(const SmartScript::Stmt &stmt)
{
    using Kind = SmartScript::Stmt::Kind;
    try {
        if (++m_steps > m_stepLimit)
            fail(QStringLiteral("stopped after %1 statements: a loop that never ends?").arg(m_stepLimit));
        switch (stmt.kind) {
        case Kind::Assign:
            write(stmt.name, evaluate(*stmt.exprs.at(0)));
            return Flow::Normal;
        case Kind::AssignIndex: {
            SmartValue list = read(stmt.name);
            if (!list.isList())
                fail(QStringLiteral("%1 is a %2, not a list").arg(stmt.name, list.typeName()));
            const SmartValue index = evaluate(*stmt.exprs.at(0));
            const SmartValue value = evaluate(*stmt.exprs.at(1));
            std::vector<SmartValue> &items = list.mutableItems();
            if (!index.isNumber() || index.number() != std::trunc(index.number()) || index.number() < 0
                || index.number() >= double(items.size()))
                fail(QStringLiteral("index %1 is outside %2, which has %3 element(s)")
                         .arg(index.toText(), stmt.name).arg(items.size()));
            items[size_t(index.number())] = value;
            write(stmt.name, list);
            return Flow::Normal;
        }
        case Kind::Const:
            if (m_globals.contains(stmt.name))
                fail(QStringLiteral("%1 already has a value").arg(stmt.name));
            m_globals.insert(stmt.name, evaluate(*stmt.exprs.at(0)));
            m_constants.insert(stmt.name);
            return Flow::Normal;
        case Kind::If:
            for (size_t i = 0; i < stmt.exprs.size(); ++i) {
                if (truth(evaluate(*stmt.exprs.at(i))))
                    return run(stmt.blocks.at(i));
            }
            if (stmt.blocks.size() > stmt.exprs.size())
                return run(stmt.blocks.back());
            return Flow::Normal;
        case Kind::For: {
            const SmartValue from = evaluate(*stmt.exprs.at(0));
            const SmartValue to = evaluate(*stmt.exprs.at(1));
            const SmartValue step = stmt.exprs.at(2) ? evaluate(*stmt.exprs.at(2)) : SmartValue(1);
            if (!from.isNumber() || !to.isNumber() || !step.isNumber())
                fail(QStringLiteral("FOR needs numbers"));
            if (step.number() == 0)
                fail(QStringLiteral("FOR with STEP 0 never ends"));
            for (double value = from.number();
                 step.number() > 0 ? value <= to.number() : value >= to.number(); value += step.number()) {
                write(stmt.name, value);
                const Flow flow = run(stmt.blocks.at(0));
                if (flow == Flow::ExitFor)
                    break;
                if (flow == Flow::Return || flow == Flow::ExitWhile)
                    return flow;
                // The body may have changed the variable, as in BASIC.
                const SmartValue now = read(stmt.name);
                if (!now.isNumber())
                    fail(QStringLiteral("the FOR variable %1 is no longer a number").arg(stmt.name));
                value = now.number();
            }
            return Flow::Normal;
        }
        case Kind::While:
            while (truth(evaluate(*stmt.exprs.at(0)))) {
                if (++m_steps > m_stepLimit)
                    fail(QStringLiteral("stopped after %1 statements: a loop that never ends?").arg(m_stepLimit));
                const Flow flow = run(stmt.blocks.at(0));
                if (flow == Flow::ExitWhile)
                    break;
                if (flow == Flow::Return || flow == Flow::ExitFor)
                    return flow;
            }
            return Flow::Normal;
        case Kind::ExitFor:
            return Flow::ExitFor;
        case Kind::ExitWhile:
            return Flow::ExitWhile;
        case Kind::Return:
            m_frames.back().returned = stmt.exprs.empty() ? SmartValue() : evaluate(*stmt.exprs.at(0));
            return Flow::Return;
        case Kind::Call: {
            std::vector<SmartValue> args;
            for (const SmartScript::ExprPtr &arg : stmt.exprs)
                args.push_back(evaluate(*arg));
            invoke(stmt.name, args, stmt.line);
            return Flow::Normal;
        }
        case Kind::Shared:
            for (const QString &name : stmt.name.split(QLatin1Char(',')))
                m_frames.back().shared.insert(name);
            return Flow::Normal;
        }
    } catch (SmartError &failure) {
        if (failure.line == 0)
            failure.line = stmt.line;
        throw;
    }
    return Flow::Normal;
}

bool SmartInterpreter::truth(const SmartValue &value) const
{
    if (value.isNothing())
        return false;
    if (!value.isNumber())
        fail(QStringLiteral("a condition must be a number, not a %1").arg(value.typeName()));
    return value.number() != 0;
}

SmartValue SmartInterpreter::read(const QString &name) const
{
    if (!m_frames.empty()) {
        const Frame &frame = m_frames.back();
        if (const auto local = frame.locals.constFind(name); local != frame.locals.constEnd())
            return *local;
    }
    if (const auto global = m_globals.constFind(name); global != m_globals.constEnd())
        return *global;
    fail(QStringLiteral("%1 has no value").arg(name));
}

void SmartInterpreter::write(const QString &name, const SmartValue &value)
{
    if (!m_frames.empty() && !m_frames.back().shared.contains(name)) {
        m_frames.back().locals.insert(name, value);
        return;
    }
    if (m_constants.contains(name))
        fail(QStringLiteral("%1 is a constant").arg(name));
    m_globals.insert(name, value);
}

SmartValue SmartInterpreter::evaluate(const SmartScript::Expr &expr)
{
    using Kind = SmartScript::Expr::Kind;
    try {
        switch (expr.kind) {
        case Kind::Literal:
            return expr.value;
        case Kind::Name:
            return read(expr.name);
        case Kind::List: {
            std::vector<SmartValue> items;
            for (const SmartScript::ExprPtr &item : expr.args)
                items.push_back(evaluate(*item));
            return SmartValue(std::move(items));
        }
        case Kind::Unary: {
            const SmartValue operand = evaluate(*expr.args.at(0));
            if (expr.name == QLatin1String("NOT"))
                return SmartValue(!truth(operand));
            if (!operand.isNumber())
                fail(QStringLiteral("- needs a number, not a %1").arg(operand.typeName()));
            return SmartValue(-operand.number());
        }
        case Kind::Binary:
            return binary(expr);
        case Kind::Call: {
            std::vector<SmartValue> args;
            for (const SmartScript::ExprPtr &arg : expr.args)
                args.push_back(evaluate(*arg));
            return invoke(expr.name, args, expr.line);
        }
        case Kind::Index: {
            const SmartValue list = evaluate(*expr.args.at(0));
            const SmartValue index = evaluate(*expr.args.at(1));
            if (!list.isList())
                fail(QStringLiteral("a %1 has no elements").arg(list.typeName()));
            if (!index.isNumber() || index.number() != std::trunc(index.number()) || index.number() < 0
                || index.number() >= double(list.items().size()))
                fail(QStringLiteral("index %1 is outside a list of %2 element(s)")
                         .arg(index.toText()).arg(list.items().size()));
            return list.items().at(size_t(index.number()));
        }
        }
    } catch (SmartError &failure) {
        if (failure.line == 0)
            failure.line = expr.line;
        throw;
    }
    return {};
}

SmartValue SmartInterpreter::binary(const SmartScript::Expr &expr)
{
    const QString &op = expr.name;
    if (op == QLatin1String("AND")) {
        if (!truth(evaluate(*expr.args.at(0))))
            return SmartValue(false);
        return SmartValue(truth(evaluate(*expr.args.at(1))));
    }
    if (op == QLatin1String("OR")) {
        if (truth(evaluate(*expr.args.at(0))))
            return SmartValue(true);
        return SmartValue(truth(evaluate(*expr.args.at(1))));
    }
    const SmartValue left = evaluate(*expr.args.at(0));
    const SmartValue right = evaluate(*expr.args.at(1));
    if (op == QLatin1String("="))
        return SmartValue(left == right);
    if (op == QLatin1String("<>"))
        return SmartValue(!(left == right));
    if (op == QLatin1String("+")) {
        if (left.isNumber() && right.isNumber())
            return SmartValue(left.number() + right.number());
        if ((left.isText() && (right.isText() || right.isNumber())) || (left.isNumber() && right.isText()))
            return SmartValue(left.toText() + right.toText());
        if (left.isList() && right.isList()) {
            std::vector<SmartValue> items = left.items();
            items.insert(items.end(), right.items().begin(), right.items().end());
            return SmartValue(std::move(items));
        }
        fail(QStringLiteral("cannot add a %1 and a %2").arg(left.typeName(), right.typeName()));
    }
    if (op == QLatin1String("<") || op == QLatin1String(">") || op == QLatin1String("<=")
        || op == QLatin1String(">=")) {
        int order = 0;
        if (left.isNumber() && right.isNumber())
            order = left.number() < right.number() ? -1 : left.number() > right.number() ? 1 : 0;
        else if (left.isText() && right.isText())
            order = QString::compare(left.text(), right.text());
        else
            fail(QStringLiteral("cannot compare a %1 with a %2").arg(left.typeName(), right.typeName()));
        if (op == QLatin1String("<"))
            return SmartValue(order < 0);
        if (op == QLatin1String(">"))
            return SmartValue(order > 0);
        if (op == QLatin1String("<="))
            return SmartValue(order <= 0);
        return SmartValue(order >= 0);
    }
    if (!left.isNumber() || !right.isNumber())
        fail(QStringLiteral("%1 needs numbers, not a %2 and a %3").arg(op, left.typeName(), right.typeName()));
    const double a = left.number();
    const double b = right.number();
    if (op == QLatin1String("-"))
        return SmartValue(a - b);
    if (op == QLatin1String("*"))
        return SmartValue(a * b);
    if (b == 0)
        fail(QStringLiteral("division by zero"));
    if (op == QLatin1String("/"))
        return SmartValue(a / b);
    return SmartValue(a - b * std::trunc(a / b)); // MOD
}

SmartValue SmartInterpreter::invoke(const QString &name, const std::vector<SmartValue> &args, int line)
{
    if (const auto function = m_script.functions().constFind(name); function != m_script.functions().constEnd()) {
        if (int(args.size()) != function->params.size())
            fail(QStringLiteral("%1 takes %2 argument(s), not %3").arg(name).arg(function->params.size()).arg(args.size()));
        if (m_frames.size() >= size_t(kMaxDepth))
            fail(QStringLiteral("%1 calls itself too deep").arg(name));
        Frame frame;
        for (qsizetype i = 0; i < function->params.size(); ++i)
            frame.locals.insert(function->params.at(i), args.at(size_t(i)));
        m_frames.push_back(std::move(frame));
        try {
            run(function->body);
        } catch (...) {
            m_frames.pop_back();
            throw;
        }
        const SmartValue returned = m_frames.back().returned;
        m_frames.pop_back();
        return returned;
    }
    if (const auto builtin = m_builtins.constFind(name); builtin != m_builtins.constEnd()) {
        try {
            return (*builtin)(args);
        } catch (SmartError &failure) {
            if (failure.line == 0)
                failure.line = line;
            throw;
        }
    }
    fail(QStringLiteral("there is no function %1").arg(name));
}

void SmartInterpreter::defineCore()
{
    define(QStringLiteral("LEN"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("LEN"), args, 1);
        if (args.at(0).isText())
            return SmartValue(int(args.at(0).text().size()));
        return SmartValue(int(listArgument(QStringLiteral("LEN"), args, 0).size()));
    });
    define(QStringLiteral("INT"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("INT"), args, 1);
        return SmartValue(std::trunc(numberArgument(QStringLiteral("INT"), args, 0)));
    });
    define(QStringLiteral("ABS"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("ABS"), args, 1);
        return SmartValue(std::abs(numberArgument(QStringLiteral("ABS"), args, 0)));
    });
    const auto extreme = [](const QString &name, bool least) {
        return [name, least](const std::vector<SmartValue> &args) {
            if (args.empty())
                fail(QStringLiteral("%1 needs at least one number").arg(name));
            double result = numberArgument(name, args, 0);
            for (int i = 1; i < int(args.size()); ++i) {
                const double value = numberArgument(name, args, i);
                result = least ? std::min(result, value) : std::max(result, value);
            }
            return SmartValue(result);
        };
    };
    define(QStringLiteral("MIN"), extreme(QStringLiteral("MIN"), true));
    define(QStringLiteral("MAX"), extreme(QStringLiteral("MAX"), false));
    define(QStringLiteral("STR"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("STR"), args, 1);
        return SmartValue(args.at(0).toText());
    });
    define(QStringLiteral("REPEAT"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("REPEAT"), args, 2);
        const int count = intArgument(QStringLiteral("REPEAT"), args, 1);
        if (count < 0)
            fail(QStringLiteral("REPEAT cannot make %1 copies").arg(count));
        return SmartValue(std::vector<SmartValue>(size_t(count), args.at(0)));
    });
    define(QStringLiteral("SLICE"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("SLICE"), args, 3);
        const std::vector<SmartValue> items = listArgument(QStringLiteral("SLICE"), args, 0); // A copy: the name is a temporary.
        const int from = qBound(0, intArgument(QStringLiteral("SLICE"), args, 1), int(items.size()));
        const int count = qBound(0, intArgument(QStringLiteral("SLICE"), args, 2), int(items.size()) - from);
        return SmartValue(std::vector<SmartValue>(items.begin() + from, items.begin() + from + count));
    });
    define(QStringLiteral("CONTAINS"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("CONTAINS"), args, 2);
        const std::vector<SmartValue> items = listArgument(QStringLiteral("CONTAINS"), args, 0); // A copy: the name is a temporary.
        return SmartValue(std::find(items.begin(), items.end(), args.at(1)) != items.end());
    });
    define(QStringLiteral("INDEXOF"), [](const std::vector<SmartValue> &args) {
        expectArguments(QStringLiteral("INDEXOF"), args, 2);
        const std::vector<SmartValue> items = listArgument(QStringLiteral("INDEXOF"), args, 0); // A copy: the name is a temporary.
        const auto found = std::find(items.begin(), items.end(), args.at(1));
        return SmartValue(found == items.end() ? -1 : int(found - items.begin()));
    });
}
