plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.marcusausias.dwgviewer"
    compileSdk = 35

    // Se fija a propósito. Sin esta línea, el plugin de Android exige su NDK por
    // defecto y falla aunque haya otro instalado y perfectamente válido,
    // obligando a descargar 2 GB de una versión concreta.
    //
    // Al cambiar de NDK hay que actualizar este número: se ve con
    // `ls ~/Library/Android/sdk/ndk/` en Mac.
    ndkVersion = "30.0.15729638"

    defaultConfig {
        applicationId = "com.marcusausias.dwgviewer"
        // API 26 cubre prácticamente todo el parque actual y es el mínimo que
        // permite usar java.time y las APIs de documentos sin retrocompatibilidad.
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            // Solo ARM de 64 bits.
            //
            // Cualquier móvil de trabajo actual lo es; armeabi-v7a solo haría
            // falta para dispositivos de hace más de una década. Quitarlo reduce
            // a la mitad el tiempo de compilar LibreDWG, que es el paso lento, y
            // adelgaza el APK unos 8 MB.
            //
            // Para volver a añadirlo: incluirlo aquí y en la lista ABIS de
            // tools/build-libredwg-android.sh, que tienen que coincidir.
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared")
                cppFlags += "-std=c++17"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
        debug {
            isJniDebuggable = true
        }
    }

    buildFeatures {
        compose = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        // libredwg.so ronda los 20 MB sin comprimir por ABI. Comprimirla en el
        // APK es obligatorio para no acercarse al límite de subida.
        jniLibs.useLegacyPackaging = false
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.10.01"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.7")

    // Acceso a carpetas del proyecto mediante el Storage Access Framework.
    implementation("androidx.documentfile:documentfile:1.0.1")

    // Las mediciones se guardan como JSON con org.json, que viene en Android, y
    // los enlaces de xref en SharedPreferences. Para unas decenas de entradas
    // por plano, una base de datos solo añadía KSP y tiempo de compilación.

    debugImplementation("androidx.compose.ui:ui-tooling")

    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
}
