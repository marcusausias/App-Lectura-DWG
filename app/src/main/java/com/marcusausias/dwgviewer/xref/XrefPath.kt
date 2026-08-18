package com.marcusausias.dwgviewer.xref

/**
 * Traducción de las rutas de referencia externa que guarda AutoCAD.
 *
 * Vienen en formato Windows y muy a menudo son absolutas de una unidad de red
 * (`N:\2017\Obra\...\base.dwg`). Ninguna de esas rutas existe en un móvil, así
 * que lo único aprovechable es su forma: los tramos relativos y, sobre todo, el
 * nombre del archivo.
 */
object XrefPath {

    /** Nombre del archivo, sin carpetas. Es lo que casi siempre acaba sirviendo. */
    fun fileName(rawPath: String): String {
        val normalized = rawPath.replace('\\', '/').trimEnd('/')
        return normalized.substringAfterLast('/').ifBlank { normalized }
    }

    /** Nombre sin extensión, para comparar cuando el DWG se guardó sin ella. */
    fun baseName(rawPath: String): String = fileName(rawPath).substringBeforeLast('.')

    /**
     * Tramos relativos de la ruta, listos para recorrer una carpeta.
     *
     * Descarta la unidad (`N:`), los tramos vacíos y los `.`, y aplica los `..`
     * quitando el tramo anterior. Devuelve lista vacía si la ruta era absoluta
     * sin parte relativa aprovechable.
     */
    fun relativeSegments(rawPath: String): List<String> {
        val normalized = rawPath.replace('\\', '/')

        // Una ruta absoluta de Windows o de red no se puede seguir tal cual;
        // solo interesa lo que vaya después.
        val withoutRoot = when {
            normalized.length > 1 && normalized[1] == ':' -> normalized.substring(2)
            normalized.startsWith("//") -> normalized.substring(2)
            else -> normalized
        }

        val segments = mutableListOf<String>()
        for (part in withoutRoot.split('/')) {
            when {
                part.isBlank() || part == "." -> Unit
                part == ".." -> if (segments.isNotEmpty()) segments.removeAt(segments.lastIndex)
                else -> segments.add(part)
            }
        }
        return segments
    }

    /** Cierto si la ruta era relativa, y por tanto se puede seguir desde la carpeta. */
    fun isRelative(rawPath: String): Boolean {
        val normalized = rawPath.replace('\\', '/')
        return !(normalized.length > 1 && normalized[1] == ':') &&
            !normalized.startsWith("/") &&
            !normalized.startsWith("//")
    }

    /** Compara dos nombres de archivo ignorando mayúsculas y la extensión. */
    fun sameFile(a: String, b: String): Boolean =
        baseName(a).equals(baseName(b), ignoreCase = true)
}
