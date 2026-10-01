plugins {
    id("com.android.application")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
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
        applicationId = "com.sampmobile.xyvern"
        minSdk = 26
        targetSdk = 36
        versionCode = 130
        versionName = "1.0"

        multiDexEnabled = true

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    splits {
        abi {
            isEnable = true
            reset()
            include("armeabi-v7a", "arm64-v8a")
            isUniversalApk = false
        }
    }

    buildTypes {
        getByName("debug") {
            isDebuggable = false
            isJniDebuggable = false
            isMinifyEnabled = false

            // Sử dụng mặc định của Android, không ép dùng cấu hình release nữa

            externalNativeBuild {
                cmake {
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

            // Đã gỡ bỏ cấu hình ký cứng để tránh lỗi tìm file jks

            externalNativeBuild {
                cmake {
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
    }

    packaging {
        jniLibs {
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

// Chỉ đăng ký task Laragon nếu đang chạy ở máy cá nhân (Không chạy trên GitHub Actions)
if (System.getenv("GITHUB_ACTIONS") == null) {
    afterEvaluate {
        android.applicationVariants.all {
            val variant = this
            val taskName = "copy${variant.name.replaceFirstChar { if (it.isLowerCase()) it.uppercase() else it.toString() }}ApksToLaragon"
            
            tasks.register<Copy>(taskName) {
                doNotTrackState("Destination directory contains unreadable content")
                description = "Copies ${variant.name} APKs to Laragon www directory"
                into("C:\\laragon\\www")
                
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
