package com.marcusausias.dwgviewer.viewer

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb

private val MeasureColor = Color(0xFFC0392B)
private val DoneColor = Color(0xFF27AE60)
private val SnapColor = Color(0xFFFFD400)

/**
 * Mediciones dibujadas sobre el plano.
 *
 * Va en Compose y no en OpenGL porque cambia en cada toque mientras la
 * geometría del plano no cambia nunca: mezclarlas obligaría a resubir búferes
 * constantemente.
 */
@Composable
fun MeasureOverlay(
    camera: Camera,
    measurements: List<Measurement>,
    pending: PendingMeasure,
    snap: SnapPreview?,
    decimals: Int,
    unit: String,
    modifier: Modifier = Modifier,
) {
    Canvas(modifier = modifier) {
        if (!camera.isUsable) return@Canvas

        measurements.forEach { drawMeasurement(camera, it, DoneColor, decimals, unit) }

        if (pending.points.isNotEmpty()) {
            drawPath(camera, pending.points, MeasureColor, close = pending.tool == MeasureTool.AREA)
            pending.points.forEach { drawHandle(camera, it, MeasureColor) }
        }

        snap?.let { drawSnapMarker(camera, it) }
    }
}

private fun DrawScope.drawMeasurement(
    camera: Camera,
    measurement: Measurement,
    color: Color,
    decimals: Int,
    unit: String,
) {
    if (measurement.isCount) {
        measurement.points.forEach { drawHandle(camera, it, color, radius = 9f) }
    } else {
        drawPath(camera, measurement.points, color, close = measurement.isArea)
        measurement.points.forEach { drawHandle(camera, it, color) }
    }

    val anchor = measurement.points.lastOrNull() ?: return
    val (screenX, screenY) = camera.worldToScreen(anchor.first, anchor.second)
    drawLabel(measurement.formatted(decimals, unit), screenX, screenY - 18f, color)
}

private fun DrawScope.drawPath(
    camera: Camera,
    points: List<Pair<Double, Double>>,
    color: Color,
    close: Boolean,
) {
    if (points.size < 2) return

    val path = Path()
    points.forEachIndexed { index, point ->
        val (x, y) = camera.worldToScreen(point.first, point.second)
        if (index == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    if (close) path.close()

    drawPath(path, color, style = Stroke(width = 3f))
}

private fun DrawScope.drawHandle(
    camera: Camera,
    point: Pair<Double, Double>,
    color: Color,
    radius: Float = 6f,
) {
    val (x, y) = camera.worldToScreen(point.first, point.second)
    drawCircle(Color.White, radius + 2f, Offset(x, y))
    drawCircle(color, radius, Offset(x, y))
}

/**
 * Marca del punto al que se engancharía.
 *
 * Se dibuja una cruz grande además del círculo: el dedo tapa la zona, y sin una
 * referencia que sobresalga no hay forma de saber si el enganche cayó donde se
 * pretendía antes de levantarlo.
 */
private fun DrawScope.drawSnapMarker(camera: Camera, snap: SnapPreview) {
    val (x, y) = camera.worldToScreen(snap.x, snap.y)

    val arm = 26f
    drawLine(SnapColor, Offset(x - arm, y), Offset(x + arm, y), strokeWidth = 2f)
    drawLine(SnapColor, Offset(x, y - arm), Offset(x, y + arm), strokeWidth = 2f)
    drawCircle(SnapColor, 10f, Offset(x, y), style = Stroke(width = 3f))

    // La etiqueta va por encima del dedo, no debajo, para que no quede tapada.
    drawLabel(snap.type.label, x, y - 40f, SnapColor)
}

private fun DrawScope.drawLabel(text: String, x: Float, y: Float, color: Color) {
    if (text.isBlank()) return

    drawIntoCanvas { canvas ->
        val paint = android.graphics.Paint().apply {
            isAntiAlias = true
            textSize = 34f
            textAlign = android.graphics.Paint.Align.CENTER
        }

        // Trazo blanco por debajo: sobre un plano lleno de líneas, un texto
        // plano se vuelve ilegible.
        paint.style = android.graphics.Paint.Style.STROKE
        paint.strokeWidth = 5f
        paint.color = android.graphics.Color.WHITE
        canvas.nativeCanvas.drawText(text, x, y, paint)

        paint.style = android.graphics.Paint.Style.FILL
        paint.color = color.toArgb()
        canvas.nativeCanvas.drawText(text, x, y, paint)
    }
}
