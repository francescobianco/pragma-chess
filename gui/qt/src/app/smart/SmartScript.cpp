#include "SmartScript.h"

#include <QSet>

namespace {

struct Token {
    enum class Type { Number, Text, Name, Symbol, Newline, Colon, End };
    Type type = Type::End;
    QString text; // Names upper case; symbols as written.
    double number = 0;
    int line = 0;
};

const QSet<QString> &keywords()
{
    static const QSet<QString> words{
        QStringLiteral("LET"),    QStringLiteral("CONST"),  QStringLiteral("IF"),       QStringLiteral("THEN"),
        QStringLiteral("ELSE"),   QStringLiteral("ELSEIF"), QStringLiteral("END"),      QStringLiteral("FOR"),
        QStringLiteral("TO"),     QStringLiteral("STEP"),   QStringLiteral("NEXT"),     QStringLiteral("WHILE"),
        QStringLiteral("WEND"),   QStringLiteral("EXIT"),   QStringLiteral("FUNCTION"), QStringLiteral("RETURN"),
        QStringLiteral("SHARED"), QStringLiteral("CALL"),   QStringLiteral("AND"),      QStringLiteral("OR"),
        QStringLiteral("NOT"),    QStringLiteral("MOD"),    QStringLiteral("TRUE"),     QStringLiteral("FALSE"),
        QStringLiteral("NOTHING"), QStringLiteral("REM"),
    };
    return words;
}

struct ParseError {
    int line;
    QString message;
};

QList<Token> tokenize(const QString &source)
{
    QList<Token> tokens;
    int line = 1;
    qsizetype i = 0;
    const qsizetype n = source.size();
    const auto add = [&](Token::Type type, const QString &text, double number = 0) {
        tokens << Token{type, text, number, line};
    };
    while (i < n) {
        const QChar c = source.at(i);
        if (c == QLatin1Char('\n')) {
            add(Token::Type::Newline, QString());
            ++line;
            ++i;
        } else if (c == QLatin1Char(' ') || c == QLatin1Char('\t') || c == QLatin1Char('\r')) {
            ++i;
        } else if (c == QLatin1Char('\'')) {
            while (i < n && source.at(i) != QLatin1Char('\n'))
                ++i;
        } else if (c == QLatin1Char(':')) {
            add(Token::Type::Colon, QStringLiteral(":"));
            ++i;
        } else if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && source.at(i + 1).isDigit())) {
            const qsizetype start = i;
            while (i < n && source.at(i).isDigit())
                ++i;
            if (i < n && source.at(i) == QLatin1Char('.')) {
                ++i;
                while (i < n && source.at(i).isDigit())
                    ++i;
            }
            add(Token::Type::Number, QString(), source.mid(start, i - start).toDouble());
        } else if (c == QLatin1Char('"')) {
            QString text;
            ++i;
            while (true) {
                if (i >= n || source.at(i) == QLatin1Char('\n'))
                    throw ParseError{line, QStringLiteral("a text is not closed")};
                if (source.at(i) == QLatin1Char('"')) {
                    if (i + 1 < n && source.at(i + 1) == QLatin1Char('"')) {
                        text += QLatin1Char('"');
                        i += 2;
                        continue;
                    }
                    ++i;
                    break;
                }
                text += source.at(i++);
            }
            add(Token::Type::Text, text);
        } else if (c.isLetter() || c == QLatin1Char('_')) {
            const qsizetype start = i;
            while (i < n && (source.at(i).isLetterOrNumber() || source.at(i) == QLatin1Char('_')))
                ++i;
            const QString name = source.mid(start, i - start).toUpper();
            if (name == QLatin1String("REM")) {
                while (i < n && source.at(i) != QLatin1Char('\n'))
                    ++i;
                continue;
            }
            add(Token::Type::Name, name);
        } else {
            static const QStringList pairs{QStringLiteral("<="), QStringLiteral(">="), QStringLiteral("<>")};
            const QString two = source.mid(i, 2);
            if (pairs.contains(two)) {
                add(Token::Type::Symbol, two);
                i += 2;
            } else if (QStringLiteral("()[],=<>+-*/").contains(c)) {
                add(Token::Type::Symbol, QString(c));
                ++i;
            } else {
                throw ParseError{line, QStringLiteral("unexpected character '%1'").arg(c)};
            }
        }
    }
    add(Token::Type::End, QString());
    return tokens;
}

} // namespace

