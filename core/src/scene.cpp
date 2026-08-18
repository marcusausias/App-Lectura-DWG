#include "dwgcore/scene.h"

#include <unordered_map>
#include <unordered_set>

namespace dwgcore {

namespace {

// Nombre del símbolo al que pertenece una entidad.
//
// Se toma el bloque con nombre más interno de la ruta. Los bloques anónimos
// —los que empiezan por '*', que AutoCAD genera para agrupaciones y cotas— se
// saltan: contar "*U" no le dice nada a nadie, mientras que contar
// "Valve Group 2" sí.
std::string symbolNameOf(const Entity& entity) {
  for (auto it = entity.blockPath.rbegin(); it != entity.blockPath.rend(); ++it) {
    if (!it->empty() && (*it)[0] != '*') return *it;
  }
  return {};
}

}  // namespace

Scene buildScene(std::vector<Entity> entities, int nodeCapacity) {
  Scene scene;
  scene.entities = std::move(entities);

  // El índice 0 queda reservado para la geometría suelta del espacio modelo.
  scene.blocks.push_back(BlockInfo{});

  std::unordered_map<std::string, uint16_t> blockByName;
  // Instancias ya contadas, para no sumar una puerta tantas veces como líneas
  // tenga dibujadas.
  std::unordered_set<uint64_t> countedInstances;

  std::unordered_map<std::string, uint16_t> byName;
  std::vector<Bounds> boxes;
  boxes.reserve(scene.entities.size());

  for (Entity& entity : scene.entities) {
    auto found = byName.find(entity.layer);
    if (found == byName.end()) {
      // El índice se guarda en 16 bits. Un plano con más de 65.000 capas no
      // existe; si apareciera, todo lo que sobra cae en la capa 0 en vez de
      // desbordar en silencio.
      if (scene.layers.size() < 0xFFFF) {
        const uint16_t id = static_cast<uint16_t>(scene.layers.size());
        scene.layers.push_back(entity.layer);
        found = byName.emplace(entity.layer, id).first;
      } else {
        entity.layerId = 0;
        entity.layer.clear();
        boxes.push_back(entity.bounds);
        scene.bounds.merge(entity.bounds);
        continue;
      }
    }
    entity.layerId = found->second;
    entity.layer.clear();

    const std::string symbol = symbolNameOf(entity);
    if (!symbol.empty()) {
      auto block = blockByName.find(symbol);
      if (block == blockByName.end() && scene.blocks.size() < 0xFFFF) {
        const uint16_t id = static_cast<uint16_t>(scene.blocks.size());
        scene.blocks.push_back(BlockInfo{symbol, 0});
        block = blockByName.emplace(symbol, id).first;
      }
      if (block != blockByName.end()) {
        entity.blockId = block->second;
        // Una instancia se cuenta una sola vez, aunque aporte cien líneas.
        const uint64_t key =
            (static_cast<uint64_t>(entity.blockId) << 32) | entity.instanceId;
        if (countedInstances.insert(key).second) {
          scene.blocks[entity.blockId].instanceCount += 1;
        }
      }
    }
    entity.blockPath.clear();
    entity.blockPath.shrink_to_fit();

    boxes.push_back(entity.bounds);
    scene.bounds.merge(entity.bounds);
  }

  scene.index.build(boxes, nodeCapacity);
  return scene;
}

void queryEntities(const Scene& scene, const Bounds& area,
                   std::vector<uint32_t>& results) {
  scene.index.query(area, results);

  size_t kept = 0;
  for (size_t i = 0; i < results.size(); ++i) {
    const Bounds& box = scene.entities[results[i]].bounds;
    if (!box.valid) continue;
    const bool overlaps = !(area.min.x > box.max.x || area.max.x < box.min.x ||
                            area.min.y > box.max.y || area.max.y < box.min.y);
    if (overlaps) results[kept++] = results[i];
  }
  results.resize(kept);
}

}  // namespace dwgcore
