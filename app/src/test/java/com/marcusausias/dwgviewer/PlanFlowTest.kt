package com.marcusausias.dwgviewer

import android.net.Uri
import android.os.Looper
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.lifecycle.ViewModelProvider
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.marcusausias.dwgviewer.nativebridge.NativePlan
import com.marcusausias.dwgviewer.viewer.MeasureTool
import com.marcusausias.dwgviewer.viewer.PlanState
import com.marcusausias.dwgviewer.viewer.ViewerViewModel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import java.io.File
import java.io.FileInputStream

/**
 * El recorrido completo con un DWG de verdad: abrirlo a través del puente
 * nativo, ver sus capas, medir y exportar.
 *
 * Usa la biblioteca nativa compilada para el ordenador y los planos de muestra
 * que trae LibreDWG. Se salta si falta alguna de las dos cosas; para tenerlas:
 *
 *     tools/probar-dwg.sh --solo-compilar
 */
@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class PlanFlowTest {

    @get:Rule
    val compose = createAndroidComposeRule<MainActivity>()

    private val samples = File(System.getProperty("dwg.samples").orEmpty())
    private val sample get() = File(samples, "2018/Dynblocks.dwg")

    @Before
    fun requireNativeLibrary() {
        assumeTrue(
            "falta la biblioteca nativa de escritorio",
            File(System.getProperty("dwg.hostlib").orEmpty()).exists(),
        )
        assumeTrue("faltan los planos de muestra", sample.exists())
    }

    private fun cacheFile(name: String) =
        File(ApplicationProvider.getApplicationContext<android.content.Context>().cacheDir, name)

    @Test
    fun `el puente nativo abre un plano y responde a todas las consultas`() {
        val cache = cacheFile("flow-test.bin").apply { delete() }
        val plan = NativePlan.open(sample.absolutePath, cache.absolutePath, sample.length(), 1L)
        assertNotNull("el plano se abre", plan)
        plan!!

        assertTrue("tiene entidades", plan.entityCount() > 1000)
        val layers = plan.layers()
        assertTrue("tiene capas", layers.size > 5)
        assertEquals("un estilo por capa", layers.size, plan.layerStyles().size)
        assertTrue("hay capas con color propio", plan.layerStyles().any { it.color != 0 })

        val bounds = plan.bounds()!!
        assertTrue("extensión válida", bounds[2] > bounds[0] && bounds[3] > bounds[1])
        assertEquals(2, plan.origin()!!.size)

        // Los lotes tienen que cubrir el búfer exacto: un vértice de más y
        // OpenGL leería fuera de él.
        val vertices = plan.vertexBuffer()!!
        val vertexCount = vertices.capacity() / 8
        val batches = plan.batches()
        assertTrue("hay lotes", batches.isNotEmpty())
        assertEquals("los lotes cubren el búfer", vertexCount, batches.sumOf { it.vertexCount })
        assertTrue(batches.all { it.layerId in layers.indices && it.firstVertex >= 0 })

        val texts = plan.visibleTexts(bounds[0], bounds[1], bounds[2], bounds[3], 0.0)
        assertTrue("devuelve textos", texts.isNotEmpty())
        assertTrue(texts.all { it.content.isNotEmpty() && it.layerId in layers.indices })

        // Enganche y selección sobre un extremo real de la geometría.
        val diagonal = Math.hypot(bounds[2] - bounds[0], bounds[3] - bounds[1])
        val x = plan.origin()!![0] + vertices.order(java.nio.ByteOrder.nativeOrder()).getFloat(0)
        val y = plan.origin()!![1] + vertices.order(java.nio.ByteOrder.nativeOrder()).getFloat(4)
        val snap = plan.snapAt(x + diagonal * 1e-4, y, diagonal * 1e-2)
        assertNotNull("engancha cerca de un vértice", snap)
        val entity = plan.pickEntity(x, y, diagonal * 1e-2)
        assertNotNull("selecciona la entidad", entity)
        val measure = plan.measureEntity(entity!!)
        assertNotNull(measure)
        assertTrue("la longitud es positiva", measure!!.length > 0.0)
        assertTrue(measure.points.size >= 2)

        // Contar símbolos: alguna entidad del plano tiene que venir de un bloque.
        val counted = (0 until plan.entityCount() step 97).firstNotNullOfOrNull { index ->
            plan.symbolName(index)?.let { it to plan.symbolInstances(index) }
        }
        assertNotNull("hay símbolos contables", counted)
        assertTrue(counted!!.second >= 1)

        val entityCount = plan.entityCount()
        plan.close()

        // Segunda apertura: tiene que salir de la caché y ser idéntica.
        assertTrue("se escribió la caché", cache.exists() && cache.length() > 0)
        val again = NativePlan.open("/no/existe.dwg", cache.absolutePath, sample.length(), 1L)
        assertNotNull("se abre desde caché sin tocar el DWG", again)
        assertEquals(entityCount, again!!.entityCount())
        assertEquals(layers, again.layers())
        again.close()

        // Y un archivo que no es un DWG se rechaza sin tumbar el proceso.
        val garbage = cacheFile("basura.dwg").apply { writeText("esto no es un plano") }
        assertEquals(null, NativePlan.open(garbage.absolutePath, cacheFile("b.bin").absolutePath, 1, 1))
        assertTrue(NativePlan.inspectXrefs(sample.absolutePath).isEmpty())
    }

    @Test
    fun `abrir medir y exportar desde la pantalla`() {
        val uri = Uri.parse("content://planos.prueba/Dynblocks.dwg")
        val context = ApplicationProvider.getApplicationContext<android.content.Context>()
        shadowOf(context.contentResolver).registerInputStreamSupplier(uri) { FileInputStream(sample) }

        lateinit var model: ViewerViewModel
        compose.activityRule.scenario.onActivity { activity ->
            model = ViewModelProvider(activity)[ViewerViewModel::class.java]
            model.open(uri)
        }

        // La carga va en un hilo aparte; se espera a que el plano esté listo.
        compose.waitUntil(timeoutMillis = 60_000) {
            shadowOf(Looper.getMainLooper()).idle()
            model.state.value !is PlanState.Loading && model.state.value !is PlanState.Empty
        }
        val state = model.state.value
        assertTrue("el plano se abre: $state", state is PlanState.Ready)
        state as PlanState.Ready
        assertEquals("Dynblocks.dwg", state.name)

        compose.onNodeWithText("Dynblocks.dwg").assertIsDisplayed()
        compose.onNodeWithContentDescription("Capas").assertIsDisplayed()
        assertTrue("la cámara encuadra el plano", model.camera.value.isUsable)

        // Capas: el panel se abre y lista las del plano.
        compose.onNodeWithContentDescription("Capas").performClick()
        compose.onNodeWithText("Capas (${state.layers.size})").assertIsDisplayed()
        compose.activityRule.scenario.onActivity { model.toggleLayer(0) }
        assertTrue(0 in model.hiddenLayers.value)
        compose.activityRule.scenario.onActivity { model.showAllLayers() }

        // Medir una distancia tocando dos puntos de la pantalla.
        val camera = model.camera.value
        compose.activityRule.scenario.onActivity {
            model.selectTool(MeasureTool.DISTANCE)
            model.onHover(camera.viewportWidth * 0.3f, camera.viewportHeight * 0.4f)
            model.onTap(camera.viewportWidth * 0.3f, camera.viewportHeight * 0.4f)
            model.onTap(camera.viewportWidth * 0.7f, camera.viewportHeight * 0.6f)
        }
        assertEquals("la distancia se cierra sola", 1, model.measurements.value.size)
        assertTrue(model.measurements.value[0].value > 0.0)

        // Superficie con tres puntos y Cerrar.
        compose.activityRule.scenario.onActivity {
            model.selectTool(MeasureTool.AREA)
            model.onTap(camera.viewportWidth * 0.2f, camera.viewportHeight * 0.2f)
            model.onTap(camera.viewportWidth * 0.8f, camera.viewportHeight * 0.2f)
            model.onTap(camera.viewportWidth * 0.8f, camera.viewportHeight * 0.8f)
            model.finishPending()
            model.setUnit("m")
            model.renameMeasurement(model.measurements.value[1].id, "Salón")
        }
        assertEquals(2, model.measurements.value.size)

        val csv = model.exportCsv()
        assertTrue(csv.contains("Plano;Dynblocks.dwg"))
        assertTrue(csv.contains(";Salón;Superficie;"))
        assertEquals("Mediciones Dynblocks.csv", model.exportFileName())

        // Reabrir el mismo plano recupera las mediciones guardadas.
        compose.activityRule.scenario.onActivity { model.open(uri) }
        compose.waitUntil(timeoutMillis = 60_000) {
            shadowOf(Looper.getMainLooper()).idle()
            model.state.value is PlanState.Ready && model.measurements.value.size == 2
        }
        assertEquals("Salón", model.measurements.value[1].label)
    }
}
