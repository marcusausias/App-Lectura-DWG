package com.marcusausias.dwgviewer.viewer

import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChanged
import kotlin.math.abs

/**
 * Paneo, zoom y toque sobre el plano.
 *
 * La navegación está siempre activa: un dedo desplaza y dos hacen zoom, sin
 * cambiar de modo. Medir se activará con un botón, y entonces cada toque
 * colocará un punto sin perder el arrastre para moverse.
 */
fun Modifier.planGestures(
    onPan: (Float, Float) -> Unit,
    onZoom: (focusX: Float, focusY: Float, factor: Float) -> Unit,
    onTap: (Float, Float) -> Unit,
    onPreview: (Float, Float) -> Unit = { _, _ -> },
    onPreviewCancel: () -> Unit = {},
): Modifier = pointerInput(Unit) {
    awaitEachGesture {
        val first = awaitFirstDown(requireUnconsumed = false)

        // Mientras el dedo está apoyado y quieto se enseña dónde engancharía.
        // Es la única forma de saberlo antes de levantarlo, porque el propio
        // dedo tapa el punto.
        onPreview(first.position.x, first.position.y)
        var previewing = true

        var totalMovement = 0f
        var previousCentroid = first.position
        var previousSpread = 0f
        var pointerCount = 1

        while (true) {
            val event = awaitPointerEvent()
            val active = event.changes.filter { it.pressed }
            if (active.isEmpty()) break

            val centroid = active.fold(Offset.Zero) { acc, change -> acc + change.position } /
                active.size.toFloat()

            // Separación media al centroide: sirve igual con dos dedos que con
            // tres, y no se descuadra si uno se levanta a mitad del gesto.
            val spread = if (active.size > 1) {
                active.fold(0f) { acc, change -> acc + (change.position - centroid).getDistance() } /
                    active.size
            } else {
                0f
            }

            // Al cambiar el número de dedos, las referencias anteriores dejan
            // de valer. Sin este reinicio el plano pega un salto cada vez que
            // se levanta o se apoya un dedo.
            if (active.size != pointerCount) {
                pointerCount = active.size
                previousCentroid = centroid
                previousSpread = spread
                active.forEach { if (it.positionChanged()) it.consume() }
                continue
            }

            if (spread > 0f && previousSpread > 0f) {
                val factor = spread / previousSpread
                if (factor.isFinite() && factor > 0f && abs(factor - 1f) > 1e-4f) {
                    onZoom(centroid.x, centroid.y, factor)
                }
            }

            val dx = centroid.x - previousCentroid.x
            val dy = centroid.y - previousCentroid.y
            if (dx != 0f || dy != 0f) onPan(dx, dy)

            totalMovement += abs(dx) + abs(dy)

            if (previewing) {
                if (totalMovement > Camera.TAP_SLOP_PX || active.size > 1) {
                    // Ya no es un toque sino un gesto de mover o hacer zoom.
                    previewing = false
                    onPreviewCancel()
                } else {
                    onPreview(centroid.x, centroid.y)
                }
            }

            previousCentroid = centroid
            previousSpread = spread

            active.forEach { if (it.positionChanged()) it.consume() }
        }

        // El mismo umbral de 6 px que ya funcionaba en VISOR 3DS: sin él, el
        // temblor normal del dedo convierte cualquier intento de mover el plano
        // en un toque accidental.
        if (previewing) onPreviewCancel()

        if (totalMovement <= Camera.TAP_SLOP_PX) {
            onTap(first.position.x, first.position.y)
        }
    }
}
