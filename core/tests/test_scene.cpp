// Tests del índice espacial y de la caché binaria.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "dwgcore/cache.h"
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

// Rejilla de líneas cortas, una por celda: permite saber de antemano cuántas
// entidades debe devolver cualquier consulta.
std::vector<Entity> makeGrid(int side, const std::string& layer = "0") {
  std::vector<Entity> entities;
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      Entity entity = makeLine({static_cast<double>(x), static_cast<double>(y)},
                               {x + 0.5, y + 0.5});
      entity.layer = layer;
      entities.push_back(std::move(entity));
    }
  }
  return entities;
}

void testIndexFindsEverything() {
  std::printf("índice: una consulta que abarca todo devuelve todo\n");
  Scene scene = buildScene(makeGrid(30));

  std::vector<uint32_t> hits;
  Bounds all;
  all.expand(Vec2{-1000, -1000});
  all.expand(Vec2{1000, 1000});
  scene.index.query(all, hits);

  check(hits.size() == 900, "las 900 entidades de la rejilla");
}

void testIndexNarrowQuery() {
  std::printf("índice: la consulta afinada devuelve solo lo que toca\n");
  Scene scene = buildScene(makeGrid(30));

  std::vector<uint32_t> hits;
  Bounds small;
  small.expand(Vec2{4.1, 4.1});
  small.expand(Vec2{4.4, 4.4});
  queryEntities(scene, small, hits);

  // La celda (4,4) va de (4,4) a (4.5,4.5), así que solo esa se cruza.
  check(hits.size() == 1, "una sola entidad");
  if (hits.size() == 1) {
    const Entity& found = scene.entities[hits[0]];
    check(std::abs(found.vertices[0].position.x - 4.0) < 1e-9, "es la esperada");
  }
}

void testIndexAgreesWithBruteForce() {
  std::printf("índice: la consulta afinada coincide con la fuerza bruta\n");
  // El índice solo puede acelerar, nunca cambiar el resultado. Se contrasta
  // contra el recorrido exhaustivo con consultas en sitios distintos.
  Scene scene = buildScene(makeGrid(40));

  std::srand(12345);
  int mismatches = 0;
  for (int attempt = 0; attempt < 200; ++attempt) {
    const double x = (std::rand() % 4000) / 100.0;
    const double y = (std::rand() % 4000) / 100.0;
    const double w = (std::rand() % 500) / 100.0;

    Bounds area;
    area.expand(Vec2{x, y});
    area.expand(Vec2{x + w, y + w});

    std::vector<uint32_t> hits;
    queryEntities(scene, area, hits);

    size_t expected = 0;
    for (const Entity& entity : scene.entities) {
      const Bounds& box = entity.bounds;
      const bool overlap = !(area.min.x > box.max.x || area.max.x < box.min.x ||
                             area.min.y > box.max.y || area.max.y < box.min.y);
      if (overlap) ++expected;
    }
    if (hits.size() != expected) ++mismatches;
  }
  check(mismatches == 0, "las 200 consultas coinciden");
}

void testIndexHandlesEmptyAndSingle() {
  std::printf("índice: casos límite\n");
  Scene empty = buildScene({});
  check(empty.index.empty(), "sin entidades no hay árbol");

  std::vector<uint32_t> hits;
  Bounds area;
  area.expand(Vec2{0, 0});
  area.expand(Vec2{1, 1});
  empty.index.query(area, hits);
  check(hits.empty(), "consultar un árbol vacío no revienta");

  Scene single = buildScene({makeLine({0, 0}, {1, 1})});
  single.index.query(area, hits);
  check(hits.size() == 1, "una sola entidad se encuentra");
}

void testLayerTable() {
  std::printf("escena: los nombres de capa no se repiten\n");
  std::vector<Entity> entities;
  for (int i = 0; i < 100; ++i) {
    Entity entity = makeLine({0, 0}, {1, 1});
    entity.layer = (i % 3 == 0) ? "MUROS" : (i % 3 == 1 ? "COTAS" : "MUROS");
    entities.push_back(std::move(entity));
  }

  Scene scene = buildScene(std::move(entities));
  check(scene.layers.size() == 2, "solo dos capas distintas");
  for (const Entity& entity : scene.entities) {
    check(entity.layerId < scene.layers.size(), "índice de capa válido");
    // La cadena se libera al pasar a índice: repetirla 100.000 veces en un
    // plano real es memoria tirada.
    check(entity.layer.empty(), "la cadena ya no se guarda por entidad");
  }
}

