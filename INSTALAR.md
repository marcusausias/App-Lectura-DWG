# Instalar la app en tu Android, paso a paso

Esta guía supone que no has compilado nunca una app de Android. Si ya tienes
Android Studio con el NDK, salta al paso 3.

Hay dos cosas que compilar, en este orden:

1. **LibreDWG**, la biblioteca que lee los archivos DWG. Se compila una sola vez.
2. **La app**, que se compila cada vez que hay cambios.

---

## Paso 1 — Instalar Android Studio y el NDK

Descarga Android Studio de <https://developer.android.com/studio> e instálalo con
las opciones por defecto.

Ábrelo y, sin necesidad de crear ningún proyecto, ve a:

- **Windows/Linux:** `File → Settings → Languages & Frameworks → Android SDK`
- **Mac:** `Android Studio → Settings → Languages & Frameworks → Android SDK`

Entra en la pestaña **SDK Tools** y marca estas dos casillas:

- ☑ **NDK (Side by side)**
- ☑ **CMake**

Pulsa **Apply** y espera a que descargue (son unos 2 GB, tarda).

Cuando termine, en esa misma pantalla verás arriba **Android SDK Location**.
Apúntala: la vas a necesitar. Suele ser:

| Sistema | Ruta habitual |
|---|---|
| Windows | `C:\Users\TU_USUARIO\AppData\Local\Android\Sdk` |
| Mac | `/Users/TU_USUARIO/Library/Android/sdk` |
| Linux | `/home/TU_USUARIO/Android/Sdk` |

El NDK queda dentro, en una subcarpeta `ndk/` con el número de versión, por
ejemplo `ndk/27.0.12077973`. Mira qué número tienes ahí, porque hace falta.

### Comprobar que tienes perl

LibreDWG genera parte de su código con perl durante la compilación.

- **Mac y Linux:** ya viene instalado. Comprueba con `perl -v`.
- **Windows:** viene con **Git for Windows**. Si instalas Git (paso 2), usa la
  terminal **Git Bash** para todos los comandos de esta guía y funcionará.

---

## Paso 2 — Descargar el proyecto

Necesitas git. En Windows, instala <https://git-scm.com/download/win> y usa la
terminal **Git Bash** que trae.

Abre la terminal y ejecuta:

```bash
git clone -b claude/dwg-reader-progress-gosjur https://github.com/marcusausias/App-Lectura-DWG
cd App-Lectura-DWG
```

Ya estás dentro de la carpeta del proyecto. Todos los comandos siguientes se
ejecutan desde aquí.

---

## Paso 3 — Compilar LibreDWG

Este paso solo se hace una vez. Sustituye la versión del NDK por la tuya:

**Mac y Linux:**
```bash
export ANDROID_NDK_HOME=~/Library/Android/sdk/ndk/27.0.12077973    # Mac
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/27.0.12077973            # Linux
tools/build-libredwg-android.sh
```

**Windows (Git Bash):**
```bash
export ANDROID_NDK_HOME=/c/Users/TU_USUARIO/AppData/Local/Android/Sdk/ndk/27.0.12077973
tools/build-libredwg-android.sh
```

Tarda unos minutos por arquitectura. Al terminar debe existir esto:

```
vendor/prebuilt/arm64-v8a/libredwg.a
vendor/prebuilt/armeabi-v7a/libredwg.a
```

Compruébalo con `ls vendor/prebuilt/*/`. Si no están, mira la sección de errores
al final.

---

## Paso 4 — Preparar el móvil

En el teléfono:

1. **Ajustes → Información del teléfono** (o *Acerca del dispositivo*).
2. Busca **Número de compilación** y **púlsalo siete veces seguidas**. Aparecerá
   un aviso de que ya eres desarrollador.
3. Vuelve atrás y entra en **Ajustes → Sistema → Opciones de desarrollador**.
   En algunos móviles está directamente en Ajustes.
4. Activa **Depuración por USB**.
5. Conecta el móvil al ordenador con el cable.
6. En el móvil saldrá un diálogo preguntando si permites la depuración desde ese
   ordenador. Marca *Permitir siempre* y acepta.

Ese diálogo es fácil de perderse. Si no aparece, desconecta y vuelve a conectar
el cable, y mira la pantalla del móvil.

---

## Paso 5 — Instalar la app

Con el móvil conectado, desde la carpeta del proyecto:

```bash
./gradlew installDebug
```

La primera vez descarga Gradle y las dependencias: puede tardar bastante y
necesita conexión. Cuando termine con `BUILD SUCCESSFUL`, la app ya está en el
móvil con el nombre **Planos DWG**. Ábrela desde el cajón de aplicaciones.

### Alternativa sin terminal

En vez del comando anterior, puedes abrir Android Studio, elegir **Open**,
seleccionar la carpeta `App-Lectura-DWG` y pulsar el botón verde **Run** ▶.
Hace exactamente lo mismo. El paso 3 hay que haberlo hecho igualmente.

---

## Qué probar

Al abrir la app, pulsa **Abrir** y elige un DWG del móvil. Si el plano tiene
referencias externas, pulsa **Carpeta** e indica la carpeta del proyecto para que
las busque solas.

Por orden de importancia:

1. **Que el plano se vea como en AutoCAD.** Compáralo con su PDF. Geometría
   desplazada, en espejo o ausente indica un fallo de conversión.
2. **Zoom profundo sobre un detalle.** Las líneas no deben temblar.
3. **Medir una cota acotada y comparar con el número escrito en ella.** Es la
   comprobación que de verdad valida la app.
4. **Capas:** apagar y aislar debe ocultar lo mismo que en AutoCAD.
5. **Mediciones:** tomar unas cuantas, cerrar la app del todo y reabrir el plano.
   Deben seguir ahí.

---

## Si algo falla

**`tools/build-libredwg-android.sh: Permission denied`**
```bash
chmod +x tools/build-libredwg-android.sh
```

**`Define ANDROID_NDK_HOME con la ruta al NDK`**
No has ejecutado el `export`, o lo hiciste en otra ventana de terminal. Cada
ventana nueva necesita su propio `export`.

**`No encuentro el toolchain del NDK en ...`**
La ruta del `export` no es correcta. Comprueba que existe:
`ls $ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`

**`../jsmn/jsmn.h: No such file or directory`**
Falta un submódulo de LibreDWG:
```bash
git -C vendor/libredwg submodule update --init --depth 1
```

**`perl: command not found`**
En Windows, usa Git Bash en vez de PowerShell o CMD.

**`SDK location not found`**
Gradle no sabe dónde está el SDK. Crea un fichero `local.properties` en la raíz
del proyecto con una línea (usa barras normales `/` incluso en Windows):
```
sdk.dir=C:/Users/TU_USUARIO/AppData/Local/Android/Sdk
```
Si abres el proyecto en Android Studio, lo crea él solo.

**`Falta vendor/prebuilt/<abi>/libredwg.a`**
No has hecho el paso 3, o falló sin que te dieras cuenta. Revísalo.

**El móvil no aparece / `no devices found`**
No has aceptado el diálogo de depuración en el móvil, o el cable es solo de
carga. Prueba otro cable. Comprueba con:
```bash
$ANDROID_HOME/platform-tools/adb devices
```
Debe aparecer tu móvil como `device`, no como `unauthorized`.

**Errores de compilación de Kotlin**
Es lo más probable en el primer intento: ninguna línea del código de interfaz se
ha llegado a ejecutar nunca. Cópiame el error tal cual y lo arreglo.
