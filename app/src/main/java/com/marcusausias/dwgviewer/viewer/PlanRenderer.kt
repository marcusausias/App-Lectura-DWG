package com.marcusausias.dwgviewer.viewer

import android.opengl.GLES30
import android.opengl.GLSurfaceView
import com.marcusausias.dwgviewer.nativebridge.RenderBatch
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * Dibuja el plano con OpenGL ES.
 *
 * Todos los vértices se suben una sola vez al abrir el plano. A partir de ahí,
 * mover el plano es cambiar una matriz: no se toca memoria al hacer paneo ni
 * zoom, que es lo que permite que vaya fluido con cientos de miles de segmentos.
 *
 * Cada capa ocupa un tramo contiguo del búfer, así que apagarla es saltarse una
 * llamada de dibujo.
 */
class PlanRenderer : GLSurfaceView.Renderer {

    private class Upload(
        val vertices: ByteBuffer,
        val batches: List<RenderBatch>,
        val originX: Double,
        val originY: Double,
    )

    @Volatile private var pending: Upload? = null
    // Se conserva el plano subido, no solo el encargo pendiente: al recrearse
    // el contexto de GL —rotar la pantalla, volver de segundo plano— la GPU
    // pierde sus búferes y hay que volver a subirlos, o la pantalla se queda
    // en blanco con el plano aparentemente cargado.
    @Volatile private var current: Upload? = null
    @Volatile private var clearRequested = false
    @Volatile private var camera: Camera = Camera()
    @Volatile private var hiddenLayers: Set<Int> = emptySet()
    @Volatile private var background: Int = 0
    @Volatile private var lineWidth: Float = 1f

    private var program = 0
    private var matrixLocation = 0
    private var colorLocation = 0
    private var positionLocation = 0
    private var vertexBuffer = 0
    private var vertexArray = 0

    private var batches: List<RenderBatch> = emptyList()
    private var originX = 0.0
    private var originY = 0.0
    private var uploaded = false

    /**
     * Entrega el plano. La subida real ocurre en el hilo de GL.
     *
     * Los vértices se copian a un búfer propio. El que llega apunta a memoria
     * del lado nativo, que se libera al cerrar el plano; y este objeto necesita
     * los datos después de eso, cada vez que Android recrea el contexto de GL.
     * Sin la copia, volver de segundo plano tras abrir otro plano leería
     * memoria ya liberada. Son unos pocos megas durante la vida de la vista.
     */
    fun setPlan(
        vertices: ByteBuffer,
        batches: List<RenderBatch>,
        originX: Double,
        originY: Double,
    ) {
        val source = vertices.duplicate()
        source.position(0)
        val copy = ByteBuffer.allocateDirect(source.remaining()).order(ByteOrder.nativeOrder())
        copy.put(source)
        copy.position(0)

        val upload = Upload(copy, batches, originX, originY)
        current = upload
        pending = upload
        clearRequested = false
    }

    /** Quita el plano de pantalla: se está cargando otro o no hay ninguno. */
    fun clearPlan() {
        current = null
        pending = null
        clearRequested = true
    }

    fun setCamera(camera: Camera) { this.camera = camera }

    fun setHiddenLayers(hidden: Set<Int>) { hiddenLayers = hidden }

    fun setDarkBackground(dark: Boolean) { background = if (dark) 1 else 0 }

    /** Grosor de línea en píxeles; en pantallas densas un píxel casi no se ve. */
    fun setLineWidth(width: Float) { lineWidth = width }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        program = buildProgram()
        matrixLocation = GLES30.glGetUniformLocation(program, "uMatrix")
        colorLocation = GLES30.glGetUniformLocation(program, "uColor")
        positionLocation = GLES30.glGetAttribLocation(program, "aPosition")

        // Los identificadores anteriores pertenecían al contexto que se acaba
        // de perder: no hay nada que liberar, solo que volver a subir.
        vertexBuffer = 0
        vertexArray = 0
        uploaded = false
        pending = current
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        GLES30.glViewport(0, 0, width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        if (clearRequested) {
            clearRequested = false
            releaseBuffers()
            batches = emptyList()
            uploaded = false
        }
        pending?.let { upload ->
            uploadVertices(upload)
            // Solo se da por hecho si nadie ha entregado otro plano entretanto.
            if (pending === upload) pending = null
        }

        val dark = background == 1
        if (dark) {
            GLES30.glClearColor(0.07f, 0.08f, 0.09f, 1f)
        } else {
            GLES30.glClearColor(0.937f, 0.937f, 0.918f, 1f)
        }
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)

