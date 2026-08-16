# App Lectura DWG — Documento base para retomar el proyecto

> **Fecha:** 16 de agosto de 2026
> **Estado del repo:** vacío (0 commits). Este es el primer archivo.
> **Propósito:** punto de partida único para retomar el proyecto sin repetir trabajo ni tropezar con los errores conocidos del dominio DWG.

---

## 0. Aviso importante sobre "el progreso anterior"

Antes de nada, hay que ser honesto sobre qué se ha podido recuperar y qué no, porque
condiciona todo lo demás.

**Lo que se ha buscado (16/08/2026):**

| Fuente | Resultado |
|---|---|
| Repo `marcusausias/App-Lectura-DWG` (GitHub) | **Vacío**. Creado, sin ninguna rama ni commit. `pushed_at: 2026-08-11` corresponde a la creación, no a código. |
| Sesiones anteriores de Claude Code | Solo aparece la sesión actual. No hay histórico de sesiones previas accesible desde aquí. |
| Repo `marcusausias/DiarioDeObra` | Existe, pero **fuera del alcance** de permisos de esta sesión (no se puede leer). |
| Google Drive — búsqueda por `DWG`, `DXF`, `visor`, `viewer`, `lectura`, `medir`, `mediciones`, `CAD`, `app` | Solo archivos `.dwg` de obra (planos reales), PDFs y BC3. **Ningún código fuente de la app.** |

**Conclusión:** el código de la app de lectura y medición de DWG **no está en ninguna
fuente accesible desde este entorno**. Lo más probable es que el trabajo previo se hiciera
en una conversación de claude.ai (chat), en la app de escritorio, o en local — sitios a los
que esta sesión no llega.

**Qué significa para ti:** este documento **no es una reconstrucción del código anterior**.
Es un documento de arranque técnico: recoge lo único relacionado que sí se ha recuperado
(ver §1), fija las decisiones que hay que tomar antes de escribir código, y documenta las
trampas del formato DWG que hacen fracasar este tipo de proyectos.

**Cómo recuperar el trabajo anterior si existe** (por orden de probabilidad):

1. Abrir claude.ai → historial de chats → buscar "DWG". Si aparece la conversación, los
   artifacts de código siguen ahí y se pueden descargar.
2. Revisar la carpeta de descargas del móvil/ordenador por si se exportó un `.html` o `.zip`.
3. Buscar en Drive por fecha, no por nombre: los archivos generados por Claude a veces se
   guardan con nombres genéricos (`index.html`, `app.html`).

Si aparece, pásamelo y actualizo este documento con el estado real en lugar del de partida.

---

## 1. Lo único relacionado que sí se recuperó: **VISOR 3DS**

En Drive (`/VISOR 3DS/`, 22–23 de mayo de 2026) hay un prototipo tuyo funcional que **no es
la app de DWG**, pero es el precedente técnico más cercano y conviene reutilizarlo.

**Qué es:** `index.html` de 34 KB, autocontenido, sin build ni servidor. Visor 3D de modelos
glTF con edición de metadatos BIM y exportación.

**Stack:**
- `three.js 0.160.0` vía `importmap` desde unpkg (sin bundler, sin `npm install`).
- `GLTFLoader`, `GLTFExporter`, `OrbitControls`.
- Todo en un único fichero HTML: CSS, JS y UI.

**Funcionalidades implementadas:**
- Carga de `.gltf`/`.glb` locales (`URL.createObjectURL`) y de modelos "integrados" del proyecto.
- Selección de elementos por *raycasting*, con umbral de movimiento (`MOVE_THRESH = 6px`)
  para no confundir un *orbit* con un *tap*. **Este detalle es oro para móvil.**
- Panel lateral que lee `userData` (los `extras` del glTF) y genera un formulario dinámico
  con los metadatos BIM del elemento.
- Edición con confirmación explícita, pila de *undo* (`Ctrl+Z`), *snapshots* del valor original,
  y marcado visual de elementos modificados (punto verde en escena + borde amarillo en el input).
- Exportación a `.glb` con los metadatos actualizados, limpiando antes las claves internas (`_`).
- Optimización móvil real: `pixelRatio` limitado a 1.5, `MeshBasicMaterial` en vez de PBR
  (sin luces = menos coste), y generación de aristas (`EdgesGeometry`) solo por debajo de
  4.000 triángulos en móvil / 20.000 en escritorio.
