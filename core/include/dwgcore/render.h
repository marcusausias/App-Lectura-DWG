// Preparación de la geometría para dibujar.
//
// Convierte la escena en buffers de segmentos listos para subir a la GPU una
// sola vez. A partir de ahí, mover el plano es cambiar una matriz: no se vuelve
// a tocar memoria al hacer paneo ni zoom.
#pragma once

#include <cstdint>
#include <vector>

#include "dwgcore/scene.h"

namespace dwgcore {

// Un tramo contiguo del buffer con toda la geometría de una capa.
//
// Al estar cada capa junta, encender o apagarla es saltarse una llamada de
// dibujo. No hay que reconstruir nada, que es lo que permite que el panel de
// capas responda al instante incluso con planos grandes.
struct RenderBatch {
  uint16_t layerId = 0;
  uint32_t firstVertex = 0;  // En vértices, no en floats.
  uint32_t vertexCount = 0;
};

struct RenderData {
  // Centro de la escena, que se ha restado a todos los vértices.
  //
  // OpenGL trabaja en float, con unas siete cifras significativas. Un plano de
  // obra tiene coordenadas de cinco o seis cifras enteras, y con coordenadas
  // UTM llega a siete, así que subirlas tal cual deja un error de milímetros
  // que se ve como geometría temblando al acercarse. Restando el centro, los
  // valores que llegan a la GPU son pequeños y el float da de sobra; la
  // posición real se recupera en la matriz, que se calcula en double.
  Vec2 origin;

  // Pares (x, y) relativos a `origin`, en el orden que espera GL_LINES: cada
  // dos vértices son un segmento independiente.
  std::vector<float> vertices;

  std::vector<RenderBatch> batches;

  size_t vertexCount() const { return vertices.size() / 2; }
  size_t segmentCount() const { return vertices.size() / 4; }
};

// `maxSagitta` es la desviación máxima al convertir los tramos curvos en
// segmentos. Con `sagittaFor` se deriva del tamaño del propio plano, porque un
// dibujo en milímetros y otro en metros necesitan detalles muy distintos.
double sagittaFor(const Bounds& bounds);

RenderData buildRenderData(const Scene& scene, double maxSagitta);

inline RenderData buildRenderData(const Scene& scene) {
  return buildRenderData(scene, sagittaFor(scene.bounds));
}

}  // namespace dwgcore
