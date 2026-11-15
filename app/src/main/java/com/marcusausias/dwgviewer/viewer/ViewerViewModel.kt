package com.marcusausias.dwgviewer.viewer

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.marcusausias.dwgviewer.data.DwgFileReader
import com.marcusausias.dwgviewer.data.MeasurementExport
import com.marcusausias.dwgviewer.data.MeasurementStore
import com.marcusausias.dwgviewer.data.SettingsStore
import com.marcusausias.dwgviewer.nativebridge.LayerStyle
import com.marcusausias.dwgviewer.nativebridge.NativePlan
import com.marcusausias.dwgviewer.nativebridge.PlanText
import com.marcusausias.dwgviewer.xref.ResolvedXref
import com.marcusausias.dwgviewer.xref.XrefResolver
import com.marcusausias.dwgviewer.xref.XrefStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

sealed interface PlanState {
    data object Empty : PlanState
    data object Loading : PlanState
    data class Failed(val message: String) : PlanState
    data class Ready(
        val plan: NativePlan,
        val layers: List<String>,
        val entityCount: Int,
        val bounds: DoubleArray,
        val name: String = "",
        val layerStyles: List<LayerStyle> = emptyList(),
        val xrefs: List<ResolvedXref> = emptyList(),
    ) : PlanState {
        val missingXrefs: List<ResolvedXref> get() = xrefs.filterNot { it.isResolved }
    }
}

class ViewerViewModel(application: Application) : AndroidViewModel(application) {

    private val reader = DwgFileReader(application)
    private val measurementStore = MeasurementStore(application)
    private val settings = SettingsStore(application)
    private val xrefStore = XrefStore(application)
    private val xrefResolver = XrefResolver(application, xrefStore)

    /** Carpeta del proyecto con permiso persistente, si ya se concedió una. */
    val projectFolder: String? get() = xrefStore.projectTreeUri

    private val _state = MutableStateFlow<PlanState>(PlanState.Empty)
    val state: StateFlow<PlanState> = _state.asStateFlow()

    private val _camera = MutableStateFlow(Camera())
    val camera: StateFlow<Camera> = _camera.asStateFlow()

    private val _hiddenLayers = MutableStateFlow<Set<Int>>(emptySet())
    val hiddenLayers: StateFlow<Set<Int>> = _hiddenLayers.asStateFlow()

    private val _visibleTexts = MutableStateFlow<List<PlanText>>(emptyList())
    val visibleTexts: StateFlow<List<PlanText>> = _visibleTexts.asStateFlow()

    private val _darkBackground = MutableStateFlow(settings.darkBackground)
    val darkBackground: StateFlow<Boolean> = _darkBackground.asStateFlow()

    private val _measurements = MutableStateFlow<List<Measurement>>(emptyList())
    val measurements: StateFlow<List<Measurement>> = _measurements.asStateFlow()

    private val _pending = MutableStateFlow(PendingMeasure())
    val pending: StateFlow<PendingMeasure> = _pending.asStateFlow()

    private val _snapPreview = MutableStateFlow<SnapPreview?>(null)
    val snapPreview: StateFlow<SnapPreview?> = _snapPreview.asStateFlow()

    private val _decimals = MutableStateFlow(settings.decimals.coerceIn(0, 6))
    val decimals: StateFlow<Int> = _decimals.asStateFlow()

    private val _unit = MutableStateFlow(settings.unit)
    val unit: StateFlow<String> = _unit.asStateFlow()

    private var fitPending = true
    private var nextMeasurementId = 1L
    private var currentPlanUri: Uri? = null
    private var currentPlanKey: String? = null

    /** Último plano abierto, para ofrecer retomarlo al arrancar. */
    val lastPlanName: String? get() = settings.lastPlanName
    val lastPlanUri: Uri? get() = settings.lastPlanUri?.let(Uri::parse)

