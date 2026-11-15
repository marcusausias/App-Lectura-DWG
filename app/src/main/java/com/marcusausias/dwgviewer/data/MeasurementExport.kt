package com.marcusausias.dwgviewer.data

import com.marcusausias.dwgviewer.viewer.MeasureTool
import com.marcusausias.dwgviewer.viewer.Measurement
import java.util.Locale

/**
 * Vuelca las mediciones a CSV para llevarlas a la hoja de mediciones.
 *
 * El formato está pensado para que Excel en español lo abra con doble clic:
 * punto y coma como separador, coma decimal y una marca BOM al principio para
 * que respete los acentos. Con comas y puntos, Excel mete cada fila entera en
 * una sola celda o convierte 12.5 en una fecha.
 */
object MeasurementExport {

    private const val BOM = "\uFEFF"

    fun toCsv(
        planName: String,
        measurements: List<Measurement>,
        decimals: Int,
        unit: String,
    ): String = buildString {
        append(BOM)
        append("Plano;").append(escape(planName)).append("\r\n")
        append("Unidad del dibujo;").append(escape(unit.ifBlank { "sin indicar" })).append("\r\n")
        append("\r\n")
        append("N.º;Descripción;Tipo;Longitud;Superficie;Cantidad;Símbolo;Aproximada\r\n")

        measurements.forEachIndexed { index, measurement ->
            val length = when {
                measurement.isCount || measurement.isArea -> ""
                else -> number(measurement.value, decimals)
            }
            val area = when {
                measurement.isArea -> number(measurement.value, decimals)
                measurement.area > 0.0 -> number(measurement.area, decimals)
                else -> ""
            }
            append(index + 1).append(';')
            append(escape(measurement.label)).append(';')
            append(escape(measurement.tool.label)).append(';')
            append(length).append(';')
            append(area).append(';')
            append(if (measurement.isCount) measurement.count.toString() else "").append(';')
            append(escape(measurement.symbol)).append(';')
            append(if (measurement.approximate) "sí" else "")
            append("\r\n")
        }

        // Totales por tipo: es lo primero que se va a hacer con la hoja.
        val lengthTotal = measurements
            .filter { !it.isCount && !it.isArea }
            .sumOf { it.value }
        val areaTotal = measurements.sumOf {
            when {
                it.isArea -> it.value
                it.tool == MeasureTool.FOLLOW -> it.area
                else -> 0.0
            }
        }
        append("\r\n")
        append(";Total;;").append(number(lengthTotal, decimals)).append(';')
        append(number(areaTotal, decimals)).append(";;;\r\n")
    }

    /** Nombre de archivo propuesto al guardar. */
    fun fileNameFor(planName: String): String {
        val base = planName.substringBeforeLast('.').ifBlank { "plano" }
            .replace(Regex("[^\\p{L}\\p{N} ._-]"), "_")
        return "Mediciones $base.csv"
    }

    // Sin separador de miles: Excel lo interpretaría como texto.
    private fun number(value: Double, decimals: Int): String =
        String.format(Locale.ROOT, "%.${decimals}f", value).replace('.', ',')

    private fun escape(text: String): String {
        if (text.none { it == ';' || it == '"' || it == '\n' || it == '\r' }) return text
        return "\"" + text.replace("\"", "\"\"") + "\""
    }
}
