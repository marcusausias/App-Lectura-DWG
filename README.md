# Planos DWG

App de Android para abrir planos DWG en el móvil, sin AutoCAD y sin conexión, y
medir sobre ellos: longitudes, superficies, recorridos y recuento de símbolos.
Los planos no salen del dispositivo.

## Qué hace

- **Abre DWG nativos** (de R13 a 2018+) con LibreDWG, desde la app o con
  «Abrir con…» desde el gestor de archivos, el correo o Drive.
- **Dibuja el plano** con OpenGL: líneas, polilíneas con arcos, círculos,
  elipses, splines, bloques anidados, cotas, textos, atributos de bloque,
  contornos de sombreado, directrices y sólidos. Con los colores del dibujo,
  ajustados para que se lean sobre fondo claro u oscuro.
- **Capas**: encender, apagar y aislar. Las que el proyectista dejó apagadas o
  congeladas arrancan ocultas, como en AutoCAD.
- **Referencias externas**: avisa de las que faltan, las busca en la carpeta
  del proyecto y recuerda dónde estaban.
- **Medir** con enganche a extremos, puntos medios, intersecciones y
  perpendiculares:

  | Herramienta | Qué hace |
  |---|---|
  | Distancia | Dos toques |
  | Polilínea | Toques seguidos y *Cerrar*; suma los tramos |
  | Superficie | Marca el contorno y *Cerrar* |
  | Seguir | Toca una línea y la mide entera, con sus arcos exactos. Si es un contorno cerrado da también la superficie |
  | Contar | Toca un símbolo y dice cuántos iguales hay |

- **Mediciones guardadas** por plano, con etiqueta editable. Se conservan al
  cerrar la app.
- **Exportar a CSV** preparado para Excel en español (punto y coma, coma
  decimal), con totales de longitud y superficie.

Las medidas se dan en **unidades de dibujo**, sin convertir: si el plano está en
metros son metros, si está en milímetros son milímetros. En *Medidas* se indica
la unidad para que acompañe al número. La comprobación que vale es medir una
cota del plano y ver que coincide con lo que pone.

## Instalarla

**Sin compilar nada:** cada cambio subido a GitHub genera el APK. En la pestaña
*Actions* del repositorio, abre la última ejecución en verde y descarga
`planos-dwg-apk` de la sección *Artifacts*. Dentro va el `.apk`: pásalo al móvil
y ábrelo (Android pedirá permitir la instalación desde esa app).

**Compilándola tú:** [INSTALAR.md](INSTALAR.md) lo explica paso a paso, y
[CONSTRUIR.md](CONSTRUIR.md) da el detalle técnico.

## Cómo está hecha

```
DWG ─► LibreDWG ─► extracción ─► aplanado de bloques ─► escena + índice espacial ─► caché
        (C)       (app/src/main/cpp)        (core/, C++ con tests)                    │
                                                                                      ▼
                         Kotlin + Compose: OpenGL para la geometría, Canvas para textos y medidas
```

| Carpeta | Contenido |
|---|---|
| `core/` | Núcleo geométrico en C++ sin dependencias: arcos, transformadas, aplanado, índice espacial, enganche, medición, caché. Con tests que corren en el ordenador |
| `app/src/main/cpp/` | Lectura del DWG con LibreDWG y puente JNI |
| `app/src/main/java/` | Interfaz: visor, gestos, capas, mediciones, xrefs |
| `app/src/test/` | Tests de la app; abren un DWG de verdad a través del puente nativo |
| `tools/` | Scripts para compilar LibreDWG y para analizar un plano desde el ordenador |

## Comprobar que todo funciona

```bash
cmake -S core -B build-core -DDWGCORE_BUILD_TESTS=ON
```
```bash
cmake --build build-core
```
```bash
ctest --test-dir build-core --output-on-failure
```
```bash
tools/probar-dwg.sh --solo-compilar
```
```bash
./gradlew testDebugUnitTest
```

Para ver qué saca la app de un plano concreto sin pasar por el móvil:

```bash
tools/probar-dwg.sh ~/planos/PROPUESTA.dwg salida.ppm
```

Imprime entidades, capas y los tipos que no se convierten, y guarda una imagen
del plano tal como lo dibujaría la app.

## Lo que no hace (todavía)

- **Espacio papel y presentaciones.** Solo se muestra el espacio modelo.
- **Rellenos y patrones de sombreado.** Se dibuja el contorno, no el relleno.
- **Tipos de línea y grosores.** Todo se traza en línea continua fina.
- **Fuentes del dibujo.** Los textos usan la fuente del sistema; su posición y
  tamaño son los del plano, su anchura puede variar algo.
- **Entidades que no se convierten:** imágenes insertadas, tablas, líneas
  múltiples (MLINE), directrices múltiples (MULTILEADER), sólidos 3D y regiones.
  `tools/probar-dwg.sh` dice cuántas hay de cada una en un plano dado.
- **Bloques dinámicos** con varios estados de visibilidad pueden mostrar todos
  los estados superpuestos.
- **Atributos** de bloques anidados dentro de otros bloques.

## Licencia

LibreDWG es **GPL-3**. Mientras la app sea de uso propio no impone nada; si se
distribuye a terceros, hay que ofrecerles también el código fuente bajo la misma
licencia.

`RETOMAR-PROYECTO.md` es el documento con el que arrancó el proyecto. Se
conserva por su análisis del formato DWG y sus trampas; la arquitectura que
describe (web con DXF) no es la que se acabó construyendo.