- UI móvil cuidada: `safe-area-inset` para el notch, menú convertido en *bottom sheet* por
  debajo de 680px, *toasts*, estado vacío, panel de ayuda.

**Qué se reutiliza tal cual en la app de DWG:**

| Elemento de VISOR 3DS | Uso en App Lectura DWG |
|---|---|
| Patrón "un solo HTML, sin build" | Permite iterar y probar en el móvil sin desplegar nada |
| Sistema de selección con umbral de movimiento | Idéntico problema: seleccionar entidad vs. hacer *pan* |
| Panel lateral con campos dinámicos | Panel de propiedades de entidad / lista de mediciones |
| Pila de *undo* + *snapshots* + indicadores visuales | Deshacer mediciones, marcar lo medido |
| Lenguaje visual (paleta `#EFEFEA` / tinta `#1a1c20` / acento `#FFD400` / ok `#27AE60`) | Mantener coherencia entre tus herramientas |
| Gestión de memoria (`dispose()` de geometrías y materiales) | Crítico con planos grandes |

**Qué no sirve:** todo el pipeline glTF. DWG es otro mundo (ver §3).

---

## 2. Definición del proyecto (a confirmar contigo)

Lo siguiente está **inferido** de tu contexto (arquitectura/dirección de obra, archivos BC3
y de mediciones en Drive, proyecto Diario de Obra). Confírmalo o corrígelo antes de programar,
porque cambia la arquitectura entera.

**Hipótesis de objetivo:**
> Abrir un DWG de proyecto en el móvil o en el navegador, sin AutoCAD, y poder medir
> longitudes y superficies sobre el plano para volcarlas a mediciones/presupuesto.

**Usuario:** tú y, potencialmente, jefes de obra y aparejadores. A pie de obra, con móvil,
posiblemente sin cobertura buena.

**Preguntas que hay que responder antes de la primera línea de código** (§9 las recoge todas):
1. ¿Solo lectura y medición, o también anotar/marcar sobre el plano?
2. ¿Móvil primero o escritorio primero?
3. ¿Puede haber servidor/nube, o tiene que funcionar 100 % local y offline?
4. ¿Las mediciones acaban en un Excel, en un BC3, o dentro de Diario de Obra?

---

## 3. La decisión crítica: **cómo se lee un DWG**

Esto es lo que hunde el proyecto si se elige mal. Hay que decidirlo antes que nada.

### El problema de fondo

DWG es un **formato binario propietario y cerrado de Autodesk**, sin especificación pública
y que cambia cada pocas versiones (`AC1024` = 2010, `AC1027` = 2013, `AC1032` = 2018…).
**No existe ninguna librería JavaScript madura que lea DWG nativo en el navegador.**
Cualquiera que diga lo contrario está hablando de DXF, que es el formato de intercambio
—abierto y documentado— al que hay que convertir el DWG.

### Las tres vías reales

#### Vía A — Convertir DWG → DXF y parsear DXF en el cliente

- **Conversión:** `ODA File Converter` (Open Design Alliance, gratuito, CLI, Win/Linux/macOS)
  o `dwg2dxf` de **LibreDWG**.
- **Parseo:** `dxf-parser` (npm) → objeto JS con entidades. Render propio en Canvas 2D o three.js.
- **Ventajas:** control total sobre el render y la medición; sin coste por archivo; funciona offline
  una vez convertido.
- **Inconvenientes:** la conversión necesita un binario, o sea **un paso manual o un pequeño
  servidor**. LibreDWG es **GPL-3** (viral: si distribuyes la app, te obliga a liberarla). El EULA
  del ODA File Converter gratuito es para uso interno — **verificar antes de empaquetarlo**.
- **Nota:** existen ports de LibreDWG a **WebAssembly** (buscar `libredwg-web` / `mlightcad`).
  Si funcionan bien, eliminan el servidor y esta vía pasa a ser la ganadora clara.
  **Hay que probarlos con tus DWG reales antes de asumir nada.**

#### Vía B — Autodesk Platform Services (APS, antes Forge)

- Subes el DWG, la **Model Derivative API** lo traduce, y el **APS Viewer** lo muestra.
- **Ventajas:** soporta DWG nativo sin conversión previa, incluidos xrefs y presentaciones;
  trae extensión de **medición** (distancia, ángulo, área, calibración) ya hecha; renderiza
  planos enormes sin que programes nada.
