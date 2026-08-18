// Mediciones sobre el plano.
//
// Todas las longitudes y superficies salen en unidades de dibujo, tal cual las
// guarda el DWG, sin conversión ni calibración: es lo que se pidió y lo que
// evita la clase de error más difícil de detectar, el de escala.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dwgcore/scene.h"

namespace dwgcore {

enum class MeasureKind : uint8_t {
  Distance,  // Entre dos puntos
  Chain,     // Polilínea de puntos, se suma
  Area,      // Superficie encerrada
  Entity,    // Longitud de una entidad del plano
  Count,     // Cuántos símbolos iguales hay
};

struct Measurement {
  MeasureKind kind = MeasureKind::Distance;
  std::vector<Vec2> points;

  // Longitud o superficie, según el tipo. En un recuento vale 0.
  double value = 0.0;

  // Cierto cuando la medida sale de geometría teselada —una spline, una elipse,
  // o un bloque con escala no uniforme— y por tanto no es exacta. La interfaz
  // debe indicarlo en lugar de presentar una cifra que parece exacta y no lo es.
  bool approximate = false;

  int count = 0;
  std::string label;
};

Measurement measureDistance(Vec2 from, Vec2 to);

// Suma de tramos consecutivos.
Measurement measureChain(const std::vector<Vec2>& points);

// Superficie del polígono definido por los puntos, cerrándolo solo.
Measurement measureArea(const std::vector<Vec2>& points);

// Longitud de una entidad del plano, siguiéndola entera de un toque. Los
// tramos curvos se calculan de forma analítica.
Measurement measureEntity(const Scene& scene, uint32_t entityIndex);

// Cuántas veces está colocado el símbolo al que pertenece la entidad.
Measurement measureCount(const Scene& scene, uint32_t entityIndex);

}  // namespace dwgcore