    /** Aviso breve para el usuario: resultado de guardar, por ejemplo. */
    private val _notice = MutableStateFlow<String?>(null)
    val notice: StateFlow<String?> = _notice.asStateFlow()

    fun dismissNotice() { _notice.value = null }

    /**
     * Recuerda la carpeta del proyecto.
     *
     * Android conserva este permiso entre sesiones, y es lo que permite que las
     * referencias externas se resuelvan solas la próxima vez sin volver a
     * preguntar nada.
     */
    fun setProjectFolder(treeUri: Uri) {
        val application = getApplication<Application>()
        try {
            application.contentResolver.takePersistableUriPermission(
                treeUri,
                android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
        } catch (error: SecurityException) {
            // Algunos proveedores no ofrecen permiso persistente. Se sigue
            // adelante: valdrá para esta sesión, y la próxima vez se pedirá.
        }
        xrefStore.projectTreeUri = treeUri.toString()
        // Con la carpeta nueva pueden aparecer las referencias que faltaban.
        currentPlanUri?.let { open(it) }
    }

    /** Enlaza a mano una xref que no se ha podido localizar, y lo recuerda. */
    fun linkXref(blockName: String, documentUri: Uri) {
        val planKey = currentPlanKey ?: return
        xrefResolver.rememberLink(planKey, blockName, documentUri)
        currentPlanUri?.let { open(it) }
    }

    fun open(uri: Uri) {
        // Una segunda apertura mientras la primera sigue en marcha dejaría dos
        // planos nativos vivos y solo uno a la vista.
        if (_state.value is PlanState.Loading) return

        viewModelScope.launch {
            // Primero se deja de enseñar el plano y luego se libera: así la
            // vista nunca llega a pedir datos a un plano ya cerrado.
            val previous = (_state.value as? PlanState.Ready)?.plan
            _state.value = PlanState.Loading
            closeCurrent(previous)
            currentPlanUri = uri

            // El permiso persistente es lo que permite reabrir el plano en la
            // siguiente sesión sin pasar otra vez por el selector.
            try {
                getApplication<Application>().contentResolver.takePersistableUriPermission(
                    uri, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
                )
            } catch (error: SecurityException) {
                // No todos los orígenes lo conceden; el plano se abre igual.
            }

            val result = withContext(Dispatchers.IO) {
                try {
                    openBlocking(uri)
                } catch (error: Throwable) {
                    PlanState.Failed("No se pudo abrir el plano: ${error.message ?: error.javaClass.simpleName}")
                }
            }
            _state.value = result

            if (result is PlanState.Ready) {
                settings.lastPlanUri = uri.toString()
                settings.lastPlanName = result.name
                // Las capas que el proyectista dejó apagadas o congeladas
                // arrancan ocultas, como en AutoCAD.
                _hiddenLayers.value = result.layerStyles
                    .mapIndexedNotNull { index, style -> index.takeIf { style.hiddenInFile } }
                    .toSet()
                fitPending = true
                applyFitIfPending()
                refreshTexts()
                restoreMeasurements()
            }
        }
    }

    private fun openBlocking(uri: Uri): PlanState {
        // LibreDWG abre archivos por ruta, y una URI del selector de Android no
        // lo es, así que el documento se copia antes a la caché de la app.
        val local = reader.copyToCache(uri)
            ?: return PlanState.Failed("No se pudo leer el archivo desde esa ubicación.")

        val planKey = planKeyFor(uri)
        currentPlanKey = planKey
        val document = reader.describe(uri)
        val safeKey = planKey.replace(Regex("[^A-Za-z0-9._-]"), "_").take(100)

        // Antes de procesar el plano hay que saber qué archivos externos busca,
        // porque su contenido forma parte de la escena resultante. Averiguarlo
        // obliga a leer el DWG entero, así que la respuesta se guarda junto a la
        // caché: sin eso, reabrir un plano costaría lo mismo que la primera vez.
        val xrefIndex = File(getApplication<Application>().cacheDir, "plan-$safeKey.xrefs")
        val declarations = readXrefIndex(xrefIndex, local.length(), document.modified)
            ?: NativePlan.inspectXrefs(local.absolutePath).also {
                writeXrefIndex(xrefIndex, local.length(), document.modified, it)
            }
        val treeUri = xrefStore.projectTreeUri?.let(Uri::parse)
        val resolved = xrefResolver.resolve(planKey, declarations, treeUri)

        val xrefPaths = resolved
            .filter { it.isResolved }
            .associate { it.blockName to it.localFile!!.absolutePath }

        // La caché se nombra por el plano, no por la copia temporal, y se sella
        // con la fecha del documento original: la copia tiene nombre y fecha
        // nuevos en cada apertura, y con ellos la caché no acertaría nunca.
        val cacheFile = File(getApplication<Application>().cacheDir, "plan-$safeKey.bin")
        val plan = NativePlan.open(
            dwgPath = local.absolutePath,
            cachePath = cacheFile.absolutePath,
            sourceSize = local.length(),
            sourceModified = document.modified,
            variant = variantOf(resolved),
            xrefs = xrefPaths,
        )

        // El DWG copiado ya no hace falta: la escena está en memoria y, si se
        // pudo, también en la caché procesada.
        local.delete()

        if (plan == null) return PlanState.Failed("LibreDWG no pudo leer este plano.")

        val bounds = plan.bounds()
        if (bounds == null || plan.entityCount() == 0) {
            plan.close()
            return PlanState.Failed("El plano no contiene geometría en espacio modelo.")
        }

        return PlanState.Ready(
            plan = plan,
            layers = plan.layers(),
            entityCount = plan.entityCount(),
            bounds = bounds,
            name = document.name,
            layerStyles = plan.layerStyles(),
            xrefs = resolved,
        )
    }

    private fun readXrefIndex(file: File, size: Long, modified: Long): List<Pair<String, String>>? {
        if (!file.exists()) return null
        return try {
            val json = org.json.JSONObject(file.readText())
            if (json.optLong("size") != size || json.optLong("modified") != modified) return null
            val flat = json.getJSONArray("xrefs")
            (0 until flat.length() step 2).map { flat.getString(it) to flat.getString(it + 1) }
        } catch (error: Exception) {
            null
        }
    }

    private fun writeXrefIndex(
        file: File,
        size: Long,
        modified: Long,
        declarations: List<Pair<String, String>>,
    ) {
        try {
            val flat = org.json.JSONArray()
            declarations.forEach { (name, path) -> flat.put(name).put(path) }
            file.writeText(
                org.json.JSONObject()
                    .put("size", size).put("modified", modified).put("xrefs", flat)
                    .toString(),
            )
        } catch (error: Exception) {
            // Solo es una optimización: sin ella se vuelve a inspeccionar.
        }
    }

    /**
     * Identificador estable de un plano.
     *
     * No incluye el tamaño del archivo a propósito. Si lo incluyera, en cuanto
     * el proyectista revisara el plano y cambiara un solo byte se perderían las
     * mediciones y habría que rehacer a mano los enlaces de xref, que es
     * justamente lo que la app promete evitar.
     *
     * El hash de la URI completa evita que dos planos con el mismo nombre en
     * carpetas distintas compartan mediciones.
     */
    private fun planKeyFor(uri: Uri): String {
        val name = uri.lastPathSegment.orEmpty().substringAfterLast('/').takeLast(48)
        val hash = uri.toString().hashCode().toUInt().toString(16)
        return "$name-$hash"
    }

    /**
     * Resumen del conjunto de referencias externas usadas.
     *
     * Si una xref se reenlaza o cambia de tamaño, este valor cambia y la caché
     * del plano se descarta: seguir dibujando con el contenido externo antiguo
     * sería un error silencioso de los peores.
     */
    private fun variantOf(resolved: List<ResolvedXref>): Long {
        var hash = 1125899906842597L
        resolved.sortedBy { it.blockName }.forEach { xref ->
            val piece = "${xref.blockName}:${xref.localFile?.length() ?: -1}"
            piece.forEach { hash = 31 * hash + it.code }
        }
        return hash
    }

    fun onViewportChanged(width: Int, height: Int) {
        _camera.value = _camera.value.resize(width, height)
        applyFitIfPending()
        refreshTexts()
    }

    fun onPan(dx: Float, dy: Float) {
        _camera.value = _camera.value.panByPixels(dx, dy)
        refreshTexts()
    }

    fun onZoom(focusX: Float, focusY: Float, factor: Float) {
        _camera.value = _camera.value.zoomAround(focusX, focusY, factor.toDouble())
        refreshTexts()
    }

    fun fitToPlan() {
        val ready = _state.value as? PlanState.Ready ?: return
        _camera.value = _camera.value.fitTo(ready.bounds)
        refreshTexts()
    }

    fun toggleLayer(layerId: Int) {
        _hiddenLayers.value = _hiddenLayers.value.toMutableSet().apply {
            if (!add(layerId)) remove(layerId)
        }
        refreshTexts()
    }

    /** Deja visible solo esa capa. Volver a pulsar sobre la misma lo deshace. */
    fun isolateLayer(layerId: Int) {
        val ready = _state.value as? PlanState.Ready ?: return
        val others = ready.layers.indices.filter { it != layerId }.toSet()
        _hiddenLayers.value = if (_hiddenLayers.value == others) emptySet() else others
        refreshTexts()
    }

    /** Deja las capas como venían en el DWG: ocultas las apagadas o congeladas. */
    fun resetLayers() {
        val ready = _state.value as? PlanState.Ready ?: return
        _hiddenLayers.value = ready.layerStyles
            .mapIndexedNotNull { index, style -> index.takeIf { style.hiddenInFile } }
            .toSet()
        refreshTexts()
    }

    fun showAllLayers() {
        _hiddenLayers.value = emptySet()
        refreshTexts()
    }

    fun toggleBackground() {
        _darkBackground.value = !_darkBackground.value
        settings.darkBackground = _darkBackground.value
    }

    private fun applyFitIfPending() {
        if (!fitPending) return
        val ready = _state.value as? PlanState.Ready ?: return
        // Sin tamaño de pantalla todavía no hay nada que encuadrar: se deja
        // pendiente y se aplica en cuanto la vista informe de sus medidas.
        if (_camera.value.viewportWidth <= 0 || _camera.value.viewportHeight <= 0) return

        _camera.value = _camera.value.fitTo(ready.bounds)
        fitPending = false
    }

    private fun refreshTexts() {
        val ready = _state.value as? PlanState.Ready ?: return
        val view = _camera.value
        if (!view.isUsable) return

        val area = view.visibleBounds()
        // Se pide al núcleo solo lo que cabe en pantalla y con altura legible:
        // el filtro se aplica antes de cruzar el puente, no después.
        val minHeight = MIN_READABLE_PX * view.unitsPerPixel()
        val hidden = _hiddenLayers.value
        _visibleTexts.value = ready.plan
            .visibleTexts(area[0], area[1], area[2], area[3], minHeight)
            .filter { it.layerId !in hidden }
    }

    // --- Medición ---------------------------------------------------------

    fun selectTool(tool: MeasureTool) {
        // Cambiar de herramienta descarta lo que se estuviera marcando: dejarlo
        // a medias mezclaría puntos de dos mediciones distintas.
        _pending.value = PendingMeasure(tool = tool)
        _snapPreview.value = null
    }

    /**
     * Toque sobre el plano.
     *
     * Siempre se intenta enganchar primero: el dedo tiene varios milímetros de
     * imprecisión y tapa el objetivo, así que un punto marcado "a ojo" casi
     * nunca cae donde se pretendía.
     */
    fun onTap(screenX: Float, screenY: Float) {
        val ready = _state.value as? PlanState.Ready ?: return
        val tool = _pending.value.tool
        if (tool == MeasureTool.NONE) return

        val view = _camera.value
        val (worldX, worldY) = view.screenToWorld(screenX, screenY)
        val radius = SNAP_RADIUS_PX * view.unitsPerPixel()
        val hidden = _hiddenLayers.value.toIntArray()

        when (tool) {
            MeasureTool.FOLLOW -> {
                val entity = ready.plan.pickEntity(worldX, worldY, radius, hidden) ?: return
                val measure = ready.plan.measureEntity(entity) ?: return
                addMeasurement(
                    Measurement(
                        id = nextMeasurementId++,
                        tool = tool,
                        points = measure.points,
                        value = measure.length,
                        area = measure.area,
                        approximate = measure.approximate,
                    ),
                )
            }

            MeasureTool.COUNT -> {
                val entity = ready.plan.pickEntity(worldX, worldY, radius, hidden) ?: return
                val symbol = ready.plan.symbolName(entity) ?: return
                addMeasurement(
                    Measurement(
                        id = nextMeasurementId++,
                        tool = tool,
                        points = listOf(worldX to worldY),
                        value = 0.0,
                        count = ready.plan.symbolInstances(entity),
                        symbol = symbol,
                    ),
                )
            }

            else -> {
                val reference = _pending.value.points.lastOrNull()
                val hit = ready.plan.snapAt(worldX, worldY, radius, reference, hidden)
                val point = if (hit != null) hit.x to hit.y else worldX to worldY

                val points = _pending.value.points + point
                _pending.value = _pending.value.copy(points = points)
                _snapPreview.value = null

                // Una distancia se cierra sola al segundo punto: pedir además
                // un botón sería un toque de más en cada medida.
                if (tool == MeasureTool.DISTANCE && points.size == 2) finishPending()
            }
        }
    }

    /** Vista previa del enganche mientras el dedo se mueve, antes de soltar. */
    fun onHover(screenX: Float, screenY: Float) {
        val ready = _state.value as? PlanState.Ready ?: return
        if (!_pending.value.isActive) return
        if (_pending.value.tool == MeasureTool.FOLLOW ||
            _pending.value.tool == MeasureTool.COUNT
        ) {
            return
        }

        val view = _camera.value
        val (worldX, worldY) = view.screenToWorld(screenX, screenY)
        val radius = SNAP_RADIUS_PX * view.unitsPerPixel()
        val hit = ready.plan.snapAt(
            worldX, worldY, radius,
            _pending.value.points.lastOrNull(),
            _hiddenLayers.value.toIntArray(),
        )
        _snapPreview.value = hit?.let { SnapPreview(it.x, it.y, it.type) }
    }

    fun clearSnapPreview() { _snapPreview.value = null }

    fun finishPending() {
        val current = _pending.value
        if (!current.canFinish) return

        val value = when (current.tool) {
            MeasureTool.AREA -> shoelaceArea(current.points)
            else -> chainLength(current.points)
        }

        addMeasurement(
            Measurement(
                id = nextMeasurementId++,
                tool = current.tool,
                points = current.points,
                value = value,
            ),
        )
        // La herramienta sigue activa para poder encadenar medidas sin volver a
        // seleccionarla cada vez.
        _pending.value = PendingMeasure(tool = current.tool)
        _snapPreview.value = null
    }

    /** Deshace el último punto marcado; si no hay ninguno, la última medición. */
    fun undo() {
        val current = _pending.value
        if (current.points.isNotEmpty()) {
            _pending.value = current.copy(points = current.points.dropLast(1))
            return
        }
        _measurements.value = _measurements.value.dropLast(1)
        persistMeasurements()
    }

    fun removeMeasurement(id: Long) {
        _measurements.value = _measurements.value.filterNot { it.id == id }
        persistMeasurements()
    }

    fun renameMeasurement(id: Long, label: String) {
        _measurements.value = _measurements.value.map {
            if (it.id == id) it.copy(label = label) else it
        }
        persistMeasurements()
    }

    fun clearMeasurements() {
        _measurements.value = emptyList()
        _pending.value = PendingMeasure(tool = _pending.value.tool)
        persistMeasurements()
    }

    fun setDecimals(value: Int) {
        _decimals.value = value.coerceIn(0, 6)
        settings.decimals = _decimals.value
    }

    fun setUnit(value: String) {
        _unit.value = value.trim()
        settings.unit = _unit.value
    }

    /** Nombre de archivo propuesto para guardar las mediciones. */
    fun exportFileName(): String =
        MeasurementExport.fileNameFor((_state.value as? PlanState.Ready)?.name.orEmpty())

    /** Mediciones del plano actual en CSV, listas para una hoja de cálculo. */
    fun exportCsv(): String = MeasurementExport.toCsv(
        planName = (_state.value as? PlanState.Ready)?.name.orEmpty(),
        measurements = _measurements.value,
        decimals = _decimals.value,
        unit = _unit.value,
    )

    /** Escribe el CSV en el documento que el usuario ha elegido. */
    fun saveCsv(destination: Uri) {
        val content = exportCsv()
        viewModelScope.launch {
            val saved = withContext(Dispatchers.IO) {
                try {
                    getApplication<Application>().contentResolver
                        .openOutputStream(destination, "wt")
                        ?.use { it.write(content.toByteArray(Charsets.UTF_8)) } != null
                } catch (error: Exception) {
                    false
                }
            }
            _notice.value = if (saved) "Mediciones guardadas." else "No se pudo guardar el archivo."
        }
    }

    private fun addMeasurement(measurement: Measurement) {
        _measurements.value = _measurements.value + measurement
        persistMeasurements()
    }

    /**
     * Vuelca las mediciones a disco.
     *
     * Se hace en cada cambio y no al cerrar: son unos pocos KB, el coste es
     * imperceptible, y si el sistema mata la app en segundo plano no se pierde
     * nada de lo medido.
     */
    private fun persistMeasurements() {
        val planKey = currentPlanKey ?: return
        val snapshot = _measurements.value
        viewModelScope.launch(Dispatchers.IO) {
            measurementStore.save(planKey, snapshot)
        }
    }

    private fun restoreMeasurements() {
        val planKey = currentPlanKey ?: return
        viewModelScope.launch {
            val restored = withContext(Dispatchers.IO) { measurementStore.load(planKey) }
            if (restored.isEmpty()) return@launch

            _measurements.value = restored
            // Los identificadores tienen que seguir donde se quedaron: si se
            // reiniciara el contador, dos mediciones compartirían id y borrar
            // una quitaría la equivocada.
            nextMeasurementId = restored.maxOf { it.id } + 1
        }
    }

    private fun closeCurrent(plan: NativePlan? = (_state.value as? PlanState.Ready)?.plan) {
        plan?.close()
        _notice.value = null
        _visibleTexts.value = emptyList()
        _hiddenLayers.value = emptySet()
        // Las mediciones son de un plano concreto: conservarlas en pantalla al
        // abrir otro daría cifras que no corresponden con lo que se ve. Solo se
        // vacía la lista en memoria; el archivo guardado sigue donde estaba.
        _measurements.value = emptyList()
        nextMeasurementId = 1L
        _pending.value = PendingMeasure()
        _snapPreview.value = null
    }

    override fun onCleared() {
        super.onCleared()
        // Sin esto se quedarían varios megas reservados por cada plano abierto.
        closeCurrent()
        reader.clearCache()
        xrefResolver.clearCache()
    }

    private companion object {
        /**
         * Radio de enganche en píxeles.
         *
         * Generoso a propósito: un dedo tapa bastante más que un cursor, y con
         * un radio pequeño enganchar se vuelve un ejercicio de puntería.
         */
        const val SNAP_RADIUS_PX = 28.0
    }
}
