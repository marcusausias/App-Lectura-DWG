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

Solo hace falta hacerlo una vez. El script descarga LibreDWG en `vendor/` (la
versión fijada en `tools/libredwg-version.sh`), lo compila para ARM de 64 bits
y deja el `.a` donde CMake lo busca.

```bash
export ANDROID_NDK_HOME=~/Library/Android/sdk/ndk/30.0.15729638   # Mac
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/30.0.15729638           # Linux
tools/build-libredwg-android.sh
```

La versión del NDK tiene que coincidir con la línea `ndkVersion` de
`app/build.gradle.kts`, o Gradle exigirá otra distinta al compilar la app.

Tarda unos minutos. Al terminar deberías tener:

```
vendor/prebuilt/arm64-v8a/libredwg.a
```

Solo se compila ARM de 64 bits. Para añadir `armeabi-v7a` hay que ponerlo en la
lista `ABIS` del script **y** en `abiFilters` de `app/build.gradle.kts`.

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

La primera vez, el wrapper descarga Gradle 8.14.3 (unos 130 MB), así que ese
primer build necesita conexión. Después ya no.

Si prefieres no tocar la terminal, abrir la carpeta del proyecto en Android
Studio y pulsar **Run** hace exactamente lo mismo. El paso 1 sí hay que
ejecutarlo antes en cualquier caso.

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

### Versión de release

`./gradlew assembleRelease` aplica R8, que reduce y renombra el código. Las
reglas de `app/proguard-rules.pro` conservan lo que el puente nativo busca por
nombre en tiempo de ejecución; sin ellas la app compila pero revienta al abrir
un plano.

Si algún día se añade una clase nueva que el C++ instancie con `FindClass`, hay
que añadirla también ahí. Se comprueba con:

```bash
grep -oP 'FindClass\("\K[^"]+' app/src/main/cpp/*.cpp | grep -v '^java/'
```

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
todavía no se convierten (imágenes, tablas, MLINE, MULTILEADER, sólidos 3D).
`tools/probar-dwg.sh plano.dwg` dice cuántas hay de cada tipo.

Las capas que en el DWG estaban apagadas o congeladas arrancan ocultas. Si echas
en falta algo, mira en *Capas*: *Ver todas* las enciende y *Como en el DWG*
vuelve al estado original.

### Referencias externas

Si el plano tiene xrefs y falta alguna, aparece una banda roja arriba. Pulsa
**Elegir carpeta** e indica la carpeta raíz del proyecto: la app buscará dentro
los archivos que faltan y **recordará el permiso**, así que la próxima vez se
resuelven solas.

Cuando una xref no aparezca ni así, **Localizar** permite señalar el archivo a
mano, y ese enlace también queda guardado.

Las rutas que guarda AutoCAD suelen ser absolutas de una unidad de red
(`N:\2017\Obra\...`), que en un móvil no existen. Por eso la búsqueda por
nombre de archivo dentro de la carpeta del proyecto es la que resuelve la
mayoría de los casos, y la ruta relativa es la excepción.

Qué comprobar:

- Abrir un plano con xrefs sin carpeta configurada → banda roja con los nombres.
- Elegir la carpeta → el plano se recarga completo y la banda desaparece.
- **Cerrar la app y volver a abrir el mismo plano → no debe volver a preguntar
  nada.** Ese es el requisito de memoria de los enlaces.

---

## Paso 4 — Probar la medición

Con un plano abierto, en la barra de abajo:

| Herramienta | Qué hace |
|---|---|
| **Distancia** | Dos toques y se cierra sola |
| **Polilínea** | Toques seguidos, luego *Cerrar*; suma los tramos |
| **Superficie** | Marca el contorno y *Cerrar*; área por la fórmula de Gauss |
| **Seguir** | Toca una línea del plano y la mide entera, arcos incluidos. Sobre un contorno cerrado (una polilínea de estancia, el borde de un sombreado) da además la superficie |
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
- **Cerrar la app del todo y reabrir el mismo plano: las mediciones deben seguir
  ahí, con sus etiquetas.** Se guardan en `filesDir`, no en la caché, para que
  Android no pueda borrarlas al quedarse sin espacio.
- Abrir otro plano y volver al primero: cada uno con las suyas, sin mezclarse.
- En *Medidas*, elegir la unidad del dibujo y los decimales, y **Guardar CSV**:
  el archivo debe abrirse en Excel con cada dato en su columna y los totales al
  final.

---

## Los tests

Ninguno necesita móvil ni emulador.

**Núcleo geométrico.** Son los que protegen la exactitud de las mediciones:

```bash
cmake -S core -B build-core -DDWGCORE_BUILD_TESTS=ON
```
```bash
cmake --build build-core
```
```bash
ctest --test-dir build-core --output-on-failure
```

Todos deben pasar. Si tocas algo de geometría, esto es lo primero que hay que
volver a ejecutar.

**La app.** Arrancan la interfaz con Robolectric y abren un DWG de verdad a
través del puente nativo, miden sobre él y exportan. Necesitan la biblioteca
nativa compilada para el ordenador, que se prepara una vez:

```bash
tools/probar-dwg.sh --solo-compilar
```
```bash
./gradlew testDebugUnitTest
```

Si no se ha preparado, los tests que abren un plano se saltan y el resto se
ejecuta igual. Lo que estos tests **no** cubren es el dibujo con OpenGL y los
gestos táctiles: eso solo se ve en un móvil.

---

## Analizar un DWG desde el ordenador

Sin pasar por el móvil, para saber qué saca la app de un plano:

```bash
tools/probar-dwg.sh ~/planos/PROPUESTA.dwg salida.ppm
```

La primera vez compila LibreDWG para el ordenador. Pasa el plano por el mismo
código nativo que la app e imprime entidades, capas, capas ocultas, textos y los
tipos de entidad que no se convierten; con el segundo argumento guarda además
una imagen del plano tal como se dibujaría.

`tools/analizar-dwg.sh` hace un análisis distinto, con las herramientas de
LibreDWG: versión del archivo, recuento por tipo, bloques y referencias
externas.

## La versión de LibreDWG

Está fijada a un commit en `tools/libredwg-version.sh`, que comparten el script
de Android y el de escritorio. LibreDWG cambia nombres de campos entre
versiones, así que no se sigue su rama principal. Para actualizar: cambia el
commit, borra `vendor/`, recompila y pasa todos los tests.
