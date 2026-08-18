package com.marcusausias.dwgviewer.viewer

import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/**
 * Encuadre actual del plano.
 *
 * El centro se guarda en `double` a propósito. Los planos de obra tienen
 * coordenadas de cinco o seis cifras enteras, y con coordenadas UTM llegan a
 * siete: en `float` el paso mínimo a esa magnitud ya es de centímetros, así que
 * el paneo avanzaría a saltos en vez de suavemente.
 *
 * [scale] son píxeles por unidad de dibujo.
 */
data class Camera(
    val centerX: Double = 0.0,
    val centerY: Double = 0.0,
    val scale: Double = 1.0,
    val viewportWidth: Int = 0,
    val viewportHeight: Int = 0,
) {

    val isUsable: Boolean get() = viewportWidth > 0 && viewportHeight > 0 && scale > 0.0

    fun resize(width: Int, height: Int): Camera =
        copy(viewportWidth = width, viewportHeight = height)

    /** Encuadra el plano entero dejando un pequeño margen. */
    fun fitTo(bounds: DoubleArray, margin: Double = 0.05): Camera {
        if (viewportWidth <= 0 || viewportHeight <= 0) return this

        val width = bounds[2] - bounds[0]
        val height = bounds[3] - bounds[1]
        val centerX = (bounds[0] + bounds[2]) / 2.0
        val centerY = (bounds[1] + bounds[3]) / 2.0

        // Un plano de extensión nula (una sola entidad, o todo en un punto) no
        // permite deducir escala: se deja una por defecto en vez de dividir
        // por cero y perder el plano de vista.
        if (width <= 0.0 || height <= 0.0) {
            return copy(centerX = centerX, centerY = centerY, scale = 1.0)
        }

        val usable = 1.0 - margin * 2.0
        val fitted = min(viewportWidth / width, viewportHeight / height) * usable
        return copy(centerX = centerX, centerY = centerY, scale = fitted)
    }

    /**
     * Desplaza la vista según un arrastre en píxeles.
     *
     * El eje Y se invierte porque en pantalla crece hacia abajo y en el dibujo
     * hacia arriba.
     */
    fun panByPixels(dx: Float, dy: Float): Camera =
        copy(centerX = centerX - dx / scale, centerY = centerY + dy / scale)

    /**
     * Aplica zoom manteniendo fijo el punto de pantalla indicado.
     *
     * Anclarlo al punto que el usuario está tocando —y no al centro de la
     * pantalla— es lo que hace que al pellizcar te acerques a lo que estás
     * mirando en vez de perderlo de vista.
     */
    fun zoomAround(focusX: Float, focusY: Float, factor: Double): Camera {
        if (!isUsable) return this

        val target = clampScale(scale * factor)
        if (target == scale) return this

        // Se mantiene invariante la coordenada de dibujo bajo el dedo.
        val (worldX, worldY) = screenToWorld(focusX, focusY)
        val offsetX = focusX - viewportWidth / 2.0
        val offsetY = focusY - viewportHeight / 2.0

        return copy(
            centerX = worldX - offsetX / target,
            centerY = worldY + offsetY / target,
            scale = target,
        )
    }

    fun screenToWorld(x: Float, y: Float): Pair<Double, Double> {
        val worldX = centerX + (x - viewportWidth / 2.0) / scale
        val worldY = centerY - (y - viewportHeight / 2.0) / scale
        return worldX to worldY
    }

    fun worldToScreen(x: Double, y: Double): Pair<Float, Float> {
        val screenX = (x - centerX) * scale + viewportWidth / 2.0
        val screenY = (centerY - y) * scale + viewportHeight / 2.0
        return screenX.toFloat() to screenY.toFloat()
    }

    /** Área de dibujo que se está viendo: minX, minY, maxX, maxY. */
    fun visibleBounds(): DoubleArray {
        val halfWidth = viewportWidth / 2.0 / scale
        val halfHeight = viewportHeight / 2.0 / scale
        return doubleArrayOf(
            centerX - halfWidth,
            centerY - halfHeight,
            centerX + halfWidth,
            centerY + halfHeight,
        )
    }

    /** Cuántas unidades de dibujo mide un píxel. Sirve para fijar tolerancias. */
    fun unitsPerPixel(): Double = if (scale > 0.0) 1.0 / scale else 0.0

    /**
     * Matriz que lleva los vértices del búfer a coordenadas de pantalla.
     *
     * Recibe el origen que se restó a los vértices al prepararlos. La resta
     * entre el origen y el centro se hace aquí, en `double`, y a la matriz solo
     * llegan números pequeños. Ese es el motivo de todo el montaje: si la
     * traslación se calculase en `float`, la geometría temblaría al acercarse.
     */
    fun toMatrix(originX: Double, originY: Double): FloatArray {
        val sx = 2.0 * scale / viewportWidth.coerceAtLeast(1)
        val sy = 2.0 * scale / viewportHeight.coerceAtLeast(1)
        val tx = sx * (originX - centerX)
        val ty = sy * (originY - centerY)

        // Column-major, como espera OpenGL.
        return floatArrayOf(
            sx.toFloat(), 0f, 0f, 0f,
            0f, sy.toFloat(), 0f, 0f,
            0f, 0f, 1f, 0f,
            tx.toFloat(), ty.toFloat(), 0f, 1f,
        )
    }

    private fun clampScale(value: Double): Double = max(MIN_SCALE, min(MAX_SCALE, value))

    companion object {
        // Límites amplios: los planos van desde milímetros hasta coordenadas
        // geográficas, así que acotar por unidades de dibujo no serviría. Solo
        // se evita llegar a valores que degeneren la matriz.
        private const val MIN_SCALE = 1e-9
        private const val MAX_SCALE = 1e9

        /** Umbral para distinguir un toque de un arrastre, en píxeles. */
        const val TAP_SLOP_PX = 6f

        fun sameView(a: Camera, b: Camera): Boolean =
            a.viewportWidth == b.viewportWidth &&
                a.viewportHeight == b.viewportHeight &&
                abs(a.centerX - b.centerX) < 1e-12 &&
                abs(a.centerY - b.centerY) < 1e-12 &&
                abs(a.scale - b.scale) < 1e-12
    }
}
