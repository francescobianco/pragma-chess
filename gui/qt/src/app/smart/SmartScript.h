#pragma once

#include "SmartValue.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

/// A SMART program read from its text (smart/README.md): the top-level
/// statements and the functions, as a tree SmartInterpreter runs. Reading
/// stops at the first mistake, with its line.
class SmartScript {
public:
    struct Expr;
    struct Stmt;
    using ExprPtr = std::shared_ptr<const Expr>;
    using StmtPtr = std::shared_ptr<const Stmt>;
    using Block = std::vector<StmtPtr>;

    struct Expr {
        enum class Kind { Literal, Name, List, Unary, Binary, Call, Index };
        Kind kind = Kind::Literal;
        int line = 0;
        SmartValue value;     // Literal
        QString name;         // Name, Call; the operator of Unary and Binary
        std::vector<ExprPtr> args; // List items, Call arguments, operands, Index: list then index
    };

    struct Stmt {
        enum class Kind { Assign, AssignIndex, Const, If, For, While, ExitFor, ExitWhile, Return, Call, Shared };
        Kind kind = Kind::Assign;
        int line = 0;
        /// The variable (Assign, AssignIndex, Const, For), the function
        /// (Call), the names (Shared, joined by commas).
        QString name;
        /// Assign/Const: the value; AssignIndex: the index then the value;
        /// For: from, to, step (may be null); While: the condition;
        /// Return: the value (may be null); Call: the arguments; If: the
        /// conditions, one per branch with a condition.
        std::vector<ExprPtr> exprs;
        /// If: one block per condition, then the ELSE block if there is one;
        /// For and While: the body.
        std::vector<Block> blocks;
    };

    struct Function {
        QString name;
        QStringList params;
        Block body;
        int line = 0;
    };

    /// Reads a program; on a mistake returns nothing and says why in `error`
    /// ("line 12: …").
    static std::optional<SmartScript> parse(const QString &source, QString *error = nullptr);

    const Block &main() const { return m_main; }
    /// By name, upper case.
    const QHash<QString, Function> &functions() const { return m_functions; }

private:
    Block m_main;
    QHash<QString, Function> m_functions;

    friend class SmartParser;
};
