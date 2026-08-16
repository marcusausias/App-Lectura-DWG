// Tests de transformadas, entidades y aplanado de bloques.
//
// Estos son los que cubren los fallos que no se ven: un plano con los bloques
// mal colocados o en espejo se sigue dibujando, y solo se detecta comparando
// con AutoCAD.

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "dwgcore/entity.h"
#include "dwgcore/flatten.h"
#include "dwgcore/transform.h"

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
    std::printf("  FALLO: %s  (esperado %.12f, obtenido %.12f)\n", what.c_str(),
                expected, actual);
  }
}

constexpr double kTight = 1e-9;
const double kPi = std::acos(-1.0);

using namespace dwgcore;

void testInsertTransformBasics() {
  std::printf("transformada: traslación, escala y rotación\n");

  const Transform2D moved = insertTransform({10.0, 5.0}, 1.0, 1.0, 0.0);
  Vec2 point = apply(moved, {1.0, 2.0});
  checkNear(point.x, 11.0, kTight, "traslación X");
  checkNear(point.y, 7.0, kTight, "traslación Y");

  const Transform2D scaled = insertTransform({0.0, 0.0}, 2.0, 3.0, 0.0);
  point = apply(scaled, {1.0, 1.0});
  checkNear(point.x, 2.0, kTight, "escala X");
  checkNear(point.y, 3.0, kTight, "escala Y");

  // Rotación de 90°: (1,0) debe acabar en (0,1).
  const Transform2D rotated = insertTransform({0.0, 0.0}, 1.0, 1.0, kPi / 2.0);
  point = apply(rotated, {1.0, 0.0});
  checkNear(point.x, 0.0, kTight, "rotación X");
  checkNear(point.y, 1.0, kTight, "rotación Y");
}

void testConformalDetection() {
  std::printf("transformada: detección de conformidad y simetría\n");

  check(isConformal(insertTransform({0, 0}, 2.0, 2.0, 0.7)), "escala uniforme");
  check(isConformal(insertTransform({0, 0}, -2.0, 2.0, 0.0)), "espejo en X");
  check(!isConformal(insertTransform({0, 0}, 2.0, 3.0, 0.0)), "escala no uniforme");

  check(!isMirrored(insertTransform({0, 0}, 2.0, 2.0, 1.0)), "sin simetría");
  check(isMirrored(insertTransform({0, 0}, -1.0, 1.0, 0.0)), "espejo en X");
  check(isMirrored(insertTransform({0, 0}, 1.0, -1.0, 0.0)), "espejo en Y");
  // Reflejar en los dos ejes es girar 180°: no hay simetría.
  check(!isMirrored(insertTransform({0, 0}, -1.0, -1.0, 0.0)), "doble espejo = giro");

  checkNear(uniformScale(insertTransform({0, 0}, 3.0, 3.0, 0.4)), 3.0, kTight,
            "factor de escala");
}

void testConcatOrder() {
  std::printf("transformada: el orden de composición importa\n");
  // Trasladar y luego escalar no es lo mismo que escalar y luego trasladar.
  // Equivocarse aquí coloca cada bloque anidado en un sitio distinto del real.
  const Transform2D translate = insertTransform({10.0, 0.0}, 1.0, 1.0, 0.0);
  const Transform2D scale = insertTransform({0.0, 0.0}, 2.0, 2.0, 0.0);

  // concat(padre, hijo) aplica primero el hijo.
  const Vec2 scaleThenTranslate = apply(concat(translate, scale), {1.0, 0.0});
  checkNear(scaleThenTranslate.x, 12.0, kTight, "escala y después traslada");

  const Vec2 translateThenScale = apply(concat(scale, translate), {1.0, 0.0});
  checkNear(translateThenScale.x, 22.0, kTight, "traslada y después escala");
}

void testArcSurvivesConformalTransform() {
  std::printf("entidad: un arco sigue siendo exacto tras rotar y escalar\n");
  // Un cuarto de círculo de radio 1, insertado a escala 3 y girado.
  Entity arc = makeArc({0.0, 0.0}, 1.0, 0.0, kPi / 2.0);
  const double originalLength = entityLength(arc);
  checkNear(originalLength, kPi / 2.0, kTight, "longitud original = π/2");

  const Transform2D transform = insertTransform({100.0, 50.0}, 3.0, 3.0, 0.9);
  const Entity placed = transformEntity(arc, transform, 0.001);

  check(!placed.approximated, "no hace falta aproximar");
  checkNear(entityLength(placed), originalLength * 3.0, kTight,
            "la longitud escala exactamente por 3");
}

