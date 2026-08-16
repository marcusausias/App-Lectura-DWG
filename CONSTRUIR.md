# Cómo compilar la app

## Qué necesitas instalado

| Herramienta | Versión | Para qué |
|---|---|---|
| Android Studio | Ladybug o posterior | SDK, NDK y emulador |
| Android NDK | r26 o posterior | Compilar LibreDWG y el núcleo C++ |
| CMake | 3.22.1 | Lo instala el propio SDK Manager |
| JDK | 17 | Lo trae Android Studio |
| perl | cualquiera | **LibreDWG genera código fuente con él durante el build** |
| git | cualquiera | Clonar LibreDWG y su submódulo |

En Android Studio: **Settings → SDK Tools** y marca `NDK (Side by side)` y
`CMake`. Anota la ruta del NDK, que suele ser
`~/Android/Sdk/ndk/<versión>`.

---

## Paso 1 — Compilar LibreDWG para Android

Solo hace falta hacerlo una vez. El script clona LibreDWG en `vendor/`, lo
compila para las dos arquitecturas ARM y deja los `.a` donde CMake los busca.

```bash
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/27.0.12077973   # ajusta la versión
tools/build-libredwg-android.sh
```

Tarda unos minutos por arquitectura. Al terminar deberías tener:

```
vendor/prebuilt/arm64-v8a/libredwg.a
vendor/prebuilt/armeabi-v7a/libredwg.a
```

### Si falla

**`../jsmn/jsmn.h: No such file or directory`**
Falta el submódulo de LibreDWG. El script ya lo trae, pero si clonaste
`vendor/libredwg` a mano:
```bash
git -C vendor/libredwg submodule update --init --depth 1
```
El error no menciona en ningún momento que falte un submódulo, así que es fácil
perder un rato con él.

**`perl: command not found`**
Instala perl. LibreDWG lo usa para generar parte de su código fuente.

**Cualquier otro error de compilación de LibreDWG**
Borra la carpeta de build y reintenta: `rm -rf vendor/libredwg/build-android-*`.
Una configuración de CMake a medias deja restos que confunden el siguiente
intento.

---

## Paso 2 — Compilar la app

```bash
./gradlew assembleDebug
```

El APK queda en `app/build/outputs/apk/debug/`.

Para instalarlo en un móvil conectado por USB con la depuración activada:

```bash
./gradlew installDebug
```

### Si falla

**`Falta vendor/prebuilt/<abi>/libredwg.a`**
No has hecho el paso 1, o lo has hecho para otra arquitectura.

**`No toolchains found in the NDK toolchains folder`**
La versión de NDK que tiene Gradle no coincide con la instalada. Comprueba la
ruta en `local.properties` o fija `ndkVersion` en `app/build.gradle.kts`.

---

## Paso 3 — Comprobar que lee tus planos

Abre la app y pulsa **Abrir un DWG**. Es la pantalla de validación de la Fase 0:
te dice versión del archivo, número de entidades, capas, bloques, referencias
externas y **cuánto ha tardado en leerlo**.

Lo que hay que mirar:

- **Versión**: debería reconocerla, de r13 a r2018.
- **Tiempo**: en el equipo de desarrollo, un archivo de 11.000 objetos tarda
  39 ms. En un móvil cuenta con bastante más, pero si un plano tuyo pasa de
  10 segundos hay que replantear el enfoque.
- **Entidades**: si sale muy por debajo de lo que esperas del plano, hay tipos
  de entidad sin convertir todavía.
- **Referencias externas**: si tu plano tiene xrefs deberían aparecer con su
  ruta completa. Ojo, serán rutas de Windows del estilo `N:\Obra\...` que no
  existen en el móvil; resolverlas es el trabajo de la Fase 3.

---

## Los tests del núcleo geométrico

No necesitan Android ni dispositivo: corren en el ordenador y son los que
protegen la exactitud de las mediciones.

```bash
cmake -S core -B build-core -DDWGCORE_BUILD_TESTS=ON
cmake --build build-core
./build-core/tests/dwgcore_tests
./build-core/tests/dwgcore_transform_tests
```

Ambos deben terminar con `0 fallos`. Si tocas algo de geometría, esto es lo
primero que hay que volver a pasar.

---

## Analizar un DWG desde el ordenador

Sin pasar por el móvil, para saber qué trae un plano por dentro:

```bash
# Una vez: compilar las herramientas de LibreDWG para el propio ordenador
git clone --depth 1 https://github.com/LibreDWG/libredwg.git /tmp/libredwg
git -C /tmp/libredwg submodule update --init --depth 1
cmake -S /tmp/libredwg -B /tmp/libredwg/build && cmake --build /tmp/libredwg/build -j

# Y ya se puede analizar cualquier plano
tools/analizar-dwg.sh /tmp/libredwg/build ~/planos/PROPUESTA.dwg
```

Da versión, recuento de entidades por tipo, capas, bloques y referencias
externas.
