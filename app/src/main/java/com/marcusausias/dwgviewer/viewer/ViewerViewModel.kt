package com.marcusausias.dwgviewer.viewer

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.marcusausias.dwgviewer.data.DwgFileReader
import com.marcusausias.dwgviewer.nativebridge.NativePlan
import com.marcusausias.dwgviewer.nativebridge.PlanText
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
    ) : PlanState
}

class ViewerViewModel(application: Application) : AndroidViewModel(application) {

    private val reader = DwgFileReader(application)

    private val _state = MutableStateFlow<PlanState>(PlanState.Empty)
    val state: StateFlow<PlanState> = _state.asStateFlow()

    private val _camera = MutableStateFlow(Camera())
    val camera: StateFlow<Camera> = _camera.asStateFlow()

    private val _hiddenLayers = MutableStateFlow<Set<Int>>(emptySet())
    val hiddenLayers: StateFlow<Set<Int>> = _hiddenLayers.asStateFlow()

    private val _visibleTexts = MutableStateFlow<List<PlanText>>(emptyList())
    val visibleTexts: StateFlow<List<PlanText>> = _visibleTexts.asStateFlow()

    private val _darkBackground = MutableStateFlow(false)
    val darkBackground: StateFlow<Boolean> = _darkBackground.asStateFlow()

    private val _measurements = MutableStateFlow<List<Measurement>>(emptyList())
    val measurements: StateFlow<List<Measurement>> = _measurements.asStateFlow()

    private val _pending = MutableStateFlow(PendingMeasure())
    val pending: StateFlow<PendingMeasure> = _pending.asStateFlow()

    private val _snapPreview = MutableStateFlow<SnapPreview?>(null)
    val snapPreview: StateFlow<SnapPreview?> = _snapPreview.asStateFlow()

    private val _decimals = MutableStateFlow(3)
    val decimals: StateFlow<Int> = _decimals.asStateFlow()

    private val _unit = MutableStateFlow("")
    val unit: StateFlow<String> = _unit.asStateFlow()

    private var fitPending = true
    private var nextMeasurementId = 1L

    fun open(uri: Uri) {
        viewModelScope.launch {
            closeCurrent()
            _state.value = PlanState.Loading

            val result = withContext(Dispatchers.IO) { openBlocking(uri) }
            _state.value = result

            if (result is PlanState.Ready) {
                fitPending = true
                applyFitIfPending()
                refreshTexts()
            }
        }
    }

    private fun openBlocking(uri: Uri): PlanState {
        // LibreDWG abre archivos por ruta, y una URI del selector de Android no
        // lo es, así que el documento se copia antes a la caché de la app.
        val local = reader.copyToCache(uri)
            ?: return PlanState.Failed("No se pudo leer el archivo desde esa ubicación.")

        val cacheFile = File(getApplication<Application>().cacheDir, "plan-${local.name}.bin")
        val plan = NativePlan.open(
            dwgPath = local.absolutePath,
            cachePath = cacheFile.absolutePath,
            sourceSize = local.length(),
            sourceModified = local.lastModified(),
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
        )
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

    fun showAllLayers() {
        _hiddenLayers.value = emptySet()
        refreshTexts()
    }

    fun toggleBackground() {
        _darkBackground.value = !_darkBackground.value
    }

    private fun applyFitIfPending() {
        if (!fitPending) return
        val ready = _state.value as? PlanState.Ready ?: return
        if (!_camera.value.isUsable && _camera.value.viewportWidth == 0) return

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
    }

    fun removeMeasurement(id: Long) {
        _measurements.value = _measurements.value.filterNot { it.id == id }
    }

    fun renameMeasurement(id: Long, label: String) {
        _measurements.value = _measurements.value.map {
            if (it.id == id) it.copy(label = label) else it
        }
    }

    fun clearMeasurements() {
        _measurements.value = emptyList()
        _pending.value = PendingMeasure(tool = _pending.value.tool)
    }

    fun setDecimals(value: Int) { _decimals.value = value.coerceIn(0, 6) }

    fun setUnit(value: String) { _unit.value = value.trim() }

    private fun addMeasurement(measurement: Measurement) {
        _measurements.value = _measurements.value + measurement
    }

    private fun closeCurrent() {
        (_state.value as? PlanState.Ready)?.plan?.close()
        _visibleTexts.value = emptyList()
        _hiddenLayers.value = emptySet()
        // Las mediciones son de un plano concreto: conservarlas al abrir otro
        // daría cifras que no corresponden con lo que se está viendo.
        _measurements.value = emptyList()
        _pending.value = PendingMeasure()
        _snapPreview.value = null
    }

    override fun onCleared() {
        super.onCleared()
        // Sin esto se quedarían varios megas reservados por cada plano abierto.
        closeCurrent()
        reader.clearCache()
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