void testMirrorFlipsBulge() {
  std::printf("entidad: la simetría invierte el sentido del arco\n");
  Entity arc = makeArc({0.0, 0.0}, 1.0, 0.0, kPi / 2.0);
  const double bulgeBefore = arc.vertices.front().bulge;

  const Transform2D mirror = insertTransform({0.0, 0.0}, -1.0, 1.0, 0.0);
  const Entity placed = transformEntity(arc, mirror, 0.001);

  checkNear(placed.vertices.front().bulge, -bulgeBefore, kTight,
            "el bulge cambia de signo");
  // La longitud es la misma: reflejar no estira nada.
  checkNear(entityLength(placed), entityLength(arc), kTight, "misma longitud");
}

void testNonUniformScaleApproximates() {
  std::printf("entidad: la escala no uniforme obliga a teselar\n");
  // Un círculo escalado 2× en X y 1× en Y es una elipse, y una elipse no se
  // puede describir con bulges. La entidad debe quedar marcada para que la
  // interfaz no presente la medida como exacta.
  Entity circle = makeCircle({0.0, 0.0}, 1.0);
  check(!circle.approximated, "el círculo de partida es exacto");

  const Transform2D squash = insertTransform({0.0, 0.0}, 2.0, 1.0, 0.0);
  const Entity placed = transformEntity(circle, squash, 0.0001);

  check(placed.approximated, "queda marcada como aproximada");
  check(placed.vertices.size() > 8, "se ha teselado en varios vértices");
  for (const PolyVertex& vertex : placed.vertices) {
    checkNear(vertex.bulge, 0.0, 0.0, "sin bulges tras teselar");
  }

  // El perímetro de una elipse de semiejes 2 y 1 vale ~9,6884. Se comprueba que
  // el teselado se acerca, no que sea exacto.
  checkNear(entityLength(placed), 9.6884, 0.01, "perímetro de la elipse");
}

void testArcBoundsIncludeBulge() {
  std::printf("entidad: la caja envolvente contiene la curvatura\n");
  // Semicircunferencia de radio 1 entre (-1,0) y (1,0). Una caja calculada solo
  // con los extremos tendría altura cero y el arco desaparecería al recortar
  // por pantalla.
  //
  // Con bulge +1 el arco gira en sentido antihorario: sale de 180°, pasa por
  // 270° —que es el punto (0,-1)— y llega a 360°. O sea, va por debajo.
  Entity below;
  below.type = EntityType::Arc;
  below.vertices = {{{-1.0, 0.0}, 1.0}, {{1.0, 0.0}, 0.0}};
  updateBounds(below);

  check(below.bounds.valid, "caja válida");
  checkNear(below.bounds.min.x, -1.0, kTight, "borde izquierdo");
  checkNear(below.bounds.max.x, 1.0, kTight, "borde derecho");
  checkNear(below.bounds.max.y, 0.0, kTight, "borde superior a la altura de la cuerda");
  checkNear(below.bounds.min.y, -1.0, kTight, "borde inferior baja hasta el arco");

  // Con bulge -1 el mismo par de vértices describe el arco de arriba.
  Entity above;
  above.type = EntityType::Arc;
  above.vertices = {{{-1.0, 0.0}, -1.0}, {{1.0, 0.0}, 0.0}};
  updateBounds(above);

  checkNear(above.bounds.min.y, 0.0, kTight, "el arco opuesto no baja de la cuerda");
  checkNear(above.bounds.max.y, 1.0, kTight, "y sube hasta el alto del arco");
}

void testFullCircleArcBecomesCircle() {
  std::printf("entidad: un arco de 360° se construye como círculo\n");
  // tan(θ/4) se va a infinito en 360°. Sin la guarda, esta entidad
  // contaminaría con infinitos todo lo que la tocase.
  const Entity arc = makeArc({0.0, 0.0}, 2.0, 0.0, 2.0 * kPi);
  for (const PolyVertex& vertex : arc.vertices) {
    check(std::isfinite(vertex.bulge), "bulge finito");
  }
  checkNear(entityLength(arc), 2.0 * kPi * 2.0, kTight, "longitud = 2πR");
}

// --- Aplanado de bloques ---------------------------------------------------

BlockDefinition makeBlockWithLine(const std::string& name, Vec2 start, Vec2 end) {
  BlockDefinition block;
  block.name = name;
  block.entities.push_back(makeLine(start, end));
  return block;
}

void testFlattenSimpleInsert() {
  std::printf("aplanado: una inserción simple\n");
  std::map<std::string, BlockDefinition> blocks;
  blocks["PUERTA"] = makeBlockWithLine("PUERTA", {0, 0}, {1, 0});

  const std::vector<InsertRef> inserts = {
      {"PUERTA", insertTransform({10.0, 20.0}, 1.0, 1.0, 0.0)}};

  const FlattenResult result = flatten({}, inserts, blocks);
  check(result.entities.size() == 1, "una entidad");
  checkNear(result.entities[0].vertices[0].position.x, 10.0, kTight, "X colocada");
  checkNear(result.entities[0].vertices[1].position.x, 11.0, kTight, "extremo");
  check(result.entities[0].blockPath.size() == 1, "rastro del bloque");
}

