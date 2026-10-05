package org.pragmachess.mobile.smart

/** A mistake found while reading or running a program: its line (0 until known) and what went wrong. */
class SmartError(var line: Int, message: String) : Exception(message)

/**
 * A SMART program read from its text: the top-level statements and the
 * functions, as a tree [SmartInterpreter] runs. Reading stops at the first
 * mistake. The grammar needs one token of lookahead and never goes back.
 */
class SmartScript private constructor(val main: List<Stmt>, val functions: Map<String, Function>) {

    class Expr(
        val kind: Kind,
        val line: Int,
        val value: SmartValue = SmartValue.None,
        /** Name, Call; the operator of Unary and Binary. */
        val name: String = "",
        /** List items, Call arguments, operands, Index: list then index. */
        val args: List<Expr> = emptyList(),
    ) {
        enum class Kind { Literal, Name, List, Unary, Binary, Call, Index }
    }

    class Stmt(
        val kind: Kind,
        val line: Int,
        var name: String = "",
        /** As on the desktop: the value; index then value; from, to, step (null); condition; arguments; conditions. */
        val exprs: MutableList<Expr?> = mutableListOf(),
        val blocks: MutableList<List<Stmt>> = mutableListOf(),
    ) {
        enum class Kind { Assign, AssignIndex, Const, If, For, While, ExitFor, ExitWhile, Return, Call, Shared }
    }

    class Function(val name: String, val params: List<String>, val body: List<Stmt>, val line: Int)

    companion object {
        /** Reads a program; throws [SmartError] on a mistake. */
        fun parse(source: String): SmartScript {
            val parser = Parser(tokenize(source))
            val (main, functions) = parser.program()
            return SmartScript(main, functions)
        }

        /** Reads a program; null, with the reason in [error], on a mistake ("line 12: …"). */
        fun parse(source: String, error: (String) -> Unit): SmartScript? =
            try {
                parse(source)
            } catch (e: SmartError) {
                error("line ${e.line}: ${e.message}")
                null
            }
    }
}

private val KEYWORDS = setOf(
    "LET", "CONST", "IF", "THEN", "ELSE", "ELSEIF", "END", "FOR", "TO", "STEP", "NEXT", "WHILE", "WEND", "EXIT",
    "FUNCTION", "RETURN", "SHARED", "CALL", "AND", "OR", "NOT", "MOD", "TRUE", "FALSE", "NOTHING", "REM",
)

private class Token(val type: Type, val text: String, val number: Double, val line: Int) {
    enum class Type { Number, Text, Name, Symbol, Newline, Colon, End }
}

private fun tokenize(source: String): List<Token> {
    val tokens = ArrayList<Token>()
    var line = 1
    var i = 0
    val n = source.length
    fun add(type: Token.Type, text: String, number: Double = 0.0) {
        tokens += Token(type, text, number, line)
    }
    while (i < n) {
        val c = source[i]
        when {
            c == '\n' -> {
                add(Token.Type.Newline, "")
                line++
                i++
            }
            c == ' ' || c == '\t' || c == '\r' -> i++
            c == '\'' -> while (i < n && source[i] != '\n') i++
            c == ':' -> {
                add(Token.Type.Colon, ":")
                i++
            }
            c.isDigit() || (c == '.' && i + 1 < n && source[i + 1].isDigit()) -> {
                val start = i
                while (i < n && source[i].isDigit()) i++
                if (i < n && source[i] == '.') {
                    i++
                    while (i < n && source[i].isDigit()) i++
                }
                add(Token.Type.Number, "", source.substring(start, i).toDouble())
            }
            c == '"' -> {
                val text = StringBuilder()
                i++
                while (true) {
                    if (i >= n || source[i] == '\n') throw SmartError(line, "a text is not closed")
                    if (source[i] == '"') {
                        if (i + 1 < n && source[i + 1] == '"') {
                            text.append('"')
                            i += 2
                            continue
                        }
                        i++
                        break
                    }
                    text.append(source[i++])
                }
                add(Token.Type.Text, text.toString())
            }
            c.isLetter() || c == '_' -> {
                val start = i
                while (i < n && (source[i].isLetterOrDigit() || source[i] == '_')) i++
                val name = source.substring(start, i).uppercase()
                if (name == "REM") {
                    while (i < n && source[i] != '\n') i++
                    continue
                }
                add(Token.Type.Name, name)
            }
            else -> {
                val two = if (i + 1 < n) source.substring(i, i + 2) else ""
                if (two == "<=" || two == ">=" || two == "<>") {
                    add(Token.Type.Symbol, two)
                    i += 2
                } else if (c in "()[],=<>+-*/") {
                    add(Token.Type.Symbol, c.toString())
                    i++
                } else {
                    throw SmartError(line, "unexpected character '$c'")
                }
            }
        }
    }
    add(Token.Type.End, "")
    return tokens
}

