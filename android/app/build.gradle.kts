plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val buildNumber = (System.getenv("GITHUB_RUN_NUMBER") ?: "1").toInt()

android {
    namespace = "com.loopdraft.app"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.loopdraft.app"
        minSdk = 26
        targetSdk = 34
        versionCode = buildNumber
        versionName = "0.9.$buildNumber"
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }   // x86_64 is for the emulator test on GitHub
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static")
                cppFlags += "-std=c++17"
            }
        }
    }

    // A fixed key, so every build can be installed over the previous one without losing the saved project.
    // It only signs private sideload builds. A store release would need its own, secret key.
    signingConfigs {
        create("sideload") {
            storeFile = rootProject.file("sideload.keystore")
            storePassword = "loopdraft"
            keyAlias = "loopdraft"
            keyPassword = "loopdraft"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("sideload")
        }
        debug {
            signingConfig = signingConfigs.getByName("sideload")
        }
    }

    // Presets and grooves from the repository's content/ folder ship inside the app (as assets/library/...).
    sourceSets { getByName("main") { assets.srcDirs("../../content") } }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true }
    composeOptions { kotlinCompilerExtensionVersion = "1.5.14" }
    lint {
        checkReleaseBuilds = false
        abortOnError = false
    }
    packaging { resources { excludes += "/META-INF/{AL2.0,LGPL2.1}" } }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.06.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.9.0")
    implementation("androidx.core:core-ktx:1.13.1")
}
