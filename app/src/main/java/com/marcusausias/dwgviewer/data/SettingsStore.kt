package com.marcusausias.dwgviewer.data

import android.content.Context
import androidx.core.content.edit

/**
 * Preferencias que deben sobrevivir al cierre de la app: cómo se presentan las
 * medidas y cuál fue el último plano abierto.
 */
class SettingsStore(context: Context) {

    private val preferences = context.getSharedPreferences("settings", Context.MODE_PRIVATE)

    var decimals: Int
        get() = preferences.getInt(KEY_DECIMALS, 3)
        set(value) = preferences.edit { putInt(KEY_DECIMALS, value) }

    /**
     * Etiqueta de la unidad del dibujo ("m", "cm", "mm" o vacía).
     *
     * Es solo una etiqueta: las medidas se dan siempre en unidades de dibujo,
     * sin convertir. Sirve para que el número lleve detrás lo que el usuario
     * sabe que significa.
     */
    var unit: String
        get() = preferences.getString(KEY_UNIT, "").orEmpty()
        set(value) = preferences.edit { putString(KEY_UNIT, value) }

    var darkBackground: Boolean
        get() = preferences.getBoolean(KEY_DARK, false)
        set(value) = preferences.edit { putBoolean(KEY_DARK, value) }

    var lastPlanUri: String?
        get() = preferences.getString(KEY_LAST_URI, null)
        set(value) = preferences.edit { putString(KEY_LAST_URI, value) }

    var lastPlanName: String?
        get() = preferences.getString(KEY_LAST_NAME, null)
        set(value) = preferences.edit { putString(KEY_LAST_NAME, value) }

    private companion object {
        const val KEY_DECIMALS = "decimals"
        const val KEY_UNIT = "unit"
        const val KEY_DARK = "dark-background"
        const val KEY_LAST_URI = "last-plan-uri"
        const val KEY_LAST_NAME = "last-plan-name"
    }
}
