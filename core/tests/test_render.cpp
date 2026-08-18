// Tests de la preparación de buffers de dibujo.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dwgcore/render.h"

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

using namespace dwgcore;

Entity lineOnLayer(Vec2 from, Vec2 to, const std::string& layer) {
  Entity entity = makeLine(from, to);
  entity.layer = layer;
  return entity;
}

void testBatchesCoverBufferExactly() {
  std::printf("render: los lotes cubren el buffer entero sin solaparse\n");
  std::vector<Entity> entities;
  for (int i = 0; i < 60; ++i) {
    const std::string layer = (i % 3 == 0) ? "MUROS" : (i % 3 == 1 ? "COTAS" : "EJES");
    entities.push_back(lineOnLayer({static_cast<double>(i), 0.0},
                                   {static_cast<double>(i) + 1.0, 1.0}, layer));
  }

  const Scene scene = buildScene(std::move(entities));
  const RenderData data = buildRenderData(scene);

  check(data.batches.size() == 3, "un lote por capa");

  // Todo vértice del buffer tiene que pertenecer a exactamente un lote: si
  // sobra alguno no se dibuja, y si se solapan se dibuja dos veces.
  size_t covered = 0;
  uint32_t previousEnd = 0;
  bool contiguous = true;
  bool sortedByLayer = true;
  uint16_t lastLayer = 0;
  for (size_t i = 0; i < data.batches.size(); ++i) {
    const RenderBatch& batch = data.batches[i];
    if (batch.firstVertex != previousEnd) contiguous = false;
    previousEnd = batch.firstVertex + batch.vertexCount;
    covered += batch.vertexCount;
    if (i > 0 && batch.layerId < lastLayer) sortedByLayer = false;
    lastLayer = batch.layerId;
  }

  check(covered == data.vertexCount(), "los lotes suman todos los vértices");
  check(contiguous, "los lotes son contiguos, sin huecos");
  check(sortedByLayer, "los lotes van ordenados por capa");
  check(previousEnd == data.vertexCount(), "el último lote llega al final");

  // 60 líneas, un segmento cada una, dos vértices por segmento.
  check(data.segmentCount() == 60, "60 segmentos");
}

void testCoordinatesAreRelativeToOrigin() {
  std::printf("render: los vértices son relativos al origen de la escena\n");
  // Coordenadas grandes, como las de un plano con origen UTM. Es justo el caso
  // en el que subir el valor absoluto como float daría error visible.
  const Vec2 from{728500.25, 4373200.5};
  const Vec2 to{728510.75, 4373210.5};

  const Scene scene = buildScene({makeLine(from, to)});
  const RenderData data = buildRenderData(scene);

  check(data.vertices.size() == 4, "un segmento");

  // Sumar el origen a lo que se sube tiene que devolver la coordenada real.
  const double recoveredX = data.origin.x + data.vertices[0];
  const double recoveredY = data.origin.y + data.vertices[1];
  check(std::abs(recoveredX - from.x) < 0.01, "X recuperada");
  check(std::abs(recoveredY - from.y) < 0.01, "Y recuperada");

  // Y lo que llega a la GPU tiene que ser pequeño: si los valores relativos
  // siguieran siendo enormes, restar el origen no habría servido de nada.
  for (float value : data.vertices) {
    check(std::abs(value) < 1000.0f, "el valor subido es pequeño");
  }
}

void testCurvesProduceMoreSegments() {
  std::printf("render: un arco genera más segmentos que la recta equivalente\n");
  const Scene straight = buildScene({makeLine({0, 0}, {10, 0})});

  Entity arc;
  arc.type = EntityType::Arc;
  arc.vertices = {{{0.0, 0.0}, 1.0}, {{10.0, 0.0}, 0.0}};
  updateBounds(arc);
  const Scene curved = buildScene({arc});

  const RenderData straightData = buildRenderData(straight);
  const RenderData curvedData = buildRenderData(curved);

  check(straightData.segmentCount() == 1, "la recta es un solo segmento");
  check(curvedData.segmentCount() > 20, "el arco se parte en muchos");

  // Y la poligonal generada debe aproximar la longitud real del arco, que es
  // πR = π·5. Si el teselado fuese basto se quedaría notablemente corta.
  double drawnLength = 0.0;
  for (size_t i = 0; i + 3 < curvedData.vertices.size(); i += 4) {
    const double dx = curvedData.vertices[i + 2] - curvedData.vertices[i];
    const double dy = curvedData.vertices[i + 3] - curvedData.vertices[i + 1];
    drawnLength += std::sqrt(dx * dx + dy * dy);
  }
  const double exact = std::acos(-1.0) * 5.0;
  check(std::abs(drawnLength - exact) < exact * 0.001,
        "la poligonal se acerca a la longitud del arco");
}

