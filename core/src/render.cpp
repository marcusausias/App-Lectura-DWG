#include "dwgcore/render.h"

#include <algorithm>
#include <cmath>

namespace dwgcore {
namespace {

// Detalle del teselado, en relación a la diagonal del plano.
//
// El valor es alto a propósito. Midiendo sobre un plano real de 81.000
// entidades, afinar el teselado 100 veces solo añadió un 3% de segmentos y
// nada de tiempo: la inmensa mayoría de la geometría de un plano ya es recta,
// y las curvas aportan una porción mínima del total. Como afinar sale casi
// gratis, se afina, y así las curvas siguen siendo curvas por mucho que se
// acerque el usuario en lugar de convertirse en polígonos visibles.
constexpr double kSagittaDivisor = 2000000.0;

// Los puntos no tienen longitud, así que se dibujan como una cruz pequeña para
// que se vean y se puedan enganchar. El tamaño va en unidades de dibujo y sale
// del mismo criterio que el teselado.
constexpr double kPointMarkFactor = 3.0;

void appendSegment(std::vector<float>& vertices, Vec2 origin, Vec2 from, Vec2 to) {
  vertices.push_back(static_cast<float>(from.x - origin.x));
  vertices.push_back(static_cast<float>(from.y - origin.y));
  vertices.push_back(static_cast<float>(to.x - origin.x));
  vertices.push_back(static_cast<float>(to.y - origin.y));
}

void appendPointMark(std::vector<float>& vertices, Vec2 origin, Vec2 at,
                     double size) {
  appendSegment(vertices, origin, {at.x - size, at.y}, {at.x + size, at.y});
  appendSegment(vertices, origin, {at.x, at.y - size}, {at.x, at.y + size});
}

}  // namespace

double sagittaFor(const Bounds& bounds) {
  if (!bounds.valid) return 1e-3;
  const double width = bounds.max.x - bounds.min.x;
  const double height = bounds.max.y - bounds.min.y;
  const double diagonal = std::sqrt(width * width + height * height);
  if (!(diagonal > 0.0)) return 1e-3;
  return diagonal / kSagittaDivisor;
}

RenderData buildRenderData(const Scene& scene, double maxSagitta) {
  RenderData data;

  if (scene.bounds.valid) {
    data.origin = {(scene.bounds.min.x + scene.bounds.max.x) * 0.5,
                   (scene.bounds.min.y + scene.bounds.max.y) * 0.5};
  }
  if (scene.entities.empty()) return data;

  if (!(maxSagitta > 0.0)) maxSagitta = sagittaFor(scene.bounds);
  const double pointSize = maxSagitta * kPointMarkFactor;

  // Las entidades se recorren agrupadas por capa en vez de en su orden
  // original. Ordenar solo los índices evita mover las entidades, que son
  // pesadas.
  std::vector<uint32_t> order(scene.entities.size());
  for (uint32_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return scene.entities[a].layerId < scene.entities[b].layerId;
  });

  // Estimación para reservar de una vez: la mayoría de las entidades son
  // líneas o polilíneas cortas. Quedarse corto solo cuesta alguna realocación.
  data.vertices.reserve(scene.entities.size() * 8);

  bool batchOpen = false;
  RenderBatch batch;

  for (uint32_t index : order) {
    const Entity& entity = scene.entities[index];

    if (batchOpen && batch.layerId != entity.layerId) {
      batch.vertexCount =
          static_cast<uint32_t>(data.vertices.size() / 2) - batch.firstVertex;
      // Una capa cuyas entidades no han producido ni un segmento no genera
      // lote: un lote vacío es una llamada de dibujo que no pinta nada.
      if (batch.vertexCount > 0) data.batches.push_back(batch);
      batchOpen = false;
    }

    if (!batchOpen) {
      batch = RenderBatch{};
      batch.layerId = entity.layerId;
      batch.firstVertex = static_cast<uint32_t>(data.vertices.size() / 2);
      batchOpen = true;
    }

    // Los textos no se dibujan aquí: van en la capa de encima, con las fuentes
    // del sistema, porque en GL harían falta atlas de glifos y el número de
    // textos visibles a la vez siempre es pequeño.
    if (entity.type == EntityType::Text) continue;

    if (entity.type == EntityType::Point) {
      if (!entity.vertices.empty()) {
        appendPointMark(data.vertices, data.origin, entity.vertices[0].position,
                        pointSize);
      }
      continue;
    }

    if (entity.vertices.size() < 2) continue;

    // tessellatePolyline resuelve los bulges y devuelve el recorrido completo.
    const std::vector<Vec2> path =
        tessellatePolyline(entity.vertices, entity.closed, maxSagitta);
    for (size_t i = 1; i < path.size(); ++i) {
      appendSegment(data.vertices, data.origin, path[i - 1], path[i]);
    }
  }

  if (batchOpen) {
    batch.vertexCount =
        static_cast<uint32_t>(data.vertices.size() / 2) - batch.firstVertex;
    if (batch.vertexCount > 0) data.batches.push_back(batch);
  }

  return data;
}

}  // namespace dwgcore
