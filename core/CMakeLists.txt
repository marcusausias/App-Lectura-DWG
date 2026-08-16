cmake_minimum_required(VERSION 3.22)
project(dwgcore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Núcleo geométrico puro: sin dependencias de Android ni de LibreDWG, para que
# los mismos tests corran en el equipo de desarrollo y en el dispositivo.
add_library(dwgcore STATIC
  src/bulge.cpp
  src/area.cpp
  src/polyline.cpp
  src/ocs.cpp
  src/spline.cpp
  src/transform.cpp
  src/entity.cpp
  src/flatten.cpp
  src/spatial.cpp
  src/scene.cpp
  src/cache.cpp
)

target_include_directories(dwgcore PUBLIC include)
target_compile_options(dwgcore PRIVATE -Wall -Wextra)

option(DWGCORE_BUILD_TESTS "Compilar los tests del núcleo geométrico" OFF)
if(DWGCORE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
