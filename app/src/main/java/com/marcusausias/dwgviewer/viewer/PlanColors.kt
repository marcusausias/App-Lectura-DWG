package com.marcusausias.dwgviewer.viewer

/**
 * Ajuste de los colores del DWG al fondo sobre el que se dibujan.
 *
 * Los planos se dibujan pensando en el fondo negro de AutoCAD: amarillos,
 * cianes y blancos que allí destacan desaparecen sobre un fondo claro. Y al
 * revés, un azul marino o un gris oscuro no se ven sobre fondo oscuro. Aquí se
 * oscurece o se aclara solo lo que no tendría contraste, conservando el tono
 * para que las capas sigan distinguiéndose por color.
 */
object PlanColors {

    private const val FOREGROUND_ON_LIGHT = 0xFF1A1C20.toInt()
    private const val FOREGROUND_ON_DARK = 0xFFE6E6E0.toInt()

    /** [color] es ARGB, o 0 para el color de primer plano. */
    fun forBackground(color: Int, dark: Boolean): Int {
        if (color == 0) return if (dark) FOREGROUND_ON_DARK else FOREGROUND_ON_LIGHT

        val red = (color shr 16) and 0xFF
        val green = (color shr 8) and 0xFF
        val blue = color and 0xFF
        // Luminancia percibida: el verde pesa mucho más que el azul.
        val luminance = (0.299 * red + 0.587 * green + 0.114 * blue) / 255.0

        return if (dark) {
            if (luminance >= MIN_ON_DARK) {
                color
            } else {
                // Se mezcla con blanco lo justo para llegar al mínimo.
                val mix = ((MIN_ON_DARK - luminance) / (1.0 - luminance)).coerceIn(0.0, 1.0)
                pack(
                    red + ((255 - red) * mix).toInt(),
                    green + ((255 - green) * mix).toInt(),
                    blue + ((255 - blue) * mix).toInt(),
                )
            }
        } else {
            if (luminance <= MAX_ON_LIGHT) {
                color
            } else {
                val factor = MAX_ON_LIGHT / luminance
                pack((red * factor).toInt(), (green * factor).toInt(), (blue * factor).toInt())
            }
        }
    }

    private fun pack(red: Int, green: Int, blue: Int): Int =
        (0xFF shl 24) or (red.coerceIn(0, 255) shl 16) or (green.coerceIn(0, 255) shl 8) or
            blue.coerceIn(0, 255)

    private const val MAX_ON_LIGHT = 0.55
    private const val MIN_ON_DARK = 0.35
}
