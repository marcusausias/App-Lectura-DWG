package com.marcusausias.dwgviewer.nativebridge

import java.nio.ByteBuffer

/** Un texto del plano, con su posición y tamaño en unidades de dibujo. */
data class PlanText(
    val content: String,
    val x: Double,
    val y: Double,
    val height: Double,
    val rotation: Double,
    val layerId: Int,
)

/** Un tramo del buffer de vértices correspondiente a una capa. */
data class RenderBatch(
    val layerId: Int,
    val firstVertex: Int,
    val vertexCount: Int,
)

/**
 * Plano abierto en el lado nativo.
 *
 * La escena vive en C++ y aquí solo se guarda un handle. Es importante para el
 * consumo de memoria: un plano son varios megas de vértices que nunca llegan a
 * copiarse al montón de Java.
 *
 * Hay que llamar a [close] al terminar. Mientras no se haga, la memoria sigue
 * reservada; y una vez hecho, el búfer de vértices que se haya entregado deja
 * de ser válido, así que no puede cerrarse mientras la vista siga dibujando.
 */
class NativePlan private constructor(private var handle: Long) : AutoCloseable {

    val isOpen: Boolean get() = handle != 0L

    /**
     * Vértices como pares (x, y) en float, ya relativos a [origin].
     *
     * Es un búfer directo: apunta a la memoria de C++, no a una copia. Se puede
     * pasar a OpenGL tal cual.
     */
    fun vertexBuffer(): ByteBuffer? = PlanNative.getVertexBuffer(handle)

    fun batches(): List<RenderBatch> {
        val flat = PlanNative.getBatches(handle) ?: return emptyList()
        return (flat.indices step 3).map { i ->
            RenderBatch(flat[i], flat[i + 1], flat[i + 2])
        }
    }

    fun layers(): List<String> = PlanNative.getLayers(handle)?.toList() ?: emptyList()

    fun entityCount(): Int = PlanNative.getEntityCount(handle)

    /** minX, minY, maxX, maxY del plano en coordenadas de dibujo. */
    fun bounds(): DoubleArray? = PlanNative.getBounds(handle)?.copyOfRange(0, 4)

    /** Punto que se ha restado a todos los vértices para no perder precisión. */
    fun origin(): DoubleArray? = PlanNative.getBounds(handle)?.copyOfRange(4, 6)

    /**
     * Textos dentro del área indicada cuya altura llegue a [minHeight].
     *
     * [limit] acota cuántos se devuelven: al alejarse mucho podrían entrar
     * miles y ninguno sería legible.
     */
    fun visibleTexts(
        minX: Double,
        minY: Double,
        maxX: Double,
        maxY: Double,
        minHeight: Double,
        limit: Int = 400,
    ): List<PlanText> =
        PlanNative.getVisibleTexts(handle, minX, minY, maxX, maxY, minHeight, limit)
            ?.toList() ?: emptyList()

    override fun close() {
        if (handle != 0L) {
            PlanNative.closePlan(handle)
            handle = 0L
        }
    }

    companion object {
        /**
         * Abre un plano, usando la caché si sigue siendo válida.
         *
         * [sourceSize] y [sourceModified] son del DWG original: si no coinciden
         * con los que guardó la caché, se reprocesa. Devuelve null si el archivo
         * no se puede leer.
         */
        fun open(
            dwgPath: String,
            cachePath: String,
            sourceSize: Long,
            sourceModified: Long,
        ): NativePlan? {
            val handle = PlanNative.openPlan(dwgPath, cachePath, sourceSize, sourceModified)
            return if (handle == 0L) null else NativePlan(handle)
        }
    }
}

/**
 * Funciones nativas en crudo. Se usan a través de [NativePlan].
 *
 * Es público a propósito: Kotlin altera el nombre de las funciones `internal`
 * al compilar a la JVM, y JNI busca el símbolo por el nombre del método, así que
 * declararlo `internal` haría que no se encontrasen las funciones nativas.
 */
object PlanNative {

    init {
        System.loadLibrary("dwgjni")
    }

    external fun openPlan(
        dwgPath: String,
        cachePath: String,
        sourceSize: Long,
        sourceModified: Long,
    ): Long

    external fun closePlan(handle: Long)
    external fun getVertexBuffer(handle: Long): ByteBuffer?
    external fun getBatches(handle: Long): IntArray?
    external fun getLayers(handle: Long): Array<String>?
    external fun getBounds(handle: Long): DoubleArray?
    external fun getEntityCount(handle: Long): Int
    external fun getVisibleTexts(
        handle: Long,
        minX: Double,
        minY: Double,
        maxX: Double,
        maxY: Double,
        minHeight: Double,
        limit: Int,
    ): Array<PlanText>?
}