void testFlattenNestedTransforms() {
  std::printf("aplanado: bloques anidados componen sus transformadas\n");
  std::map<std::string, BlockDefinition> blocks;
  blocks["HOJA"] = makeBlockWithLine("HOJA", {0, 0}, {1, 0});

  // VENTANA contiene HOJA desplazada 2 y a escala 2.
  BlockDefinition ventana;
  ventana.name = "VENTANA";
  ventana.inserts.push_back({"HOJA", insertTransform({2.0, 0.0}, 2.0, 2.0, 0.0)});
  blocks["VENTANA"] = ventana;

  // Y el espacio modelo inserta VENTANA desplazada 10 y a escala 3.
  const std::vector<InsertRef> inserts = {
      {"VENTANA", insertTransform({10.0, 0.0}, 3.0, 3.0, 0.0)}};

  const FlattenResult result = flatten({}, inserts, blocks);
  check(result.entities.size() == 1, "una entidad");

  // La línea local (0,0)-(1,0) queda: escalada ×2 y movida a 2 dentro de
  // VENTANA, y todo ello escalado ×3 y movido a 10. Inicio: 10 + 3·2 = 16.
  // Final: 10 + 3·(2 + 2·1) = 22.
  checkNear(result.entities[0].vertices[0].position.x, 16.0, kTight, "inicio");
  checkNear(result.entities[0].vertices[1].position.x, 22.0, kTight, "final");
  check(result.entities[0].blockPath.size() == 2, "rastro de dos niveles");
}

void testFlattenDetectsCycle() {
  std::printf("aplanado: un ciclo no cuelga la app\n");
  // A contiene B y B contiene A. Sin detección de ciclos esto no termina nunca.
  std::map<std::string, BlockDefinition> blocks;
  BlockDefinition a;
  a.name = "A";
  a.entities.push_back(makeLine({0, 0}, {1, 0}));
  a.inserts.push_back({"B", Transform2D{}});
  blocks["A"] = a;

  BlockDefinition b;
  b.name = "B";
  b.inserts.push_back({"A", Transform2D{}});
  blocks["B"] = b;

  const FlattenResult result = flatten({}, {{"A", Transform2D{}}}, blocks);
  check(result.skippedCyclic > 0, "detecta el ciclo");
  check(result.entities.size() == 1, "emite la geometría una sola vez");
}

void testFlattenDepthLimit() {
  std::printf("aplanado: se respeta el límite de profundidad\n");
  // Cadena de 12 bloques, cada uno insertando el siguiente.
  std::map<std::string, BlockDefinition> blocks;
  for (int i = 0; i < 12; ++i) {
    BlockDefinition block;
    block.name = "N" + std::to_string(i);
    block.entities.push_back(makeLine({0, 0}, {1, 0}));
    if (i < 11) {
      block.inserts.push_back({"N" + std::to_string(i + 1), Transform2D{}});
    }
    blocks[block.name] = block;
  }

  FlattenOptions options;
  options.maxDepth = 5;
  const FlattenResult result = flatten({}, {{"N0", Transform2D{}}}, blocks, options);

  check(result.skippedTooDeep > 0, "corta al llegar al límite");
  check(static_cast<int>(result.entities.size()) == options.maxDepth,
        "emite exactamente los niveles permitidos");
}

void testFlattenReportsMissingBlocks() {
  std::printf("aplanado: avisa de los bloques que faltan\n");
  // Es el síntoma de una referencia externa sin resolver. Dibujar el plano
  // incompleto sin avisar sería peor que no dibujarlo.
  std::map<std::string, BlockDefinition> blocks;
  const std::vector<InsertRef> inserts = {
      {"XREF-BASE", Transform2D{}},
      {"XREF-BASE", insertTransform({5.0, 0.0}, 1.0, 1.0, 0.0)},
  };

  const FlattenResult result = flatten({}, inserts, blocks);
  check(result.entities.empty(), "no inventa geometría");
  check(result.missingBlocks.size() == 1, "el bloque que falta se lista una vez");
  check(result.missingBlocks[0] == "XREF-BASE", "con su nombre");
}

}  // namespace

int main() {
  testInsertTransformBasics();
  testConformalDetection();
  testConcatOrder();
  testArcSurvivesConformalTransform();
  testMirrorFlipsBulge();
  testNonUniformScaleApproximates();
  testArcBoundsIncludeBulge();
  testFullCircleArcBecomesCircle();
  testFlattenSimpleInsert();
  testFlattenNestedTransforms();
  testFlattenDetectsCycle();
  testFlattenDepthLimit();
  testFlattenReportsMissingBlocks();

  std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
