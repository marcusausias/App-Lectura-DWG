// Modelo de entidad normalizada.
//
// Toda la geometría del dibujo se reduce a un único tipo: una lista de vértices
// con bulge, en coordenadas de mundo. Una línea son dos vértices sin bulge; un
// círculo, dos vértices con bulge 1; un arco, dos vértices con el bulge que
// corresponda.
//
// La razón de unificarlo así es la medición. El bulge codifica el ángulo del
// arco, y el ángulo no cambia al rotar, escalar por igual ni desplazar: solo
// cambia de signo al reflejar. Es decir, un arco insertado dentro de tres
// bloques anidados sigue siendo un arco exacto, y su longitud se puede seguir
// calculando de forma analítica en vez de sumando segmentos.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dwgcore/geometry.h"
#include "dwgcore/transform.h"

namespace dwgcore {

enum class EntityType {
  Unknown,
  Line,
  Polyline,
  Arc,
  Circle,
  Ellipse,
  Spline,
  Point,
  Text,
  Hatch,
};

struct Bounds {
  Vec2 min{0.0, 0.0};
  Vec2 max{0.0, 0.0};
  bool valid = false;

  void expand(Vec2 point);
  // Nombre distinto a propósito: con dos sobrecargas, una llamada con lista
  // entre llaves no puede decidir si construye un Vec2 o un Bounds.
  void merge(const Bounds& other);
};

struct Entity {
  uint64_t id = 0;
  EntityType type = EntityType::Unknown;
  // Nombre de la capa según sale del DWG. Al montar la escena se sustituye por
  // `layerId`, que apunta a la tabla de capas y no repite la cadena 80.000
  // veces.
  std::string layer;
  uint16_t layerId = 0;

  std::vector<PolyVertex> vertices;  // Coordenadas de mundo.
  bool closed = false;

  // true cuando una escala no uniforme obligó a teselar los arcos: la
  // geometría se sigue dibujando bien, pero su longitud ya es aproximada y la
  // interfaz debe advertirlo antes de dar la medida por buena.
  bool approximated = false;

  // Solo para EntityType::Text. La posición es el primer vértice.
  std::string text;
  double textHeight = 0.0;
  double textRotation = 0.0;

  Bounds bounds;

  // Ruta de bloques desde la que llegó, para poder rastrear de dónde sale cada
  // línea cuando una medición no cuadra.
  std::vector<std::string> blockPath;
};

// Recalcula la caja envolvente teniendo en cuenta la curvatura.
//
// Un arco puede sobresalir bastante de sus dos extremos, así que una caja
// calculada solo con los vértices deja fuera parte de la geometría y provoca
// que el recorte por pantalla haga desaparecer arcos por los bordes.
void updateBounds(Entity& entity);

// Aplica una transformada a la entidad.
//
// Si la transformada es conforme conserva los bulges (cambiándoles el signo
// cuando hay simetría) y la medición sigue siendo exacta. Si no lo es, tesela
// las curvas con `maxSagitta` y marca la entidad como aproximada.
Entity transformEntity(const Entity& entity, const Transform2D& transform,
                       double maxSagitta);

// Longitud de la entidad, analítica en los tramos curvos.
double entityLength(const Entity& entity);

// Área encerrada. Solo tiene sentido en entidades cerradas.
double entityArea(const Entity& entity);

// Constructores de las formas que no llegan ya como polilínea.
Entity makeLine(Vec2 start, Vec2 end);
Entity makeCircle(Vec2 center, double radius);
Entity makeArc(Vec2 center, double radius, double startAngle, double endAngle);
Entity makePolyline(std::vector<PolyVertex> vertices, bool closed);

// Elipse o arco de elipse.
//
// `majorAxis` es el vector del centro al extremo del eje mayor, y `ratio` la
// proporción entre el eje menor y el mayor. `startParam` y `endParam` son
// ángulos paramétricos, no ángulos reales sobre la elipse: es como los guarda
// el DWG y confundirlos deforma el trazado.
//
// Una elipse no se puede representar con bulges, así que sale ya teselada y
// marcada como aproximada.
Entity makeEllipse(Vec2 center, Vec2 majorAxis, double ratio, double startParam,
                   double endParam, double maxSagitta);

}  // namespace dwgcore
