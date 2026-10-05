plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

// X.Y.Z from the top-level CMakeLists.txt; versionCode is X·10000 + Y·100 + Z,
// so every release is greater than the one before.
val pragmaVersion: String = Regex("""set\(PRAGMA_VERSION\s+(\d+\.\d+\.\d+)""")
    .find(rootProject.file("../../CMakeLists.txt").readText())
    ?.groupValues?.get(1)
    ?: error("PRAGMA_VERSION not found in the top-level CMakeLists.txt")
val pragmaVersionCode: Int = pragmaVersion.split('.').map(String::toInt).let { (major, minor, patch) ->
    major * 10000 + minor * 100 + patch
}

// The SMART programs of the repository (smart/*.smart: Explain and the
// tutor, the same files the desktop runs) go into the APK as assets under
// smart/; the JVM tests read them from the folder itself.
val smartPrograms = rootProject.file("../../smart")
val copySmartPrograms by tasks.registering(Sync::class) {
    from(smartPrograms) { include("*.smart") }
    into(layout.buildDirectory.dir("generated/smart/assets/smart"))
}

android {
    namespace = "org.pragmachess.mobile"
    compileSdk = 36

    defaultConfig {
        applicationId = "org.pragmachess.mobile"
        minSdk = 26
        targetSdk = 36
        // The same version as the desktop client: PRAGMA_VERSION of the
        // top-level CMakeLists.txt, the one place a release changes it.
        versionName = pragmaVersion
        versionCode = pragmaVersionCode
    }

    // Phones only (emulators on x86 are not worth a quarter of the APK), and
    // one APK per architecture besides the universal one: almost every phone
    // takes the smaller arm64 APK.
    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "armeabi-v7a")
            isUniversalApk = true
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            // Signed with the debug key for now: testers installed a debug APK,
            // and Android only updates an app signed with the same key.
            // A release key replaces it before the app goes to a store.
            signingConfig = signingConfigs.getByName("debug")
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
        // The universal APK too: no emulator libraries.
        jniLibs.excludes += listOf("lib/x86/**", "lib/x86_64/**")
    }
    testOptions {
        unitTests.isReturnDefaultValues = true
    }
    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/smart/assets"))
}

tasks.named("preBuild") { dependsOn(copySmartPrograms) }
tasks.matching { it.name.endsWith("Assets") || it.name.startsWith("lint") }.configureEach { dependsOn(copySmartPrograms) }
tasks.withType<Test>().configureEach {
    systemProperty("pragma.smart.dir", smartPrograms.absolutePath)
    inputs.dir(smartPrograms) // A change to a program runs the tests again.
}

kotlin {
    jvmToolchain(17)
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
