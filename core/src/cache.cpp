#include "dwgcore/cache.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace dwgcore {
namespace {

constexpr char kMagic[8] = {'D', 'W', 'G', 'C', 'A', 'C', 'H', 'E'};
constexpr uint32_t kVersion = 1;

// Banderas empaquetadas en un byte por entidad.
constexpr uint8_t kFlagClosed = 1 << 0;
constexpr uint8_t kFlagApproximated = 1 << 1;
constexpr uint8_t kFlagBoundsValid = 1 << 2;
// La inmensa mayoría de las entidades de un plano no tienen ni un tramo curvo:
// líneas, textos, puntos, splines ya teseladas. Guardar un cero de 8 bytes por
// vértice en todas ellas es un tercio del archivo tirado, así que los bulges
// solo se escriben cuando alguno no es cero.
constexpr uint8_t kFlagHasBulges = 1 << 3;

class Writer {
 public:
  explicit Writer(std::FILE* file) : file_(file) {}

  template <typename T>
  void value(const T& item) {
    if (!ok_) return;
    ok_ = std::fwrite(&item, sizeof(T), 1, file_) == 1;
  }

  template <typename T>
  void array(const std::vector<T>& items) {
    value(static_cast<uint32_t>(items.size()));
    if (!ok_ || items.empty()) return;
    ok_ = std::fwrite(items.data(), sizeof(T), items.size(), file_) == items.size();
  }

  void text(const std::string& item) {
    value(static_cast<uint32_t>(item.size()));
    if (!ok_ || item.empty()) return;
    ok_ = std::fwrite(item.data(), 1, item.size(), file_) == item.size();
  }

  bool ok() const { return ok_; }

 private:
  std::FILE* file_;
  bool ok_ = true;
};

class Reader {
 public:
  explicit Reader(std::FILE* file) : file_(file) {}

  template <typename T>
  bool value(T& item) {
    if (!ok_) return false;
    ok_ = std::fread(&item, sizeof(T), 1, file_) == 1;
    return ok_;
  }

  template <typename T>
  bool array(std::vector<T>& items, size_t limit) {
    uint32_t count = 0;
    if (!value(count)) return false;
    // Un archivo truncado o manipulado puede declarar un tamaño enorme y
    // provocar una reserva que tumbe la app antes de llegar a leer nada.
    if (count > limit) {
      ok_ = false;
      return false;
    }
    items.resize(count);
    if (count == 0) return true;
    ok_ = std::fread(items.data(), sizeof(T), count, file_) == count;
    return ok_;
  }

  bool text(std::string& item, size_t limit) {
    uint32_t length = 0;
    if (!value(length)) return false;
    if (length > limit) {
      ok_ = false;
      return false;
    }
    item.resize(length);
    if (length == 0) return true;
    ok_ = std::fread(&item[0], 1, length, file_) == length;
    return ok_;
  }

  bool ok() const { return ok_; }

 private:
  std::FILE* file_;
  bool ok_ = true;
};

constexpr size_t kMaxEntities = 20'000'000;
constexpr size_t kMaxVertices = 50'000'000;
constexpr size_t kMaxTextLength = 1 << 20;

}  // namespace

bool writeCache(const std::string& path, const Scene& scene,
                const CacheStamp& stamp) {
  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) return false;

  Writer out(file);
  std::fwrite(kMagic, 1, sizeof(kMagic), file);
  out.value(kVersion);
  out.value(stamp.sourceSize);
  out.value(stamp.sourceModified);

  out.value(static_cast<uint32_t>(scene.layers.size()));
  for (const std::string& layer : scene.layers) out.text(layer);

  out.value(scene.bounds);
  out.value(static_cast<uint32_t>(scene.entities.size()));
  for (const Entity& entity : scene.entities) {
    out.value(static_cast<uint8_t>(entity.type));
    bool hasBulges = false;
    for (const PolyVertex& vertex : entity.vertices) {
      if (vertex.bulge != 0.0) {
        hasBulges = true;
        break;
      }
    }

    uint8_t flags = 0;
    if (hasBulges) flags |= kFlagHasBulges;
    if (entity.closed) flags |= kFlagClosed;
    if (entity.approximated) flags |= kFlagApproximated;
    if (entity.bounds.valid) flags |= kFlagBoundsValid;
    out.value(flags);
    out.value(entity.layerId);
    out.value(entity.id);
    out.value(entity.bounds.min);
    out.value(entity.bounds.max);

    out.value(static_cast<uint32_t>(entity.vertices.size()));
    for (const PolyVertex& vertex : entity.vertices) out.value(vertex.position);
    if (hasBulges) {
      for (const PolyVertex& vertex : entity.vertices) out.value(vertex.bulge);
    }

    if (entity.type == EntityType::Text) {
      out.text(entity.text);
      out.value(entity.textHeight);
      out.value(entity.textRotation);
    }
  }

  out.array(scene.index.nodes());
  out.array(scene.index.links());
  out.value(scene.index.root());

  const bool ok = out.ok();
  std::fclose(file);
  if (!ok) std::remove(path.c_str());
  return ok;
}

