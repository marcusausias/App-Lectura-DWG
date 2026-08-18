package com.marcusausias.dwgviewer.viewer

import android.content.Context
import android.opengl.GLSurfaceView
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.marcusausias.dwgviewer.nativebridge.NativePlan

private class PlanSurfaceView(context: Context, val renderer: PlanRenderer) :
    GLSurfaceView(context) {
    init {
        setEGLContextClientVersion(3)
        setRenderer(renderer)
        // Solo se redibuja cuando algo cambia. Con el plano quieto el consumo
        // cae a cero, que a pie de obra con la batería justa se agradece.
        renderMode = RENDERMODE_WHEN_DIRTY
    }
}

/**
 * Superficie de dibujo del plano.
 *
 * Se redibuja cada vez que cambia la cámara o la visibilidad de las capas.
 */
@Composable
fun PlanGLView(
    plan: NativePlan?,
    camera: Camera,
    hiddenLayers: Set<Int>,
    darkBackground: Boolean,
    modifier: Modifier = Modifier,
) {
    val renderer = remember { PlanRenderer() }
    var surface by remember { mutableStateOf<PlanSurfaceView?>(null) }

    AndroidView(
        modifier = modifier,
        factory = { context -> PlanSurfaceView(context, renderer).also { surface = it } },
        update = { view ->
            renderer.setCamera(camera)
            renderer.setHiddenLayers(hiddenLayers)
            renderer.setDarkBackground(darkBackground)
            view.requestRender()
        },
    )

    // La subida del plano se lanza aparte de `update` para no repetirla en cada
    // recomposición: son varios megas de vértices.
    DisposableEffect(plan) {
        val vertices = plan?.vertexBuffer()
        val origin = plan?.origin()
        if (vertices != null && origin != null) {
            renderer.setPlan(vertices, plan.batches(), origin[0], origin[1])
            // Sin esto el plano recién cargado no aparece hasta que el usuario
            // toca la pantalla, porque solo se dibuja cuando algo cambia.
            surface?.requestRender()
        }
        onDispose { }
    }
}
