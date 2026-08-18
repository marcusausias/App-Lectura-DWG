cmake_minimum_required(VERSION 3.22)
project(dwgjni CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAIZ "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")

# Núcleo geométrico compartido con los tests que corren en el equipo.
add_subdirectory("${RAIZ}/core" dwgcore_build)

# LibreDWG se compila aparte con tools/build-libredwg-android.sh: su build
# tiene sus propias comprobaciones de configuración y generación de código, y
# meterlo dentro del build de Gradle lo hace mucho más frágil.
set(LIBREDWG_LIB "${RAIZ}/vendor/prebuilt/${ANDROID_ABI}/libredwg.a")
set(LIBREDWG_INCLUDE "${RAIZ}/vendor/libredwg/include")

if(NOT EXISTS "${LIBREDWG_LIB}")
  message(FATAL_ERROR
    "Falta ${LIBREDWG_LIB}.\n"
    "Ejecuta primero:  ANDROID_NDK_HOME=... tools/build-libredwg-android.sh")
endif()

add_library(redwg STATIC IMPORTED)
set_target_properties(redwg PROPERTIES
  IMPORTED_LOCATION "${LIBREDWG_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${LIBREDWG_INCLUDE}"
)

add_library(dwgjni SHARED
  dwg_jni.cpp
  dwg_document.cpp
  dwg_extract.cpp
  dwg_scene_jni.cpp
)

target_link_libraries(dwgjni PRIVATE dwgcore redwg log m)

# Sin esto el enlazador arrastra los 20 MB completos de LibreDWG en lugar de
# quedarse en los ~8 MB que realmente se usan.
target_link_options(dwgjni PRIVATE "-Wl,--gc-sections")
target_compile_options(dwgjni PRIVATE -ffunction-sections -fdata-sections -Wall)
