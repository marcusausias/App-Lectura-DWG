package com.marcusausias.dwgviewer.viewer

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.marcusausias.dwgviewer.xref.ResolvedXref

/**
 * Aviso de referencias externas sin localizar.
 *
 * Es deliberadamente difícil de pasar por alto. Un plano al que le falta una
 * xref se dibuja igual, solo que incompleto, y medir sobre él daría cifras
 * equivocadas sin que nada indique por qué. Vale más molestar con un aviso que
 * dejar que alguien presupueste sobre medio plano.
 */
@Composable
fun XrefBanner(
    missing: List<ResolvedXref>,
    onLink: (String) -> Unit,
    onPickFolder: () -> Unit,
    hasProjectFolder: Boolean,
    modifier: Modifier = Modifier,
) {
    if (missing.isEmpty()) return

    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(Color(0xFFC0392B))
            .padding(horizontal = 12.dp, vertical = 8.dp),
    ) {
        Text(
            text = "Faltan ${missing.size} referencias externas: el plano está incompleto.",
            color = Color.White,
            style = MaterialTheme.typography.bodyMedium,
        )

        if (!hasProjectFolder) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = "Indica la carpeta del proyecto y se buscarán solas.",
                    color = Color.White,
                    style = MaterialTheme.typography.bodySmall,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = onPickFolder) {
                    Text("Elegir carpeta", color = Color.White)
                }
            }
        }

        missing.forEach { xref ->
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = xref.blockName,
                    color = Color.White,
                    style = MaterialTheme.typography.bodySmall,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = { onLink(xref.blockName) }) {
                    Text("Localizar", color = Color.White)
                }
            }
        }
    }
}
