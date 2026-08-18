package com.marcusausias.dwgviewer.viewer

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel

/** Ancho a partir del cual el panel de capas cabe fijo al lado del plano. */
private const val WIDE_LAYOUT_DP = 720

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ViewerScreen(model: ViewerViewModel = viewModel()) {
    val state by model.state.collectAsStateWithLifecycle()
    val camera by model.camera.collectAsStateWithLifecycle()
    val hiddenLayers by model.hiddenLayers.collectAsStateWithLifecycle()
    val texts by model.visibleTexts.collectAsStateWithLifecycle()
    val dark by model.darkBackground.collectAsStateWithLifecycle()
    val measurements by model.measurements.collectAsStateWithLifecycle()
    val pending by model.pending.collectAsStateWithLifecycle()
    val snapPreview by model.snapPreview.collectAsStateWithLifecycle()
    val decimals by model.decimals.collectAsStateWithLifecycle()
    val unit by model.unit.collectAsStateWithLifecycle()

    var showLayerSheet by remember { mutableStateOf(false) }
    var showMeasureSheet by remember { mutableStateOf(false) }
    var widthDp by remember { mutableStateOf(0) }

    val picker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri -> if (uri != null) model.open(uri) }

    // Carpeta del proyecto: se pide una vez y Android conserva el permiso, que
    // es lo que permite resolver las referencias externas sin volver a
    // preguntar en sesiones posteriores.
    val folderPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocumentTree(),
    ) { uri -> if (uri != null) model.setProjectFolder(uri) }

    // Reenlace manual de una xref concreta que no se ha localizado.
    var linkingBlock by remember { mutableStateOf<String?>(null) }
    val xrefPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri ->
        val block = linkingBlock
        linkingBlock = null
        if (uri != null && block != null) model.linkXref(block, uri)
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Row(modifier = Modifier.fillMaxSize()) {
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .onSizeChanged { size ->
                        widthDp = size.width
                        model.onViewportChanged(size.width, size.height)
                    },
            ) {
                val ready = state as? PlanState.Ready

                PlanGLView(
                    plan = ready?.plan,
                    camera = camera,
                    hiddenLayers = hiddenLayers,
                    darkBackground = dark,
                    modifier = Modifier.fillMaxSize(),
                )

                TextOverlay(
                    texts = texts,
                    camera = camera,
                    color = if (dark) Color(0xFFE8E8E4) else Color(0xFF1A1C20),
                    modifier = Modifier.fillMaxSize(),
                )

                // Las mediciones y los gestos van en la capa de más arriba para
                // que ningún toque se quede por el camino.
                MeasureOverlay(
                    camera = camera,
                    measurements = measurements,
                    pending = pending,
                    snap = snapPreview,
                    decimals = decimals,
                    unit = unit,
                    modifier = Modifier
                        .fillMaxSize()
                        .planGestures(
                            onPan = model::onPan,
                            onZoom = model::onZoom,
                            onTap = model::onTap,
                            onPreview = model::onHover,
                            onPreviewCancel = { model.clearSnapPreview() },
                        ),
                )

                ViewerStatus(
                    state = state,
                    onPick = { picker.launch(arrayOf("*/*")) },
                    modifier = Modifier.align(Alignment.Center),
                )

                if (ready != null && ready.missingXrefs.isNotEmpty()) {
                    XrefBanner(
                        missing = ready.missingXrefs,
                        hasProjectFolder = model.projectFolder != null,
                        onPickFolder = { folderPicker.launch(null) },
                        onLink = { block ->
                            linkingBlock = block
                            xrefPicker.launch(arrayOf("*/*"))
                        },
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .windowInsetsPadding(WindowInsets.safeDrawing)
                            .padding(top = 48.dp),
                    )
                }

                ViewerToolbar(
                    hasPlan = ready != null,
                    onPick = { picker.launch(arrayOf("*/*")) },
                    onFit = model::fitToPlan,
                    onLayers = { showLayerSheet = true },
                    onMeasurements = { showMeasureSheet = true },
                    onToggleBackground = model::toggleBackground,
                    onPickFolder = { folderPicker.launch(null) },
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .windowInsetsPadding(WindowInsets.safeDrawing),
                )

                if (ready != null) {
                    MeasureToolbar(
                        activeTool = pending.tool,
                        pending = pending,
                        onSelectTool = model::selectTool,
                        onFinish = model::finishPending,
                        onUndo = model::undo,
                        modifier = Modifier
                            .align(Alignment.BottomCenter)
                            .fillMaxWidth()
                            .windowInsetsPadding(WindowInsets.safeDrawing),
                    )
                }
            }

            // En pantalla ancha el panel se queda fijo; en móvil se abre como
            // hoja deslizante para no comerse el plano.
            val ready = state as? PlanState.Ready
            if (ready != null && widthDp >= WIDE_LAYOUT_DP) {
                Column(
                    modifier = Modifier
                        .width(300.dp)
                        .fillMaxHeight()
                        .windowInsetsPadding(WindowInsets.safeDrawing),
                ) {
                    MeasurementList(
                        measurements = measurements,
                        decimals = decimals,
                        unit = unit,
                        onRename = model::renameMeasurement,
                        onRemove = model::removeMeasurement,
                        onClear = model::clearMeasurements,
                    )
                    LayerPanel(
                        layers = ready.layers,
                        hiddenLayers = hiddenLayers,
                        onToggle = model::toggleLayer,
                        onIsolate = model::isolateLayer,
                        onShowAll = model::showAllLayers,
                    )
                }
            }
        }

        val ready = state as? PlanState.Ready
        if (showMeasureSheet && ready != null && widthDp < WIDE_LAYOUT_DP) {
            ModalBottomSheet(onDismissRequest = { showMeasureSheet = false }) {
                MeasurementList(
                    measurements = measurements,
                    decimals = decimals,
                    unit = unit,
                    onRename = model::renameMeasurement,
                    onRemove = model::removeMeasurement,
                    onClear = model::clearMeasurements,
                )
            }
        }

        if (showLayerSheet && ready != null && widthDp < WIDE_LAYOUT_DP) {
            ModalBottomSheet(onDismissRequest = { showLayerSheet = false }) {
                LayerPanel(
                    layers = ready.layers,
                    hiddenLayers = hiddenLayers,
                    onToggle = model::toggleLayer,
                    onIsolate = model::isolateLayer,
                    onShowAll = model::showAllLayers,
                )
            }
        }
    }
}

