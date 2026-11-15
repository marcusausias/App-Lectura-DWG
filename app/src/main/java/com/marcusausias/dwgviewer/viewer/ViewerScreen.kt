package com.marcusausias.dwgviewer.viewer

import android.content.Intent
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Contrast
import androidx.compose.material.icons.filled.FitScreen
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.Layers
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Straighten
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.delay

/** Ancho a partir del cual el panel de capas cabe fijo al lado del plano. */
private const val WIDE_LAYOUT_DP = 900

/**
 * Tipos que se ofrecen en el selector. Un DWG no tiene un tipo MIME único:
 * según el origen llega como imagen, como aplicación o como binario genérico,
 * así que filtrar por uno solo dejaría planos sin poder elegirse.
 */
private val DWG_MIME_TYPES = arrayOf("*/*")

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
    val notice by model.notice.collectAsStateWithLifecycle()

    var showLayerSheet by remember { mutableStateOf(false) }
    var showMeasureSheet by remember { mutableStateOf(false) }
    var totalWidthPx by remember { mutableIntStateOf(0) }

    val context = LocalContext.current
    val density = LocalDensity.current
    val isWide = with(density) { totalWidthPx.toDp() } >= WIDE_LAYOUT_DP.dp

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

    val csvSaver = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("text/csv"),
    ) { uri -> if (uri != null) model.saveCsv(uri) }

    val shareCsv = {
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_SUBJECT, model.exportFileName().removeSuffix(".csv"))
            // Sin la marca BOM, que solo tiene sentido dentro de un archivo.
            putExtra(Intent.EXTRA_TEXT, model.exportCsv().removePrefix("\uFEFF"))
        }
        context.startActivity(Intent.createChooser(intent, "Compartir mediciones"))
    }

    // Los avisos se retiran solos: no piden ninguna decisión.
    LaunchedEffect(notice) {
        if (notice != null) {
            delay(3500)
            model.dismissNotice()
        }
    }

    val ready = state as? PlanState.Ready

    val measurementPanel: @Composable () -> Unit = {
        MeasurementList(
            measurements = measurements,
            decimals = decimals,
            unit = unit,
            onRename = model::renameMeasurement,
            onRemove = model::removeMeasurement,
            onClear = model::clearMeasurements,
            onUnitChange = model::setUnit,
            onDecimalsChange = model::setDecimals,
            onExport = { csvSaver.launch(model.exportFileName()) },
            onShare = shareCsv,
        )
    }
    val layerPanel: @Composable () -> Unit = {
        if (ready != null) {
            LayerPanel(
                layers = ready.layers,
                styles = ready.layerStyles,
                hiddenLayers = hiddenLayers,
                onToggle = model::toggleLayer,
                onIsolate = model::isolateLayer,
                onShowAll = model::showAllLayers,
                onReset = model::resetLayers,
            )
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .onSizeChanged { totalWidthPx = it.width },
    ) {
        Row(modifier = Modifier.fillMaxSize()) {
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .onSizeChanged { size -> model.onViewportChanged(size.width, size.height) },
            ) {
                PlanGLView(
                    plan = ready?.plan,
                    camera = camera,
                    hiddenLayers = hiddenLayers,
                    darkBackground = dark,
                    modifier = Modifier.fillMaxSize(),
                )

                if (ready != null) {
                    TextOverlay(
                        texts = texts,
                        camera = camera,
                        color = if (dark) Color(0xFFE8E8E4) else Color(0xFF1A1C20),
                        modifier = Modifier.fillMaxSize(),
                    )

                    // Las mediciones y los gestos van en la capa de más arriba
                    // para que ningún toque se quede por el camino.
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
                }

                ViewerStatus(
                    state = state,
                    dark = dark,
                    lastPlanName = model.lastPlanName,
                    onPick = { picker.launch(DWG_MIME_TYPES) },
                    onReopen = { model.lastPlanUri?.let(model::open) },
                    modifier = Modifier
                        .align(Alignment.Center)
                        .windowInsetsPadding(WindowInsets.safeDrawing),
                )

                Column(
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .fillMaxWidth()
                        .windowInsetsPadding(WindowInsets.safeDrawing),
                ) {
                    if (ready != null) {
                        ViewerToolbar(
                            title = ready.name,
                            subtitle = "${ready.entityCount} entidades · ${ready.layers.size} capas",
                            showPanelButtons = !isWide,
                            onPick = { picker.launch(DWG_MIME_TYPES) },
                            onFit = model::fitToPlan,
                            onLayers = { showLayerSheet = true },
                            onMeasurements = { showMeasureSheet = true },
                            onToggleBackground = model::toggleBackground,
                            onPickFolder = { folderPicker.launch(null) },
                        )
                    }

                    if (ready != null && ready.missingXrefs.isNotEmpty()) {
                        XrefBanner(
                            missing = ready.missingXrefs,
                            hasProjectFolder = model.projectFolder != null,
                            onPickFolder = { folderPicker.launch(null) },
                            onLink = { block ->
                                linkingBlock = block
                                xrefPicker.launch(DWG_MIME_TYPES)
                            },
                        )
                    }

                    notice?.let { message ->
                        Surface(
                            color = Color(0xFF1A1C20),
                            shape = RoundedCornerShape(8.dp),
                            modifier = Modifier
                                .align(Alignment.CenterHorizontally)
                                .padding(8.dp),
                        ) {
                            Text(
                                text = message,
                                color = Color.White,
                                style = MaterialTheme.typography.bodyMedium,
                                modifier = Modifier.padding(horizontal = 14.dp, vertical = 8.dp),
                            )
                        }
                    }
                }

                if (ready != null) {
                    MeasureToolbar(
                        activeTool = pending.tool,
                        pending = pending,
                        decimals = decimals,
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

            // En pantalla ancha los paneles se quedan fijos; en móvil se abren
            // como hoja deslizante para no comerse el plano.
            if (ready != null && isWide) {
                Column(
                    modifier = Modifier
                        .width(320.dp)
                        .fillMaxHeight()
                        .background(MaterialTheme.colorScheme.surface)
                        .windowInsetsPadding(WindowInsets.safeDrawing)
                        .verticalScroll(rememberScrollState()),
                ) {
                    measurementPanel()
                    layerPanel()
                }
            }
        }

        if (showMeasureSheet && ready != null && !isWide) {
            ModalBottomSheet(onDismissRequest = { showMeasureSheet = false }) {
                measurementPanel()
            }
        }

        if (showLayerSheet && ready != null && !isWide) {
            ModalBottomSheet(onDismissRequest = { showLayerSheet = false }) {
                layerPanel()
            }
        }
    }
}

/**
 * Barra superior.
 *
 * Va con iconos y no con texto porque seis botones de texto no caben en el
 * ancho de un móvil: los últimos quedaban fuera de la pantalla.
 */
@Composable
private fun ViewerToolbar(
    title: String,
    subtitle: String,
    showPanelButtons: Boolean,
    onPick: () -> Unit,
    onFit: () -> Unit,
    onLayers: () -> Unit,
    onMeasurements: () -> Unit,
    onToggleBackground: () -> Unit,
    onPickFolder: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var menuOpen by remember { mutableStateOf(false) }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.92f))
            .padding(start = 12.dp, end = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title.ifBlank { "Plano" },
                style = MaterialTheme.typography.titleSmall,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodySmall,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        IconButton(onClick = onFit) {
            Icon(Icons.Filled.FitScreen, contentDescription = "Encuadrar el plano")
        }
        if (showPanelButtons) {
            IconButton(onClick = onLayers) {
                Icon(Icons.Filled.Layers, contentDescription = "Capas")
            }
            IconButton(onClick = onMeasurements) {
                Icon(Icons.Filled.Straighten, contentDescription = "Mediciones")
            }
        }
        IconButton(onClick = onToggleBackground) {
            Icon(Icons.Filled.Contrast, contentDescription = "Cambiar fondo claro u oscuro")
        }
        Box {
            IconButton(onClick = { menuOpen = true }) {
                Icon(Icons.Filled.MoreVert, contentDescription = "Más opciones")
            }
            DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
                DropdownMenuItem(
                    text = { Text("Abrir otro plano") },
                    leadingIcon = { Icon(Icons.Filled.FolderOpen, contentDescription = null) },
                    onClick = {
                        menuOpen = false
                        onPick()
                    },
                )
                DropdownMenuItem(
                    text = { Text("Carpeta del proyecto (xrefs)") },
                    onClick = {
                        menuOpen = false
                        onPickFolder()
                    },
                )
            }
        }
    }
}

