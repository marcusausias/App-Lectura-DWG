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

    private var fitPending = true

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

    private fun closeCurrent() {
        (_state.value as? PlanState.Ready)?.plan?.close()
        _visibleTexts.value = emptyList()
        _hiddenLayers.value = emptySet()
    }

    override fun onCleared() {
        super.onCleared()
        // Sin esto se quedarían varios megas reservados por cada plano abierto.
        closeCurrent()
        reader.clearCache()
    }
}