- **Inconvenientes:** **de pago por traducción** (modelo de créditos — verificar tarifas
  actuales, cambian), requiere nube obligatoriamente, los planos salen del control local,
  y hay dependencia total de Autodesk. **No funciona offline.**
- **Cuándo elegirlo:** si el objetivo es tener algo funcionando en días y el coste por plano
  es asumible.

#### Vía C — SDK comercial (ODA Open Cloud / visores CAD de pago)

- **Ventajas:** soporte y fidelidad de render profesional.
- **Inconvenientes:** licencias caras, pensadas para producto comercial. **Descartable** salvo
  que esto vaya a venderse.

### Recomendación

> **Empezar por la Vía A, y hacerlo en dos pasos.**
>
> **Paso 1 — Validar el formato antes de invertir en la app.** Coger tus DWG reales de Drive
> (`PROPUESTA.dwg` 7,8 MB y `Estado Actual.dwg` 4,4 MB, en `/Planos/`), convertirlos a DXF y
> comprobar qué sale: versión del DWG, número de entidades, si hay xrefs, si hay bloques
> anidados, si las polilíneas tienen arcos. **Esto es medio día de trabajo y determina si el
> proyecto es viable o no.** Todo lo demás depende de este resultado.
>
> **Paso 2 — Si el DXF sale limpio,** construir el visor con `dxf-parser` + render propio,
> siguiendo el patrón de fichero único de VISOR 3DS.
>
> La Vía B queda como plan de contingencia si tus DWG resultan ser demasiado complejos
> (muchos xrefs, presentaciones múltiples, sombreados pesados).

---

## 4. Arquitectura propuesta (Vía A)

```
┌──────────────────────────────────────────────────────┐
│  1. INGESTA                                          │
│     DWG ──[ODA File Converter / libredwg-wasm]──▶ DXF│
├──────────────────────────────────────────────────────┤
│  2. PARSEO            dxf-parser → AST de entidades  │
├──────────────────────────────────────────────────────┤
│  3. NORMALIZACIÓN     ⚠️ AQUÍ ESTÁN TODOS LOS BUGS   │
│     · OCS → WCS (algoritmo del eje arbitrario)       │
│     · Aplanado de INSERT/BLOCK con transformadas     │
│     · Teselado de arcos, splines y bulges            │
│     · Resolución de unidades ($INSUNITS)             │
│     → Geometría plana: polilíneas en coords. mundo   │
├──────────────────────────────────────────────────────┤
│  4. ÍNDICE ESPACIAL   rbush (R-tree) para picking    │
│                       y snapping en O(log n)         │
├──────────────────────────────────────────────────────┤
│  5. RENDER            Canvas 2D (simple, hasta ~50k  │
│                       entidades) o three.js          │
│                       LineSegments en un único       │
│                       BufferGeometry (mucho más)     │
├──────────────────────────────────────────────────────┤
│  6. MEDICIÓN          Herramientas + snapping        │
├──────────────────────────────────────────────────────┤
│  7. EXPORTACIÓN       CSV / XLSX / BC3 / Diario Obra │
└──────────────────────────────────────────────────────┘
```

**Decisión de render:** empezar en **Canvas 2D**. Es mucho más simple de depurar, el texto se
dibuja bien de forma nativa, y un plano de arquitectura típico va sobrado. Migrar a WebGL
solo si se mide un problema real de rendimiento, no por precaución.

---

## 5. Modelo de datos mínimo

```js
// Entidad normalizada: todo se reduce a esto tras la fase 3
{
  id: 'ent_0042',
  type: 'LINE' | 'POLYLINE' | 'ARC' | 'CIRCLE' | 'TEXT' | 'HATCH',
  layer: 'A-MURO-EXT',
  closed: false,
  // SIEMPRE en coordenadas de mundo y en metros, ya teselado.
  // Los arcos vienen aquí convertidos en segmentos.
  points: [[x, y], [x, y], ...],
  bbox: [minX, minY, maxX, maxY],   // para el R-tree
  source: { handle: '1A4', blockPath: ['BLOQUE_PUERTA'] } // trazabilidad
}

// Medición
{
  id: 'med_001',
  kind: 'longitud' | 'area' | 'angulo' | 'contar',
  points: [[x, y], ...],
  value: 12.457,          // en unidad base (metros / m²)
  unit: 'm' | 'm2',
  label: 'Tabique P1 salón',
  layer: 'A-MURO-EXT',    // capa de las entidades a las que se enganchó
  createdAt: '2026-08-16T19:00:00Z'
}
```

