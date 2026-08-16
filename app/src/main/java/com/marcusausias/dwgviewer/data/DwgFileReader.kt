package com.marcusausias.dwgviewer.data

import android.content.Context
import android.net.Uri
import java.io.File

/**
 * Prepara un documento del Storage Access Framework para que el núcleo nativo
 * pueda leerlo.
 *
 * LibreDWG abre archivos por ruta del sistema, y las URIs de Android no lo son:
 * un `content://` no se puede pasar a `fopen`. La única vía fiable es copiar el
 * documento a la caché de la app. Con planos de 4 a 8 MB el coste es
 * despreciable frente al tiempo de parseo.
 */
class DwgFileReader(private val context: Context) {

    fun copyToCache(uri: Uri): File? {
        val destination = File(context.cacheDir, "dwg-${System.currentTimeMillis()}.dwg")
        return try {
            context.contentResolver.openInputStream(uri)?.use { input ->
                destination.outputStream().use { output -> input.copyTo(output) }
            } ?: return null
            destination
        } catch (error: Exception) {
            destination.delete()
            null
        }
    }

    /** Borra las copias que hayan quedado de sesiones anteriores. */
    fun clearCache() {
        context.cacheDir.listFiles()
            ?.filter { it.name.startsWith("dwg-") }
            ?.forEach { it.delete() }
    }
}