@Composable
private fun ViewerToolbar(
    hasPlan: Boolean,
    onPick: () -> Unit,
    onFit: () -> Unit,
    onLayers: () -> Unit,
    onMeasurements: () -> Unit,
    onToggleBackground: () -> Unit,
    onPickFolder: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(modifier = modifier.padding(8.dp)) {
        TextButton(onClick = onPick) { Text("Abrir") }
        if (hasPlan) {
            TextButton(onClick = onFit) { Text("Encuadrar") }
            TextButton(onClick = onLayers) { Text("Capas") }
            TextButton(onClick = onMeasurements) { Text("Medidas") }
            TextButton(onClick = onToggleBackground) { Text("Fondo") }
            TextButton(onClick = onPickFolder) { Text("Carpeta") }
        }
    }
}

@Composable
private fun ViewerStatus(
    state: PlanState,
    onPick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    when (state) {
        is PlanState.Loading -> Column(
            modifier = modifier,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            CircularProgressIndicator()
            Text("Procesando el plano…", modifier = Modifier.padding(top = 12.dp))
        }

        is PlanState.Failed -> Column(
            modifier = modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(state.message, style = MaterialTheme.typography.bodyMedium)
            Button(onClick = onPick, modifier = Modifier.padding(top = 12.dp)) {
                Text("Elegir otro archivo")
            }
        }

        is PlanState.Empty -> Button(onClick = onPick, modifier = modifier) {
            Text("Abrir un plano DWG")
        }

        is PlanState.Ready -> Unit
    }
}
