// Tests del núcleo geométrico.
//
// Sin dependencias externas a propósito: los mismos tests deben poder
// compilarse con el NDK y ejecutarse en el dispositivo si algún día hace falta
// descartar una diferencia de coma flotante entre el equipo y el móvil.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dwgcore/geometry.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::printf("  FALLO: %s\n", what.c_str());
  }
}

void checkNear(double actual, double expected, double tolerance,
               const std::string& what) {
  ++g_checks;
  const double error = std::abs(actual - expected);
  if (!(error <= tolerance)) {
    ++g_failures;
    std::printf("  FALLO: %s  (esperado %.12f, obtenido %.12f, error %.3e)\n",
                what.c_str(), expected, actual, error);
  }
}

constexpr double kTight = 1e-9;
const double kPi = std::acos(-1.0);

using namespace dwgcore;

// --- Bulge -----------------------------------------------------------------

void testBulgeQuarterCircle() {
  std::printf("bulge: arco de 90°\n");
  // Cuerda unidad de (0,0) a (1,0) con bulge = tan(90°/4) = tan(22.5°).
  const double bulge = std::tan(kPi / 8.0);
  Arc arc;
  check(arcFromBulge({0.0, 0.0}, {1.0, 0.0}, bulge, arc), "debe producir arco");

  checkNear(arc.sweep, kPi / 2.0, kTight, "ángulo abarcado = 90°");
  checkNear(arc.radius, 1.0 / std::sqrt(2.0), kTight, "radio = 1/√2");
  checkNear(arc.center.x, 0.5, kTight, "centro X");
  // Con bulge positivo el arco gira en sentido antihorario y el centro queda a
  // la izquierda de la cuerda. Si este signo se invierte, todos los arcos del
  // plano salen reflejados.
  checkNear(arc.center.y, 0.5, kTight, "centro Y (a la izquierda de la cuerda)");

  // El extremo del arco debe caer exactamente sobre el vértice final.
  const double endAngle = arc.startAngle + arc.sweep;
  checkNear(arc.center.x + arc.radius * std::cos(endAngle), 1.0, kTight, "extremo X");
  checkNear(arc.center.y + arc.radius * std::sin(endAngle), 0.0, kTight, "extremo Y");
}

void testBulgeSignFlipsCenter() {
  std::printf("bulge: el signo invierte el lado del centro\n");
  const double bulge = std::tan(kPi / 8.0);
  Arc positive;
  Arc negative;
  arcFromBulge({0.0, 0.0}, {1.0, 0.0}, bulge, positive);
  arcFromBulge({0.0, 0.0}, {1.0, 0.0}, -bulge, negative);

  checkNear(negative.center.y, -positive.center.y, kTight, "centros simétricos");
  checkNear(negative.sweep, -positive.sweep, kTight, "ángulos opuestos");
  checkNear(arcLength(negative), arcLength(positive), kTight, "misma longitud");
}

void testBulgeSemicircle() {
  std::printf("bulge: semicircunferencia\n");
  // bulge = 1 equivale a θ = 180°.
  Arc arc;
  check(arcFromBulge({0.0, 0.0}, {2.0, 0.0}, 1.0, arc), "debe producir arco");
  checkNear(arc.radius, 1.0, kTight, "radio = 1");
  checkNear(arc.sweep, kPi, kTight, "ángulo = 180°");
  checkNear(arcLength(arc), kPi, kTight, "longitud = π·R");
}

void testStraightSegment() {
  std::printf("bulge: tramo recto\n");
  Arc arc;
  check(!arcFromBulge({0.0, 0.0}, {3.0, 4.0}, 0.0, arc), "bulge 0 no es arco");
  checkNear(bulgeSegmentLength({0.0, 0.0}, {3.0, 4.0}, 0.0), 5.0, kTight,
            "longitud = distancia euclídea");
}

void testAnalyticLengthBeatsTessellation() {
  std::printf("bulge: la longitud analítica no depende del teselado\n");
  // Este es el motivo por el que las mediciones no se calculan sobre el
  // teselado: la poligonal siempre queda corta, y el error crece cuanto más
  // basto es el teselado.
  Arc arc;
  arcFromBulge({0.0, 0.0}, {2.0, 0.0}, 1.0, arc);
  const double exact = arcLength(arc);

  Vec2 previous{0.0, 0.0};
  double tessellated = 0.0;
  for (const Vec2& point : tessellateArc(arc, {2.0, 0.0}, 0.05)) {
    tessellated += distance(previous, point);
    previous = point;
  }
  check(tessellated < exact, "la poligonal es más corta que el arco");
  check(exact - tessellated > 1e-6, "con teselado basto el error es apreciable");

  // Afinando el teselado converge al valor analítico: confirma que ambos
  // caminos describen la misma curva y que el error es solo de discretización.
  previous = {0.0, 0.0};
  double fine = 0.0;
  for (const Vec2& point : tessellateArc(arc, {2.0, 0.0}, 1e-7, 4096)) {
    fine += distance(previous, point);
    previous = point;
  }
  checkNear(fine, exact, 1e-4, "el teselado fino converge a la longitud exacta");
}

