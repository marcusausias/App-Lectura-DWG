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

## Paso 3 — Comprobar que lee y dibuja tus planos

Abre la app, pulsa **Abrir** y elige un DWG. La primera vez tarda en procesarlo;
a partir de ahí se abre desde caché y es casi instantáneo.

Lo que hay que mirar, por orden de importancia:

1. **Que se vea como en AutoCAD.** Compáralo con el PDF del mismo plano. Lo que
   delata un fallo de conversión es geometría desplazada, en espejo, o que falte
   una parte del dibujo.
2. **Zoom profundo sobre un detalle.** Las líneas no deben temblar ni vibrar al
   acercarse. Si tiemblan, hay un problema de precisión con las coordenadas.
3. **Tiempo de la primera apertura.** En el equipo de desarrollo, un plano de
   81.000 entidades tarda 110 ms en procesarse y 25 ms en reabrirse. En un móvil
   cuenta con bastante más, pero si un plano tuyo pasa de 10 segundos hay que
   replantear el enfoque.
4. **Capas.** *Capas* abre la lista; apagar o aislar una debe ocultar lo mismo
   que en AutoCAD.
5. **Fondo.** Alterna claro y oscuro; a pleno sol suele leerse mejor el claro.

Si el plano se abre pero sale muy vacío, es que trae tipos de entidad que
todavía no se convierten. Quedan pendientes los sombreados (HATCH) y las
directrices (LEADER).

**Referencias externas:** todavía no se resuelven. Un plano con xrefs se abrirá
sin el contenido referenciado. Es el trabajo de la Fase 3, y sus rutas serán de
Windows (`N:\Obra\...`), que no existen en el móvil.

---

## Paso 4 — Probar la medición

Con un plano abierto, en la barra de abajo:

| Herramienta | Qué hace |
|---|---|
| **Distancia** | Dos toques y se cierra sola |
| **Polilínea** | Toques seguidos, luego *Cerrar*; suma los tramos |
| **Superficie** | Marca el contorno y *Cerrar*; área por la fórmula de Gauss |
| **Seguir** | Toca una línea del plano y la mide entera, arcos incluidos |
| **Contar** | Toca un símbolo y dice cuántos iguales hay |

Al apoyar el dedo aparece una cruz amarilla con el punto al que engancharía y su
tipo (extremo, intersección, punto medio…). Si mueves el dedo más de 6 px, el
gesto pasa a ser paneo y la vista previa desaparece.

**La prueba que de verdad importa:** busca una cota acotada en tu plano, mide esa
misma distancia con la herramienta y comprueba que **coincide con el valor
escrito en la cota**. Si no coincide, hay un problema de escala o de enganche y
hay que resolverlo antes que cualquier otra cosa.

Lo que conviene comprobar además:

- Enganchar a una esquina debe dar la esquina exacta, no un punto cercano.
- Con una capa apagada, no debe poder engancharse a su geometría.
- Una medición sobre una spline o una elipse debe aparecer marcada
  **(aprox.)**: esa geometría llega ya teselada y su longitud es buena pero no
  exacta.
- *Contar* sobre un símbolo repetido debe dar el número correcto. En el archivo
  de pruebas del proyecto salen 54 rociadores y 28 laterales.

---

## Los tests del núcleo geométrico

No necesitan Android ni dispositivo: corren en el ordenador y son los que
protegen la exactitud de las mediciones.

```bash
cmake -S core -B build-core -DDWGCORE_BUILD_TESTS=ON
cmake --build build-core
./build-core/tests/dwgcore_tests
./build-core/tests/dwgcore_transform_tests
./build-core/tests/dwgcore_scene_tests
./build-core/tests/dwgcore_render_tests
./build-core/tests/dwgcore_snap_tests
```

Todos deben terminar con `0 fallos`. Si tocas algo de geometría, esto es lo
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