/// Reads the tokens into SmartScript's tree, by recursive descent.
class SmartParser {
public:
    explicit SmartParser(QList<Token> tokens)
        : m_tokens(std::move(tokens))
    {
    }

    SmartScript program()
    {
        SmartScript script;
        while (true) {
            skipTerminators();
            if (at(Token::Type::End))
                break;
            if (isWord(QStringLiteral("FUNCTION"))) {
                SmartScript::Function function = parseFunction();
                if (script.m_functions.contains(function.name))
                    throw ParseError{function.line, QStringLiteral("%1 is defined twice").arg(function.name)};
                script.m_functions.insert(function.name, function);
                continue;
            }
            script.m_main.push_back(statement());
            endOfStatement();
        }
        return script;
    }

private:
    using Expr = SmartScript::Expr;
    using Stmt = SmartScript::Stmt;
    using ExprPtr = SmartScript::ExprPtr;
    using StmtPtr = SmartScript::StmtPtr;
    using Block = SmartScript::Block;

    const Token &peek(int ahead = 0) const { return m_tokens.at(qMin(m_pos + ahead, int(m_tokens.size()) - 1)); }
    bool at(Token::Type type) const { return peek().type == type; }
    bool isWord(const QString &word, int ahead = 0) const
    {
        return peek(ahead).type == Token::Type::Name && peek(ahead).text == word;
    }
    bool isSymbol(const QString &symbol, int ahead = 0) const
    {
        return peek(ahead).type == Token::Type::Symbol && peek(ahead).text == symbol;
    }
    bool isTerminator() const
    {
        return at(Token::Type::Newline) || at(Token::Type::Colon) || at(Token::Type::End);
    }
    Token take() { return m_tokens.at(m_pos < m_tokens.size() - 1 ? m_pos++ : m_pos); }
    [[noreturn]] void fail(const QString &message) const { throw ParseError{peek().line, message}; }
    QString describe(const Token &token) const
    {
        switch (token.type) {
        case Token::Type::Number: return SmartValue::numberText(token.number);
        case Token::Type::Text: return QStringLiteral("a text");
        case Token::Type::Name: return token.text;
        case Token::Type::Symbol: return token.text;
        case Token::Type::Newline: return QStringLiteral("the end of the line");
        case Token::Type::Colon: return QStringLiteral(":");
        case Token::Type::End: return QStringLiteral("the end of the file");
        }
        return {};
    }
    void expectWord(const QString &word)
    {
        if (!isWord(word))
            fail(QStringLiteral("%1 expected, found %2").arg(word, describe(peek())));
        take();
    }
    void expectSymbol(const QString &symbol)
    {
        if (!isSymbol(symbol))
            fail(QStringLiteral("'%1' expected, found %2").arg(symbol, describe(peek())));
        take();
    }
    QString name()
    {
        if (!at(Token::Type::Name) || keywords().contains(peek().text))
            fail(QStringLiteral("a name expected, found %1").arg(describe(peek())));
        return take().text;
    }
    void skipTerminators()
    {
        while (at(Token::Type::Newline) || at(Token::Type::Colon))
            take();
    }
    void endOfStatement()
    {
        if (!isTerminator())
            fail(QStringLiteral("the end of the statement expected, found %1").arg(describe(peek())));
    }

    /// Statements until a line starting with one of `enders`, which is left to the caller.
    Block block(const QStringList &enders, const QString &opened, int openedLine)
    {
        Block statements;
        while (true) {
            skipTerminators();
            if (at(Token::Type::End))
                throw ParseError{openedLine, QStringLiteral("%1 is never closed").arg(opened)};
            for (const QString &ender : enders) {
                if (isWord(ender))
                    return statements;
            }
            statements.push_back(statement());
            endOfStatement();
        }
    }

    SmartScript::Function parseFunction()
    {
        SmartScript::Function function;
        function.line = peek().line;
        expectWord(QStringLiteral("FUNCTION"));
        function.name = name();
        expectSymbol(QStringLiteral("("));
        if (!isSymbol(QStringLiteral(")"))) {
            do {
                const QString param = name();
                if (function.params.contains(param))
                    fail(QStringLiteral("%1 is a parameter twice").arg(param));
                function.params << param;
            } while (isSymbol(QStringLiteral(",")) && (take(), true));
        }
        expectSymbol(QStringLiteral(")"));
        endOfStatement();
        m_inFunction = true;
        function.body = block({QStringLiteral("END")}, QStringLiteral("FUNCTION ") + function.name, function.line);
        m_inFunction = false;
        expectWord(QStringLiteral("END"));
        expectWord(QStringLiteral("FUNCTION"));
        return function;
    }

