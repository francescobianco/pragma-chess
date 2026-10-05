package org.pragmachess.mobile.smart

import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.truncate

/**
 * Runs a [SmartScript]: the client defines its functions (the chess, and the
 * commands that collect what the program shows), loads the program (its
 * top-level statements run once) and then calls its entry functions as often
 * as it needs; the globals live in between, as the program's memory.
 */
class SmartInterpreter(private val script: SmartScript) {
    /** A function of the client: gets the evaluated arguments, may [fail] for a wrong use. */
    fun interface Builtin {
        fun call(args: List<SmartValue>): SmartValue
    }

    private val builtins = HashMap<String, Builtin>()
    private val globals = HashMap<String, SmartValue>()
    private val constants = HashSet<String>()

    private class Frame {
        val locals = HashMap<String, SmartValue>()
        val shared = HashSet<String>()
        var returned: SmartValue = SmartValue.None
    }

    private val frames = ArrayList<Frame>()
    private var steps = 0L

    /** Statements one call may run before it is stopped. */
    var stepLimit = 2_000_000L

    init {
        defineCore()
    }

    fun define(name: String, builtin: Builtin) {
        builtins[name.uppercase()] = builtin
    }

    /** A constant of the client (WHITE, BLACK…), before [load]. */
    fun defineConstant(name: String, value: SmartValue) {
        globals[name.uppercase()] = value
        constants += name.uppercase()
    }

    /** Runs the top-level statements, once. Null on success, else the mistake ("line 12: …"). */
    fun load(): String? {
        frames.clear()
        steps = 0
        return try {
            run(script.main)
            null
        } catch (e: SmartError) {
            "line ${e.line}: ${e.message}"
        }
    }

    /** Calls a function of the program; its value, or a failure with the mistake ("line 12: …"). */
    fun call(function: String, args: List<SmartValue> = emptyList()): Result<SmartValue> {
        frames.clear()
        steps = 0
        return try {
            val name = function.uppercase()
            if (name !in script.functions) fail("the program has no function $name")
            Result.success(invoke(name, args, 0))
        } catch (e: SmartError) {
            frames.clear()
            Result.failure(IllegalStateException("line ${e.line}: ${e.message}"))
        }
    }

    fun hasFunction(name: String) = name.uppercase() in script.functions

    fun global(name: String): SmartValue = globals[name.uppercase()] ?: SmartValue.None

    fun setGlobal(name: String, value: SmartValue) {
        globals[name.uppercase()] = value
    }

    private enum class Flow { Normal, Return, ExitFor, ExitWhile }

    private fun run(block: List<SmartScript.Stmt>): Flow {
        for (stmt in block) {
            val flow = run(stmt)
            if (flow != Flow.Normal) return flow
        }
        return Flow.Normal
    }

    private fun countStep() {
        if (++steps > stepLimit) fail("stopped after $stepLimit statements: a loop that never ends?")
    }

