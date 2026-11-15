# Herramienta de escritorio: pasa un DWG por el mismo código nativo que usa la
# app (extracción, aplanado, escena y búfer de dibujo) y vuelca un resumen y
# una imagen. Sirve para comprobar un plano sin pasar por el móvil.
#
#   tools/probar-dwg.sh plano.dwg salida.ppm
cmake_minimum_required(VERSION 3.22)
project(dwgprobe CXX C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAIZ "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(LIBREDWG_LIB "${RAIZ}/vendor/libredwg/build-host/libredwg.a")

add_subdirectory("${RAIZ}/core" dwgcore_build)

add_executable(dwgprobe
  dwgprobe.cpp
  "${RAIZ}/app/src/main/cpp/dwg_extract.cpp"
)
target_include_directories(dwgprobe PRIVATE
  "${RAIZ}/app/src/main/cpp"
  "${RAIZ}/vendor/libredwg/include"
  "${RAIZ}/vendor/libredwg/build-host/src"
)
target_link_libraries(dwgprobe PRIVATE dwgcore "${LIBREDWG_LIB}" m)

# La misma biblioteca JNI que lleva la app, compilada para el ordenador. Con
# ella los tests de Gradle pueden abrir un DWG de verdad a través del puente
# nativo sin móvil ni emulador (ver AppSmokeTest y tools/probar-dwg.sh).
find_package(JNI)
if(JNI_FOUND)
  add_library(dwgjni SHARED
    "${RAIZ}/app/src/main/cpp/dwg_jni.cpp"
    "${RAIZ}/app/src/main/cpp/dwg_document.cpp"
    "${RAIZ}/app/src/main/cpp/dwg_extract.cpp"
    "${RAIZ}/app/src/main/cpp/dwg_scene_jni.cpp"
  )
  target_include_directories(dwgjni PRIVATE
    ${JNI_INCLUDE_DIRS}
    "${RAIZ}/app/src/main/cpp"
    "${RAIZ}/vendor/libredwg/include"
    "${RAIZ}/vendor/libredwg/build-host/src"
  )
  target_link_libraries(dwgjni PRIVATE dwgcore "${LIBREDWG_LIB}" m)
endif()