private class Parser(private val tokens: List<Token>) {
    private var pos = 0
    private var inFunction = false
    private var loops = 0

    private fun peek() = tokens[pos]
    private fun at(type: Token.Type) = peek().type == type
    private fun isWord(word: String) = peek().type == Token.Type.Name && peek().text == word
    private fun isSymbol(symbol: String) = peek().type == Token.Type.Symbol && peek().text == symbol
    private fun isTerminator() = at(Token.Type.Newline) || at(Token.Type.Colon) || at(Token.Type.End)
    private fun take(): Token = tokens[if (pos < tokens.size - 1) pos++ else pos]
    private fun fail(message: String): Nothing = throw SmartError(peek().line, message)

    private fun describe(token: Token): String = when (token.type) {
        Token.Type.Number -> SmartValue.numberText(token.number)
        Token.Type.Text -> "a text"
        Token.Type.Name, Token.Type.Symbol -> token.text
        Token.Type.Newline -> "the end of the line"
        Token.Type.Colon -> ":"
        Token.Type.End -> "the end of the file"
    }

    private fun expectWord(word: String) {
        if (!isWord(word)) fail("$word expected, found ${describe(peek())}")
        take()
    }

    private fun expectSymbol(symbol: String) {
        if (!isSymbol(symbol)) fail("'$symbol' expected, found ${describe(peek())}")
        take()
    }

    private fun name(): String {
        if (!at(Token.Type.Name) || peek().text in KEYWORDS) fail("a name expected, found ${describe(peek())}")
        return take().text
    }

    private fun skipTerminators() {
        while (at(Token.Type.Newline) || at(Token.Type.Colon)) take()
    }

    private fun endOfStatement() {
        if (!isTerminator()) fail("the end of the statement expected, found ${describe(peek())}")
    }

    fun program(): Pair<List<SmartScript.Stmt>, Map<String, SmartScript.Function>> {
        val main = ArrayList<SmartScript.Stmt>()
        val functions = LinkedHashMap<String, SmartScript.Function>()
        while (true) {
            skipTerminators()
            if (at(Token.Type.End)) break
            if (isWord("FUNCTION")) {
                val function = parseFunction()
                if (function.name in functions) throw SmartError(function.line, "${function.name} is defined twice")
                functions[function.name] = function
                continue
            }
            main += statement()
            endOfStatement()
        }
        return main to functions
    }

    /** Statements until a line starting with one of [enders], which is left to the caller. */
    private fun block(enders: List<String>, opened: String, openedLine: Int): List<SmartScript.Stmt> {
        val statements = ArrayList<SmartScript.Stmt>()
        while (true) {
            skipTerminators()
            if (at(Token.Type.End)) throw SmartError(openedLine, "$opened is never closed")
            if (enders.any { isWord(it) }) return statements
            statements += statement()
            endOfStatement()
        }
    }

    private fun parseFunction(): SmartScript.Function {
        val line = peek().line
        expectWord("FUNCTION")
        val name = name()
        expectSymbol("(")
        val params = ArrayList<String>()
        if (!isSymbol(")")) {
            while (true) {
                val param = name()
                if (param in params) fail("$param is a parameter twice")
                params += param
                if (!isSymbol(",")) break
                take()
            }
        }
        expectSymbol(")")
        endOfStatement()
        inFunction = true
        val body = block(listOf("END"), "FUNCTION $name", line)
        inFunction = false
        expectWord("END")
        expectWord("FUNCTION")
        return SmartScript.Function(name, params, body, line)
    }

