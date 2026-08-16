#include "dwgcore/entity.h"

#include <algorithm>
#include <cmath>

namespace dwgcore {
namespace {

const double kTwoPi = 2.0 * std::acos(-1.0);

// ¿Cae `angle` dentro del barrido que arranca en `start` y recorre `sweep`?
// Se compara sobre el desfase acumulado, no sobre el ángulo absoluto, para que
// funcione igual con barridos horarios y con arcos que cruzan el origen.
bool angleWithinSweep(double start, double sweep, double angle) {
  double delta = angle - start;
  if (sweep >= 0.0) {
    while (delta < 0.0) delta += kTwoPi;
    while (delta >= kTwoPi) delta -= kTwoPi;
    return delta <= sweep;
  }
  while (delta > 0.0) delta -= kTwoPi;
  while (delta <= -kTwoPi) delta += kTwoPi;
  return delta >= sweep;
}

// Extremos reales de un arco: sus dos puntas más los puntos cardinales que
// queden dentro del barrido, que son donde el arco alcanza su máximo en cada
// eje.
void expandWithArc(Bounds& bounds, const Arc& arc) {
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    const double angle = quadrant * (kTwoPi / 4.0);
    if (!angleWithinSweep(arc.startAngle, arc.sweep, angle)) continue;
    bounds.expand(Vec2{arc.center.x + arc.radius * std::cos(angle),
                       arc.center.y + arc.radius * std::sin(angle)});
  }
}

}  // namespace

void Bounds::expand(Vec2 point) {
  if (!valid) {
    min = point;
    max = point;
    valid = true;
    return;
  }
  min.x = std::min(min.x, point.x);
  min.y = std::min(min.y, point.y);
  max.x = std::max(max.x, point.x);
  max.y = std::max(max.y, point.y);
}

void Bounds::merge(const Bounds& other) {
  if (!other.valid) return;
  expand(other.min);
  expand(other.max);
}

void updateBounds(Entity& entity) {
  Bounds bounds;
  const size_t count = entity.vertices.size();
  for (const PolyVertex& vertex : entity.vertices) bounds.expand(vertex.position);

  const size_t lastSegment = entity.closed ? count : (count > 0 ? count - 1 : 0);
  for (size_t i = 0; i < lastSegment; ++i) {
    const PolyVertex& current = entity.vertices[i];
    const PolyVertex& next = entity.vertices[(i + 1) % count];
    Arc arc;
    if (!arcFromBulge(current.position, next.position, current.bulge, arc)) continue;
    expandWithArc(bounds, arc);
  }

  entity.bounds = bounds;
}

Entity transformEntity(const Entity& entity, const Transform2D& transform,
                       double maxSagitta) {
  Entity result = entity;

  if (isConformal(transform)) {
    // Caso normal: la forma se conserva. Basta con mover los vértices y, si hay
    // simetría, invertir el sentido de giro de los arcos.
    const bool mirrored = isMirrored(transform);
    for (PolyVertex& vertex : result.vertices) {
      vertex.position = apply(transform, vertex.position);
      if (mirrored) vertex.bulge = -vertex.bulge;
    }
    updateBounds(result);
    return result;
  }

  // Escala no uniforme: los arcos pasan a ser elipses. No hay forma de
  // representarlos con un bulge, así que se teselan antes de transformar y la
  // entidad queda marcada como aproximada.
  const std::vector<Vec2> flat =
      tessellatePolyline(entity.vertices, entity.closed, maxSagitta);

  result.vertices.clear();
  result.vertices.reserve(flat.size());
  for (const Vec2& point : flat) {
    result.vertices.push_back({apply(transform, point), 0.0});
  }

  // tessellatePolyline repite el vértice inicial al cerrar el recorrido; se
  // quita para no dejar un tramo de longitud cero.
  if (result.closed && result.vertices.size() > 1) {
    const Vec2& first = result.vertices.front().position;
    const Vec2& last = result.vertices.back().position;
    if (distance(first, last) < 1e-12) result.vertices.pop_back();
  }

  result.approximated = true;
  updateBounds(result);
  return result;
}

double entityLength(const Entity& entity) {
  return polylineLength(entity.vertices, entity.closed);
}

double entityArea(const Entity& entity) {
  return polylineArea(entity.vertices);
}

Entity makeLine(Vec2 start, Vec2 end) {
  Entity entity;
  entity.type = EntityType::Line;
  entity.vertices = {{start, 0.0}, {end, 0.0}};
  updateBounds(entity);
  return entity;
}

Entity makeCircle(Vec2 center, double radius) {
  // Dos semicircunferencias: bulge 1 equivale a un barrido de 180°.
  Entity entity;
  entity.type = EntityType::Circle;
  entity.closed = true;
  entity.vertices = {
      {{center.x - radius, center.y}, 1.0},
      {{center.x + radius, center.y}, 1.0},
  };
  updateBounds(entity);
  return entity;
}

Entity makeArc(Vec2 center, double radius, double startAngle, double endAngle) {
  Entity entity;
  entity.type = EntityType::Arc;

  // En DWG los arcos van siempre en sentido antihorario del inicio al final.
  double sweep = endAngle - startAngle;
  while (sweep <= 0.0) sweep += kTwoPi;

  // El bulge es tan(θ/4), que se va a infinito cuando θ llega a 360°. Un arco
  // completo es en realidad un círculo, y como círculo hay que construirlo.
  // Sin esta guarda, un arco de 360° —o uno cuyos ángulos inicial y final
  // coincidan— genera un bulge infinito que contamina toda la geometría.
  constexpr double kFullCircleTolerance = 1e-9;
  if (sweep >= kTwoPi - kFullCircleTolerance) {
    return makeCircle(center, radius);
  }

  const Vec2 start{center.x + radius * std::cos(startAngle),
                   center.y + radius * std::sin(startAngle)};
  const Vec2 end{center.x + radius * std::cos(endAngle),
                 center.y + radius * std::sin(endAngle)};

  entity.vertices = {{start, std::tan(sweep / 4.0)}, {end, 0.0}};
  updateBounds(entity);
  return entity;
}

}  // namespace dwgcore