    private fun run(stmt: SmartScript.Stmt): Flow {
        try {
            countStep()
            when (stmt.kind) {
                SmartScript.Stmt.Kind.Assign -> write(stmt.name, evaluate(stmt.exprs[0]!!))
                SmartScript.Stmt.Kind.AssignIndex -> {
                    val list = read(stmt.name)
                    if (list !is SmartValue.Items) fail("${stmt.name} is a ${list.typeName}, not a list")
                    val index = evaluate(stmt.exprs[0]!!)
                    val value = evaluate(stmt.exprs[1]!!)
                    val items = list.items
                    if (index !is SmartValue.Number || index.value != truncate(index.value) || index.value < 0 ||
                        index.value >= items.size)
                        fail("index ${index.toText()} is outside ${stmt.name}, which has ${items.size} element(s)")
                    val changed = items.toMutableList()
                    changed[index.value.toInt()] = value
                    write(stmt.name, SmartValue.Items(changed))
                }
                SmartScript.Stmt.Kind.Const -> {
                    if (stmt.name in globals) fail("${stmt.name} already has a value")
                    globals[stmt.name] = evaluate(stmt.exprs[0]!!)
                    constants += stmt.name
                }
                SmartScript.Stmt.Kind.If -> {
                    for (i in stmt.exprs.indices) {
                        if (truth(evaluate(stmt.exprs[i]!!))) return run(stmt.blocks[i])
                    }
                    if (stmt.blocks.size > stmt.exprs.size) return run(stmt.blocks.last())
                }
                SmartScript.Stmt.Kind.For -> {
                    val from = evaluate(stmt.exprs[0]!!)
                    val to = evaluate(stmt.exprs[1]!!)
                    val step = stmt.exprs[2]?.let { evaluate(it) } ?: SmartValue.Number(1.0)
                    if (from !is SmartValue.Number || to !is SmartValue.Number || step !is SmartValue.Number)
                        fail("FOR needs numbers")
                    if (step.value == 0.0) fail("FOR with STEP 0 never ends")
                    var value = from.value
                    while (if (step.value > 0) value <= to.value else value >= to.value) {
                        write(stmt.name, SmartValue.Number(value))
                        val flow = run(stmt.blocks[0])
                        if (flow == Flow.ExitFor) break
                        if (flow == Flow.Return || flow == Flow.ExitWhile) return flow
                        // The body may have changed the variable, as in BASIC.
                        val now = read(stmt.name)
                        if (now !is SmartValue.Number) fail("the FOR variable ${stmt.name} is no longer a number")
                        value = now.value + step.value
                    }
                }
                SmartScript.Stmt.Kind.While -> {
                    while (truth(evaluate(stmt.exprs[0]!!))) {
                        countStep()
                        val flow = run(stmt.blocks[0])
                        if (flow == Flow.ExitWhile) break
                        if (flow == Flow.Return || flow == Flow.ExitFor) return flow
                    }
                }
                SmartScript.Stmt.Kind.ExitFor -> return Flow.ExitFor
                SmartScript.Stmt.Kind.ExitWhile -> return Flow.ExitWhile
                SmartScript.Stmt.Kind.Return -> {
                    frames.last().returned = stmt.exprs.firstOrNull()?.let { evaluate(it) } ?: SmartValue.None
                    return Flow.Return
                }
                SmartScript.Stmt.Kind.Call -> invoke(stmt.name, stmt.exprs.map { evaluate(it!!) }, stmt.line)
                SmartScript.Stmt.Kind.Shared -> frames.last().shared += stmt.name.split(',')
            }
        } catch (e: SmartError) {
            if (e.line == 0) e.line = stmt.line
            throw e
        }
        return Flow.Normal
    }

    private fun truth(value: SmartValue): Boolean {
        if (value === SmartValue.None) return false
        if (value !is SmartValue.Number) fail("a condition must be a number, not a ${value.typeName}")
        return value.value != 0.0
    }

    private fun read(name: String): SmartValue {
        frames.lastOrNull()?.locals?.get(name)?.let { return it }
        return globals[name] ?: fail("$name has no value")
    }

    private fun write(name: String, value: SmartValue) {
        val frame = frames.lastOrNull()
        if (frame != null && name !in frame.shared) {
            frame.locals[name] = value
            return
        }
        if (name in constants) fail("$name is a constant")
        globals[name] = value
    }

