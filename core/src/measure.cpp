#include "dwgcore/measure.h"

namespace dwgcore {

Measurement measureDistance(Vec2 from, Vec2 to) {
  Measurement measurement;
  measurement.kind = MeasureKind::Distance;
  measurement.points = {from, to};
  measurement.value = distance(from, to);
  return measurement;
}

Measurement measureChain(const std::vector<Vec2>& points) {
  Measurement measurement;
  measurement.kind = MeasureKind::Chain;
  measurement.points = points;
  for (size_t i = 1; i < points.size(); ++i) {
    measurement.value += distance(points[i - 1], points[i]);
  }
  return measurement;
}

Measurement measureArea(const std::vector<Vec2>& points) {
  Measurement measurement;
  measurement.kind = MeasureKind::Area;
  measurement.points = points;
  // Con menos de tres puntos no hay superficie: polygonArea ya devuelve 0.
  measurement.value = polygonArea(points);
  return measurement;
}

Measurement measureEntity(const Scene& scene, uint32_t entityIndex) {
  Measurement measurement;
  measurement.kind = MeasureKind::Entity;
  if (entityIndex >= scene.entities.size()) return measurement;

  const Entity& entity = scene.entities[entityIndex];
  measurement.value = entityLength(entity);
  // Una spline o una elipse llegan ya teseladas y su longitud es buena pero no
  // exacta; conviene que quien lea la cifra lo sepa.
  measurement.approximate = entity.approximated;

  measurement.points.reserve(entity.vertices.size());
  for (const PolyVertex& vertex : entity.vertices) {
    measurement.points.push_back(vertex.position);
  }

  if (entity.blockId != 0 && entity.blockId < scene.blocks.size()) {
    measurement.label = scene.blocks[entity.blockId].name;
  }
  return measurement;
}

Measurement measureCount(const Scene& scene, uint32_t entityIndex) {
  Measurement measurement;
  measurement.kind = MeasureKind::Count;
  if (entityIndex >= scene.entities.size()) return measurement;

  const Entity& entity = scene.entities[entityIndex];
  // La entidad puede no venir de ningún bloque: entonces no hay símbolo que
  // contar, y devolver 0 es más honesto que contar 1.
  if (entity.blockId == 0 || entity.blockId >= scene.blocks.size()) {
    return measurement;
  }

  const BlockInfo& block = scene.blocks[entity.blockId];
  measurement.count = block.instanceCount;
  measurement.label = block.name;
  if (!entity.vertices.empty()) {
    measurement.points = {entity.vertices.front().position};
  }
  return measurement;
}

}  // namespace dwgcore