**Regla de oro:** normalizar **todo a metros** en la fase 3 y no volver a tocar unidades hasta
la presentación. Los bugs de unidades son los más difíciles de detectar porque el resultado
*parece* razonable.

---

## 6. Roadmap por fases

Cada fase tiene un criterio de aceptación verificable. **No pasar a la siguiente sin cumplirlo.**

### Fase 0 — Validación del formato (medio día) 🔴 BLOQUEANTE
- [ ] Descargar `PROPUESTA.dwg` y `Estado Actual.dwg` de Drive.
- [ ] Convertir a DXF (ODA File Converter o libredwg).
- [ ] Volcar un informe: versión DWG, nº de entidades por tipo, capas, bloques, xrefs,
      `$INSUNITS`, presencia de espacio papel.
- [ ] **Criterio:** se sabe con certeza si el DXF resultante es completo y utilizable.

### Fase 1 — Visor mínimo (2–3 días)
- [ ] Cargar DXF desde el `<input type=file>` (patrón de VISOR 3DS).
- [ ] Parsear con `dxf-parser`, normalizar LINE / LWPOLYLINE / CIRCLE / ARC.
- [ ] Dibujar en Canvas 2D con zoom y *pan* (rueda + pellizco).
- [ ] **Criterio:** tu plano se ve **igual** que en AutoCAD, sin geometría desplazada
      ni faltante. Comparar contra el PDF del mismo plano que ya tienes en Drive.

### Fase 2 — Capas y bloques (2 días)
- [ ] Aplanar `INSERT`/`BLOCK` con sus transformadas (incluido anidamiento).
- [ ] Panel de capas: encender/apagar, colores reales de capa.
- [ ] **Criterio:** el número de entidades visibles coincide con el de AutoCAD, y
      apagar una capa oculta exactamente lo mismo que allí.

### Fase 3 — Medición (3–4 días) ← *el corazón del proyecto*
- [ ] Escala real: leer `$INSUNITS` y permitir **calibración manual** contra una cota conocida.
- [ ] Medir distancia entre dos puntos.
- [ ] Medir polilínea (suma de tramos).
- [ ] Medir área (fórmula del área de Gauss / *shoelace*).
- [ ] *Snapping* a extremo, punto medio, centro e intersección, con feedback visual.
- [ ] **Criterio:** medir una cota acotada del plano y que **coincida con el valor de la cota**.
      Este es el test que no se puede saltar.

### Fase 4 — Gestión de mediciones (2 días)
- [ ] Lista de mediciones con etiqueta editable, agrupables por capa o partida.
- [ ] *Undo* (reutilizar el patrón de VISOR 3DS).
- [ ] Persistencia en `localStorage` / `IndexedDB` para no perder trabajo.
- [ ] **Criterio:** cerrar el navegador y recuperar las mediciones intactas.

### Fase 5 — Exportación (1–2 días)
- [ ] CSV/XLSX como mínimo.
- [ ] Valorar BC3 si el destino es Presto.
- [ ] **Criterio:** el archivo abre correctamente en tu herramienta de presupuestos.

### Fase 6 — Pulido móvil (2 días)
- [ ] Gestos: 1 dedo *pan*, 2 dedos zoom, *tap* selecciona (umbral de 6px de VISOR 3DS).
- [ ] Lupa de precisión al colocar un punto de medición (el dedo tapa el objetivo).
- [ ] `safe-area-inset`, *bottom sheet*, botones de 44px mínimo.
- [ ] **Criterio:** medir un tabique en el móvil, de pie, sin frustrarse.

---

## 7. Trampas conocidas — la sección que evita perder días

Ordenadas por probabilidad de morder. Cada una es un bug real y clásico de este dominio.

### 7.1 Los *bulges* de las polilíneas ⚠️ EL FALLO MÁS PROBABLE
En DXF, una `LWPOLYLINE` puede tener tramos curvos codificados como **bulge** (código 42):
`bulge = tan(θ/4)`, donde θ es el ángulo del arco. Si se ignora, cada arco se dibuja y se mide
como una **cuerda recta**.

