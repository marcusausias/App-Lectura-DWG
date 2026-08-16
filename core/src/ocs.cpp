#include "dwgcore/geometry.h"

namespace dwgcore {
namespace {

// Umbral del algoritmo del eje arbitrario, fijado por la especificación DXF.
constexpr double kAxisThreshold = 1.0 / 64.0;

}  // namespace

void arbitraryAxes(Vec3 normal, Vec3& outAxisX, Vec3& outAxisY) {
  const Vec3 unitNormal = normalize(normal);

  // Cuando la normal está muy próxima al eje Z global se toma Y como
  // referencia; en cualquier otro caso, Z. Es lo que evita que el eje X
  // resultante degenere a un vector nulo.
  const bool nearGlobalZ = std::abs(unitNormal.x) < kAxisThreshold &&
                           std::abs(unitNormal.y) < kAxisThreshold;
  const Vec3 reference = nearGlobalZ ? Vec3{0.0, 1.0, 0.0} : Vec3{0.0, 0.0, 1.0};

  outAxisX = normalize(cross(reference, unitNormal));
  outAxisY = normalize(cross(unitNormal, outAxisX));
}

Vec3 ocsToWcs(Vec3 point, Vec3 normal) {
  Vec3 axisX;
  Vec3 axisY;
  arbitraryAxes(normal, axisX, axisY);
  const Vec3 unitNormal = normalize(normal);
  return axisX * point.x + axisY * point.y + unitNormal * point.z;
}

}  // namespace dwgcore