bool readCache(const std::string& path, const CacheStamp& expected, Scene& scene) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return false;

  char magic[sizeof(kMagic)] = {0};
  if (std::fread(magic, 1, sizeof(magic), file) != sizeof(magic) ||
      std::memcmp(magic, kMagic, sizeof(magic)) != 0) {
    std::fclose(file);
    return false;
  }

  Reader in(file);
  uint32_t version = 0;
  CacheStamp stamp;
  if (!in.value(version) || version != kVersion || !in.value(stamp.sourceSize) ||
      !in.value(stamp.sourceModified)) {
    std::fclose(file);
    return false;
  }
  if (stamp.sourceSize != expected.sourceSize ||
      stamp.sourceModified != expected.sourceModified) {
    std::fclose(file);
    return false;
  }

  Scene loaded;

  uint32_t layerCount = 0;
  if (!in.value(layerCount) || layerCount > 0xFFFF) {
    std::fclose(file);
    return false;
  }
  loaded.layers.resize(layerCount);
  for (std::string& layer : loaded.layers) {
    if (!in.text(layer, kMaxTextLength)) {
      std::fclose(file);
      return false;
    }
  }

  uint32_t entityCount = 0;
  if (!in.value(loaded.bounds) || !in.value(entityCount) ||
      entityCount > kMaxEntities) {
    std::fclose(file);
    return false;
  }

  loaded.entities.resize(entityCount);
  for (Entity& entity : loaded.entities) {
    uint8_t type = 0;
    uint8_t flags = 0;
    if (!in.value(type) || !in.value(flags) || !in.value(entity.layerId) ||
        !in.value(entity.id) || !in.value(entity.bounds.min) ||
        !in.value(entity.bounds.max)) {
      std::fclose(file);
      return false;
    }

    uint32_t vertexCount = 0;
    if (!in.value(vertexCount) || vertexCount > kMaxVertices) {
      std::fclose(file);
      return false;
    }
    entity.vertices.resize(vertexCount);
    for (PolyVertex& vertex : entity.vertices) {
      if (!in.value(vertex.position)) {
        std::fclose(file);
        return false;
      }
    }
    if ((flags & kFlagHasBulges) != 0) {
      for (PolyVertex& vertex : entity.vertices) {
        if (!in.value(vertex.bulge)) {
          std::fclose(file);
          return false;
        }
      }
    }

    entity.type = static_cast<EntityType>(type);
    entity.closed = (flags & kFlagClosed) != 0;
    entity.approximated = (flags & kFlagApproximated) != 0;
    entity.bounds.valid = (flags & kFlagBoundsValid) != 0;

    if (entity.type == EntityType::Text) {
      if (!in.text(entity.text, kMaxTextLength) || !in.value(entity.textHeight) ||
          !in.value(entity.textRotation)) {
        std::fclose(file);
        return false;
      }
    }
  }

  std::vector<IndexNode> nodes;
  std::vector<uint32_t> links;
  uint32_t root = 0;
  if (!in.array(nodes, kMaxEntities) || !in.array(links, kMaxEntities) ||
      !in.value(root)) {
    std::fclose(file);
    return false;
  }
  // Una raíz fuera de rango dejaría el índice apuntando a memoria ajena en la
  // primera consulta.
  if (!nodes.empty() && root >= nodes.size()) {
    std::fclose(file);
    return false;
  }
  loaded.index.restore(std::move(nodes), std::move(links), root);

  std::fclose(file);
  scene = std::move(loaded);
  return true;
}

}  // namespace dwgcore
