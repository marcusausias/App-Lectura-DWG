#include "dwgcore/geometry.h"

namespace dwgcore {
namespace {

// Recorre los tramos de la polilínea llamando a `visit(inicio, fin, bulge)`.
// Un tramo va del vértice i al i+1 y lleva el bulge del vértice i, que es el
// convenio de LWPOLYLINE en DXF.
template <typename Visitor>
void forEachSegment(const std::vector<PolyVertex>& vertices, bool closed,
                    Visitor visit) {
  const size_t count = vertices.size();
  if (count < 2) return;

  for (size_t i = 0; i + 1 < count; ++i) {
    visit(vertices[i].position, vertices[i + 1].position, vertices[i].bulge);
  }
  if (closed) {
    visit(vertices[count - 1].position, vertices[0].position,
          vertices[count - 1].bulge);
  }
}

}  // namespace

double polylineLength(const std::vector<PolyVertex>& vertices, bool closed) {
  double total = 0.0;
  forEachSegment(vertices, closed, [&](Vec2 start, Vec2 end, double bulge) {
    total += bulgeSegmentLength(start, end, bulge);
  });
  return total;
}

double polylineArea(const std::vector<PolyVertex>& vertices) {
  const size_t count = vertices.size();
  if (count < 2) return 0.0;

  // Área del polígono de cuerdas...
  std::vector<Vec2> corners;
  corners.reserve(count);
  for (const PolyVertex& vertex : vertices) corners.push_back(vertex.position);
  double total = signedArea(corners);

  // ...más el área de cada segmento circular, con signo.
  // Para un arco de radio R y ángulo θ: A = R²(θ - sen θ)/2, que es impar en θ,
  // de modo que un arco horario resta y uno antihorario suma. Así el arco que
  // sobresale del polígono suma y el que se mete hacia dentro resta.
  forEachSegment(vertices, /*closed=*/true, [&](Vec2 start, Vec2 end, double bulge) {
    Arc arc;
    if (!arcFromBulge(start, end, bulge, arc)) return;
    total += 0.5 * arc.radius * arc.radius * (arc.sweep - std::sin(arc.sweep));
  });

  return std::abs(total);
}

std::vector<Vec2> tessellatePolyline(const std::vector<PolyVertex>& vertices,
                                     bool closed, double maxSagitta) {
  std::vector<Vec2> points;
  if (vertices.empty()) return points;

  points.push_back(vertices.front().position);
  forEachSegment(vertices, closed, [&](Vec2 start, Vec2 end, double bulge) {
    Arc arc;
    if (!arcFromBulge(start, end, bulge, arc)) {
      points.push_back(end);
      return;
    }
    for (const Vec2& point : tessellateArc(arc, end, maxSagitta)) {
      points.push_back(point);
    }
  });
  return points;
}

}  // namespace dwgcore
