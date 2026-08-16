package com.marcusausias.dwgviewer.nativebridge

/**
 * Referencia externa declarada dentro de un dibujo.
 *
 * [path] viene tal cual la escribió AutoCAD, en formato Windows y a menudo como
 * ruta absoluta de una unidad de red ("N:\\2017\\Obra\\...\\base.dwg"). Ninguna
 * de esas rutas existe en Android, así que resolverla siempre implica buscar
 * dentro de la carpeta del proyecto: primero por ruta relativa y, si falla, por
 * nombre de archivo.
 */
data class XrefRef(
    val name: String,
    val path: String,
    val isOverlay: Boolean,
)

/**
 * Resumen de un DWG, suficiente para decidir qué hacer con él antes de
 * procesarlo entero.
 *
 * [libredwgError] es un mapa de bits, no un código único. Un valor distinto de
 * cero es normal y no impide leer el archivo: los propios ejemplos de AutoCAD
 * 2018 devuelven 68 (clase no soportada + valor fuera de rango). Solo a partir
 * de 128 hay un fallo real, y en ese caso [opened] ya viene a false.
 */
data class DocumentSummary(
    val opened: Boolean,
    val versionCode: String,
    val versionName: String,
    val libredwgError: Int,
    val objectCount: Long,
    val entityCount: Long,
    val layerCount: Long,
    val blockCount: Long,
    val xrefs: Array<XrefRef>,
    val errorMessage: String,
) {
    // Array rompe equals/hashCode generados: se implementan a mano para que
    // comparar dos resúmenes no dé falsos negativos.
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is DocumentSummary) return false
        return opened == other.opened &&
            versionCode == other.versionCode &&
            versionName == other.versionName &&
            libredwgError == other.libredwgError &&
            objectCount == other.objectCount &&
            entityCount == other.entityCount &&
            layerCount == other.layerCount &&
            blockCount == other.blockCount &&
            errorMessage == other.errorMessage &&
            xrefs.contentEquals(other.xrefs)
    }

    override fun hashCode(): Int {
        var result = opened.hashCode()
        result = 31 * result + versionCode.hashCode()
        result = 31 * result + objectCount.hashCode()
        result = 31 * result + xrefs.contentHashCode()
        return result
    }
}

/**
 * Fachada sobre el núcleo nativo.
 *
 * [summarize] necesita una ruta real del sistema de archivos. LibreDWG no
 * entiende las URIs del Storage Access Framework, así que el documento debe
 * copiarse antes a la caché de la app.
 */
object DwgNative {

    init {
        System.loadLibrary("dwgjni")
    }

    external fun summarize(path: String): DocumentSummary?
}
