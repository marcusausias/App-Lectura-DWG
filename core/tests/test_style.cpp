// Tests de colores, capas ocultas y superficie de contornos.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "dwgcore/cache.h"
#include "dwgcore/flatten.h"
#include "dwgcore/measure.h"
#include "dwgcore/render.h"
#include "dwgcore/scene.h"

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

constexpr uint32_t kRed = 0xFFFF0000u;
constexpr uint32_t kGreen = 0xFF00FF00u;
constexpr uint32_t kBlue = 0xFF0000FFu;

Entity line(double x, const std::string& layer, uint32_t color) {
  Entity entity = makeLine({x, 0.0}, {x + 1.0, 1.0});
  entity.layer = layer;
  entity.color = color;
  return entity;
}

void testByLayerTakesLayerColor() {
  std::printf("estilo: el color por capa sale de la tabla de capas\n");
  LayerTable table;
  table["MUROS"] = LayerStyle{kRed, false};
  table["AUX"] = LayerStyle{kGreen, true};

  std::vector<Entity> entities = {
      line(0, "MUROS", kColorByLayer),
      line(1, "MUROS", kBlue),
      line(2, "AUX", kColorByLayer),
      line(3, "SIN_TABLA", kColorByLayer),
  };
  const Scene scene = buildScene(std::move(entities), 16, &table);

  check(scene.entities[0].color == kRed, "por capa toma el rojo de MUROS");
  check(scene.entities[1].color == kBlue, "un color propio no se toca");
  check(scene.entities[2].color == kGreen, "por capa toma el verde de AUX");
  check(scene.entities[3].color == kColorForeground,
        "una capa que no está en la tabla queda en primer plano");
  check(scene.layerStyles.size() == scene.layers.size(), "un estilo por capa");
  check(!scene.layerStyles[scene.entities[0].layerId].hidden, "MUROS visible");
  check(scene.layerStyles[scene.entities[2].layerId].hidden,
        "AUX arranca oculta porque venía apagada");
}

void testWithoutTableEverythingIsForeground() {
  std::printf("estilo: sin tabla de capas todo va en primer plano\n");
  const Scene scene = buildScene({line(0, "A", kColorByLayer), line(1, "A", kColorByBlock)});
  check(scene.entities[0].color == kColorForeground, "por capa sin tabla");
  check(scene.entities[1].color == kColorForeground, "por bloque fuera de un bloque");
}

void testBlockInheritsInsertLayerAndColor() {
  std::printf("estilo: la capa 0 y el color por bloque heredan de la inserción\n");
  BlockDefinition door;
  door.name = "PUERTA";
  door.entities = {
      line(0, "0", kColorByLayer),       // adopta capa y color de la inserción
      line(1, "0", kColorByBlock),       // adopta el color propio de la inserción
      line(2, "HERRAJES", kColorByLayer) // conserva su capa
  };
  std::map<std::string, BlockDefinition> blocks{{"PUERTA", door}};

  InsertRef onDoors;
  onDoors.blockName = "PUERTA";
  onDoors.layer = "CARPINTERIA";
  onDoors.color = kBlue;

  LayerTable table;
  table["CARPINTERIA"] = LayerStyle{kRed, false};
  table["HERRAJES"] = LayerStyle{kGreen, false};

  FlattenResult flat = flatten({}, {onDoors}, blocks);
  check(flat.entities.size() == 3, "tres entidades aplanadas");
  const Scene scene = buildScene(std::move(flat.entities), 16, &table);

  const auto layerOf = [&](size_t i) { return scene.layers[scene.entities[i].layerId]; };
  check(layerOf(0) == "CARPINTERIA", "la capa 0 pasa a la capa de la inserción");
  check(scene.entities[0].color == kRed, "y su color por capa es el de esa capa");
  check(scene.entities[1].color == kBlue, "por bloque toma el color de la inserción");
  check(layerOf(2) == "HERRAJES", "una capa con nombre se conserva");
  check(scene.entities[2].color == kGreen, "con su propio color de capa");
}

