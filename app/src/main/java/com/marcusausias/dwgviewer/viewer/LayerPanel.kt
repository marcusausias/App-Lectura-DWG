package com.marcusausias.dwgviewer.viewer

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/**
 * Capas del plano, con interruptor y aislado.
 *
 * Aislar es lo que de verdad se usa midiendo: en un plano cargado, dejar solo
 * la capa de tabiquería es la diferencia entre poder enganchar el punto que
 * buscas o pelearte con las instalaciones que pasan por encima.
 */
@Composable
fun LayerPanel(
    layers: List<String>,
    hiddenLayers: Set<Int>,
    onToggle: (Int) -> Unit,
    onIsolate: (Int) -> Unit,
    onShowAll: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.background(MaterialTheme.colorScheme.surface)) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = "Capas (${layers.size})",
                style = MaterialTheme.typography.titleSmall,
            )
            if (hiddenLayers.isNotEmpty()) {
                TextButton(onClick = onShowAll) { Text("Ver todas") }
            }
        }

        LazyColumn(modifier = Modifier.heightIn(max = 420.dp)) {
            itemsIndexed(layers) { index, name ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { onToggle(index) }
                        .padding(horizontal = 12.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Checkbox(
                        checked = index !in hiddenLayers,
                        onCheckedChange = { onToggle(index) },
                        modifier = Modifier.size(40.dp),
                    )
                    Text(
                        // Una capa sin nombre aparece así en el propio DWG.
                        text = name.ifBlank { "(sin nombre)" },
                        modifier = Modifier.weight(1f).padding(start = 4.dp),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    TextButton(onClick = { onIsolate(index) }) { Text("Solo") }
                }
            }
        }
    }
}
