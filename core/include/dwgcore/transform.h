// Transformadas afines 2D para insertar bloques.
//
// Un INSERT aplica traslación, escala (que puede ser negativa, o sea espejo) y
// rotación, y puede contener otros INSERT dentro. Componer estas matrices en el
// orden equivocado descoloca puertas, ventanas y mobiliario por todo el plano.
#pragma once

#include "dwgcore/vec.h"

namespace dwgcore {

// Matriz afín 2×3:
//
//   | a  c  tx |
//   | b  d  ty |
//
// La parte 2×2 (a, b, c, d) es lineal; (tx, ty) es la traslación.
struct Transform2D {
  double a = 1.0, b = 0.0;
  double c = 0.0, d = 1.0;
  double tx = 0.0, ty = 0.0;
};

// Transformada de un INSERT. `rotation` en radianes.
Transform2D insertTransform(Vec2 insertionPoint, double scaleX, double scaleY,
                            double rotation);

// Aplica la transformada a un punto.
Vec2 apply(const Transform2D& transform, Vec2 point);

// Compone dos transformadas: el resultado aplica primero `child` y luego
// `parent`. Este orden es el que hace que un bloque anidado herede
// correctamente la colocación de su contenedor.
Transform2D concat(const Transform2D& parent, const Transform2D& child);

// Determinante de la parte lineal. Negativo cuando la transformada incluye una
// simetría, que es el caso de escala negativa en un solo eje.
double determinant(const Transform2D& transform);

inline bool isMirrored(const Transform2D& transform) {
  return determinant(transform) < 0.0;
}

// Una transformada es conforme cuando conserva los ángulos: rotación, escala
// uniforme y simetría, pero no escalados distintos en X e Y.
//
// Importa porque bajo una transformada conforme un arco sigue siendo un arco y
// el bulge no cambia (solo cambia de signo si hay simetría), de modo que la
// medición sigue siendo exacta. Con escala no uniforme el arco se convierte en
// una elipse y ya no hay más remedio que teselarlo.
bool isConformal(const Transform2D& transform, double tolerance = 1e-9);

// Factor de escala de una transformada conforme. Sin sentido si no lo es.
double uniformScale(const Transform2D& transform);

}  // namespace dwgcore
