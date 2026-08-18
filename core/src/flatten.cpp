#include "dwgcore/flatten.h"

#include <algorithm>

namespace dwgcore {
namespace {

struct Context {
  const std::map<std::string, BlockDefinition>& blocks;
  const FlattenOptions& options;
  FlattenResult& result;

  // Bloques abiertos en la rama actual. Sirve para detectar ciclos: si un
  // bloque acaba insertándose a sí mismo, aunque sea a través de otros, el
  // aplanado no terminaría nunca.
  std::vector<std::string> openBlocks;

  // Cada expansión de un INSERT recibe un número propio. Se empieza en 1 para
  // que el 0 quede libre y signifique "suelto en el espacio modelo".
  uint32_t nextInstance = 1;
  uint32_t currentInstance = 0;
};

void emit(Context& context, const Entity& entity, const Transform2D& transform) {
  Entity placed = transformEntity(entity, transform, context.options.maxSagitta);
  placed.blockPath = context.openBlocks;
  placed.instanceId = context.currentInstance;
  context.result.entities.push_back(std::move(placed));
}

void expand(Context& context, const std::vector<Entity>& entities,
            const std::vector<InsertRef>& inserts, const Transform2D& transform,
            int depth);

void expandInsert(Context& context, const InsertRef& insert,
                  const Transform2D& parentTransform, int depth) {
  if (depth >= context.options.maxDepth) {
    ++context.result.skippedTooDeep;
    return;
  }

  const auto found = context.blocks.find(insert.blockName);
  if (found == context.blocks.end()) {
    // Se registra una sola vez: un bloque que falta suele estar insertado
    // decenas de veces y no aporta nada repetirlo.
    auto& missing = context.result.missingBlocks;
    if (std::find(missing.begin(), missing.end(), insert.blockName) ==
        missing.end()) {
      missing.push_back(insert.blockName);
    }
    return;
  }

  const auto& open = context.openBlocks;
  if (std::find(open.begin(), open.end(), insert.blockName) != open.end()) {
    ++context.result.skippedCyclic;
    return;
  }

  // La transformada del hijo se aplica primero y la del padre después, de modo
  // que un bloque anidado hereda la colocación completa de sus contenedores.
  const Transform2D combined = concat(parentTransform, insert.transform);

  const uint32_t previousInstance = context.currentInstance;
  context.currentInstance = context.nextInstance++;

  context.openBlocks.push_back(insert.blockName);
  expand(context, found->second.entities, found->second.inserts, combined,
         depth + 1);
  context.openBlocks.pop_back();

  context.currentInstance = previousInstance;
}

void expand(Context& context, const std::vector<Entity>& entities,
            const std::vector<InsertRef>& inserts, const Transform2D& transform,
            int depth) {
  for (const Entity& entity : entities) emit(context, entity, transform);
  for (const InsertRef& insert : inserts) {
    expandInsert(context, insert, transform, depth);
  }
}

}  // namespace

FlattenResult flatten(const std::vector<Entity>& entities,
                      const std::vector<InsertRef>& inserts,
                      const std::map<std::string, BlockDefinition>& blocks,
                      const FlattenOptions& options) {
  FlattenResult result;
  Context context{blocks, options, result, {}, 1, 0};
  expand(context, entities, inserts, Transform2D{}, 0);
  return result;
}

}  // namespace dwgcore