    StmtPtr statement()
    {
        auto stmt = std::make_shared<Stmt>();
        stmt->line = peek().line;
        if (!at(Token::Type::Name))
            fail(QStringLiteral("a statement expected, found %1").arg(describe(peek())));
        const QString word = peek().text;
        if (word == QLatin1String("LET")) {
            take();
            return assignment(stmt);
        }
        if (word == QLatin1String("CONST")) {
            take();
            if (m_inFunction)
                fail(QStringLiteral("CONST belongs at the top of the program"));
            stmt->kind = Stmt::Kind::Const;
            stmt->name = name();
            expectSymbol(QStringLiteral("="));
            stmt->exprs.push_back(expression());
            return stmt;
        }
        if (word == QLatin1String("IF"))
            return ifStatement(stmt);
        if (word == QLatin1String("FOR"))
            return forStatement(stmt);
        if (word == QLatin1String("WHILE")) {
            take();
            stmt->kind = Stmt::Kind::While;
            stmt->exprs.push_back(expression());
            endOfStatement();
            ++m_loops;
            stmt->blocks.push_back(block({QStringLiteral("WEND")}, QStringLiteral("WHILE"), stmt->line));
            --m_loops;
            expectWord(QStringLiteral("WEND"));
            return stmt;
        }
        if (word == QLatin1String("EXIT")) {
            take();
            if (isWord(QStringLiteral("FOR")))
                stmt->kind = Stmt::Kind::ExitFor;
            else if (isWord(QStringLiteral("WHILE")))
                stmt->kind = Stmt::Kind::ExitWhile;
            else
                fail(QStringLiteral("EXIT FOR or EXIT WHILE expected"));
            take();
            if (m_loops == 0)
                fail(QStringLiteral("EXIT outside a loop"));
            return stmt;
        }
        if (word == QLatin1String("RETURN")) {
            take();
            if (!m_inFunction)
                fail(QStringLiteral("RETURN outside a function"));
            stmt->kind = Stmt::Kind::Return;
            if (!isTerminator() && !isWord(QStringLiteral("ELSE")))
                stmt->exprs.push_back(expression());
            return stmt;
        }
        if (word == QLatin1String("SHARED")) {
            take();
            if (!m_inFunction)
                fail(QStringLiteral("SHARED belongs in a function"));
            stmt->kind = Stmt::Kind::Shared;
            QStringList names{name()};
            while (isSymbol(QStringLiteral(","))) {
                take();
                names << name();
            }
            stmt->name = names.join(QLatin1Char(','));
            return stmt;
        }
        if (word == QLatin1String("CALL")) {
            take();
            stmt->kind = Stmt::Kind::Call;
            stmt->name = name();
            expectSymbol(QStringLiteral("("));
            stmt->exprs = arguments();
            return stmt;
        }
        if (word == QLatin1String("FUNCTION"))
            fail(QStringLiteral("a FUNCTION cannot be inside another statement"));
        if (keywords().contains(word))
            fail(QStringLiteral("%1 cannot start a statement").arg(word));
        // A name: an assignment, or a call.
        if (isSymbol(QStringLiteral("="), 1) || isSymbol(QStringLiteral("["), 1))
            return assignment(stmt);
        stmt->kind = Stmt::Kind::Call;
        stmt->name = take().text;
        if (isSymbol(QStringLiteral("("))) {
            // Parentheses around all the arguments, unless they belong to the first one.
            const int saved = m_pos;
            take();
            std::vector<ExprPtr> args = arguments();
            if (isTerminator() || isWord(QStringLiteral("ELSE"))) {
                stmt->exprs = args;
                return stmt;
            }
            m_pos = saved;
        }
        if (!isTerminator() && !isWord(QStringLiteral("ELSE"))) {
            stmt->exprs.push_back(expression());
            while (isSymbol(QStringLiteral(","))) {
                take();
                stmt->exprs.push_back(expression());
            }
        }
        return stmt;
    }

    StmtPtr assignment(const std::shared_ptr<Stmt> &stmt)
    {
        stmt->name = name();
        stmt->kind = Stmt::Kind::Assign;
        if (isSymbol(QStringLiteral("["))) {
            take();
            stmt->kind = Stmt::Kind::AssignIndex;
            stmt->exprs.push_back(expression());
            expectSymbol(QStringLiteral("]"));
        }
        expectSymbol(QStringLiteral("="));
        stmt->exprs.push_back(expression());
        return stmt;
    }

