package com.marcusausias.dwgviewer

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.marcusausias.dwgviewer.data.MeasurementStore
import com.marcusausias.dwgviewer.data.SettingsStore
import com.marcusausias.dwgviewer.viewer.MeasureTool
import com.marcusausias.dwgviewer.viewer.Measurement
import com.marcusausias.dwgviewer.xref.XrefPath
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

/**
 * Arranque de la app y persistencia, ejecutados en el ordenador.
 *
 * No sustituye a probar en un móvil —aquí no hay OpenGL ni biblioteca nativa—,
 * pero detecta lo que más duele: que la app no arranque.
 */
@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class AppSmokeTest {

    @get:Rule
    val compose = createAndroidComposeRule<MainActivity>()

    @Test
    fun `la app arranca y ofrece abrir un plano`() {
        compose.onNodeWithText("Abrir un plano DWG").assertIsDisplayed()
    }

    @Test
    fun `las mediciones sobreviven a guardar y volver a leer`() {
        val store = MeasurementStore(ApplicationProvider.getApplicationContext())
        val saved = listOf(
            Measurement(
                id = 7, tool = MeasureTool.FOLLOW,
                points = listOf(0.0 to 0.0, 4.0 to 0.0, 4.0 to 3.0),
                value = 14.0, approximate = true, label = "Salón", area = 12.0,
            ),
            Measurement(8, MeasureTool.COUNT, listOf(1.0 to 2.0), 0.0, count = 3, symbol = "P1"),
        )
        store.save("plano-ñ/raro:1", saved)
        assertEquals(saved, store.load("plano-ñ/raro:1"))

        store.save("plano-ñ/raro:1", emptyList())
        assertTrue(store.load("plano-ñ/raro:1").isEmpty())
    }

    @Test
    fun `los ajustes se recuerdan`() {
        val context = ApplicationProvider.getApplicationContext<android.content.Context>()
        SettingsStore(context).apply {
            unit = "cm"
            decimals = 1
        }
        val again = SettingsStore(context)
        assertEquals("cm", again.unit)
        assertEquals(1, again.decimals)
    }

    @Test
    fun `las rutas de xref de Windows se reducen a algo aprovechable`() {
        assertEquals("base.dwg", XrefPath.fileName("N:\\2017\\Obra\\base.dwg"))
        assertEquals(listOf("xrefs", "base.dwg"), XrefPath.relativeSegments("..\\xrefs\\.\\base.dwg"))
        assertTrue(XrefPath.isRelative("xrefs\\base.dwg"))
        assertTrue(!XrefPath.isRelative("N:\\Obra\\base.dwg"))
        assertTrue(XrefPath.sameFile("BASE.DWG", "base"))
    }
}
