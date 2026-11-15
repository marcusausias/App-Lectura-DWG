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
        versionCode = 2
        versionName = "0.2.0"

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

    signingConfigs {
        // Clave de depuración guardada en el repositorio, con la contraseña
        // estándar de Android. No protege nada ni lo pretende: existe para que
        // el APK salga firmado igual lo compile quien lo compile —este equipo,
        // otro, o GitHub—. Con la clave que genera cada máquina por su cuenta,
        // Android rechaza instalar una versión nueva encima de la anterior y
        // obliga a desinstalar, lo que borra las mediciones guardadas.
        //
        // Para publicar la app en una tienda hace falta una clave propia y
        // privada, que no debe estar aquí.
        getByName("debug") {
            val shared = rootProject.file("keystore/debug.keystore")
            if (shared.exists()) {
                storeFile = shared
                storePassword = "android"
                keyAlias = "androiddebugkey"
                keyPassword = "android"
            }
        }
    }

    buildTypes {
        release {
            // Se firma con la misma clave para que el APK de release se pueda
            // instalar directamente en el móvil. Ver la nota de arriba.
            signingConfig = signingConfigs.getByName("debug")
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

    testOptions {
        unitTests {
            isIncludeAndroidResources = true
            all { test ->
                // Biblioteca nativa compilada para el ordenador y planos de
                // muestra de LibreDWG. Si no están (no se ha ejecutado
                // tools/probar-dwg.sh --solo-compilar), los tests que abren un
                // plano de verdad se saltan y el resto se ejecuta igual.
                val hostLibs = rootProject.file("build-host")
                test.systemProperty("java.library.path", hostLibs.absolutePath)
                test.systemProperty("dwg.hostlib", File(hostLibs, System.mapLibraryName("dwgjni")).absolutePath)
                test.systemProperty(
                    "dwg.samples",
                    rootProject.file("vendor/libredwg/test/test-data").absolutePath,
                )
                test.inputs.files(fileTree(hostLibs) { include("*dwgjni*") })
                    .withPropertyName("hostNativeLibrary")
                    .optional()
            }
        }
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

    debugImplementation("androidx.compose.ui:ui-test-manifest")

    testImplementation("junit:junit:4.13.2")
    // Robolectric ejecuta la interfaz en el ordenador, sin móvil ni emulador.
    // No dibuja OpenGL ni carga la biblioteca nativa, pero sí detecta que la
    // app arranque y que la pantalla se componga sin reventar.
    testImplementation("org.robolectric:robolectric:4.14.1")
    testImplementation("androidx.test:core:1.6.1")
    testImplementation("androidx.test.ext:junit:1.2.1")
    testImplementation("androidx.compose.ui:ui-test-junit4")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
}
