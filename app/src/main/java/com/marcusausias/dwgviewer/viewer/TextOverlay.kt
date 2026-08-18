package com.marcusausias.dwgviewer.viewer

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import com.marcusausias.dwgviewer.nativebridge.PlanText
import kotlin.math.PI

/**
 * Textos del plano, dibujados encima de la geometría.
 *
 * Van aparte de OpenGL a propósito: dibujar texto en GL exigiría construir un
 * atlas de glifos y resolver a mano el acentuado, mientras que el número de
 * textos visibles a la vez siempre es pequeño y el Canvas del sistema los
 * dibuja bien y con las fuentes correctas.
 */
@Composable
fun TextOverlay(
    texts: List<PlanText>,
    camera: Camera,
    color: Color,
    modifier: Modifier = Modifier,
) {
    val paint = remember { android.graphics.Paint().apply { isAntiAlias = true } }

    Canvas(modifier = modifier) {
        if (texts.isEmpty() || !camera.isUsable) return@Canvas

        drawIntoCanvas { canvas ->
            val native = canvas.nativeCanvas
            paint.color = color.toArgb()

            for (text in texts) {
                val (screenX, screenY) = camera.worldToScreen(text.x, text.y)
                // La altura del DWG está en unidades de dibujo; en pantalla
                // depende del zoom.
                val sizePx = (text.height * camera.scale).toFloat()
                if (sizePx < MIN_READABLE_PX) continue

                paint.textSize = sizePx

                native.save()
                // El texto del DWG gira en sentido antihorario y el Canvas lo
                // hace al revés, de ahí el cambio de signo.
                val degrees = (-text.rotation * 180.0 / PI).toFloat()
                native.rotate(degrees, screenX, screenY)
                native.drawText(text.content, screenX, screenY, paint)
                native.restore()
            }
        }
    }
}

/**
 * Altura mínima para molestarse en dibujar un texto.
 *
 * Por debajo de esto no se lee nada y, en un plano de instalaciones con miles
 * de textos, dibujarlos igualmente es lo que rompería la fluidez del paneo.
 */
const val MIN_READABLE_PX = 7f