@Composable
private fun ViewerStatus(
    state: PlanState,
    dark: Boolean,
    lastPlanName: String?,
    onPick: () -> Unit,
    onReopen: () -> Unit,
    modifier: Modifier = Modifier,
) {
    // El texto va directamente sobre el fondo del visor, que no sigue el tema.
    val ink = if (dark) Color(0xFFE8E8E4) else Color(0xFF1A1C20)

    when (state) {
        is PlanState.Loading -> Column(
            modifier = modifier,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            CircularProgressIndicator()
            Text("Procesando el plano…", color = ink, modifier = Modifier.padding(top = 12.dp))
        }

        is PlanState.Failed -> Column(
            modifier = modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                state.message,
                color = ink,
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
            )
            Button(onClick = onPick, modifier = Modifier.padding(top = 12.dp)) {
                Text("Elegir otro archivo")
            }
        }

        is PlanState.Empty -> Column(
            modifier = modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text("Planos DWG", color = ink, style = MaterialTheme.typography.headlineSmall)
            Text(
                "Abre un plano para verlo y medir sobre él.",
                color = ink,
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(top = 4.dp, bottom = 16.dp),
            )
            Button(onClick = onPick) { Text("Abrir un plano DWG") }
            if (!lastPlanName.isNullOrBlank()) {
                OutlinedButton(onClick = onReopen, modifier = Modifier.padding(top = 8.dp)) {
                    Text(
                        "Reabrir $lastPlanName",
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        }

        is PlanState.Ready -> Unit
    }
}

/** Abre el plano que llega desde otra app ("Abrir con…"). */
fun ViewerViewModel.openFromIntent(uri: Uri?) {
    if (uri != null) open(uri)
}
