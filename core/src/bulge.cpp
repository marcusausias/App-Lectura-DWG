#include <algorithm>

#include "dwgcore/geometry.h"

namespace dwgcore {
namespace {

// Por debajo de este bulge el tramo se considera recto. Un bulge de 1e-10
// corresponde a un ángulo de 4e-10 rad: indistinguible de una recta a
// cualquier zoom, y evita dividir por cero al calcular el radio.
constexpr double kStraightBulge = 1e-10;

}  // namespace

bool arcFromBulge(Vec2 start, Vec2 end, double bulge, Arc& outArc) {
  if (std::abs(bulge) < kStraightBulge) return false;

  const Vec2 chord = end - start;
  const double chordLength = length(chord);
  if (chordLength <= 0.0) return false;

  // b = tan(θ/4)  ⇒  θ = 4·atan(b), con signo.
  const double sweep = 4.0 * std::atan(bulge);

  // R = L(1+b²)/4b   y   h = L(1-b²)/4b, donde h es la distancia con signo
  // desde el punto medio de la cuerda hasta el centro del arco.
  const double signedRadius = chordLength * (1.0 + bulge * bulge) / (4.0 * bulge);
  const double centerOffset = chordLength * (1.0 - bulge * bulge) / (4.0 * bulge);

  const Vec2 midpoint = (start + end) * 0.5;
  const Vec2 perpendicular = perpLeft(normalize(chord));

  outArc.center = midpoint + perpendicular * centerOffset;
  outArc.radius = std::abs(signedRadius);
  outArc.startAngle = std::atan2(start.y - outArc.center.y, start.x - outArc.center.x);
  outArc.sweep = sweep;
  return true;
}

double arcLength(const Arc& arc) { return arc.radius * std::abs(arc.sweep); }

double bulgeSegmentLength(Vec2 start, Vec2 end, double bulge) {
  Arc arc;
  if (!arcFromBulge(start, end, bulge, arc)) return distance(start, end);
  return arcLength(arc);
}

std::vector<Vec2> tessellateArc(const Arc& arc, Vec2 end, double maxSagitta,
                                int maxSegments) {
  std::vector<Vec2> points;
  if (arc.radius <= 0.0) {
    points.push_back(end);
    return points;
  }

  // Sagitta de un tramo que abarca Δ: s = R(1 - cos(Δ/2)).
  // Imponiendo s ≤ ε queda Δ ≤ 2·acos(1 - ε/R).
  const double cosLimit = std::clamp(1.0 - maxSagitta / arc.radius, -1.0, 1.0);
  const double maxStep = 2.0 * std::acos(cosLimit);

  int segments = 1;
  if (maxStep > 0.0) {
    segments = static_cast<int>(std::ceil(std::abs(arc.sweep) / maxStep));
  }
  segments = std::clamp(segments, 1, maxSegments);

  points.reserve(static_cast<size_t>(segments));
  for (int i = 1; i < segments; ++i) {
    const double angle =
        arc.startAngle + arc.sweep * (static_cast<double>(i) / segments);
    points.push_back({arc.center.x + arc.radius * std::cos(angle),
                      arc.center.y + arc.radius * std::sin(angle)});
  }
  // El último punto se toma del vértice original en lugar de recalcularlo:
  // así los polígonos cierran exactamente y no queda una rendija de 1e-15.
  points.push_back(end);
  return points;
}

}  // namespace dwgcore
