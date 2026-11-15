package com.marcusausias.dwgviewer

import com.marcusausias.dwgviewer.data.MeasurementExport
import com.marcusausias.dwgviewer.viewer.MeasureTool
import com.marcusausias.dwgviewer.viewer.Measurement
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MeasurementExportTest {

    private val sample = listOf(
        Measurement(1, MeasureTool.DISTANCE, emptyList(), 12.3456, label = "Tabique; salón"),
        Measurement(2, MeasureTool.AREA, emptyList(), 20.5),
        Measurement(3, MeasureTool.FOLLOW, emptyList(), 14.0, area = 12.0, approximate = true),
        Measurement(4, MeasureTool.COUNT, emptyList(), 0.0, count = 54, symbol = "ROCIADOR"),
    )

    @Test
    fun `usa punto y coma y coma decimal para que Excel lo abra bien`() {
        val csv = MeasurementExport.toCsv("Planta 1.dwg", sample, decimals = 2, unit = "m")
        val lines = csv.removePrefix("\uFEFF").split("\r\n")

        assertTrue("empieza con BOM", csv.startsWith("\uFEFF"))
        assertEquals("Plano;Planta 1.dwg", lines[0])
        assertEquals("Unidad del dibujo;m", lines[1])
        assertEquals("1;\"Tabique; salón\";Distancia;12,35;;;;", lines[4])
        assertEquals("2;;Superficie;;20,50;;;", lines[5])
        assertEquals("3;;Seguir;14,00;12,00;;;sí", lines[6])
        assertEquals("4;;Contar;;;54;ROCIADOR;", lines[7])
    }

    @Test
    fun `los totales suman longitudes y superficies por separado`() {
        val csv = MeasurementExport.toCsv("p.dwg", sample, decimals = 2, unit = "")
        val total = csv.trim().split("\r\n").last()
        // 12,3456 + 14 de longitud; 20,5 + 12 de superficie. El recuento no suma.
        assertEquals(";Total;;26,35;32,50;;;", total)
    }

    @Test
    fun `el nombre de archivo sale del plano`() {
        assertEquals("Mediciones Planta 1.csv", MeasurementExport.fileNameFor("Planta 1.dwg"))
        assertEquals("Mediciones plano.csv", MeasurementExport.fileNameFor(""))
        assertEquals("Mediciones a_b.csv", MeasurementExport.fileNameFor("a/b.dwg"))
    }
}