void testClosedPolylineIsClosed() {
  std::printf("render: una polilínea cerrada dibuja también el tramo de cierre\n");
  std::vector<PolyVertex> square = {
      {{0, 0}, 0.0}, {{1, 0}, 0.0}, {{1, 1}, 0.0}, {{0, 1}, 0.0}};

  const Scene open = buildScene({makePolyline(square, false)});
  const Scene closed = buildScene({makePolyline(square, true)});

  check(buildRenderData(open).segmentCount() == 3, "abierta: 3 lados");
  check(buildRenderData(closed).segmentCount() == 4, "cerrada: 4 lados");
}

void testPointsGetAMark() {
  std::printf("render: los puntos se dibujan como una cruz\n");
  // Un punto no tiene longitud: sin marca no se vería ni se podría enganchar.
  Entity point;
  point.type = EntityType::Point;
  point.vertices = {{{5.0, 5.0}, 0.0}};
  updateBounds(point);

  // Se acompaña de una línea para que la escena tenga extensión y el tamaño de
  // la marca salga de algo.
  const Scene scene = buildScene({point, makeLine({0, 0}, {10, 10})});
  const RenderData data = buildRenderData(scene);

  // Dos trazos de la cruz más el segmento de la línea.
  check(data.segmentCount() == 3, "cruz de dos trazos más la línea");
}

void testTextIsNotDrawnAsGeometry() {
  std::printf("render: los textos no entran en la geometría\n");
  Entity text;
  text.type = EntityType::Text;
  text.text = "COCINA";
  text.textHeight = 0.2;
  text.vertices = {{{1.0, 1.0}, 0.0}};
  updateBounds(text);

  const Scene scene = buildScene({text, makeLine({0, 0}, {10, 10})});
  const RenderData data = buildRenderData(scene);

  check(data.segmentCount() == 1, "solo la línea");
}

void testEmptyAndDegenerateScenes() {
  std::printf("render: escenas vacías o degeneradas\n");
  const RenderData empty = buildRenderData(buildScene({}));
  check(empty.vertices.empty(), "escena vacía no produce vértices");
  check(empty.batches.empty(), "ni lotes");

  // Una entidad de un solo vértice no es dibujable, pero no puede hacer que se
  // pierda la geometría que la acompaña.
  Entity lonely;
  lonely.type = EntityType::Polyline;
  lonely.vertices = {{{0.0, 0.0}, 0.0}};
  updateBounds(lonely);

  const RenderData mixed = buildRenderData(buildScene({lonely, makeLine({0, 0}, {1, 1})}));
  check(mixed.segmentCount() == 1, "la línea se dibuja igual");

  // Ningún lote puede quedar vacío: sería una llamada de dibujo inútil.
  bool anyEmpty = false;
  for (const RenderBatch& batch : mixed.batches) {
    if (batch.vertexCount == 0) anyEmpty = true;
  }
  check(!anyEmpty, "no se generan lotes vacíos");
}

void testSagittaScalesWithDrawing() {
  std::printf("render: el detalle se adapta al tamaño del plano\n");
  // El mismo dibujo en milímetros y en metros debe generar el mismo número de
  // segmentos: el detalle es relativo, no absoluto.
  Entity small;
  small.type = EntityType::Arc;
  small.vertices = {{{0.0, 0.0}, 1.0}, {{1.0, 0.0}, 0.0}};
  updateBounds(small);

  Entity large;
  large.type = EntityType::Arc;
  large.vertices = {{{0.0, 0.0}, 1.0}, {{1000.0, 0.0}, 0.0}};
  updateBounds(large);

  const size_t smallSegments = buildRenderData(buildScene({small})).segmentCount();
  const size_t largeSegments = buildRenderData(buildScene({large})).segmentCount();
  check(smallSegments == largeSegments, "mismo detalle a cualquier escala");
}

}  // namespace

int main() {
  testBatchesCoverBufferExactly();
  testCoordinatesAreRelativeToOrigin();
  testCurvesProduceMoreSegments();
  testClosedPolylineIsClosed();
  testPointsGetAMark();
  testTextIsNotDrawnAsGeometry();
  testEmptyAndDegenerateScenes();
  testSagittaScalesWithDrawing();

  std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
