package com.marcusausias.dwgviewer.viewer

import android.content.Context
import android.opengl.GLSurfaceView
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.marcusausias.dwgviewer.nativebridge.NativePlan

private class PlanSurfaceView(context: Context, val renderer: PlanRenderer) :
    GLSurfaceView(context) {
    init {
        setEGLContextClientVersion(3)
        // Si el sistema puede conservar el contexto al pasar a segundo plano,
        // al volver no hay que subir de nuevo los vértices.
        preserveEGLContextOnPause = true
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
        factory = { context ->
            // Un píxel físico en una pantalla de 400 ppp es un hilo casi
            // invisible a pleno sol; se escala con la densidad.
            val density = context.resources.displayMetrics.density
            renderer.setLineWidth((density * 0.6f).coerceIn(1f, 3f))
            PlanSurfaceView(context, renderer).also { surface = it }
        },
        update = { view ->
            renderer.setCamera(camera)
            renderer.setHiddenLayers(hiddenLayers)
            renderer.setDarkBackground(darkBackground)
            view.requestRender()
        },
    )

    // GLSurfaceView tiene su propio hilo de dibujo y hay que avisarle de las
    // pausas de la actividad: si no, sigue vivo en segundo plano y, al volver,
    // puede quedarse con una superficie que ya no existe.
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    DisposableEffect(lifecycle, surface) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> surface?.onResume()
                Lifecycle.Event.ON_PAUSE -> surface?.onPause()
                else -> Unit
            }
        }
        lifecycle.addObserver(observer)
        onDispose { lifecycle.removeObserver(observer) }
    }

    // La subida del plano se lanza aparte de `update` para no repetirla en cada
    // recomposición: son varios megas de vértices.
    DisposableEffect(plan, surface) {
        val vertices = plan?.takeIf { it.isOpen }?.vertexBuffer()
        val origin = plan?.origin()
        if (plan != null && vertices != null && origin != null) {
            renderer.setPlan(vertices, plan.batches(), origin[0], origin[1])
        } else {
            renderer.clearPlan()
        }
        // Sin esto el plano recién cargado no aparece hasta que el usuario
        // toca la pantalla, porque solo se dibuja cuando algo cambia.
        surface?.requestRender()
        onDispose { }
    }
}
