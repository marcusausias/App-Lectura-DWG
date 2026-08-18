#include "dwgcore/snap.h"

#include <algorithm>
#include <cmath>

namespace dwgcore {
namespace {

struct Segment {
  Vec2 from;
  Vec2 to;
  uint32_t entity = kNoEntity;
};

// Prioridad entre tipos de enganche. Cuanto menor, antes se elige.
//
// Coincide con la que usa AutoCAD y con lo que espera quien mide: si hay una
// esquina cerca, se quiere la esquina, no un punto cualquiera de la línea que
// pase por ahí. El punto sobre la línea es el último recurso.
int priorityOf(SnapType type) {
  switch (type) {
    case SnapType::Endpoint: return 0;
    case SnapType::Intersection: return 1;
    case SnapType::Midpoint: return 2;
    case SnapType::Perpendicular: return 3;
    case SnapType::OnEdge: return 4;
    default: return 100;
  }
}

bool isHidden(const std::vector<uint16_t>& hidden, uint16_t layerId) {
  return std::find(hidden.begin(), hidden.end(), layerId) != hidden.end();
}

// Punto del segmento más cercano a `point`, y su parámetro sobre el segmento.
Vec2 closestOnSegment(Vec2 from, Vec2 to, Vec2 point, double& outT) {
  const Vec2 direction = to - from;
  const double lengthSquared = dot(direction, direction);
  if (lengthSquared <= 0.0) {
    outT = 0.0;
    return from;
  }
  double t = dot(point - from, direction) / lengthSquared;
  t = std::clamp(t, 0.0, 1.0);
  outT = t;
  return from + direction * t;
}

// Cruce entre dos segmentos. Solo cuenta si cae dentro de ambos: prolongarlos
// daría enganches a cruces que en el plano no existen.
bool segmentIntersection(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2, Vec2& out) {
  const Vec2 da = a2 - a1;
  const Vec2 db = b2 - b1;
  const double denominator = cross(da, db);
  // Paralelos o degenerados: no hay un punto de cruce único.
  if (std::abs(denominator) < 1e-12) return false;

  const Vec2 delta = b1 - a1;
  const double t = cross(delta, db) / denominator;
  const double u = cross(delta, da) / denominator;
  if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) return false;

  out = a1 + da * t;
  return true;
}

bool overlapsArea(const Bounds& box, Vec2 center, double radius) {
  if (!box.valid) return false;
  return !(center.x + radius < box.min.x || center.x - radius > box.max.x ||
           center.y + radius < box.min.y || center.y - radius > box.max.y);
}

// Reúne los segmentos de las entidades cercanas, ya teselados y filtrados.
std::vector<Segment> collectSegments(const Scene& scene, Vec2 query,
                                     const SnapOptions& options,
                                     const std::vector<uint16_t>& hiddenLayers) {
  std::vector<Segment> segments;

  Bounds area;
  area.expand(Vec2{query.x - options.radius, query.y - options.radius});
  area.expand(Vec2{query.x + options.radius, query.y + options.radius});

  std::vector<uint32_t> candidates;
  queryEntities(scene, area, candidates);

  // El teselado se afina con el radio de búsqueda, no con el tamaño del plano:
  // lo que importa aquí es la precisión a la escala a la que se está mirando.
  const double sagitta = std::max(options.radius / 8.0, 1e-12);

  for (uint32_t index : candidates) {
    if (static_cast<int>(segments.size()) >= options.maxSegments) break;

    const Entity& entity = scene.entities[index];
    if (entity.type == EntityType::Text) continue;
    if (entity.vertices.size() < 2) continue;
    if (isHidden(hiddenLayers, entity.layerId)) continue;

    const std::vector<Vec2> path =
        tessellatePolyline(entity.vertices, entity.closed, sagitta);

    for (size_t i = 1; i < path.size(); ++i) {
      if (static_cast<int>(segments.size()) >= options.maxSegments) break;

      // Una polilínea larga puede cruzar el área con un solo tramo útil; el
      // resto se descarta antes de hacer ninguna cuenta.
      Bounds box;
      box.expand(path[i - 1]);
      box.expand(path[i]);
      if (!overlapsArea(box, query, options.radius)) continue;

      segments.push_back({path[i - 1], path[i], index});
    }
  }
  return segments;
}

}  // namespace

SnapResult snap(const Scene& scene, Vec2 query, const SnapOptions& options,
                const std::vector<uint16_t>& hiddenLayers) {
  SnapResult best;
  int bestPriority = 1000;

  const std::vector<Segment> segments =
      collectSegments(scene, query, options, hiddenLayers);

  auto consider = [&](SnapType type, Vec2 point, uint32_t entity) {
    const double distance = dwgcore::distance(query, point);
    if (distance > options.radius) return;

    const int priority = priorityOf(type);
    // Entre dos enganches del mismo tipo gana el más cercano; entre tipos
    // distintos, el de más prioridad aunque quede algo más lejos.
    if (priority > bestPriority) return;
    if (priority == bestPriority && best.found() && distance >= best.distance) return;

    best.type = type;
    best.point = point;
    best.entityIndex = entity;
    best.distance = distance;
    bestPriority = priority;
  };

  for (const Segment& segment : segments) {
    if (options.endpoint) {
      consider(SnapType::Endpoint, segment.from, segment.entity);
      consider(SnapType::Endpoint, segment.to, segment.entity);
    }
    if (options.midpoint) {
      consider(SnapType::Midpoint, (segment.from + segment.to) * 0.5, segment.entity);
    }

    double t = 0.0;
    const Vec2 nearest = closestOnSegment(segment.from, segment.to, query, t);
    if (options.onEdge) consider(SnapType::OnEdge, nearest, segment.entity);

    if (options.perpendicular && options.hasReference) {
      double footT = 0.0;
      const Vec2 foot =
          closestOnSegment(segment.from, segment.to, options.reference, footT);
      // Solo vale si el pie cae dentro del tramo: fuera de él la perpendicular
      // no toca la línea que se ve dibujada.
      if (footT > 0.0 && footT < 1.0) {
        consider(SnapType::Perpendicular, foot, segment.entity);
      }
    }
  }

  if (options.intersection) {
    int pairs = 0;
    for (size_t i = 0; i < segments.size() && pairs < options.maxIntersectionPairs; ++i) {
      for (size_t j = i + 1; j < segments.size(); ++j) {
        if (++pairs >= options.maxIntersectionPairs) break;
        // Dos tramos de la misma polilínea comparten vértice por definición:
        // ese cruce ya lo cubre el enganche a extremo.
        if (segments[i].entity == segments[j].entity) continue;

        Vec2 point;
        if (segmentIntersection(segments[i].from, segments[i].to, segments[j].from,
                                segments[j].to, point)) {
          consider(SnapType::Intersection, point, segments[i].entity);
        }
      }
    }
  }

  return best;
}

uint32_t pickEntity(const Scene& scene, Vec2 query, double radius,
                    const std::vector<uint16_t>& hiddenLayers) {
  SnapOptions options;
  options.radius = radius;
  options.endpoint = false;
  options.midpoint = false;
  options.intersection = false;
  options.perpendicular = false;
  options.onEdge = true;

  const SnapResult result = snap(scene, query, options, hiddenLayers);
  return result.found() ? result.entityIndex : kNoEntity;
}

}  // namespace dwgcore
