package com.marcusausias.dwgviewer.viewer

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/**
 * Barra de herramientas de medición.
 *
 * La navegación no se apaga nunca: se puede seguir moviendo el plano con la
 * herramienta activa, y cada toque coloca un punto. Obligar a cambiar de modo
 * para moverse haría insufrible medir varias cosas seguidas.
 */
@Composable
fun MeasureToolbar(
    activeTool: MeasureTool,
    pending: PendingMeasure,
    onSelectTool: (MeasureTool) -> Unit,
    onFinish: () -> Unit,
    onUndo: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.background(MaterialTheme.colorScheme.surface.copy(alpha = 0.92f))) {
        Row(
            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())
                .padding(horizontal = 8.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            MeasureTool.entries.forEach { tool ->
                FilterChip(
                    selected = tool == activeTool,
                    onClick = { onSelectTool(tool) },
                    label = { Text(tool.label) },
                )
            }
        }

        if (activeTool != MeasureTool.NONE) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 2.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = if (pending.points.isEmpty()) {
                        activeTool.hint
                    } else {
                        // Ver la medida mientras se marca evita tener que
                        // cerrarla solo para comprobar si va bien.
                        val value = Measurement.formatNumber(pending.preview(), 3)
                        "${pending.points.size} puntos · $value"
                    },
                    style = MaterialTheme.typography.bodySmall,
                    modifier = Modifier.weight(1f),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                if (pending.points.isNotEmpty()) {
                    TextButton(onClick = onUndo) { Text("Deshacer") }
                }
                if (pending.canFinish) {
                    TextButton(onClick = onFinish) { Text("Cerrar") }
                }
            }
        }
    }
}

/** Lista de mediciones tomadas, con etiqueta editable. */
@Composable
fun MeasurementList(
    measurements: List<Measurement>,
    decimals: Int,
    unit: String,
    onRename: (Long, String) -> Unit,
    onRemove: (Long) -> Unit,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.background(MaterialTheme.colorScheme.surface)) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("Mediciones (${measurements.size})", style = MaterialTheme.typography.titleSmall)
            if (measurements.isNotEmpty()) {
                TextButton(onClick = onClear) { Text("Borrar todas") }
            }
        }

        if (measurements.isEmpty()) {
            Text(
                text = "Elige una herramienta y toca el plano.",
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
            )
            return@Column
        }

        LazyColumn(modifier = Modifier.heightIn(max = 320.dp)) {
            items(measurements, key = { it.id }) { measurement ->
                Row(
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = measurement.formatted(decimals, unit),
                            style = MaterialTheme.typography.bodyLarge,
                        )
                        OutlinedTextField(
                            value = measurement.label,
                            onValueChange = { onRename(measurement.id, it) },
                            placeholder = { Text(measurement.tool.label) },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                    }
                    TextButton(onClick = { onRemove(measurement.id) }) { Text("Quitar") }
                }
            }
        }
    }
}