    private fun statement(): SmartScript.Stmt {
        val line = peek().line
        if (!at(Token.Type.Name)) fail("a statement expected, found ${describe(peek())}")
        val word = peek().text
        when (word) {
            "LET" -> {
                take()
                return assignment(name(), line)
            }
            "CONST" -> {
                take()
                if (inFunction) fail("CONST belongs at the top of the program")
                val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.Const, line, name())
                expectSymbol("=")
                stmt.exprs += expression()
                return stmt
            }
            "IF" -> return ifStatement(line)
            "FOR" -> return forStatement(line)
            "WHILE" -> {
                take()
                val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.While, line)
                stmt.exprs += expression()
                endOfStatement()
                loops++
                stmt.blocks += block(listOf("WEND"), "WHILE", line)
                loops--
                expectWord("WEND")
                return stmt
            }
            "EXIT" -> {
                take()
                val kind = when {
                    isWord("FOR") -> SmartScript.Stmt.Kind.ExitFor
                    isWord("WHILE") -> SmartScript.Stmt.Kind.ExitWhile
                    else -> fail("EXIT FOR or EXIT WHILE expected")
                }
                take()
                if (loops == 0) fail("EXIT outside a loop")
                return SmartScript.Stmt(kind, line)
            }
            "RETURN" -> {
                take()
                if (!inFunction) fail("RETURN outside a function")
                val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.Return, line)
                if (!isTerminator() && !isWord("ELSE")) stmt.exprs += expression()
                return stmt
            }
            "SHARED" -> {
                take()
                if (!inFunction) fail("SHARED belongs in a function")
                val names = arrayListOf(name())
                while (isSymbol(",")) {
                    take()
                    names += name()
                }
                return SmartScript.Stmt(SmartScript.Stmt.Kind.Shared, line, names.joinToString(","))
            }
            "CALL" -> {
                take()
                val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.Call, line, name())
                expectSymbol("(")
                stmt.exprs += arguments()
                return stmt
            }
            "FUNCTION" -> fail("a FUNCTION cannot be inside another statement")
        }
        if (word in KEYWORDS) fail("$word cannot start a statement")
        // A name: an assignment if "=" or "[" follows, else a call, its
        // arguments without parentheses. One token decides.
        val name = take().text
        if (isSymbol("=") || isSymbol("[")) return assignment(name, line)
        val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.Call, line, name)
        if (!isTerminator() && !isWord("ELSE")) {
            stmt.exprs += expression()
            while (isSymbol(",")) {
                take()
                stmt.exprs += expression()
            }
        }
        return stmt
    }

    /** The rest of an assignment, its variable already read. */
    private fun assignment(name: String, line: Int): SmartScript.Stmt {
        val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.Assign, line, name)
        if (isSymbol("[")) {
            take()
            stmt.exprs += expression()
            expectSymbol("]")
            val indexed = SmartScript.Stmt(SmartScript.Stmt.Kind.AssignIndex, line, name, stmt.exprs)
            expectSymbol("=")
            indexed.exprs += expression()
            return indexed
        }
        expectSymbol("=")
        stmt.exprs += expression()
        return stmt
    }

    private fun ifStatement(line: Int): SmartScript.Stmt {
        val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.If, line)
        take() // IF
        stmt.exprs += expression()
        expectWord("THEN")
        if (!at(Token.Type.Newline) && !at(Token.Type.End)) {
            // All on one line: statements up to ELSE or the end of the line.
            stmt.blocks += lineStatements()
            if (isWord("ELSE")) {
                take()
                stmt.blocks += lineStatements()
            }
            return stmt
        }
        val enders = listOf("ELSEIF", "ELSE", "END")
        stmt.blocks += block(enders, "IF", line)
        while (isWord("ELSEIF")) {
            take()
            stmt.exprs += expression()
            expectWord("THEN")
            endOfStatement()
            stmt.blocks += block(enders, "IF", line)
        }
        if (isWord("ELSE")) {
            take()
            endOfStatement()
            stmt.blocks += block(listOf("END"), "IF", line)
        }
        expectWord("END")
        expectWord("IF")
        return stmt
    }

    private fun lineStatements(): List<SmartScript.Stmt> {
        val statements = ArrayList<SmartScript.Stmt>()
        while (true) {
            statements += statement()
            if (at(Token.Type.Colon)) {
                take()
                continue
            }
            if (isWord("ELSE") || at(Token.Type.Newline) || at(Token.Type.End)) return statements
            fail("the end of the statement expected, found ${describe(peek())}")
        }
    }

    private fun forStatement(line: Int): SmartScript.Stmt {
        take() // FOR
        val stmt = SmartScript.Stmt(SmartScript.Stmt.Kind.For, line, name())
        expectSymbol("=")
        stmt.exprs += expression()
        expectWord("TO")
        stmt.exprs += expression()
        if (isWord("STEP")) {
            take()
            stmt.exprs += expression()
        } else {
            stmt.exprs += null
        }
        endOfStatement()
        loops++
        stmt.blocks += block(listOf("NEXT"), "FOR ${stmt.name}", line)
        loops--
        expectWord("NEXT")
        if (at(Token.Type.Name) && peek().text !in KEYWORDS) {
            if (peek().text != stmt.name) fail("NEXT ${peek().text} closes FOR ${stmt.name}")
            take()
        }
        return stmt
    }

    /** The arguments after an opening parenthesis, and the closing one. */
    private fun arguments(): List<SmartScript.Expr> {
        val args = ArrayList<SmartScript.Expr>()
        if (!isSymbol(")")) {
            args += expression()
            while (isSymbol(",")) {
                take()
                args += expression()
            }
        }
        expectSymbol(")")
        return args
    }

    private fun node(kind: SmartScript.Expr.Kind, line: Int, name: String, args: List<SmartScript.Expr> = emptyList(),
                     value: SmartValue = SmartValue.None) = SmartScript.Expr(kind, line, value, name, args)

    private fun expression(): SmartScript.Expr = orExpression()

    private fun orExpression(): SmartScript.Expr {
        var left = andExpression()
        while (isWord("OR")) {
            val line = take().line
            left = node(SmartScript.Expr.Kind.Binary, line, "OR", listOf(left, andExpression()))
        }
        return left
    }

    private fun andExpression(): SmartScript.Expr {
        var left = notExpression()
        while (isWord("AND")) {
            val line = take().line
            left = node(SmartScript.Expr.Kind.Binary, line, "AND", listOf(left, notExpression()))
        }
        return left
    }

    private fun notExpression(): SmartScript.Expr {
        if (isWord("NOT")) {
            val line = take().line
            return node(SmartScript.Expr.Kind.Unary, line, "NOT", listOf(notExpression()))
        }
        return comparison()
    }

    private val comparisons = setOf("=", "<>", "<", ">", "<=", ">=")

    private fun comparison(): SmartScript.Expr {
        var left = additive()
        if (at(Token.Type.Symbol) && peek().text in comparisons) {
            val op = take()
            left = node(SmartScript.Expr.Kind.Binary, op.line, op.text, listOf(left, additive()))
            if (at(Token.Type.Symbol) && peek().text in comparisons) fail("comparisons cannot be chained: use AND")
        }
        return left
    }

    private fun additive(): SmartScript.Expr {
        var left = multiplicative()
        while (isSymbol("+") || isSymbol("-")) {
            val op = take()
            left = node(SmartScript.Expr.Kind.Binary, op.line, op.text, listOf(left, multiplicative()))
        }
        return left
    }

    private fun multiplicative(): SmartScript.Expr {
        var left = unary()
        while (isSymbol("*") || isSymbol("/") || isWord("MOD")) {
            val op = take()
            left = node(SmartScript.Expr.Kind.Binary, op.line, op.text, listOf(left, unary()))
        }
        return left
    }

    private fun unary(): SmartScript.Expr {
        if (isSymbol("-")) {
            val line = take().line
            return node(SmartScript.Expr.Kind.Unary, line, "-", listOf(unary()))
        }
        return postfix()
    }

    private fun postfix(): SmartScript.Expr {
        var expr = primary()
        while (isSymbol("[")) {
            val line = take().line
            val index = expression()
            expectSymbol("]")
            expr = node(SmartScript.Expr.Kind.Index, line, "", listOf(expr, index))
        }
        return expr
    }

    private fun primary(): SmartScript.Expr {
        val token = peek()
        when (token.type) {
            Token.Type.Number -> {
                take()
                return node(SmartScript.Expr.Kind.Literal, token.line, "", value = SmartValue.Number(token.number))
            }
            Token.Type.Text -> {
                take()
                return node(SmartScript.Expr.Kind.Literal, token.line, "", value = SmartValue.Text(token.text))
            }
            Token.Type.Symbol -> {
                if (token.text == "(") {
                    take()
                    val inner = expression()
                    expectSymbol(")")
                    return inner
                }
                if (token.text == "[") {
                    take()
                    val items = ArrayList<SmartScript.Expr>()
                    if (!isSymbol("]")) {
                        items += expression()
                        while (isSymbol(",")) {
                            take()
                            items += expression()
                        }
                    }
                    expectSymbol("]")
                    return node(SmartScript.Expr.Kind.List, token.line, "", items)
                }
            }
            Token.Type.Name -> {
                if (token.text == "TRUE" || token.text == "FALSE") {
                    take()
                    return node(SmartScript.Expr.Kind.Literal, token.line, "", value = SmartValue.of(token.text == "TRUE"))
                }
                if (token.text == "NOTHING") {
                    take()
                    return node(SmartScript.Expr.Kind.Literal, token.line, "", value = SmartValue.None)
                }
                if (token.text !in KEYWORDS) {
                    take()
                    if (isSymbol("(")) {
                        take()
                        return node(SmartScript.Expr.Kind.Call, token.line, token.text, arguments())
                    }
                    return node(SmartScript.Expr.Kind.Name, token.line, token.text)
                }
            }
            else -> {}
        }
        fail("a value expected, found ${describe(token)}")
    }
}
