#include "dwgcore/geometry.h"

namespace dwgcore {

double signedArea(const std::vector<Vec2>& points) {
  const size_t count = points.size();
  if (count < 3) return 0.0;

  // Fórmula de Gauss. Se recorre cerrando el polígono contra el primer punto.
  double twiceArea = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const Vec2& current = points[i];
    const Vec2& next = points[(i + 1) % count];
    twiceArea += cross(current, next);
  }
  return twiceArea * 0.5;
}

double polygonArea(const std::vector<Vec2>& points) {
  return std::abs(signedArea(points));
}

}  // namespace dwgcore
