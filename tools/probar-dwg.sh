#!/usr/bin/env bash
# Pasa un DWG por el mismo código nativo que usa la app, en el ordenador, y
# enseña qué sale: entidades, capas, lo que no se convierte y una imagen.
#
# Uso:
#   tools/probar-dwg.sh plano.dwg [salida.ppm] [ancho]
#   tools/probar-dwg.sh --solo-compilar
#
# La primera vez clona y compila LibreDWG para el ordenador (unos minutos).
# También deja compilada build-host/libdwgjni.so, que es lo que usan los tests
# de Gradle para abrir planos de verdad:
#   ./gradlew testDebugUnitTest
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR="$RAIZ/vendor/libredwg"
# shellcheck source=tools/libredwg-version.sh
. "$RAIZ/tools/libredwg-version.sh"

NUCLEOS="$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) | head -1 )"

obtener_libredwg "$VENDOR"

if [ ! -f "$VENDOR/build-host/libredwg.a" ]; then
  echo "▶ Compilando LibreDWG para este equipo…" >&2
  cmake -S "$VENDOR" -B "$VENDOR/build-host" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLIBREDWG_LIBONLY=ON \
    -DLIBREDWG_DISABLE_WRITE=ON \
    -DLIBREDWG_DISABLE_JSON=ON \
    -DDISABLE_WERROR=ON \
    -DENABLE_LTO=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON >&2
  cmake --build "$VENDOR/build-host" --target redwg -j"$NUCLEOS" >&2
fi

cmake -S "$RAIZ/tools/host" -B "$RAIZ/build-host" -G Ninja -DCMAKE_BUILD_TYPE=Release >&2
cmake --build "$RAIZ/build-host" -j"$NUCLEOS" >&2

if [ "${1:-}" = "--solo-compilar" ]; then
  exit 0
fi
if [ $# -lt 1 ]; then
  echo "uso: tools/probar-dwg.sh plano.dwg [salida.ppm] [ancho]" >&2
  exit 2
fi

exec "$RAIZ/build-host/dwgprobe" "$@"
