package com.marcusausias.dwgviewer

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import com.marcusausias.dwgviewer.viewer.ViewerScreen
import com.marcusausias.dwgviewer.viewer.ViewerViewModel
import com.marcusausias.dwgviewer.viewer.openFromIntent

class MainActivity : ComponentActivity() {

    private val model: ViewerViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        // Solo en el arranque real: al recrearse la actividad el plano ya está
        // abierto en el ViewModel y volver a abrirlo lo recargaría sin motivo.
        if (savedInstanceState == null) openPlanFrom(intent)

        setContent {
            MaterialTheme(colorScheme = AppColors) {
                Surface(modifier = Modifier.fillMaxSize()) {
                    ViewerScreen(model)
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        openPlanFrom(intent)
    }

    /** "Abrir con…" desde el gestor de archivos, el correo o Drive. */
    private fun openPlanFrom(intent: Intent?) {
        if (intent?.action == Intent.ACTION_VIEW) model.openFromIntent(intent.data)
    }
}

// La paleta del resto de herramientas del estudio: fondo hueso, tinta casi
// negra y acento amarillo.
private val AppColors = lightColorScheme(
    primary = Color(0xFF1A1C20),
    onPrimary = Color.White,
    secondary = Color(0xFF8A6D00),
    secondaryContainer = Color(0xFFFFE680),
    onSecondaryContainer = Color(0xFF1A1C20),
    surface = Color(0xFFFAFAF7),
    onSurface = Color(0xFF1A1C20),
    background = Color(0xFFEFEFEA),
    onBackground = Color(0xFF1A1C20),
)
