import java.net.URI
import java.security.MessageDigest

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

// Stockfish for Android, the official arm64 build, is downloaded at build time
// (about 80 MB, not committed) and shipped as a native library so that it can
// be executed from nativeLibraryDir. Build with -Ppragma.stockfish=false to
// leave it out: the app then says the engine is not available.
val stockfishRelease = "sf_19"
val stockfishArchive = "stockfish-android-arm64-universal.tar.gz"
val stockfishSha256 = "ebb24051aa4a222b4daaf049b882ecf1163d370c128fe02316602643f4d5e426"
val bundleStockfish = (findProperty("pragma.stockfish") as String?)?.toBoolean() ?: true
val stockfishJniDir = layout.buildDirectory.dir("generated/stockfish/jniLibs")
val stockfishAssetsDir = layout.buildDirectory.dir("generated/stockfish/assets")

val fetchStockfish by tasks.registering {
    description = "Downloads the official Stockfish Android build into the APK's native libraries."
    val cache = File(gradle.gradleUserHomeDir, "caches/pragma-chess/$stockfishRelease/$stockfishArchive")
    val output = stockfishJniDir.map { it.file("arm64-v8a/libstockfish.so") }
    val license = stockfishAssetsDir.map { it.file("licenses/stockfish-COPYING.txt") }
    outputs.file(output)
    outputs.file(license)
    inputs.property("sha256", stockfishSha256)
    doLast {
        fun sha256(file: File): String = MessageDigest.getInstance("SHA-256").let { digest ->
            file.inputStream().use { input ->
                val buffer = ByteArray(1 shl 16)
                while (true) {
                    val read = input.read(buffer)
                    if (read < 0) break
                    digest.update(buffer, 0, read)
                }
            }
            digest.digest().joinToString("") { "%02x".format(it) }
        }
        if (!cache.exists() || sha256(cache) != stockfishSha256) {
            cache.parentFile.mkdirs()
            val url = "https://github.com/official-stockfish/Stockfish/releases/download/$stockfishRelease/$stockfishArchive"
            logger.lifecycle("Downloading $url")
            val partial = File(cache.path + ".part")
            URI(url).toURL().openStream().use { input -> partial.outputStream().use { input.copyTo(it) } }
            check(sha256(partial) == stockfishSha256) { "Stockfish archive checksum mismatch" }
            partial.renameTo(cache)
        }
        val target = output.get().asFile
        target.parentFile.mkdirs()
        val extracted = temporaryDir
        extracted.deleteRecursively()
        extracted.mkdirs()
        val tar = ProcessBuilder("tar", "-xzf", cache.absolutePath, "-C", extracted.absolutePath).inheritIO().start()
        check(tar.waitFor() == 0) { "Could not extract $cache" }
        val binary = extracted.walkTopDown().first { it.isFile && it.name == "stockfish-android-arm64-universal" }
        binary.copyTo(target, overwrite = true)
        // GPL v3: the license travels with the binary, shown in About ▸ Licenses.
        extracted.walkTopDown().first { it.isFile && it.name == "Copying.txt" }.copyTo(license.get().asFile, overwrite = true)
        extracted.deleteRecursively()
    }
}

android {
    namespace = "org.pragmachess.mobile"
    compileSdk = 36

    defaultConfig {
        applicationId = "org.pragmachess.mobile"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        buildConfigField("boolean", "HAS_STOCKFISH", bundleStockfish.toString())
        buildConfigField("String", "STOCKFISH_RELEASE", "\"$stockfishRelease\"")
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures {
        compose = true
        buildConfig = true
    }
    packaging {
        // The engine is executed from nativeLibraryDir, so it must be extracted.
        jniLibs.useLegacyPackaging = true
    }
    sourceSets {
        getByName("main") {
            if (bundleStockfish) {
                jniLibs.srcDir(stockfishJniDir)
                assets.srcDir(stockfishAssetsDir)
            }
        }
    }
    testOptions {
        unitTests.isReturnDefaultValues = true
    }
}

kotlin {
    jvmToolchain(17)
}

if (bundleStockfish) {
    tasks.named("preBuild") { dependsOn(fetchStockfish) }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.material.icons)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.okhttp)
    implementation(libs.secp256k1.android)
    implementation(libs.webrtc)
    implementation(libs.zxing.embedded)
    debugImplementation(libs.androidx.compose.ui.tooling)

    testImplementation(libs.junit)
    testImplementation(libs.secp256k1.jvm)
    testImplementation(libs.json)
}
