package com.marcusausias.dwgviewer

import com.marcusausias.dwgviewer.viewer.Camera
import com.marcusausias.dwgviewer.viewer.MeasureTool
import com.marcusausias.dwgviewer.viewer.Measurement
import com.marcusausias.dwgviewer.viewer.PlanColors
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Locale

class ViewerLogicTest {

    @Test
    fun `la camara va y vuelve entre pantalla y dibujo sin perder el punto`() {
        val camera = Camera(viewportWidth = 1080, viewportHeight = 1920)
            .fitTo(doubleArrayOf(430_000.0, 4_580_000.0, 430_120.0, 4_580_090.0))

        val (worldX, worldY) = camera.screenToWorld(300f, 700f)
        val (screenX, screenY) = camera.worldToScreen(worldX, worldY)
        assertEquals(300f, screenX, 0.01f)
        assertEquals(700f, screenY, 0.01f)
    }

    @Test
    fun `el zoom mantiene fijo el punto bajo el dedo`() {
        val camera = Camera(viewportWidth = 1000, viewportHeight = 800)
            .fitTo(doubleArrayOf(0.0, 0.0, 50.0, 40.0))
        val before = camera.screenToWorld(250f, 600f)
        val after = camera.zoomAround(250f, 600f, 3.0).screenToWorld(250f, 600f)
        assertEquals(before.first, after.first, 1e-9)
        assertEquals(before.second, after.second, 1e-9)
    }

    @Test
    fun `encuadrar deja el plano entero dentro de la pantalla`() {
        val bounds = doubleArrayOf(-10.0, 5.0, 90.0, 25.0)
        val camera = Camera(viewportWidth = 1000, viewportHeight = 1000).fitTo(bounds)
        val (left, bottom) = camera.worldToScreen(bounds[0], bounds[1])
        val (right, top) = camera.worldToScreen(bounds[2], bounds[3])
        assertTrue(left >= 0f && right <= 1000f && top >= 0f && bottom <= 1000f)
    }

    @Test
    fun `los colores sin contraste se ajustan al fondo`() {
        val yellow = 0xFFFFFF00.toInt()
        val navy = 0xFF000080.toInt()

        fun luminance(color: Int) =
            (0.299 * ((color shr 16) and 0xFF) + 0.587 * ((color shr 8) and 0xFF) +
                0.114 * (color and 0xFF)) / 255.0

        // El amarillo no se lee sobre fondo claro: se oscurece. Sobre oscuro se deja.
        assertTrue(luminance(PlanColors.forBackground(yellow, dark = false)) <= 0.56)
        assertEquals(yellow, PlanColors.forBackground(yellow, dark = true))
        // El azul marino, al revés.
        assertTrue(luminance(PlanColors.forBackground(navy, dark = true)) >= 0.34)
        assertEquals(navy, PlanColors.forBackground(navy, dark = false))
        // El color de primer plano cambia con el fondo.
        assertTrue(luminance(PlanColors.forBackground(0, dark = false)) < 0.2)
        assertTrue(luminance(PlanColors.forBackground(0, dark = true)) > 0.8)
    }

    @Test
    fun `una medicion se presenta con su unidad y su superficie`() {
        Locale.setDefault(Locale("es", "ES"))
        val length = Measurement(1, MeasureTool.DISTANCE, emptyList(), 3.14159)
        assertEquals("3,14 m", length.formatted(2, "m"))
        assertEquals("3,142", length.formatted(3, ""))

        val area = Measurement(2, MeasureTool.AREA, emptyList(), 12.5)
        assertEquals("12,50 m²", area.formatted(2, "m"))
        assertEquals("12,50 u²", area.formatted(2, ""))

        val contour = Measurement(3, MeasureTool.FOLLOW, emptyList(), 14.0, area = 12.0)
        assertEquals("14,0 m · 12,0 m²", contour.formatted(1, "m"))

        val count = Measurement(4, MeasureTool.COUNT, emptyList(), 0.0, count = 7, symbol = "PUERTA")
        assertEquals("7 × PUERTA", count.formatted(2, "m"))
    }
}
