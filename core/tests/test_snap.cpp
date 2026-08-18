// Tests del enganche y de la medición.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dwgcore/measure.h"
#include "dwgcore/snap.h"

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
  if (!(std::abs(actual - expected) <= tolerance)) {
    ++g_failures;
    std::printf("  FALLO: %s  (esperado %.9f, obtenido %.9f)\n", what.c_str(),
                expected, actual);
  }
}

constexpr double kTight = 1e-9;
using namespace dwgcore;

void testEndpointWins() {
  std::printf("enganche: gana el extremo sobre el punto suelto\n");
  // Un cuadrado de 10×10. Tocando cerca de la esquina, el enganche tiene que
  // ser la esquina exacta, no un punto cualquiera del lado.
  Scene scene = buildScene({makeLine({0, 0}, {10, 0}), makeLine({10, 0}, {10, 10})});

  SnapOptions options;
  options.radius = 1.0;
  const SnapResult result = snap(scene, {9.7, 0.2}, options);

  check(result.found(), "encuentra algo");
  check(result.type == SnapType::Endpoint, "es un extremo");
  checkNear(result.point.x, 10.0, kTight, "X exacta de la esquina");
  checkNear(result.point.y, 0.0, kTight, "Y exacta de la esquina");
}

void testMidpoint() {
  std::printf("enganche: punto medio\n");
  Scene scene = buildScene({makeLine({0, 0}, {10, 0})});

  SnapOptions options;
  options.radius = 1.0;
  const SnapResult result = snap(scene, {5.1, 0.3}, options);

  check(result.type == SnapType::Midpoint, "es el punto medio");
  checkNear(result.point.x, 5.0, kTight, "en el centro del tramo");
}

void testIntersection() {
  std::printf("enganche: cruce de dos líneas\n");
  // Dos líneas que se cruzan en (5,5) sin compartir ningún vértice: el cruce
  // solo aparece si se calcula, no está dibujado en ninguna parte.
  Scene scene = buildScene({makeLine({0, 5}, {10, 5}), makeLine({5, 0}, {5, 10})});

  SnapOptions options;
  options.radius = 1.0;
  const SnapResult result = snap(scene, {5.2, 5.2}, options);

  check(result.type == SnapType::Intersection, "es una intersección");
  checkNear(result.point.x, 5.0, kTight, "X del cruce");
  checkNear(result.point.y, 5.0, kTight, "Y del cruce");
}

void testPerpendicular() {
  std::printf("enganche: perpendicular desde el punto anterior\n");
  // Midiendo el ancho de una estancia: desde un punto de la pared de abajo,
  // la perpendicular a la de arriba da el ancho real y no una diagonal.
  Scene scene = buildScene({makeLine({0, 10}, {20, 10})});

  SnapOptions options;
  options.radius = 3.0;
  options.hasReference = true;
  options.reference = {7.0, 0.0};
  options.endpoint = false;
  options.midpoint = false;
  options.intersection = false;

  const SnapResult result = snap(scene, {7.4, 9.5}, options);

  check(result.type == SnapType::Perpendicular, "es perpendicular");
  checkNear(result.point.x, 7.0, kTight, "cae justo encima de la referencia");
  checkNear(result.point.y, 10.0, kTight, "sobre la pared");
}

void testSnapRespectsRadius() {
  std::printf("enganche: no engancha a lo que queda lejos\n");
  Scene scene = buildScene({makeLine({0, 0}, {10, 0})});

  SnapOptions options;
  options.radius = 0.5;
  check(!snap(scene, {5.0, 3.0}, options).found(), "fuera del radio no engancha");
  check(snap(scene, {5.0, 0.2}, options).found(), "dentro del radio sí");
}

void testHiddenLayersDoNotSnap() {
  std::printf("enganche: lo que está oculto no engancha\n");
  // Enganchar a una capa apagada daría medidas que el usuario no puede
  // explicar, porque no ve de dónde salen.
  Entity visible = makeLine({0, 0}, {10, 0});
  visible.layer = "MUROS";
  Entity hidden = makeLine({0, 1}, {10, 1});
  hidden.layer = "INSTALACIONES";

  Scene scene = buildScene({visible, hidden});
  const uint16_t hiddenId = scene.entities[1].layerId;

  SnapOptions options;
  options.radius = 2.0;

  const SnapResult withAll = snap(scene, {5.0, 0.9}, options);
  check(withAll.found(), "con todo visible engancha");

  const SnapResult withHidden = snap(scene, {5.0, 0.9}, options, {hiddenId});
  check(withHidden.found(), "sigue habiendo algo que enganchar");
  check(withHidden.point.y < 0.5, "engancha a la capa visible, no a la oculta");
}

void testSnapOnCurve() {
  std::printf("enganche: sobre un arco\n");
  // Semicircunferencia de radio 5 centrada en el origen, por debajo.
  Entity arc;
  arc.type = EntityType::Arc;
  arc.vertices = {{{-5.0, 0.0}, 1.0}, {{5.0, 0.0}, 0.0}};
  updateBounds(arc);

  Scene scene = buildScene({arc});

  SnapOptions options;
  options.radius = 1.0;
  options.endpoint = false;
  options.midpoint = false;
  options.intersection = false;

  // El punto más bajo del arco es (0,-5).
  const SnapResult result = snap(scene, {0.0, -4.6}, options);
  check(result.found(), "engancha al arco");
  // El teselado deja el enganche muy cerca de la curva real.
  checkNear(std::hypot(result.point.x, result.point.y), 5.0, 0.05,
            "el punto está sobre la circunferencia");
}