void testTessellationEndsExactly() {
  std::printf("teselado: el último punto es el vértice original\n");
  Arc arc;
  arcFromBulge({1.0, 2.0}, {5.0, 7.0}, 0.6, arc);
  const std::vector<Vec2> points = tessellateArc(arc, {5.0, 7.0}, 0.01);
  check(!points.empty(), "genera puntos");
  // Si el último punto se recalculase por trigonometría quedaría una rendija de
  // ~1e-15 que impide cerrar polígonos y rompe el cálculo de áreas.
  checkNear(points.back().x, 5.0, 0.0, "X exacta, sin tolerancia");
  checkNear(points.back().y, 7.0, 0.0, "Y exacta, sin tolerancia");
}

// --- Áreas -----------------------------------------------------------------

void testSquareArea() {
  std::printf("área: cuadrado unidad\n");
  const std::vector<Vec2> square = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  checkNear(polygonArea(square), 1.0, kTight, "área = 1");
  check(signedArea(square) > 0.0, "recorrido antihorario da área positiva");

  const std::vector<Vec2> clockwise = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
  check(signedArea(clockwise) < 0.0, "recorrido horario da área negativa");
  checkNear(polygonArea(clockwise), 1.0, kTight, "el valor absoluto no cambia");
}

void testCircleAreaFromTwoBulges() {
  std::printf("área: círculo formado por dos bulges\n");
  // Una circunferencia de radio 1 descrita como dos semicircunferencias.
  // Si se ignorasen los bulges el área saldría 0, porque las dos cuerdas son
  // el mismo segmento.
  const std::vector<PolyVertex> circle = {
      {{0.0, 0.0}, 1.0},
      {{2.0, 0.0}, 1.0},
  };
  checkNear(polylineArea(circle), kPi, kTight, "área = π·R²");
  checkNear(polylineLength(circle, /*closed=*/true), 2.0 * kPi, kTight,
            "perímetro = 2π·R");
}

void testRoundedSquareArea() {
  std::printf("área: cuadrado con una esquina redondeada\n");
  // Cuadrado 2×2 recorrido en sentido antihorario, con la esquina (2,2)
  // sustituida por un cuarto de círculo de radio 1 entre (2,1) y (1,2).
  //
  // El signo del bulge decide de qué lado se curva el arco, y con él si el
  // redondeo quita o añade material. Se comprueban los dos sentidos porque
  // invertir este signo es un error que dibuja el plano entero en espejo sin
  // dar ningún aviso.
  const double bulge = std::tan(kPi / 8.0);
  const double chordPolygon = 3.5;                 // el cuadrado menos el triángulo
  const double segment = 0.5 * (kPi / 2.0 - 1.0);  // área entre cuerda y arco

  // Bulge positivo: el arco se abomba hacia la esquina. Es el redondeo normal
  // de AutoCAD, que solo recorta el trozo entre la esquina y el arco.
  const std::vector<PolyVertex> convex = {
      {{0.0, 0.0}, 0.0}, {{2.0, 0.0}, 0.0},   {{2.0, 1.0}, bulge},
      {{1.0, 2.0}, 0.0}, {{0.0, 2.0}, 0.0},
  };
  checkNear(polylineArea(convex), chordPolygon + segment, kTight,
            "esquina redondeada convexa = 4 - (1 - π/4)");
  checkNear(polylineArea(convex), 4.0 - (1.0 - kPi / 4.0), kTight,
            "coincide con el cálculo directo");

  // Bulge negativo: el arco se hunde hacia dentro y quita más material.
  const std::vector<PolyVertex> concave = {
      {{0.0, 0.0}, 0.0}, {{2.0, 0.0}, 0.0},   {{2.0, 1.0}, -bulge},
      {{1.0, 2.0}, 0.0}, {{0.0, 2.0}, 0.0},
  };
  checkNear(polylineArea(concave), chordPolygon - segment, kTight,
            "esquina cóncava resta el mismo segmento");
}

void testPolylineLengthClosedSquare() {
  std::printf("longitud: cuadrado cerrado\n");
  const std::vector<PolyVertex> square = {
      {{0, 0}, 0.0}, {{1, 0}, 0.0}, {{1, 1}, 0.0}, {{0, 1}, 0.0}};
  checkNear(polylineLength(square, /*closed=*/false), 3.0, kTight, "abierto = 3");
  checkNear(polylineLength(square, /*closed=*/true), 4.0, kTight, "cerrado = 4");
}

// --- OCS -------------------------------------------------------------------

void testOcsIdentity() {
  std::printf("OCS: normal (0,0,1) no altera nada\n");
  const Vec3 point = ocsToWcs({3.0, 4.0, 5.0}, {0.0, 0.0, 1.0});
  checkNear(point.x, 3.0, kTight, "X");
  checkNear(point.y, 4.0, kTight, "Y");
  checkNear(point.z, 5.0, kTight, "Z");
}

