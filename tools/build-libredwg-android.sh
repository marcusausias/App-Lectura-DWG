#!/usr/bin/env bash
# Compila LibreDWG para Android con el NDK como biblioteca ESTÁTICA y la deja
# en vendor/prebuilt/<abi>/libredwg.a, lista para que CMake la enlace.
#
# Uso:  ANDROID_NDK_HOME=/ruta/al/ndk  tools/build-libredwg-android.sh
#
# Por qué estática y no compartida (medido en el equipo de desarrollo):
#   libredwg.so completa ....................... 20,2 MB por ABI (ya con strip)
#   libredwg.a enlazada con --gc-sections ....... 7,6 MB por ABI
#   El strip apenas quita nada: son 20 MB de código real, sobre todo las tablas
#   generadas para cada tipo de objeto de cada versión de DWG. Enlazando en
#   estático el enlazador descarta lo que no se llama, y con dos ABIs eso son
#   25 MB menos de APK.
#
# Requisitos aprendidos a base de tropezar:
#   - El submódulo `jsmn` DEBE estar presente aunque se desactive el soporte
#     JSON. Sin él el build muere en in_json.c con un error que no menciona en
#     ningún momento que falte un submódulo, y `git clone --depth 1` no lo trae.
#   - Hace falta perl: LibreDWG genera código fuente durante la compilación.
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR="$RAIZ/vendor/libredwg"
DESTINO="$RAIZ/vendor/prebuilt"
API_MINIMA=26
# Debe coincidir con abiFilters en app/build.gradle.kts.
ABIS=("arm64-v8a")

if [ -z "${ANDROID_NDK_HOME:-}" ]; then
  echo "Define ANDROID_NDK_HOME con la ruta al NDK." >&2
  exit 1
fi

TOOLCHAIN="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
  echo "No encuentro el toolchain del NDK en $TOOLCHAIN" >&2
  exit 1
fi

if ! command -v perl >/dev/null; then
  echo "Falta perl: LibreDWG lo necesita para generar código durante el build." >&2
  exit 1
fi

if [ ! -d "$VENDOR" ]; then
  echo "▶ Clonando LibreDWG en vendor/…"
  git clone --depth 1 https://github.com/LibreDWG/libredwg.git "$VENDOR"
fi

echo "▶ Asegurando el submódulo jsmn…"
git -C "$VENDOR" submodule update --init --depth 1

for abi in "${ABIS[@]}"; do
  echo "▶ Compilando para $abi…"
  BUILD="$VENDOR/build-android-$abi"

  # LIBREDWG_LIBONLY deja fuera las herramientas de línea de comandos y hace
  # que el objetivo `redwg` produzca libredwg.a.
  # DISABLE_WRITE quita el codificador entero: la app solo lee.
  cmake -S "$VENDOR" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="android-$API_MINIMA" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLIBREDWG_LIBONLY=ON \
    -DLIBREDWG_DISABLE_WRITE=ON \
    -DLIBREDWG_DISABLE_JSON=ON \
    -DDISABLE_WERROR=ON \
    -DENABLE_LTO=OFF \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections"

  # ENABLE_LTO viene activado por defecto y en el equipo se veía "IPO / LTO
  # enabled". Cruzando con el NDK, el formato de objeto que deja LTO en el
  # archivo estático choca con el --gc-sections que aplicamos al enlazar
  # libdwgjni.so, así que se desactiva aquí. DISABLE_WERROR evita que un aviso
  # nuevo del clang del NDK tumbe toda la compilación.

  cmake --build "$BUILD" --target redwg -j"$(nproc)"

  mkdir -p "$DESTINO/$abi"
  cp "$BUILD/libredwg.a" "$DESTINO/$abi/"
  echo "  ✓ $DESTINO/$abi/libredwg.a ($(( $(stat -c%s "$DESTINO/$abi/libredwg.a") / 1024 / 1024 )) MB en archivo; al enlazar se queda en ~8 MB)"
done

echo
echo "Listo. Las cabeceras quedan en $VENDOR/include, que es donde las busca"
echo "app/src/main/cpp/CMakeLists.txt."