void testPickEntity() {
  std::printf("selección: se elige la entidad más cercana al toque\n");
  Scene scene = buildScene({makeLine({0, 0}, {10, 0}), makeLine({0, 5}, {10, 5})});

  check(pickEntity(scene, {5.0, 0.2}, 1.0) == 0, "la de abajo");
  check(pickEntity(scene, {5.0, 4.8}, 1.0) == 1, "la de arriba");
  check(pickEntity(scene, {5.0, 2.5}, 0.5) == kNoEntity, "nada cerca");
}

// --- Medición --------------------------------------------------------------

void testDistanceAndPolyline() {
  std::printf("medición: distancia y polilínea encadenada\n");
  Measurement measurement = measureDistance({0, 0}, {3, 4});
  checkNear(measurement.value, 5.0, kTight, "distancia 3-4-5");
  check(measurement.kind == MeasureKind::Distance, "tipo distancia");

  measurement = measureChain({{0, 0}, {3, 4}, {3, 14}});
  checkNear(measurement.value, 15.0, kTight, "suma de los dos tramos");
  check(measurement.kind == MeasureKind::Chain, "tipo polilínea");
}

void testAreaMeasurement() {
  std::printf("medición: superficie\n");
  const Measurement measurement =
      measureArea({{0, 0}, {4, 0}, {4, 3}, {0, 3}});
  checkNear(measurement.value, 12.0, kTight, "área del rectángulo");
  check(measurement.kind == MeasureKind::Area, "tipo área");

  // Con menos de tres puntos no hay superficie que medir.
  check(measureArea({{0, 0}, {1, 1}}).value == 0.0, "dos puntos no encierran nada");
}

void testFollowEntity() {
  std::printf("medición: seguir una entidad del plano\n");
  // Un tabique dibujado como polilínea con un tramo curvo: seguirlo de un
  // toque tiene que dar la longitud exacta, arco incluido.
  std::vector<PolyVertex> wall = {
      {{0.0, 0.0}, 0.0},
      {{10.0, 0.0}, 1.0},  // semicircunferencia de radio 2.5
      {{15.0, 0.0}, 0.0},
  };
  Entity entity = makePolyline(wall, false);
  Scene scene = buildScene({entity});

  const Measurement measurement = measureEntity(scene, 0);
  check(measurement.kind == MeasureKind::Entity, "tipo entidad");
  // 10 rectos + π·2.5 de arco + 0 (el último bulge no abre tramo).
  checkNear(measurement.value, 10.0 + std::acos(-1.0) * 2.5, kTight,
            "longitud analítica, con el arco exacto");
  check(!measurement.approximate, "no es aproximada");
}

void testFollowApproximatedEntity() {
  std::printf("medición: una entidad aproximada se declara como tal\n");
  // Una spline o una elipse llegan ya teseladas: su longitud es buena, pero no
  // exacta, y la interfaz tiene que poder decirlo en vez de dar una cifra
  // aparentemente exacta.
  Entity spline = makePolyline({{{0, 0}, 0.0}, {{1, 0}, 0.0}, {{2, 1}, 0.0}}, false);
  spline.type = EntityType::Spline;
  spline.approximated = true;

  Scene scene = buildScene({spline});
  check(measureEntity(scene, 0).approximate, "se marca como aproximada");
}

void testCountInstances() {
  std::printf("medición: contar símbolos iguales\n");
  Scene scene;
  scene.blocks = {BlockInfo{}, BlockInfo{"PUERTA", 12}, BlockInfo{"VENTANA", 5}};

  Entity door = makeLine({0, 0}, {1, 0});
  door.blockId = 1;
  scene.entities = {door};

  const Measurement measurement = measureCount(scene, 0);
  check(measurement.kind == MeasureKind::Count, "tipo recuento");
  check(measurement.count == 12, "doce puertas");
  check(measurement.label == "PUERTA", "con el nombre del símbolo");

  // Una entidad suelta no pertenece a ningún símbolo.
  Entity loose = makeLine({0, 0}, {1, 0});
  Scene plain;
  plain.blocks = {BlockInfo{}};
  plain.entities = {loose};
  check(measureCount(plain, 0).count == 0, "sin símbolo no hay nada que contar");
}

void testMeasurementsOutOfRange() {
  std::printf("medición: índices fuera de rango no revientan\n");
  Scene scene = buildScene({makeLine({0, 0}, {1, 0})});
  check(measureEntity(scene, 99).value == 0.0, "entidad inexistente");
  check(measureCount(scene, 99).count == 0, "recuento de entidad inexistente");
}

}  // namespace

int main() {
  testEndpointWins();
  testMidpoint();
  testIntersection();
  testPerpendicular();
  testSnapRespectsRadius();
  testHiddenLayersDoNotSnap();
  testSnapOnCurve();
  testPickEntity();
  testDistanceAndPolyline();
  testAreaMeasurement();
  testFollowEntity();
  testFollowApproximatedEntity();
  testCountInstances();
  testMeasurementsOutOfRange();

  std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
