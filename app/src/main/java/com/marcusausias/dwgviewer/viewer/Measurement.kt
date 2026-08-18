package com.marcusausias.dwgviewer.viewer

import com.marcusausias.dwgviewer.nativebridge.SnapType
import java.util.Locale

/** Herramienta de medición activa. */
enum class MeasureTool(val label: String, val hint: String) {
    NONE("Navegar", ""),
    DISTANCE("Distancia", "Marca dos puntos"),
    CHAIN("Polilínea", "Marca puntos seguidos; pulsa Cerrar para terminar"),
    AREA("Superficie", "Marca el contorno; pulsa Cerrar para terminar"),
    FOLLOW("Seguir", "Toca una línea del plano y se mide entera"),
    COUNT("Contar", "Toca un símbolo y se cuentan los iguales"),
}

/**
 * Una medición tomada.
 *
 * [value] va en unidades de dibujo, tal cual las guarda el DWG. No hay
 * conversión ni calibración: es lo que se decidió y lo que evita el error de
 * escala, que es el más difícil de detectar porque el número parece razonable.
 */
data class Measurement(
    val id: Long,
    val tool: MeasureTool,
    val points: List<Pair<Double, Double>>,
    val value: Double,
    val approximate: Boolean = false,
    val count: Int = 0,
    val symbol: String = "",
    val label: String = "",
) {
    val isArea: Boolean get() = tool == MeasureTool.AREA
    val isCount: Boolean get() = tool == MeasureTool.COUNT

    /** Texto que se muestra en la lista y sobre el plano. */
    fun formatted(decimals: Int = 3, unit: String = ""): String = when {
        isCount -> "$count × ${symbol.ifBlank { "símbolo" }}"
        else -> buildString {
            append(formatNumber(value, decimals))
            if (unit.isNotBlank()) append(" ").append(if (isArea) "$unit²" else unit)
            else if (isArea) append(" u²")
            // Una cifra que sale de geometría teselada no es exacta, y quien la
            // lee tiene derecho a saberlo antes de meterla en un presupuesto.
            if (approximate) append(" (aprox.)")
        }
    }

    companion object {
        fun formatNumber(value: Double, decimals: Int): String =
            String.format(Locale.getDefault(), "%,.${decimals}f", value)
    }
}

/**
 * Medición a medio tomar.
 *
 * Se mantiene aparte de las ya cerradas para poder deshacer punto a punto sin
 * tocar el historial.
 */
data class PendingMeasure(
    val tool: MeasureTool = MeasureTool.NONE,
    val points: List<Pair<Double, Double>> = emptyList(),
) {
    val isActive: Boolean get() = tool != MeasureTool.NONE

    /** Con dos puntos ya se puede cerrar una distancia; un área necesita tres. */
    val canFinish: Boolean
        get() = when (tool) {
            MeasureTool.DISTANCE -> points.size >= 2
            MeasureTool.CHAIN -> points.size >= 2
            MeasureTool.AREA -> points.size >= 3
            else -> false
        }

    /** Valor provisional, para poder ir viendo la medida mientras se marca. */
    fun preview(): Double = when (tool) {
        MeasureTool.AREA -> shoelaceArea(points)
        MeasureTool.DISTANCE, MeasureTool.CHAIN -> chainLength(points)
        else -> 0.0
    }
}

internal fun chainLength(points: List<Pair<Double, Double>>): Double {
    var total = 0.0
    for (i in 1 until points.size) {
        val dx = points[i].first - points[i - 1].first
        val dy = points[i].second - points[i - 1].second
        total += kotlin.math.sqrt(dx * dx + dy * dy)
    }
    return total
}

/**
 * Área por la fórmula de Gauss.
 *
 * Se calcula también en Kotlin, y no solo en el núcleo, para poder ir mostrando
 * la superficie mientras se marca el contorno sin cruzar el puente nativo en
 * cada toque.
 */
internal fun shoelaceArea(points: List<Pair<Double, Double>>): Double {
    if (points.size < 3) return 0.0
    var twice = 0.0
    for (i in points.indices) {
        val current = points[i]
        val next = points[(i + 1) % points.size]
        twice += current.first * next.second - next.first * current.second
    }
    return kotlin.math.abs(twice) / 2.0
}

/** Punto al que se engancharía el dedo ahora mismo. */
data class SnapPreview(
    val x: Double,
    val y: Double,
    val type: SnapType,
)