**Consecuencia:** el plano *parece* correcto de un vistazo, pero **todas las longitudes de
paredes curvas, esquinas redondeadas y áreas quedan mal**. Y no salta ningún error.

**Solución:** teselar cada tramo con bulge en arco real antes de medir.
Radio: `R = L·(1 + b²) / (4b)` con `L` = longitud de la cuerda y `b` = bulge.

### 7.2 Sistema de coordenadas del objeto (OCS) y el eje arbitrario ⚠️
Muchas entidades DXF guardan sus coordenadas en un sistema **local** (OCS), no en coordenadas
de mundo, junto a una **dirección de extrusión** (código 210). Si esa dirección es `(0,0,-1)`
—cosa habitual en objetos hechos con espejo— y se ignora, **la geometría aparece invertida**.

**Solución:** implementar el *arbitrary axis algorithm* de la especificación DXF y aplicarlo
a toda entidad con código 210 distinto de `(0,0,1)`. Afecta a `CIRCLE`, `ARC`, `LWPOLYLINE`,
`TEXT`, `INSERT`.

### 7.3 Unidades: el error silencioso
`$INSUNITS` en la cabecera indica las unidades (`1`=pulgadas, `4`=mm, `5`=cm, `6`=m). Pero:
- Muchos DWG españoles de arquitectura vienen con `$INSUNITS = 0` (sin definir) y dibujados
  **en milímetros** o **en metros**, según el estudio.
- Un plano dibujado en mm interpretado como m da mediciones **1000 veces mayores**.

**Solución:** no fiarse nunca del header. Ofrecer **calibración obligatoria** al abrir:
"marca dos puntos de una cota conocida e introduce su valor real". Es lo que hacen todas las
apps serias de medición sobre plano, y por esta razón.

### 7.4 Espacio modelo vs. espacio papel
El espacio papel contiene *viewports* con **factores de escala** propios (1:50, 1:100…).
Medir sobre espacio papel sin aplicar la escala del viewport da resultados absurdos.

**Solución (fase 1):** trabajar **solo en espacio modelo** e ignorar el espacio papel
explícitamente. Documentar la limitación en la interfaz en lugar de dar números falsos.

### 7.5 Referencias externas (xrefs)
Si el DWG referencia otros DWG (`XREF`), el DXF exportado **no los incluye**: aparecen como
`INSERT` que apuntan a un bloque vacío. El plano se ve incompleto y falta justo lo que se
quería medir.

**Solución:** detectar xrefs al cargar y **avisar al usuario explícitamente**. En AutoCAD se
resuelve con `_BIND` o `ETRANSMIT` antes de exportar. Un aviso claro evita que alguien mida
sobre un plano al que le falta la mitad.

### 7.6 Bloques anidados y sus transformadas
Un `INSERT` aplica traslación, **escala en X/Y/Z (que puede ser negativa = espejo)** y rotación.
Y puede contener otros `INSERT` dentro. Componer mal las matrices en el orden equivocado
descoloca puertas, ventanas y mobiliario.

**Solución:** función recursiva que acumula la matriz de transformación, con límite de
profundidad (10) para no colgarse con referencias circulares.

### 7.7 Rendimiento con planos reales
Un plano de ejecución puede superar las 200.000 entidades. Dibujar cada una con su propia
llamada a Canvas, o crear un `Mesh` de three.js por entidad, congela el navegador — y mucho
antes en un móvil.

**Solución:**
- Canvas 2D: agrupar por color/capa y trazar un único `path` por grupo.
- three.js: **un solo** `BufferGeometry` con `LineSegments` para todo el plano.
- *Culling* por *viewport* usando el R-tree.
- Recordar el `dispose()` que ya está resuelto en VISOR 3DS.

### 7.8 Sombreados (HATCH)
Los `HATCH` con patrones densos pueden tener decenas de miles de líneas y multiplicar por
diez el peso del plano, aportando cero a la medición.

**Solución:** cargarlos como **capa desactivada por defecto**, o solo su contorno.
Su contorno, eso sí, es utilísimo para medir superficies de un tirón.

### 7.9 Precisión táctil en móvil
El dedo tapa el punto y tiene ~10 mm de imprecisión. Sin *snapping* y sin lupa, medir en el
móvil es inutilizable, por muy bien que funcione todo lo demás.

