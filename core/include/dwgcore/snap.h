// Enganche del puntero a los puntos notables del dibujo.
//
// Sin esto, medir en un móvil es inútil: el dedo tapa el objetivo y tiene una
// imprecisión de varios milímetros, así que una medida "a ojo" nunca cae sobre
// la esquina que se pretendía marcar. Enganchando al punto notable más cercano,
// el resultado es el mismo que marcaría alguien con el ratón en AutoCAD.
#pragma once

#include <cstdint>
#include <vector>

#include "dwgcore/scene.h"

namespace dwgcore {

enum class SnapType : uint8_t {
  None = 0,
  Endpoint,       // Extremo de un tramo
  Intersection,   // Cruce entre dos entidades
  Midpoint,       // Punto medio de un tramo
  Perpendicular,  // Pie de la perpendicular desde el punto anterior
  OnEdge,         // Punto cualquiera pegado a una línea
};

constexpr uint32_t kNoEntity = 0xFFFFFFFFu;

struct SnapOptions {
  bool endpoint = true;
  bool midpoint = true;
  bool intersection = true;
  bool perpendicular = true;
  bool onEdge = true;

  // Radio de búsqueda en unidades de dibujo. Quien llama lo obtiene de un radio
  // en píxeles, porque lo que importa es la distancia en pantalla: enganchar
  // debe sentirse igual de fácil a cualquier zoom.
  double radius = 1.0;

  // Punto de referencia para la perpendicular, normalmente el punto anterior de
  // la medición en curso.
  Vec2 reference;
  bool hasReference = false;

  // Topes de trabajo. Existen porque un toque no puede tardar más de unos pocos
  // milisegundos aunque caiga sobre una zona con mucha geometría encima.
  int maxSegments = 400;
  int maxIntersectionPairs = 4000;
};

struct SnapResult {
  SnapType type = SnapType::None;
  Vec2 point;
  uint32_t entityIndex = kNoEntity;
  double distance = 0.0;  // Del punto consultado al enganche, en unidades.

  bool found() const { return type != SnapType::None; }
};

// Busca el mejor enganche alrededor de `query`.
//
// `hiddenLayers` son índices de capa apagados: lo que no se ve no se engancha,
// porque enganchar a algo invisible produce medidas que el usuario no puede
// explicar.
SnapResult snap(const Scene& scene, Vec2 query, const SnapOptions& options,
                const std::vector<uint16_t>& hiddenLayers = {});

// Entidad visible más cercana al punto, para seleccionar tocando.
uint32_t pickEntity(const Scene& scene, Vec2 query, double radius,
                    const std::vector<uint16_t>& hiddenLayers = {});

}  // namespace dwgcore