    private fun evaluate(expr: SmartScript.Expr): SmartValue {
        try {
            return when (expr.kind) {
                SmartScript.Expr.Kind.Literal -> expr.value
                SmartScript.Expr.Kind.Name -> read(expr.name)
                SmartScript.Expr.Kind.List -> SmartValue.Items(expr.args.map { evaluate(it) })
                SmartScript.Expr.Kind.Unary -> {
                    val operand = evaluate(expr.args[0])
                    if (expr.name == "NOT") return SmartValue.of(!truth(operand))
                    if (operand !is SmartValue.Number) fail("- needs a number, not a ${operand.typeName}")
                    SmartValue.Number(-operand.value)
                }
                SmartScript.Expr.Kind.Binary -> binary(expr)
                SmartScript.Expr.Kind.Call -> invoke(expr.name, expr.args.map { evaluate(it) }, expr.line)
                SmartScript.Expr.Kind.Index -> {
                    val list = evaluate(expr.args[0])
                    val index = evaluate(expr.args[1])
                    if (list !is SmartValue.Items) fail("a ${list.typeName} has no elements")
                    if (index !is SmartValue.Number || index.value != truncate(index.value) || index.value < 0 ||
                        index.value >= list.items.size)
                        fail("index ${index.toText()} is outside a list of ${list.items.size} element(s)")
                    list.items[index.value.toInt()]
                }
            }
        } catch (e: SmartError) {
            if (e.line == 0) e.line = expr.line
            throw e
        }
    }

    private fun binary(expr: SmartScript.Expr): SmartValue {
        val op = expr.name
        if (op == "AND") {
            if (!truth(evaluate(expr.args[0]))) return SmartValue.FALSE
            return SmartValue.of(truth(evaluate(expr.args[1])))
        }
        if (op == "OR") {
            if (truth(evaluate(expr.args[0]))) return SmartValue.TRUE
            return SmartValue.of(truth(evaluate(expr.args[1])))
        }
        val left = evaluate(expr.args[0])
        val right = evaluate(expr.args[1])
        when (op) {
            "=" -> return SmartValue.of(left == right)
            "<>" -> return SmartValue.of(left != right)
            "+" -> {
                if (left is SmartValue.Number && right is SmartValue.Number) return SmartValue.Number(left.value + right.value)
                if ((left is SmartValue.Text && (right is SmartValue.Text || right is SmartValue.Number)) ||
                    (left is SmartValue.Number && right is SmartValue.Text))
                    return SmartValue.Text(left.toText() + right.toText())
                if (left is SmartValue.Items && right is SmartValue.Items) return SmartValue.Items(left.items + right.items)
                fail("cannot add a ${left.typeName} and a ${right.typeName}")
            }
            "<", ">", "<=", ">=" -> {
                val order = when {
                    left is SmartValue.Number && right is SmartValue.Number -> left.value.compareTo(right.value).coerceIn(-1, 1)
                    left is SmartValue.Text && right is SmartValue.Text -> left.value.compareTo(right.value)
                    else -> fail("cannot compare a ${left.typeName} with a ${right.typeName}")
                }
                return SmartValue.of(when (op) {
                    "<" -> order < 0
                    ">" -> order > 0
                    "<=" -> order <= 0
                    else -> order >= 0
                })
            }
        }
        if (left !is SmartValue.Number || right !is SmartValue.Number)
            fail("$op needs numbers, not a ${left.typeName} and a ${right.typeName}")
        val a = left.value
        val b = right.value
        return when (op) {
            "-" -> SmartValue.Number(a - b)
            "*" -> SmartValue.Number(a * b)
            else -> {
                if (b == 0.0) fail("division by zero")
                if (op == "/") SmartValue.Number(a / b) else SmartValue.Number(a - b * truncate(a / b)) // MOD
            }
        }
    }

    private fun invoke(name: String, args: List<SmartValue>, line: Int): SmartValue {
        val function = script.functions[name]
        if (function != null) {
            if (args.size != function.params.size)
                fail("$name takes ${function.params.size} argument(s), not ${args.size}")
            if (frames.size >= MAX_DEPTH) fail("$name calls itself too deep")
            val frame = Frame()
            function.params.forEachIndexed { i, param -> frame.locals[param] = args[i] }
            frames += frame
            try {
                run(function.body)
            } finally {
                frames.removeAt(frames.size - 1)
            }
            return frame.returned
        }
        val builtin = builtins[name] ?: fail("there is no function $name")
        try {
            return builtin.call(args)
        } catch (e: SmartError) {
            if (e.line == 0) e.line = line
            throw e
        }
    }