    StmtPtr ifStatement(const std::shared_ptr<Stmt> &stmt)
    {
        stmt->kind = Stmt::Kind::If;
        take(); // IF
        stmt->exprs.push_back(expression());
        expectWord(QStringLiteral("THEN"));
        if (!at(Token::Type::Newline) && !at(Token::Type::End)) {
            // All on one line: statements up to ELSE or the end of the line.
            stmt->blocks.push_back(lineStatements());
            if (isWord(QStringLiteral("ELSE"))) {
                take();
                stmt->blocks.push_back(lineStatements());
            }
            return stmt;
        }
        stmt->blocks.push_back(block({QStringLiteral("ELSEIF"), QStringLiteral("ELSE"), QStringLiteral("END")},
                                     QStringLiteral("IF"), stmt->line));
        while (isWord(QStringLiteral("ELSEIF"))) {
            take();
            stmt->exprs.push_back(expression());
            expectWord(QStringLiteral("THEN"));
            endOfStatement();
            stmt->blocks.push_back(block({QStringLiteral("ELSEIF"), QStringLiteral("ELSE"), QStringLiteral("END")},
                                         QStringLiteral("IF"), stmt->line));
        }
        if (isWord(QStringLiteral("ELSE"))) {
            take();
            endOfStatement();
            stmt->blocks.push_back(block({QStringLiteral("END")}, QStringLiteral("IF"), stmt->line));
        }
        expectWord(QStringLiteral("END"));
        expectWord(QStringLiteral("IF"));
        return stmt;
    }

    Block lineStatements()
    {
        Block statements;
        while (true) {
            statements.push_back(statement());
            if (at(Token::Type::Colon)) {
                take();
                continue;
            }
            if (isWord(QStringLiteral("ELSE")) || at(Token::Type::Newline) || at(Token::Type::End))
                return statements;
            fail(QStringLiteral("the end of the statement expected, found %1").arg(describe(peek())));
        }
    }

    StmtPtr forStatement(const std::shared_ptr<Stmt> &stmt)
    {
        stmt->kind = Stmt::Kind::For;
        take(); // FOR
        stmt->name = name();
        expectSymbol(QStringLiteral("="));
        stmt->exprs.push_back(expression());
        expectWord(QStringLiteral("TO"));
        stmt->exprs.push_back(expression());
        if (isWord(QStringLiteral("STEP"))) {
            take();
            stmt->exprs.push_back(expression());
        } else {
            stmt->exprs.push_back(nullptr);
        }
        endOfStatement();
        ++m_loops;
        stmt->blocks.push_back(block({QStringLiteral("NEXT")}, QStringLiteral("FOR ") + stmt->name, stmt->line));
        --m_loops;
        expectWord(QStringLiteral("NEXT"));
        if (at(Token::Type::Name) && !keywords().contains(peek().text)) {
            if (peek().text != stmt->name)
                fail(QStringLiteral("NEXT %1 closes FOR %2").arg(peek().text, stmt->name));
            take();
        }
        return stmt;
    }

    /// The arguments after an opening parenthesis, and the closing one.
    std::vector<ExprPtr> arguments()
    {
        std::vector<ExprPtr> args;
        if (!isSymbol(QStringLiteral(")"))) {
            args.push_back(expression());
            while (isSymbol(QStringLiteral(","))) {
                take();
                args.push_back(expression());
            }
        }
        expectSymbol(QStringLiteral(")"));
        return args;
    }

    ExprPtr node(Expr::Kind kind, int line, const QString &name, std::vector<ExprPtr> args = {},
                 SmartValue value = {})
    {
        auto expr = std::make_shared<Expr>();
        expr->kind = kind;
        expr->line = line;
        expr->name = name;
        expr->args = std::move(args);
        expr->value = std::move(value);
        return expr;
    }

    ExprPtr expression() { return orExpression(); }

    ExprPtr orExpression()
    {
        ExprPtr left = andExpression();
        while (isWord(QStringLiteral("OR"))) {
            const int line = take().line;
            left = node(Expr::Kind::Binary, line, QStringLiteral("OR"), {left, andExpression()});
        }
        return left;
    }

    ExprPtr andExpression()
    {
        ExprPtr left = notExpression();
        while (isWord(QStringLiteral("AND"))) {
            const int line = take().line;
            left = node(Expr::Kind::Binary, line, QStringLiteral("AND"), {left, notExpression()});
        }
        return left;
    }

