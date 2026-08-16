#include "dwgcore/transform.h"

#include <cmath>

namespace dwgcore {

Transform2D insertTransform(Vec2 insertionPoint, double scaleX, double scaleY,
                            double rotation) {
  const double cosR = std::cos(rotation);
  const double sinR = std::sin(rotation);

  Transform2D transform;
  transform.a = scaleX * cosR;
  transform.b = scaleX * sinR;
  transform.c = -scaleY * sinR;
  transform.d = scaleY * cosR;
  transform.tx = insertionPoint.x;
  transform.ty = insertionPoint.y;
  return transform;
}

Vec2 apply(const Transform2D& transform, Vec2 point) {
  return {transform.a * point.x + transform.c * point.y + transform.tx,
          transform.b * point.x + transform.d * point.y + transform.ty};
}

Transform2D concat(const Transform2D& parent, const Transform2D& child) {
  Transform2D result;
  result.a = parent.a * child.a + parent.c * child.b;
  result.b = parent.b * child.a + parent.d * child.b;
  result.c = parent.a * child.c + parent.c * child.d;
  result.d = parent.b * child.c + parent.d * child.d;
  result.tx = parent.a * child.tx + parent.c * child.ty + parent.tx;
  result.ty = parent.b * child.tx + parent.d * child.ty + parent.ty;
  return result;
}

double determinant(const Transform2D& transform) {
  return transform.a * transform.d - transform.b * transform.c;
}

bool isConformal(const Transform2D& transform, double tolerance) {
  // Las dos columnas de la parte lineal deben ser perpendiculares y medir lo
  // mismo. Si se cumple, la transformada solo rota, escala por igual y quizá
  // refleja.
  const Vec2 columnX{transform.a, transform.b};
  const Vec2 columnY{transform.c, transform.d};

  const double lengthX = length(columnX);
  const double lengthY = length(columnY);
  if (lengthX <= 0.0 || lengthY <= 0.0) return false;

  // La tolerancia se aplica en relativo: un bloque insertado a escala 1000
  // acumula error absoluto proporcional a su tamaño.
  const double scale = (lengthX + lengthY) * 0.5;
  if (std::abs(lengthX - lengthY) > tolerance * scale) return false;
  if (std::abs(dot(columnX, columnY)) > tolerance * scale * scale) return false;
  return true;
}

double uniformScale(const Transform2D& transform) {
  return std::sqrt(std::abs(determinant(transform)));
}

}  // namespace dwgcore