void testCacheRoundTrip() {
  std::printf("caché: lo que se escribe es lo que se lee\n");
  std::vector<Entity> entities = makeGrid(20, "MUROS");

  // Una entidad con curvatura y otra de texto, para cubrir los campos que no
  // lleva una línea normal.
  Entity arc = makeArc({5.0, 5.0}, 2.0, 0.0, std::acos(-1.0));
  arc.layer = "CURVAS";
  arc.approximated = true;
  entities.push_back(arc);

  Entity text;
  text.type = EntityType::Text;
  text.layer = "TEXTOS";
  text.vertices = {{{3.0, 4.0}, 0.0}};
  text.text = "Salón 24,5 m²";
  text.textHeight = 0.25;
  text.textRotation = 1.2;
  updateBounds(text);
  entities.push_back(text);

  const Scene original = buildScene(std::move(entities));

  const std::string path = "/tmp/dwgcore-cache-test.bin";
  CacheStamp stamp{123456, 987654};
  check(writeCache(path, original, stamp), "se escribe");

  Scene restored;
  check(readCache(path, stamp, restored), "se lee");

  check(restored.entities.size() == original.entities.size(), "mismas entidades");
  check(restored.layers == original.layers, "mismas capas");
  check(std::abs(restored.bounds.min.x - original.bounds.min.x) < 1e-12,
        "misma extensión");

  bool sameGeometry = true;
  bool sameFlags = true;
  for (size_t i = 0; i < restored.entities.size(); ++i) {
    const Entity& a = original.entities[i];
    const Entity& b = restored.entities[i];
    if (a.vertices.size() != b.vertices.size()) { sameGeometry = false; break; }
    for (size_t v = 0; v < a.vertices.size(); ++v) {
      if (std::abs(a.vertices[v].position.x - b.vertices[v].position.x) > 1e-12 ||
          std::abs(a.vertices[v].bulge - b.vertices[v].bulge) > 1e-12) {
        sameGeometry = false;
      }
    }
    if (a.closed != b.closed || a.approximated != b.approximated ||
        a.layerId != b.layerId || a.type != b.type) {
      sameFlags = false;
    }
  }
  check(sameGeometry, "geometría idéntica al bit");
  check(sameFlags, "banderas, capa y tipo idénticos");

  // Longitudes: es lo que de verdad importa, porque es lo que se le enseña al
  // usuario.
  double lengthBefore = 0.0;
  double lengthAfter = 0.0;
  for (const Entity& entity : original.entities) lengthBefore += entityLength(entity);
  for (const Entity& entity : restored.entities) lengthAfter += entityLength(entity);
  check(std::abs(lengthBefore - lengthAfter) < 1e-9, "mismas longitudes");

  const Entity& restoredText = restored.entities.back();
  check(restoredText.text == "Salón 24,5 m²", "el texto sobrevive con acentos");
  check(std::abs(restoredText.textHeight - 0.25) < 1e-12, "altura del texto");

  // El índice tiene que venir usable sin reconstruirlo.
  std::vector<uint32_t> hits;
  Bounds all;
  all.expand(Vec2{-1000, -1000});
  all.expand(Vec2{1000, 1000});
  restored.index.query(all, hits);
  check(hits.size() == restored.entities.size(), "el índice se restaura entero");

  std::remove(path.c_str());
}

void testCacheRejectsStaleFile() {
  std::printf("caché: se descarta si el DWG ha cambiado\n");
  const Scene scene = buildScene(makeGrid(5));
  const std::string path = "/tmp/dwgcore-cache-stale.bin";

  check(writeCache(path, scene, CacheStamp{1000, 2000}), "se escribe");

  Scene restored;
  // Mismo tamaño pero otra fecha: el archivo se ha vuelto a guardar. Dibujar el
  // plano viejo sin avisar sería peor que reprocesarlo.
  check(!readCache(path, CacheStamp{1000, 2001}, restored), "fecha distinta");
  check(!readCache(path, CacheStamp{1001, 2000}, restored), "tamaño distinto");
  check(readCache(path, CacheStamp{1000, 2000}, restored), "sello correcto");

  // Y si cambian las referencias externas, aunque el DWG sea idéntico: seguir
  // dibujando con el contenido externo antiguo sería peor que reprocesar.
  check(!readCache(path, CacheStamp{1000, 2000, 99}, restored), "xrefs distintas");

  std::remove(path.c_str());
}

void testCacheRejectsGarbage() {
  std::printf("caché: un archivo corrupto no tumba la app\n");
  const std::string path = "/tmp/dwgcore-cache-garbage.bin";

  std::FILE* file = std::fopen(path.c_str(), "wb");
  const char junk[] = "esto no es una cache en absoluto, solo texto suelto";
  std::fwrite(junk, 1, sizeof(junk), file);
  std::fclose(file);

  Scene restored;
  check(!readCache(path, CacheStamp{0, 0}, restored), "se rechaza");

  // Y una caché válida a la que se le corta la cola.
  const Scene scene = buildScene(makeGrid(10));
  const std::string truncatedPath = "/tmp/dwgcore-cache-truncated.bin";
  writeCache(truncatedPath, scene, CacheStamp{1, 1});

  file = std::fopen(truncatedPath.c_str(), "rb");
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::vector<char> head(static_cast<size_t>(size / 2));
  std::fseek(file, 0, SEEK_SET);
  const size_t readBytes = std::fread(head.data(), 1, head.size(), file);
  std::fclose(file);

  file = std::fopen(truncatedPath.c_str(), "wb");
  std::fwrite(head.data(), 1, readBytes, file);
  std::fclose(file);

  check(!readCache(truncatedPath, CacheStamp{1, 1}, restored), "truncada se rechaza");

  std::remove(path.c_str());
  std::remove(truncatedPath.c_str());
}

void testCacheLeavesSceneUntouchedOnFailure() {
  std::printf("caché: una lectura fallida no destroza lo que ya había\n");
  // Si al abrir un plano la caché resulta ilegible, la escena que el usuario
  // tenía cargada no puede quedarse a medias.
  Scene current = buildScene(makeGrid(6));
  const size_t before = current.entities.size();

  check(!readCache("/tmp/no-existe-esta-cache.bin", CacheStamp{1, 1}, current),
        "no existe");
  check(current.entities.size() == before, "la escena sigue intacta");
}

}  // namespace

int main() {
  testIndexFindsEverything();
  testIndexNarrowQuery();
  testIndexAgreesWithBruteForce();
  testIndexHandlesEmptyAndSingle();
  testLayerTable();
  testCacheRoundTrip();
  testCacheRejectsStaleFile();
  testCacheRejectsGarbage();
  testCacheLeavesSceneUntouchedOnFailure();

  std::printf("\n%d comprobaciones, %d fallos\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
