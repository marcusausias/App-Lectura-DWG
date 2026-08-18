package com.marcusausias.dwgviewer.xref

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit

/**
 * Memoria de los enlaces de referencias externas.
 *
 * Es el requisito que distingue a esta app: una vez que se ha localizado dónde
 * está una xref, no se vuelve a preguntar. Se guarda tanto la carpeta raíz del
 * proyecto —cuyo permiso Android conserva entre sesiones— como cada enlace
 * hecho a mano, que es lo que hay que recordar cuando la ruta original no
 * lleva a ninguna parte.
 */
class XrefStore(context: Context) {

    private val preferences: SharedPreferences =
        context.getSharedPreferences("xref-links", Context.MODE_PRIVATE)

    /** Carpeta del proyecto sobre la que se concedió acceso. */
    var projectTreeUri: String?
        get() = preferences.getString(KEY_TREE, null)
        set(value) = preferences.edit { putString(KEY_TREE, value) }

    /**
     * Enlace guardado para una xref de un plano concreto.
     *
     * La clave lleva el plano además del bloque porque dos planos distintos
     * pueden referenciar bloques con el mismo nombre y archivos diferentes.
     */
    fun linkFor(planKey: String, blockName: String): String? =
        preferences.getString(linkKey(planKey, blockName), null)

    fun saveLink(planKey: String, blockName: String, documentUri: String) {
        preferences.edit { putString(linkKey(planKey, blockName), documentUri) }
    }

    fun forgetLink(planKey: String, blockName: String) {
        preferences.edit { remove(linkKey(planKey, blockName)) }
    }

    fun forgetAll() = preferences.edit { clear() }

    private fun linkKey(planKey: String, blockName: String) = "link:$planKey:$blockName"

    private companion object {
        const val KEY_TREE = "project-tree"
    }
}
