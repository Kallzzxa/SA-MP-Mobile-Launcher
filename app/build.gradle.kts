import java.io.File
import java.net.URI

plugins {
    id("com.android.application")
    id("com.google.gms.google-services") apply false
    id("com.google.firebase.crashlytics") apply false
}

// Firebase must be configured for the actual application ID, not the upstream app.
// A build without matching configuration works without Firebase services.
val firebaseConfig = file("google-services.json")
val hasHopePrideFirebase = firebaseConfig.exists() &&
    Regex("\"package_name\"\\s*:\\s*\"com\\.hopepride\\.samp\"")
        .containsMatchIn(firebaseConfig.readText())
if (hasHopePrideFirebase) {
    apply(plugin = "com.google.gms.google-services")
    apply(plugin = "com.google.firebase.crashlytics")
} else {
    logger.lifecycle("Firebase disabled: add app/google-services.json for com.hopepride.samp to enable it")
}

val stageGlossHookJniLibs = tasks.register<Sync>("stageGlossHookJniLibs") {
    from("src/main/cpp/samp/vendor/GlossHook/libs/ARM64") {
        include("libGlossHook.so")
        into("arm64-v8a")
    }
    from("src/main/cpp/samp/vendor/GlossHook/libs/ARM") {
        include("libGlossHook.so")
        into("armeabi-v7a")
    }
    into(layout.buildDirectory.dir("generated/glosshook-jniLibs"))
}

