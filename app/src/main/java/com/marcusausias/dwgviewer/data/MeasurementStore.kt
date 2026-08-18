package com.marcusausias.dwgviewer.data

import android.content.Context
import android.util.Log
import com.marcusausias.dwgviewer.viewer.MeasureTool
import com.marcusausias.dwgviewer.viewer.Measurement
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Guarda las mediciones de cada plano.
 *
 * Van a `filesDir` y no a `cacheDir` a propósito. La caché de escenas procesadas
 * sí puede vivir en `cacheDir` porque se regenera sola, pero una medición es
 * trabajo del usuario: Android vacía `cacheDir` sin avisar cuando queda poco
 * espacio, que es justo la situación de un móvil lleno de fotos a media obra.
 *
 * El formato es JSON con `org.json`, que viene en Android. Para una lista de
 * unas decenas de entradas por plano no compensa arrastrar una base de datos.
 */
class MeasurementStore(context: Context) {

    private val directory = File(context.filesDir, "measurements")

    fun load(planKey: String): List<Measurement> {
        val file = fileFor(planKey)
        if (!file.exists()) return emptyList()

        return try {
            val array = JSONArray(file.readText())
            (0 until array.length()).mapNotNull { index ->
                parse(array.optJSONObject(index))
            }
        } catch (error: Exception) {
            // Un JSON corrupto no puede impedir abrir el plano. Se pierden las
            // mediciones de ese archivo, que es malo, pero no tanto como que la
            // app no arranque con ese plano nunca más.
            Log.w(TAG, "No se pudieron leer las mediciones de $planKey", error)
            emptyList()
        }
    }

    fun save(planKey: String, measurements: List<Measurement>) {
        try {
            if (measurements.isEmpty()) {
                fileFor(planKey).delete()
                return
            }
            directory.mkdirs()

            val array = JSONArray()
            measurements.forEach { array.put(serialize(it)) }

            // Se escribe a un temporal y se renombra: si la app muere a mitad,
            // el archivo anterior sigue intacto en vez de quedar a medias.
            val temporary = File(directory, "${safeName(planKey)}.json.tmp")
            temporary.writeText(array.toString())
            temporary.renameTo(fileFor(planKey))
        } catch (error: Exception) {
            Log.w(TAG, "No se pudieron guardar las mediciones de $planKey", error)
        }
    }

    fun clear(planKey: String) {
        fileFor(planKey).delete()
    }

    private fun serialize(measurement: Measurement): JSONObject {
        val points = JSONArray()
        measurement.points.forEach { (x, y) ->
            points.put(x)
            points.put(y)
        }

        return JSONObject().apply {
            put("id", measurement.id)
            // El nombre del enum, no su posición: reordenar las herramientas no
            // puede convertir una superficie guardada en un recuento.
            put("tool", measurement.tool.name)
            put("points", points)
            put("value", measurement.value)
            put("approximate", measurement.approximate)
            put("count", measurement.count)
            put("symbol", measurement.symbol)
            put("label", measurement.label)
        }
    }

    private fun parse(json: JSONObject?): Measurement? {
        if (json == null) return null

        val tool = MeasureTool.entries.firstOrNull { it.name == json.optString("tool") }
            ?: return null

        val flat = json.optJSONArray("points") ?: JSONArray()
        val points = ArrayList<Pair<Double, Double>>(flat.length() / 2)
        var index = 0
        while (index + 1 < flat.length()) {
            points.add(flat.optDouble(index) to flat.optDouble(index + 1))
            index += 2
        }

        return Measurement(
            id = json.optLong("id"),
            tool = tool,
            points = points,
            value = json.optDouble("value", 0.0),
            approximate = json.optBoolean("approximate", false),
            count = json.optInt("count", 0),
            symbol = json.optString("symbol"),
            label = json.optString("label"),
        )
    }

    private fun fileFor(planKey: String) = File(directory, "${safeName(planKey)}.json")

    private fun safeName(planKey: String) =
        planKey.replace(Regex("[^A-Za-z0-9._-]"), "_").take(120)

    private companion object {
        const val TAG = "MeasurementStore"
    }
}
