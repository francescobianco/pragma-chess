package org.pragmachess.mobile.engine

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.XmlResourceParser
import android.os.Build
import org.xmlpull.v1.XmlPullParser
import java.io.File

/**
 * A chess engine installed as a separate app through the Open Exchange (OEX)
 * protocol, as DroidFish and other chess apps use it: the app declares an
 * activity for "intent.chess.provider.ENGINE", lists its engines in
 * res/xml/enginelist.xml, and ships each one as a native library.
 */
data class OexEngine(
    val name: String,
    /** Package of the app that provides the engine. */
    val packageName: String,
    /** File name in enginelist.xml, usually "libstockfish.so". */
    val fileName: String,
) {
    /** Stable across launches, to remember the user's choice. */
    val id: String get() = "$packageName/$fileName"
}

/** One <engine> of an enginelist.xml. */
data class EngineListEntry(val name: String, val fileName: String, val target: String)

object OexEngines {
    const val ACTION = "intent.chess.provider.ENGINE"
    private const val AUTHORITY = "chess.provider.engine.authority"

    /**
     * Free engine apps for this protocol, offered when none is installed:
     * Stockfish 19 Chess Engine, OEX, free and without ads.
     */
    const val SUGGESTED_PACKAGE = "com.stockfish141"

    /** The entries of a list that run on a phone with [abis] (Build.SUPPORTED_ABIS). */
    fun matching(entries: List<EngineListEntry>, abis: List<String>): List<EngineListEntry> {
        val supported = abis.map { it.lowercase() }.toSet() + "all"
        return entries.filter { entry ->
            entry.name.isNotBlank() && entry.fileName.isNotBlank() &&
                entry.target.split('|').any { it.trim().lowercase() in supported }
        }.distinctBy { it.name }
    }

    /**
     * The file to execute: providers ship engines as libraries, so they sit in
     * their nativeLibraryDir, under the listed name or with a "lib" prefix.
     */
    fun binaryCandidates(fileName: String): List<String> {
        val base = File(fileName).name
        return if (base.startsWith("lib")) listOf(base) else listOf(base, "lib$base")
    }

    /** The engines installed on this phone, by name. */
    fun installed(context: Context): List<OexEngine> {
        val pm = context.packageManager
        val providers = pm.queryIntentActivities(Intent(ACTION), PackageManager.GET_META_DATA)
        val abis = Build.SUPPORTED_ABIS.toList()
        val result = ArrayList<OexEngine>()
        for (info in providers) {
            val activity = info.activityInfo ?: continue
            if (activity.metaData?.getString(AUTHORITY).isNullOrBlank()) continue
            val entries = runCatching {
                val resources = pm.getResourcesForApplication(activity.applicationInfo)
                val id = resources.getIdentifier("enginelist", "xml", activity.packageName)
                if (id == 0) emptyList() else resources.getXml(id).use { readEngineList(it) }
            }.getOrDefault(emptyList())
            for (entry in matching(entries, abis)) {
                result += OexEngine(entry.name, activity.packageName, entry.fileName)
            }
        }
        return result.distinctBy { it.id }.sortedBy { it.name.lowercase() }
    }

    /** The executable of [engine], or null if its app is gone or ships no such file. */
    fun binary(context: Context, engine: OexEngine): File? {
        val dir = runCatching {
            context.packageManager.getApplicationInfo(engine.packageName, 0).nativeLibraryDir
        }.getOrNull() ?: return null
        return binaryCandidates(engine.fileName).map { File(dir, it) }.firstOrNull { it.isFile }
    }

    private fun readEngineList(parser: XmlResourceParser): List<EngineListEntry> {
        val entries = ArrayList<EngineListEntry>()
        var event = parser.eventType
        while (event != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.START_TAG && parser.name.equals("engine", ignoreCase = true)) {
                entries += EngineListEntry(
                    name = parser.getAttributeValue(null, "name").orEmpty(),
                    fileName = parser.getAttributeValue(null, "filename").orEmpty(),
                    target = parser.getAttributeValue(null, "target").orEmpty(),
                )
            }
            event = parser.next()
        }
        return entries
    }
}