void testOcsMirroredNormal() {
  std::printf("OCS: normal (0,0,-1) refleja el eje X\n");
  // Es el caso clásico: cualquier objeto hecho con simetría queda con la
  // extrusión invertida. Ignorarlo dibuja el plano en espejo.
  const Vec3 point = ocsToWcs({1.0, 0.0, 0.0}, {0.0, 0.0, -1.0});
  checkNear(point.x, -1.0, kTight, "X invertida");
  checkNear(point.y, 0.0, kTight, "Y sin cambios");
}

void testOcsAxesAreOrthonormal() {
  std::printf("OCS: los ejes generados son ortonormales\n");
  const std::vector<Vec3> normals = {
      {0.0, 0.0, 1.0},  {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0},
      {0.0, 1.0, 0.0},  {1.0, 2.0, 3.0},  {0.001, 0.001, 1.0}};

  for (const Vec3& normal : normals) {
    Vec3 axisX;
    Vec3 axisY;
    arbitraryAxes(normal, axisX, axisY);
    checkNear(length(axisX), 1.0, kTight, "eje X unitario");
    checkNear(length(axisY), 1.0, kTight, "eje Y unitario");

    const Vec3 unitNormal = normalize(normal);
    const double dotXY = axisX.x * axisY.x + axisX.y * axisY.y + axisX.z * axisY.z;
    const double dotXN =
        axisX.x * unitNormal.x + axisX.y * unitNormal.y + axisX.z * unitNormal.z;
    checkNear(dotXY, 0.0, kTight, "X ⟂ Y");
    checkNear(dotXN, 0.0, kTight, "X ⟂ normal");
  }
}

// --- Splines ---------------------------------------------------------------

void testSplineHitsEndpoints() {
  std::printf("spline: una B-spline sujeta pasa por sus extremos\n");
  const std::vector<Vec2> control = {{0, 0}, {1, 2}, {3, 3}, {5, 1}, {6, 2}};
  const std::vector<double> knots = {0, 0, 0, 0, 1, 2, 2, 2, 2};
  const int degree = 3;

  const Vec2 first = evaluateBSpline(control, knots, degree, 0.0);
  checkNear(first.x, 0.0, kTight, "arranca en el primer punto de control (X)");
  checkNear(first.y, 0.0, kTight, "arranca en el primer punto de control (Y)");

  const Vec2 last = evaluateBSpline(control, knots, degree, 2.0);
  checkNear(last.x, 6.0, kTight, "termina en el último punto de control (X)");
  checkNear(last.y, 2.0, kTight, "termina en el último punto de control (Y)");
}

void testSplineCollinearStaysStraight() {
  std::printf("spline: puntos de control alineados dan una recta\n");
  const std::vector<Vec2> control = {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}};
  const std::vector<double> knots = {0, 0, 0, 0, 1, 2, 2, 2, 2};

  for (const Vec2& point : tessellateBSpline(control, knots, 3, 25)) {
    checkNear(point.y, point.x, kTight, "el punto cae sobre y = x");
  }
}

void testSplineTessellationCount() {
  std::printf("spline: el teselado devuelve el número de muestras pedido\n");
  const std::vector<Vec2> control = {{0, 0}, {1, 2}, {3, 3}, {5, 1}, {6, 2}};
  const std::vector<double> knots = {0, 0, 0, 0, 1, 2, 2, 2, 2};
  check(tessellateBSpline(control, knots, 3, 40).size() == 40, "40 muestras");
}

void testDegenerateInputsAreSafe() {
  std::printf("robustez: entradas degeneradas no revientan\n");
  // Un DWG real trae geometría rota: polilíneas de un solo punto, splines sin
  // nudos suficientes, vértices duplicados. Nada de esto puede tumbar la app.
  check(polylineArea({}) == 0.0, "polilínea vacía");
  check(polylineArea({{{0, 0}, 0.0}}) == 0.0, "un solo vértice");
  check(polylineLength({{{0, 0}, 0.0}}, true) == 0.0, "longitud de un vértice");
  check(polygonArea({{0, 0}, {1, 1}}) == 0.0, "dos puntos no encierran área");

  Arc arc;
  check(!arcFromBulge({2.0, 2.0}, {2.0, 2.0}, 0.5, arc), "vértices coincidentes");

  const std::vector<Vec2> control = {{0, 0}, {1, 1}};
  const std::vector<double> knots = {0, 0, 1, 1};
  evaluateBSpline(control, knots, 3, 0.5);  // grado mayor que los puntos
  evaluateBSpline({}, {}, 3, 0.5);          // sin puntos de control
}

}  // namespace

int main() {
  testBulgeQuarterCircle();
  testBulgeSignFlipsCenter();
  testBulgeSemicircle();
  testStraightSegment();
  testAnalyticLengthBeatsTessellation();
  testTessellationEndsExactly();
  testSquareArea();
  testCircleAreaFromTwoBulges();
  testRoundedSquareArea();
  testPolylineLengthClosedSquare();
  testOcsIdentity();
  testOcsMirroredNormal();
  testOcsAxesAreOrthonormal();
  testSplineHitsEndpoints();
  testSplineCollinearStaysStraight();
  testSplineTessellationCount();
  testDegenerateInputsAreSafe();

  std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