        if (!uploaded || batches.isEmpty() || program == 0) return

        val view = camera
        if (!view.isUsable) return

        GLES30.glUseProgram(program)
        GLES30.glUniformMatrix4fv(matrixLocation, 1, false, view.toMatrix(originX, originY), 0)
        GLES30.glLineWidth(lineWidth)

        val hidden = hiddenLayers
        var lastColor = Int.MIN_VALUE
        var first = true

        GLES30.glBindVertexArray(vertexArray)
        for (batch in batches) {
            if (batch.layerId in hidden) continue
            if (first || batch.color != lastColor) {
                val argb = PlanColors.forBackground(batch.color, dark)
                GLES30.glUniform4f(
                    colorLocation,
                    ((argb shr 16) and 0xFF) / 255f,
                    ((argb shr 8) and 0xFF) / 255f,
                    (argb and 0xFF) / 255f,
                    1f,
                )
                lastColor = batch.color
                first = false
            }
            GLES30.glDrawArrays(GLES30.GL_LINES, batch.firstVertex, batch.vertexCount)
        }
        GLES30.glBindVertexArray(0)
    }

    private fun uploadVertices(upload: Upload) {
        releaseBuffers()

        val buffer = upload.vertices.duplicate().order(ByteOrder.nativeOrder())
        buffer.position(0)

        val arrays = IntArray(1)
        GLES30.glGenVertexArrays(1, arrays, 0)
        vertexArray = arrays[0]

        val buffers = IntArray(1)
        GLES30.glGenBuffers(1, buffers, 0)
        vertexBuffer = buffers[0]

        GLES30.glBindVertexArray(vertexArray)
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vertexBuffer)
        // GL_STATIC_DRAW porque el contenido no cambia nunca: el movimiento se
        // hace con la matriz, no reescribiendo los vértices.
        GLES30.glBufferData(
            GLES30.GL_ARRAY_BUFFER, buffer.capacity(), buffer, GLES30.GL_STATIC_DRAW,
        )
        GLES30.glEnableVertexAttribArray(positionLocation)
        GLES30.glVertexAttribPointer(positionLocation, 2, GLES30.GL_FLOAT, false, 0, 0)
        GLES30.glBindVertexArray(0)

        batches = upload.batches
        originX = upload.originX
        originY = upload.originY
        uploaded = true
    }

    private fun releaseBuffers() {
        if (vertexBuffer != 0) {
            GLES30.glDeleteBuffers(1, intArrayOf(vertexBuffer), 0)
            vertexBuffer = 0
        }
        if (vertexArray != 0) {
            GLES30.glDeleteVertexArrays(1, intArrayOf(vertexArray), 0)
            vertexArray = 0
        }
    }

    private fun buildProgram(): Int {
        val vertex = compile(GLES30.GL_VERTEX_SHADER, VERTEX_SHADER)
        val fragment = compile(GLES30.GL_FRAGMENT_SHADER, FRAGMENT_SHADER)
        val id = GLES30.glCreateProgram()
        GLES30.glAttachShader(id, vertex)
        GLES30.glAttachShader(id, fragment)
        GLES30.glLinkProgram(id)
        GLES30.glDeleteShader(vertex)
        GLES30.glDeleteShader(fragment)
        return id
    }

    private fun compile(type: Int, source: String): Int {
        val id = GLES30.glCreateShader(type)
        GLES30.glShaderSource(id, source)
        GLES30.glCompileShader(id)
        return id
    }

    private companion object {
        // "#version" tiene que ser lo primero del texto, sin nada delante: con
        // un salto de línea o un espacio antes, algunos controladores rechazan
        // el shader y el plano no se dibuja.
        val VERTEX_SHADER = """
            #version 300 es
            uniform mat4 uMatrix;
            in vec2 aPosition;
            void main() {
                gl_Position = uMatrix * vec4(aPosition, 0.0, 1.0);
            }
        """.trimIndent()

        val FRAGMENT_SHADER = """
            #version 300 es
            precision mediump float;
            uniform vec4 uColor;
            out vec4 fragColor;
            void main() {
                fragColor = uColor;
            }
        """.trimIndent()
    }
}