void testNestedInsertOnLayerZero() {
  std::printf("estilo: una inserción anidada en la capa 0 hereda de la exterior\n");
  BlockDefinition inner;
  inner.name = "HOJA";
  inner.entities = {line(0, "0", kColorByBlock)};

  InsertRef innerRef;
  innerRef.blockName = "HOJA";
  innerRef.layer = "0";
  innerRef.color = kColorByBlock;

  BlockDefinition outer;
  outer.name = "PUERTA";
  outer.inserts = {innerRef};

  std::map<std::string, BlockDefinition> blocks{{"HOJA", inner}, {"PUERTA", outer}};

  InsertRef top;
  top.blockName = "PUERTA";
  top.layer = "CARPINTERIA";
  top.color = kGreen;

  FlattenResult flat = flatten({}, {top}, blocks);
  check(flat.entities.size() == 1, "una entidad");
  check(flat.entities[0].layer == "CARPINTERIA", "llega la capa de la inserción exterior");
  check(flat.entities[0].color == kGreen, "y su color");
}

void testOneBatchPerLayerAndColor() {
  std::printf("estilo: un lote por capa y color, sin huecos\n");
  std::vector<Entity> entities;
  for (int i = 0; i < 30; ++i) {
    entities.push_back(line(i, i % 2 == 0 ? "A" : "B", i % 3 == 0 ? kRed : kBlue));
  }
  const Scene scene = buildScene(std::move(entities));
  const RenderData data = buildRenderData(scene);

  check(data.batches.size() == 4, "dos capas por dos colores son cuatro lotes");
  uint32_t expected = 0;
  bool contiguous = true;
  for (const RenderBatch& batch : data.batches) {
    contiguous = contiguous && batch.firstVertex == expected;
    expected += batch.vertexCount;
    check(batch.color == kRed || batch.color == kBlue, "cada lote lleva su color");
  }
  check(contiguous, "los lotes son contiguos");
  check(expected == data.vertexCount(), "y cubren el buffer entero");
}

void testCacheKeepsStyles() {
  std::printf("estilo: la caché conserva colores y capas ocultas\n");
  LayerTable table;
  table["MUROS"] = LayerStyle{kRed, false};
  table["AUX"] = LayerStyle{kGreen, true};
  const Scene original = buildScene(
      {line(0, "MUROS", kColorByLayer), line(1, "AUX", kBlue)}, 16, &table);

  const std::string path = "dwgcore_style_cache.bin";
  const CacheStamp stamp{10, 20, 30};
  check(writeCache(path, original, stamp), "se escribe");

  Scene restored;
  check(readCache(path, stamp, restored), "se lee");
  check(restored.layerStyles.size() == 2, "dos estilos de capa");
  if (restored.layerStyles.size() == 2 && restored.entities.size() == 2) {
    check(restored.layerStyles[0].color == kRed, "color de la primera capa");
    check(!restored.layerStyles[0].hidden, "primera capa visible");
    check(restored.layerStyles[1].hidden, "segunda capa oculta");
    check(restored.entities[0].color == kRed, "color de la primera entidad");
    check(restored.entities[1].color == kBlue, "color de la segunda entidad");
  }
  std::remove(path.c_str());
}

void testClosedEntityReportsArea() {
  std::printf("medición: un contorno cerrado da perímetro y superficie\n");
  std::vector<PolyVertex> room = {
      {{0, 0}, 0.0}, {{4, 0}, 0.0}, {{4, 3}, 0.0}, {{0, 3}, 0.0}};
  Entity closed = makePolyline(room, /*closed=*/true);
  Entity open = makePolyline(room, /*closed=*/false);
  Entity circle = makeCircle({10, 10}, 2.0);
  const Scene scene = buildScene({closed, open, circle});

  const Measurement a = measureEntity(scene, 0);
  check(std::fabs(a.value - 14.0) < 1e-9, "perímetro 14");
  check(std::fabs(a.area - 12.0) < 1e-9, "superficie 12");

  const Measurement b = measureEntity(scene, 1);
  check(std::fabs(b.value - 11.0) < 1e-9, "abierta: longitud 11");
  check(b.area == 0.0, "abierta: sin superficie");

  const Measurement c = measureEntity(scene, 2);
  const double pi = 3.14159265358979323846;
  check(std::fabs(c.value - 4.0 * pi) < 1e-9, "círculo: perímetro exacto");
  check(std::fabs(c.area - 4.0 * pi) < 1e-9, "círculo: superficie exacta");
}

}  // namespace

int main() {
  testByLayerTakesLayerColor();
  testWithoutTableEverythingIsForeground();
  testBlockInheritsInsertLayerAndColor();
  testNestedInsertOnLayerZero();
  testOneBatchPerLayerAndColor();
  testCacheKeepsStyles();
  testClosedEntityReportsArea();

  std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
