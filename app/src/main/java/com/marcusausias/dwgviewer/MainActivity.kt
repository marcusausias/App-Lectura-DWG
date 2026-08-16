package com.marcusausias.dwgviewer

import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.marcusausias.dwgviewer.data.DwgFileReader
import com.marcusausias.dwgviewer.nativebridge.DwgNative

/**
 * Pantalla de validación de la Fase 0.
 *
 * No es la interfaz definitiva: existe para comprobar en un dispositivo real
 * que LibreDWG compilado con el NDK abre los planos de obra, cuánto tarda en
 * hacerlo y qué referencias externas declaran. Hasta que esto funcione con los
 * planos de verdad no tiene sentido construir el visor.
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    Fase0Screen()
                }
            }
        }
    }
}

@Composable
private fun Fase0Screen() {
    val context = LocalContext.current
    val reader = remember { DwgFileReader(context) }
    var report by remember { mutableStateOf("Elige un archivo .dwg para analizarlo.") }

    val picker = rememberLauncherForDocument { uri ->
        if (uri == null) return@rememberLauncherForDocument
        report = "Leyendo…"
        report = buildReport(reader, uri)
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(20.dp)
            .verticalScroll(rememberScrollState()),
    ) {
        Text("Fase 0 — validación del motor", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(12.dp))
        Button(onClick = { picker.launch(arrayOf("*/*")) }) {
            Text("Abrir un DWG")
        }
        Spacer(Modifier.height(20.dp))
        Text(report, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun rememberLauncherForDocument(onResult: (Uri?) -> Unit) =
    androidx.activity.compose.rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument(),
        onResult = onResult,
    )

private fun buildReport(reader: DwgFileReader, uri: Uri): String {
    val prepared = reader.copyToCache(uri)
        ?: return "No se pudo leer el archivo desde esa ubicación."

    val startedAt = System.nanoTime()
    val summary = DwgNative.summarize(prepared.absolutePath)
    val elapsedMs = (System.nanoTime() - startedAt) / 1_000_000

    prepared.delete()

    if (summary == null) return "El núcleo nativo no devolvió nada."
    if (!summary.opened) return "No se pudo abrir: ${summary.errorMessage}"

    return buildString {
        appendLine("Versión    : ${summary.versionCode} (${summary.versionName})")
        appendLine("Tiempo     : $elapsedMs ms")
        appendLine("Objetos    : ${summary.objectCount}")
        appendLine("Entidades  : ${summary.entityCount}")
        appendLine("Capas      : ${summary.layerCount}")
        appendLine("Bloques    : ${summary.blockCount}")
        // Un código distinto de cero por debajo de 128 son avisos, no un fallo.
        appendLine("Aviso libredwg: ${summary.libredwgError}")
        appendLine()
        if (summary.xrefs.isEmpty()) {
            appendLine("Sin referencias externas.")
        } else {
            appendLine("Referencias externas (${summary.xrefs.size}):")
            summary.xrefs.forEach { xref ->
                appendLine("  · ${xref.name}")
                appendLine("      ${xref.path}")
            }
        }
    }
}
