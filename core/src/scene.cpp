#include "dwgcore/scene.h"

#include <unordered_map>

namespace dwgcore {

Scene buildScene(std::vector<Entity> entities, int nodeCapacity) {
  Scene scene;
  scene.entities = std::move(entities);

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
