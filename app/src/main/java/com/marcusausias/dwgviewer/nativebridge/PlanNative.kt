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

/** Tipo de punto notable al que se ha enganchado el dedo. */
enum class SnapType {
    NONE, ENDPOINT, INTERSECTION, MIDPOINT, PERPENDICULAR, ON_EDGE;

    val label: String
        get() = when (this) {
            ENDPOINT -> "extremo"
            INTERSECTION -> "intersección"
            MIDPOINT -> "punto medio"
            PERPENDICULAR -> "perpendicular"
            ON_EDGE -> "sobre línea"
            NONE -> ""
        }

    companion object {
        fun fromCode(code: Int): SnapType = entries.getOrElse(code) { NONE }
    }
}

/** Resultado de enganchar: dónde y a qué. */
data class SnapHit(
    val type: SnapType,
    val x: Double,
    val y: Double,
    val entityIndex: Int,
)

/** Longitud de una entidad del plano, con su recorrido para resaltarla. */
data class EntityMeasure(
    val length: Double,
    val approximate: Boolean,
    val points: List<Pair<Double, Double>>,
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

    /**
     * Engancha al punto notable más cercano.
     *
     * [radius] va en unidades de dibujo; se calcula desde un radio en píxeles
     * para que enganchar se sienta igual de fácil a cualquier zoom.
     *
     * [reference] es el punto anterior de la medición en curso, necesario para
     * la perpendicular.
     */
    fun snapAt(
        x: Double,
        y: Double,
        radius: Double,
        reference: Pair<Double, Double>? = null,
        hiddenLayers: IntArray = IntArray(0),
    ): SnapHit? {
        val raw = PlanNative.snapAt(
            handle, x, y, radius,
            reference?.first ?: 0.0, reference?.second ?: 0.0, reference != null,
            hiddenLayers,
        ) ?: return null

        return SnapHit(
            type = SnapType.fromCode(raw[0].toInt()),
            x = raw[1],
            y = raw[2],
            entityIndex = raw[3].toInt(),
        )
    }

    /** Entidad tocada, o null si no hay ninguna dentro del radio. */
    fun pickEntity(
        x: Double,
        y: Double,
        radius: Double,
        hiddenLayers: IntArray = IntArray(0),
    ): Int? = PlanNative.pickEntity(handle, x, y, radius, hiddenLayers).takeIf { it >= 0 }

    /** Longitud de una entidad siguiéndola entera, con los arcos exactos. */
    fun measureEntity(entityIndex: Int): EntityMeasure? {
        val raw = PlanNative.measureEntity(handle, entityIndex) ?: return null
        if (raw.size < 3) return null

        val count = raw[2].toInt()
        val points = ArrayList<Pair<Double, Double>>(count)
        for (i in 0 until count) {
            points.add(raw[3 + i * 2] to raw[4 + i * 2])
        }
        return EntityMeasure(length = raw[0], approximate = raw[1] != 0.0, points = points)
    }

    /** Nombre del símbolo del que procede la entidad, si viene de alguno. */
    fun symbolName(entityIndex: Int): String? = PlanNative.countSymbolName(handle, entityIndex)

    /** Cuántas veces está colocado ese símbolo en el plano. */
    fun symbolInstances(entityIndex: Int): Int =
        PlanNative.countSymbolInstances(handle, entityIndex)

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
            variant: Long = 0L,
            xrefs: Map<String, String> = emptyMap(),
        ): NativePlan? {
            val names = xrefs.keys.toTypedArray()
            val paths = names.map { xrefs.getValue(it) }.toTypedArray()
            val handle = PlanNative.openPlan(
                dwgPath, cachePath, sourceSize, sourceModified, variant, names, paths,
            )
            return if (handle == 0L) null else NativePlan(handle)
        }

        /**
         * Referencias externas que declara el archivo, sin abrirlo del todo.
         *
         * Devuelve pares (nombre de bloque, ruta original tal cual la guardó
         * AutoCAD).
         */
        fun inspectXrefs(dwgPath: String): List<Pair<String, String>> {
            val flat = PlanNative.inspectXrefs(dwgPath) ?: return emptyList()
            return (flat.indices step 2)
                .filter { it + 1 < flat.size }
                .map { flat[it] to flat[it + 1] }
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
        variant: Long,
        xrefNames: Array<String>,
        xrefPaths: Array<String>,
    ): Long

    external fun inspectXrefs(dwgPath: String): Array<String>?

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

    external fun snapAt(
        handle: Long,
        x: Double,
        y: Double,
        radius: Double,
        referenceX: Double,
        referenceY: Double,
        hasReference: Boolean,
        hiddenLayers: IntArray,
    ): DoubleArray?

    external fun pickEntity(
        handle: Long,
        x: Double,
        y: Double,
        radius: Double,
        hiddenLayers: IntArray,
    ): Int

    external fun measureEntity(handle: Long, entityIndex: Int): DoubleArray?
    external fun countSymbolName(handle: Long, entityIndex: Int): String?
    external fun countSymbolInstances(handle: Long, entityIndex: Int): Int
}
