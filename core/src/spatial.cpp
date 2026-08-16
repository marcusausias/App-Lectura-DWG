#include "dwgcore/spatial.h"

#include <algorithm>
#include <cmath>

namespace dwgcore {
namespace {

bool overlaps(const Bounds& a, const Bounds& b) {
  if (!a.valid || !b.valid) return false;
  return !(b.min.x > a.max.x || b.max.x < a.min.x || b.min.y > a.max.y ||
           b.max.y < a.min.y);
}

Vec2 centerOf(const Bounds& bounds) {
  return {(bounds.min.x + bounds.max.x) * 0.5,
          (bounds.min.y + bounds.max.y) * 0.5};
}

}  // namespace

void SpatialIndex::build(const std::vector<Bounds>& boxes, int nodeCapacity) {
  nodes_.clear();
  links_.clear();
  root_ = 0;
  rootBounds_ = Bounds{};

  const int capacity = std::max(2, nodeCapacity);

  // Solo entran las entidades con caja válida. Una entidad sin geometría no se
  // puede recortar ni tocar, y colarla dejaría cajas sin sentido que arrastran
  // hacia arriba todo el árbol.
  std::vector<uint32_t> pending;      // Qué agrupar en este nivel.
  std::vector<Bounds> pendingBounds;
  for (uint32_t i = 0; i < boxes.size(); ++i) {
    if (!boxes[i].valid) continue;
    pending.push_back(i);
    pendingBounds.push_back(boxes[i]);
  }
  if (pending.empty()) return;

  bool leafLevel = true;
  while (true) {
    // Reparto Sort-Tile-Recursive: se ordena por X, se corta en franjas
    // verticales, y dentro de cada franja se ordena por Y. Cada nodo agrupa así
    // elementos que además están juntos en el plano, que es lo que permite que
    // el recorte por pantalla descarte ramas enteras de una vez.
    std::vector<uint32_t> slot(pending.size());
    for (size_t i = 0; i < slot.size(); ++i) slot[i] = static_cast<uint32_t>(i);

    std::sort(slot.begin(), slot.end(), [&](uint32_t a, uint32_t b) {
      return centerOf(pendingBounds[a]).x < centerOf(pendingBounds[b]).x;
    });

    const size_t total = slot.size();
    const size_t nodeCount = (total + capacity - 1) / capacity;
    const size_t sliceCount = std::max<size_t>(
        1, static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(nodeCount)))));
    const size_t perSlice = (total + sliceCount - 1) / sliceCount;

    for (size_t start = 0; start < total; start += perSlice) {
      const size_t end = std::min(start + perSlice, total);
      std::sort(slot.begin() + start, slot.begin() + end,
                [&](uint32_t a, uint32_t b) {
                  return centerOf(pendingBounds[a]).y < centerOf(pendingBounds[b]).y;
                });
    }

    std::vector<uint32_t> parents;
    std::vector<Bounds> parentBounds;

    for (size_t start = 0; start < total; start += capacity) {
      const size_t end = std::min(start + capacity, total);

      IndexNode node;
      node.leaf = leafLevel ? 1 : 0;
      node.childCount = static_cast<uint32_t>(end - start);
      // Los hijos no quedan contiguos en el array de nodos, porque cada nivel
      // los reordena. Por eso se guardan sus índices en una tabla de enlaces
      // aparte y el nodo solo apunta al primero.
      node.firstChild = static_cast<uint32_t>(links_.size());

      for (size_t i = start; i < end; ++i) {
        links_.push_back(pending[slot[i]]);
        node.bounds.merge(pendingBounds[slot[i]]);
      }

      parents.push_back(static_cast<uint32_t>(nodes_.size()));
      parentBounds.push_back(node.bounds);
      nodes_.push_back(node);
    }

    if (parents.size() == 1) {
      root_ = parents.front();
      rootBounds_ = nodes_[root_].bounds;
      return;
    }

    pending = std::move(parents);
    pendingBounds = std::move(parentBounds);
    leafLevel = false;
  }
}

void SpatialIndex::query(const Bounds& area, std::vector<uint32_t>& results) const {
  results.clear();
  if (nodes_.empty() || !overlaps(nodes_[root_].bounds, area)) return;

  // Recorrido con pila propia en vez de recursión: el coste en memoria es
  // predecible y no depende de la profundidad que acabe teniendo el árbol.
  std::vector<uint32_t> stack;
  stack.push_back(root_);

  while (!stack.empty()) {
    const IndexNode node = nodes_[stack.back()];
    stack.pop_back();

    if (node.leaf) {
      for (uint32_t i = 0; i < node.childCount; ++i) {
        results.push_back(links_[node.firstChild + i]);
      }
      continue;
    }

    for (uint32_t i = 0; i < node.childCount; ++i) {
      const uint32_t child = links_[node.firstChild + i];
      if (overlaps(nodes_[child].bounds, area)) stack.push_back(child);
    }
  }
}

void SpatialIndex::restore(std::vector<IndexNode> nodes,
                           std::vector<uint32_t> links, uint32_t root) {
  nodes_ = std::move(nodes);
  links_ = std::move(links);
  root_ = root;
  rootBounds_ = nodes_.empty() ? Bounds{} : nodes_[root_].bounds;
}

}  // namespace dwgcore
