# Reglas para la versión de release.
#
# Todas están aquí por el mismo motivo de fondo: R8 renombra y elimina lo que no
# ve usado desde Kotlin, y el puente con el código nativo se resuelve por nombre
# en tiempo de ejecución. R8 no puede saberlo, así que hay que decírselo.

# --- Clases que el código nativo instancia por su nombre -----------------------
#
# dwg_scene_jni.cpp y dwg_jni.cpp hacen FindClass() con la ruta completa y luego
# GetMethodID(..., "<init>", ...). Si R8 renombra la clase, FindClass devuelve
# null y la app revienta al abrir un plano. `includedescriptorclasses` mantiene
# además los tipos que aparecen en la firma del constructor.
-keep,includedescriptorclasses class com.marcusausias.dwgviewer.nativebridge.PlanText {
    <init>(...);
}
-keep,includedescriptorclasses class com.marcusausias.dwgviewer.nativebridge.XrefRef {
    <init>(...);
}
-keep,includedescriptorclasses class com.marcusausias.dwgviewer.nativebridge.DocumentSummary {
    <init>(...);
}

# --- Clases con métodos nativos ----------------------------------------------
#
# El símbolo de una función JNI incluye el paquete y el nombre de la clase
# (Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_openPlan). Renombrar
# la clase rompe el enlace con la biblioteca nativa.
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# --- Diagnóstico --------------------------------------------------------------
#
# Sin esto, un fallo en release da una traza sin números de línea y no hay forma
# de saber de dónde salió.
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
