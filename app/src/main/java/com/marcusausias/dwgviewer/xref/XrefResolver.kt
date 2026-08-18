package com.marcusausias.dwgviewer.xref

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.File

/** Una referencia externa y dónde ha acabado localizándose. */
data class ResolvedXref(
    val blockName: String,
    val rawPath: String,
    val localFile: File?,
    val source: Source,
) {
    enum class Source { RELATIVE_PATH, FILE_NAME, SAVED_LINK, UNRESOLVED }

    val isResolved: Boolean get() = localFile != null
}

/**
 * Localiza los archivos de las referencias externas dentro de la carpeta del
 * proyecto.
 *
 * El orden de intentos no es casual. En los planos reales la ruta guardada es
 * casi siempre absoluta de una unidad de red que aquí no existe, así que la
 * búsqueda por nombre de archivo es la que resuelve la mayoría de los casos, no
 * la excepción.
 */
class XrefResolver(
    private val context: Context,
    private val store: XrefStore,
) {

    fun resolve(
        planKey: String,
        declarations: List<Pair<String, String>>,
        treeUri: Uri?,
    ): List<ResolvedXref> {
        val root = treeUri?.let { DocumentFile.fromTreeUri(context, it) }
        // El índice por nombre se construye una vez y sirve para todas las
        // xrefs: recorrer el árbol por cada una sería lentísimo en un proyecto
        // con muchas carpetas.
        val byName = root?.let { indexByName(it) } ?: emptyMap()

        return declarations.map { (blockName, rawPath) ->
            resolveOne(planKey, blockName, rawPath, root, byName)
        }
    }

    private fun resolveOne(
        planKey: String,
        blockName: String,
        rawPath: String,
        root: DocumentFile?,
        byName: Map<String, DocumentFile>,
    ): ResolvedXref {
        // 1. Un enlace que el usuario ya hizo a mano manda sobre todo lo demás:
        //    si tuvo que corregirlo una vez, no se le vuelve a preguntar.
        store.linkFor(planKey, blockName)?.let { saved ->
            copyToCache(Uri.parse(saved), blockName)?.let { file ->
                return ResolvedXref(blockName, rawPath, file, ResolvedXref.Source.SAVED_LINK)
            }
        }

        // 2. Ruta relativa desde la carpeta del proyecto, que es como debería
        //    funcionar cuando el proyecto está bien montado.
        if (root != null && XrefPath.isRelative(rawPath)) {
            followSegments(root, XrefPath.relativeSegments(rawPath))?.let { document ->
                copyToCache(document.uri, blockName)?.let { file ->
                    return ResolvedXref(
                        blockName, rawPath, file, ResolvedXref.Source.RELATIVE_PATH,
                    )
                }
            }
        }

        // 3. Por nombre de archivo en cualquier punto del árbol.
        byName[XrefPath.baseName(rawPath).lowercase()]?.let { document ->
            copyToCache(document.uri, blockName)?.let { file ->
                return ResolvedXref(blockName, rawPath, file, ResolvedXref.Source.FILE_NAME)
            }
        }

        return ResolvedXref(blockName, rawPath, null, ResolvedXref.Source.UNRESOLVED)
    }

    /** Guarda un enlace hecho a mano para no volver a preguntar por esta xref. */
    fun rememberLink(planKey: String, blockName: String, documentUri: Uri) {
        store.saveLink(planKey, blockName, documentUri.toString())
    }

    private fun followSegments(root: DocumentFile, segments: List<String>): DocumentFile? {
        if (segments.isEmpty()) return null

        var current: DocumentFile = root
        for ((index, segment) in segments.withIndex()) {
            val child = current.listFiles().firstOrNull {
                it.name.equals(segment, ignoreCase = true)
            } ?: return null
            if (index == segments.lastIndex) return child.takeIf { it.isFile }
            if (!child.isDirectory) return null
            current = child
        }
        return null
    }

    /**
     * Índice de todos los DWG del árbol por nombre sin extensión.
     *
     * La profundidad está acotada: un árbol muy hondo, o con enlaces circulares,
     * podría dejar la app colgada recorriendo carpetas.
     */
    private fun indexByName(root: DocumentFile, maxDepth: Int = 6): Map<String, DocumentFile> {
        val found = HashMap<String, DocumentFile>()
        val pending = ArrayDeque<Pair<DocumentFile, Int>>()
        pending.add(root to 0)

        while (pending.isNotEmpty()) {
            val (directory, depth) = pending.removeFirst()
            if (depth > maxDepth) continue

            for (child in directory.listFiles()) {
                if (child.isDirectory) {
                    pending.add(child to depth + 1)
                    continue
                }
                val name = child.name ?: continue
                if (!name.endsWith(".dwg", ignoreCase = true)) continue
                // El primero que aparece se queda: si hay dos con el mismo
                // nombre en carpetas distintas, el usuario tendrá que enlazar
                // a mano el que quiera.
                found.putIfAbsent(name.substringBeforeLast('.').lowercase(), child)
            }
        }
        return found
    }

    /**
     * LibreDWG abre por ruta del sistema, y una URI del árbol no lo es, así que
     * el archivo externo tiene que copiarse a la caché antes de leerlo.
     */
    private fun copyToCache(uri: Uri, blockName: String): File? {
        val safeName = blockName.replace(Regex("[^A-Za-z0-9._-]"), "_").take(60)
        val destination = File(context.cacheDir, "xref-$safeName.dwg")
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

    fun clearCache() {
        context.cacheDir.listFiles()
            ?.filter { it.name.startsWith("xref-") }
            ?.forEach { it.delete() }
    }
}