    private fun defineCore() {
        define("LEN") { args ->
            expectArguments("LEN", args, 1)
            val value = args[0]
            if (value is SmartValue.Text) SmartValue.of(value.value.length) else SmartValue.of(listArgument("LEN", args, 0).size)
        }
        define("INT") { args ->
            expectArguments("INT", args, 1)
            SmartValue.Number(truncate(numberArgument("INT", args, 0)))
        }
        define("ABS") { args ->
            expectArguments("ABS", args, 1)
            SmartValue.Number(abs(numberArgument("ABS", args, 0)))
        }
        fun extreme(name: String, least: Boolean) = Builtin { args ->
            if (args.isEmpty()) fail("$name needs at least one number")
            var result = numberArgument(name, args, 0)
            for (i in 1 until args.size) {
                val value = numberArgument(name, args, i)
                result = if (least) min(result, value) else max(result, value)
            }
            SmartValue.Number(result)
        }
        define("MIN", extreme("MIN", true))
        define("MAX", extreme("MAX", false))
        define("STR") { args ->
            expectArguments("STR", args, 1)
            SmartValue.Text(args[0].toText())
        }
        define("FIXED") { args ->
            expectArguments("FIXED", args, 2)
            val decimals = intArgument("FIXED", args, 1).coerceIn(0, 12)
            SmartValue.Text(SmartValue.fixed(numberArgument("FIXED", args, 0), decimals))
        }
        define("TRIM") { args ->
            expectArguments("TRIM", args, 1)
            SmartValue.Text(textArgument("TRIM", args, 0).trim())
        }
        define("REPEAT") { args ->
            expectArguments("REPEAT", args, 2)
            val count = intArgument("REPEAT", args, 1)
            if (count < 0) fail("REPEAT cannot make $count copies")
            SmartValue.Items(List(count) { args[0] })
        }
        define("SLICE") { args ->
            expectArguments("SLICE", args, 3)
            val items = listArgument("SLICE", args, 0)
            val from = intArgument("SLICE", args, 1).coerceIn(0, items.size)
            val count = intArgument("SLICE", args, 2).coerceIn(0, items.size - from)
            SmartValue.Items(items.subList(from, from + count).toList())
        }
        define("CONTAINS") { args ->
            expectArguments("CONTAINS", args, 2)
            SmartValue.of(args[1] in listArgument("CONTAINS", args, 0))
        }
        define("INDEXOF") { args ->
            expectArguments("INDEXOF", args, 2)
            SmartValue.of(listArgument("INDEXOF", args, 0).indexOf(args[1]))
        }
    }

    companion object {
        private const val MAX_DEPTH = 200

        /** For the client's functions: stops the program with [message]. */
        fun fail(message: String): Nothing = throw SmartError(0, message)

        fun expectArguments(function: String, args: List<SmartValue>, count: Int) {
            if (args.size != count) fail("$function takes $count argument(s), not ${args.size}")
        }

        private fun wrong(function: String, kind: String, args: List<SmartValue>, index: Int): Nothing =
            fail("$function needs a $kind as argument ${index + 1}, not ${args.getOrNull(index)?.typeName ?: "nothing"}")

        fun numberArgument(function: String, args: List<SmartValue>, index: Int): Double =
            (args.getOrNull(index) as? SmartValue.Number)?.value ?: wrong(function, "number", args, index)

        fun intArgument(function: String, args: List<SmartValue>, index: Int): Int =
            truncate(numberArgument(function, args, index)).toInt()

        fun textArgument(function: String, args: List<SmartValue>, index: Int): String =
            (args.getOrNull(index) as? SmartValue.Text)?.value ?: wrong(function, "text", args, index)

        fun listArgument(function: String, args: List<SmartValue>, index: Int): List<SmartValue> =
            (args.getOrNull(index) as? SmartValue.Items)?.items ?: wrong(function, "list", args, index)
    }
}