android {
    namespace = "com.sampmobile.xyvern"
    //noinspection GradleDependency
    compileSdk = 34

    defaultConfig {
        // Public relay endpoint only. Never put a Discord webhook/token in the APK.
        val relayUrl = providers.gradleProperty("hopeprideEventRelayUrl").orElse("").get().trim()
        if (relayUrl.isNotEmpty()) {
            val uri = try { URI(relayUrl) } catch (_: Exception) { null }
            val host = uri?.host?.lowercase() ?: ""
            require(uri != null && uri.scheme == "https" && host.isNotEmpty() && uri.userInfo == null &&
                    uri.rawQuery == null && uri.fragment == null && host != "discord.com" &&
                    !host.endsWith(".discord.com") && host != "discordapp.com" && !host.endsWith(".discordapp.com")) {
                "hopeprideEventRelayUrl must be a public HTTPS relay URL, never a Discord webhook"
            }
        }
        val escapedRelayUrl = relayUrl.replace("\\", "\\\\").replace("\"", "\\\"")
        buildConfigField("String", "EVENT_RELAY_URL", "\"$escapedRelayUrl\"")
        // Direct mode embeds this credential into the APK. Use only when accepting that risk.
        val webhookUrl = providers.environmentVariable("HOPEPRIDE_DISCORD_WEBHOOK_URL").orElse("").get().trim()
        if (webhookUrl.isNotEmpty()) {
            val uri = try { URI(webhookUrl) } catch (_: Exception) { null }
            require(uri != null && uri.scheme == "https" && uri.host == "discord.com" && uri.port == -1 &&
                    uri.userInfo == null && uri.rawQuery == null && uri.fragment == null &&
                    Regex("^/api/webhooks/[0-9]+/[A-Za-z0-9._-]+$").matches(uri.path ?: "")) {
                "HOPEPRIDE_DISCORD_WEBHOOK_URL is not a valid Discord webhook URL"
            }
        }
        val escapedWebhook = webhookUrl.replace("\\", "\\\\").replace("\"", "\\\"")
        buildConfigField("String", "DISCORD_WEBHOOK_URL", "\"$escapedWebhook\"")
        applicationId = "com.hopepride.samp"
        minSdk = 26
        targetSdk = 34
        versionCode = 164
        versionName = "1.7.0"

        multiDexEnabled = true

        ndk {
            abiFilters.addAll(listOf("arm64-v8a", "armeabi-v7a"))
        }

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    splits {
        abi {
            isEnable = false
            reset()
            include("armeabi-v7a", "arm64-v8a")
            isUniversalApk = false
        }
    }

    signingConfigs {
        getByName("debug") {
            enableV1Signing = true
            enableV2Signing = true
        }
        create("release") {
            storeFile = file("${rootDir}/app-release-key.jks")
            storePassword = "password123456"
            keyAlias = "sampkey"
            keyPassword = "password123456"
            enableV1Signing = true
            enableV2Signing = true
        }
    }

    buildTypes {
        getByName("debug") {
            isDebuggable = true
            isJniDebuggable = false
            isMinifyEnabled = false

            // Debug APKs use the local debug key, never the release signing key.
            signingConfig = signingConfigs.getByName("debug")

            externalNativeBuild {
                cmake {
                    abiFilters.addAll(listOf("arm64-v8a", "armeabi-v7a"))
                    cppFlags += "-fvisibility=default"
                }
            }

            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }

        getByName("release") {
            isDebuggable = false
            isJniDebuggable = false
            isMinifyEnabled = false

            signingConfig = signingConfigs.getByName("release")

            externalNativeBuild {
                cmake {
                    abiFilters.addAll(listOf("arm64-v8a", "armeabi-v7a"))
                    cppFlags += "-fvisibility=hidden"
                }
            }

            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    sourceSets {
        getByName("main") {
            jniLibs.srcDir("src/main/jniLibs")
            jniLibs.srcDir("src/main/cpp/samp/vendor/bass/libs")
            jniLibs.srcDir(layout.buildDirectory.dir("generated/glosshook-jniLibs"))
        }
    }

    ndkVersion = "26.2.11394342"

    lint {
        abortOnError = false
        checkReleaseBuilds = false
    }

    buildFeatures {
        prefab = true
        viewBinding = true
        buildConfig = true
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
            excludes += "META-INF/*"
        }
        resources {
            excludes += "META-INF/*"
        }
    }
}

tasks.configureEach {
    if (name.startsWith("merge") &&
        (name.endsWith("JniLibFolders") || name.endsWith("NativeLibs"))) {
        dependsOn(stageGlossHookJniLibs)
    }
}

dependencies {
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar"))))

    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.constraintlayout)

    implementation(libs.navigation.fragment)
    implementation(libs.navigation.ui)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.test.ext.junit)
    androidTestImplementation(libs.espresso.core)

    implementation(libs.prdownloader)
    implementation(libs.volley)
    implementation(libs.sdp)

    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.analytics)
    implementation(libs.firebase.crashlytics.ndk)
    implementation(libs.firebase.messaging)

    implementation("com.github.PhilJay:MPAndroidChart:v3.1.0")
    implementation(libs.ini4j)
    implementation(libs.glide)
    implementation(libs.lifecycle.process)
    implementation(libs.paranoid)
    implementation(libs.shadowhook)
}

// Optional Laragon deployment on local Windows development machine only
val laragonDir = File("C:\\laragon\\www")
if (System.getenv("GITHUB_ACTIONS") == null && laragonDir.exists()) {
    afterEvaluate {
        android.applicationVariants.all {
            val variant = this
            val taskName = "copy${variant.name.replaceFirstChar { if (it.isLowerCase()) it.uppercase() else it.toString() }}ApksToLaragon"
            
            tasks.register<Copy>(taskName) {
                doNotTrackState("Destination directory contains unreadable content")
                description = "Copies ${variant.name} APKs to Laragon www directory"
                into(laragonDir)
                
                variant.outputs.all {
                    @Suppress("DEPRECATION")
                    val output = this as com.android.build.gradle.api.BaseVariantOutput
                    from(output.outputFile)
                }
                
                doLast {
                    println("Copied APKs for variant '${variant.name}' to C:\\laragon\\www")
                }
            }
            
            variant.assembleProvider.configure {
                finalizedBy(taskName)
            }
        }
    }
}