**Solución:** *snapping* agresivo (radio de 20–30 px) con indicador visual del tipo de punto
al que engancha, más lupa flotante desplazada del dedo.

### 7.10 Splines
Las `SPLINE` (NURBS) requieren evaluación de la curva base para teselarse. `dxf-parser` las
devuelve como puntos de control, **no como la curva dibujada**. Unir los puntos de control
con rectas dibuja una forma parecida pero incorrecta.

**Solución:** implementar evaluación de B-spline, o —si son pocas y decorativas— marcarlas
como "no medibles" y dibujarlas en gris. Decisión a tomar tras la Fase 0, cuando se sepa
cuántas hay en tus planos.

---

## 8. Material de prueba disponible

Ya tienes archivos reales en Drive, y hay que usarlos desde el principio: probar con planos
sintéticos es la mejor forma de descubrir todos estos problemas tarde.

| Archivo | Tamaño | Ubicación (Drive) |
|---|---|---|
| `PROPUESTA.dwg` | 7,9 MB | `/Planos/` |
| `Estado Actual.dwg` | 4,4 MB | `/Planos/` |
| `PROPUESTA (1).dwg` | 8,4 MB | `/Planos/` |
| PDFs equivalentes (`DEM 1-3`, `PROPUESTA P1-P2`, `PROPUESTA CUBIERTA`) | — | `/Planos/` |

Los **PDF son la referencia visual** para verificar la Fase 1: si el render coincide con el PDF,
la normalización es correcta.

También hay `N+10 NSA2 mediciones y presupuesto.bc3` (2018) como ejemplo de formato BC3 real
por si la exportación acaba yendo por ahí.

---

## 9. Decisiones pendientes

Ninguna de estas la puedo resolver yo. Responderlas al retomar el proyecto:

1. **¿Apareció el trabajo anterior?** (§0) Cambia si esto es partir de cero o continuar.
2. **¿Local o nube?** Determina Vía A o Vía B (§3). Si los planos no pueden salir del
   dispositivo por confidencialidad, la Vía B queda descartada de entrada.
3. **¿Cuál es el destino de las mediciones?** ¿Excel, BC3/Presto, o integrarse con Diario de Obra?
   Si es lo último, hay que ver ese repo (hoy fuera de alcance de permisos).
4. **¿Móvil primero o escritorio primero?** Afecta al render y a toda la interacción.
5. **¿Distribución?** Web privada, PWA instalable, o app nativa. Condiciona el tema de licencias
   GPL de LibreDWG (§3).
6. **¿Hay presupuesto para APS?** Si lo hay, la Vía B ahorra semanas de trabajo.

---

## 10. Primer paso concreto al retomar

No empezar por la interfaz. **Empezar por la Fase 0**, que es la única que puede invalidar
todo lo demás:

```bash
# 1. Conseguir un conversor (elegir uno)
#    a) ODA File Converter — descarga gratuita desde opendesign.com/guestfiles
#    b) libredwg:  apt install libredwg-tools   →   dwg2dxf archivo.dwg

# 2. Convertir un plano real
dwg2dxf -v3 "PROPUESTA.dwg" -o propuesta.dxf

# 3. Inspeccionar qué ha salido realmente
grep -c "^  0$" propuesta.dxf              # nº aproximado de entidades
grep -A1 "\$INSUNITS" propuesta.dxf        # unidades declaradas
grep -c "SPLINE" propuesta.dxf             # ¿hay splines?
grep -c "HATCH"  propuesta.dxf             # ¿cuánto sombreado?
```

Con esa salida sobre la mesa, se decide vía, render y alcance — y **entonces** se escribe código.

---

## 11. Convenciones del repositorio

- **Rama de desarrollo:** `claude/dwg-reader-progress-gosjur`
- **Estructura sugerida** (cuando empiece el código):
  ```
  /docs        → este documento y decisiones técnicas
  /prototipo   → HTML autocontenido estilo VISOR 3DS (iteración rápida)
  /muestras    → DXF de prueba pequeños, versionables
  /src         → solo si el prototipo crece lo suficiente
  ```
- **Regla:** los `.dwg` y `.dxf` grandes **no van al repositorio**. Se quedan en Drive y se
  referencian desde aquí.
- Mantener este documento vivo: cada decisión tomada se anota, con su porqué. Es lo que
  evita repetir la conversación dentro de tres meses.
