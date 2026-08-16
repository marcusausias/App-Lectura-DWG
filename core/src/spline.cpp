#include <algorithm>

#include "dwgcore/geometry.h"

namespace dwgcore {
namespace {

// Localiza el intervalo de nudos que contiene a t. El resultado se acota al
// rango válido [degree, lastControl] para que t en el extremo superior del
// dominio no se salga del vector.
int findKnotSpan(const std::vector<double>& knots, int degree, int controlCount,
                 double t) {
  const int lastControl = controlCount - 1;
  if (t <= knots[degree]) return degree;
  if (t >= knots[lastControl + 1]) return lastControl;

  int low = degree;
  int high = lastControl + 1;
  int mid = (low + high) / 2;
  while (t < knots[mid] || t >= knots[mid + 1]) {
    if (t < knots[mid]) {
      high = mid;
    } else {
      low = mid;
    }
    mid = (low + high) / 2;
  }
  return mid;
}

}  // namespace

Vec2 evaluateBSpline(const std::vector<Vec2>& controlPoints,
                     const std::vector<double>& knots, int degree, double t) {
  const int controlCount = static_cast<int>(controlPoints.size());
  if (controlCount == 0) return {};
  if (degree < 1 || controlCount <= degree) return controlPoints.front();
  if (static_cast<int>(knots.size()) < controlCount + degree + 1) {
    return controlPoints.front();
  }

  const int span = findKnotSpan(knots, degree, controlCount, t);

  // Algoritmo de De Boor: se parte de los degree+1 puntos de control que
  // influyen en este intervalo y se interpolan sucesivamente.
  std::vector<Vec2> working(static_cast<size_t>(degree) + 1);
  for (int j = 0; j <= degree; ++j) {
    working[static_cast<size_t>(j)] =
        controlPoints[static_cast<size_t>(j + span - degree)];
  }

  for (int round = 1; round <= degree; ++round) {
    for (int j = degree; j >= round; --j) {
      const double lower = knots[static_cast<size_t>(j + span - degree)];
      const double upper = knots[static_cast<size_t>(j + 1 + span - round)];
      const double span_width = upper - lower;
      // Un nudo repetido anula el intervalo: el peso se va entero al punto
      // anterior en vez de dividir por cero.
      const double alpha = span_width > 0.0 ? (t - lower) / span_width : 0.0;
      working[static_cast<size_t>(j)] =
          working[static_cast<size_t>(j - 1)] * (1.0 - alpha) +
          working[static_cast<size_t>(j)] * alpha;
    }
  }
  return working[static_cast<size_t>(degree)];
}

std::vector<Vec2> tessellateBSpline(const std::vector<Vec2>& controlPoints,
                                    const std::vector<double>& knots, int degree,
                                    int samples) {
  std::vector<Vec2> points;
  const int controlCount = static_cast<int>(controlPoints.size());
  if (controlCount == 0) return points;
  if (degree < 1 || controlCount <= degree ||
      static_cast<int>(knots.size()) < controlCount + degree + 1) {
    return controlPoints;
  }

  samples = std::max(samples, 2);
  const double start = knots[static_cast<size_t>(degree)];
  const double end = knots[static_cast<size_t>(controlCount)];

  points.reserve(static_cast<size_t>(samples));
  for (int i = 0; i < samples; ++i) {
    const double t = start + (end - start) * (static_cast<double>(i) / (samples - 1));
    points.push_back(evaluateBSpline(controlPoints, knots, degree, t));
  }
  return points;
}

}  // namespace dwgcore