    ExprPtr notExpression()
    {
        if (isWord(QStringLiteral("NOT"))) {
            const int line = take().line;
            return node(Expr::Kind::Unary, line, QStringLiteral("NOT"), {notExpression()});
        }
        return comparison();
    }

    ExprPtr comparison()
    {
        ExprPtr left = additive();
        static const QStringList operators{QStringLiteral("="), QStringLiteral("<>"), QStringLiteral("<"),
                                           QStringLiteral(">"), QStringLiteral("<="), QStringLiteral(">=")};
        if (at(Token::Type::Symbol) && operators.contains(peek().text)) {
            const Token op = take();
            left = node(Expr::Kind::Binary, op.line, op.text, {left, additive()});
            if (at(Token::Type::Symbol) && operators.contains(peek().text))
                fail(QStringLiteral("comparisons cannot be chained: use AND"));
        }
        return left;
    }

    ExprPtr additive()
    {
        ExprPtr left = multiplicative();
        while (isSymbol(QStringLiteral("+")) || isSymbol(QStringLiteral("-"))) {
            const Token op = take();
            left = node(Expr::Kind::Binary, op.line, op.text, {left, multiplicative()});
        }
        return left;
    }

    ExprPtr multiplicative()
    {
        ExprPtr left = unary();
        while (isSymbol(QStringLiteral("*")) || isSymbol(QStringLiteral("/")) || isWord(QStringLiteral("MOD"))) {
            const Token op = take();
            left = node(Expr::Kind::Binary, op.line, op.text, {left, unary()});
        }
        return left;
    }

    ExprPtr unary()
    {
        if (isSymbol(QStringLiteral("-"))) {
            const int line = take().line;
            return node(Expr::Kind::Unary, line, QStringLiteral("-"), {unary()});
        }
        return postfix();
    }

    ExprPtr postfix()
    {
        ExprPtr expr = primary();
        while (isSymbol(QStringLiteral("["))) {
            const int line = take().line;
            ExprPtr index = expression();
            expectSymbol(QStringLiteral("]"));
            expr = node(Expr::Kind::Index, line, QString(), {expr, index});
        }
        return expr;
    }

    ExprPtr primary()
    {
        const Token token = peek();
        switch (token.type) {
        case Token::Type::Number:
            take();
            return node(Expr::Kind::Literal, token.line, QString(), {}, SmartValue(token.number));
        case Token::Type::Text:
            take();
            return node(Expr::Kind::Literal, token.line, QString(), {}, SmartValue(token.text));
        case Token::Type::Symbol:
            if (token.text == QLatin1String("(")) {
                take();
                ExprPtr inner = expression();
                expectSymbol(QStringLiteral(")"));
                return inner;
            }
            if (token.text == QLatin1String("[")) {
                take();
                std::vector<ExprPtr> items;
                if (!isSymbol(QStringLiteral("]"))) {
                    items.push_back(expression());
                    while (isSymbol(QStringLiteral(","))) {
                        take();
                        items.push_back(expression());
                    }
                }
                expectSymbol(QStringLiteral("]"));
                return node(Expr::Kind::List, token.line, QString(), std::move(items));
            }
            break;
        case Token::Type::Name:
            if (token.text == QLatin1String("TRUE") || token.text == QLatin1String("FALSE")) {
                take();
                return node(Expr::Kind::Literal, token.line, QString(), {},
                            SmartValue(token.text == QLatin1String("TRUE")));
            }
            if (token.text == QLatin1String("NOTHING")) {
                take();
                return node(Expr::Kind::Literal, token.line, QString(), {}, SmartValue());
            }
            if (!keywords().contains(token.text)) {
                take();
                if (isSymbol(QStringLiteral("("))) {
                    take();
                    return node(Expr::Kind::Call, token.line, token.text, arguments());
                }
                return node(Expr::Kind::Name, token.line, token.text);
            }
            break;
        default:
            break;
        }
        fail(QStringLiteral("a value expected, found %1").arg(describe(token)));
    }

    QList<Token> m_tokens;
    int m_pos = 0;
    bool m_inFunction = false;
    int m_loops = 0;
};

std::optional<SmartScript> SmartScript::parse(const QString &source, QString *error)
{
    try {
        return SmartParser(tokenize(source)).program();
    } catch (const ParseError &failure) {
        if (error)
            *error = QStringLiteral("line %1: %2").arg(failure.line).arg(failure.message);
        return std::nullopt;
    }
}
