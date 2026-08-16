// Núcleo geométrico: conversiones de DWG a geometría medible.
//
// Principio de diseño: la geometría curva NUNCA se mide sobre su teselado.
// El teselado existe solo para dibujar; las longitudes y áreas se calculan de
// forma analítica a partir de los parámetros del arco. Sumar segmentos
// teselados acumula error en cualquier muro curvo y el fallo no se detecta
// hasta que alguien compara con una cota del plano.
#pragma once

#include <vector>

#include "dwgcore/vec.h"

namespace dwgcore {

// --- Arcos a partir de bulge (DXF código 42) -------------------------------
//
// El bulge codifica un tramo curvo de polilínea como b = tan(θ/4), donde θ es
// el ángulo abarcado por el arco, con signo (positivo = antihorario).

struct Arc {
  Vec2 center;
  double radius = 0.0;      // Siempre positivo.
  double startAngle = 0.0;  // Radianes, medidos desde el centro.
  double sweep = 0.0;       // Radianes con signo: + antihorario, - horario.
};

// Convierte un tramo de polilínea con bulge en sus parámetros de arco.
// Devuelve false si el tramo es recto (bulge ~ 0) o degenerado (P1 == P2),
// en cuyo caso `outArc` queda sin tocar.
bool arcFromBulge(Vec2 start, Vec2 end, double bulge, Arc& outArc);

// Longitud analítica del arco: R·|θ|.
double arcLength(const Arc& arc);

// Longitud analítica de un tramo de polilínea, recto o curvo.
double bulgeSegmentLength(Vec2 start, Vec2 end, double bulge);

// Tesela un arco en puntos, sin incluir `start` y sí incluyendo `end`.
// `maxSagitta` es la desviación máxima permitida entre la cuerda y el arco real,
// expresada en unidades de dibujo. Determina cuántos segmentos se generan.
std::vector<Vec2> tessellateArc(const Arc& arc, Vec2 end, double maxSagitta,
                                int maxSegments = 256);

// --- Polilíneas ------------------------------------------------------------

// Vértice de polilínea. `bulge` describe la curvatura del tramo que ARRANCA en
// este vértice y termina en el siguiente (convenio de DXF LWPOLYLINE).
struct PolyVertex {
  Vec2 position;
  double bulge = 0.0;
};

// Longitud total, con los tramos curvos calculados analíticamente.
double polylineLength(const std::vector<PolyVertex>& vertices, bool closed);

// Área encerrada, con los tramos curvos contabilizados como segmentos
// circulares sobre el polígono de cuerdas. Siempre positiva.
// Una polilínea abierta se trata como si se cerrase con un tramo recto.
double polylineArea(const std::vector<PolyVertex>& vertices);

// Teselado completo para dibujar, incluidos todos los vértices.
std::vector<Vec2> tessellatePolyline(const std::vector<PolyVertex>& vertices,
                                     bool closed, double maxSagitta);

// --- Áreas -----------------------------------------------------------------

// Área con signo por la fórmula de Gauss: positiva si el recorrido es
// antihorario. Útil para conocer la orientación además del valor.
double signedArea(const std::vector<Vec2>& points);

// Valor absoluto de la anterior. Es la medición de superficie de la app.
double polygonArea(const std::vector<Vec2>& points);

// --- Sistema de coordenadas del objeto (OCS) -------------------------------
//
// Muchas entidades DWG guardan sus coordenadas en un sistema local definido por
// una dirección de extrusión (DXF código 210). Ignorarlo dibuja la geometría
// invertida en cuanto un objeto se ha hecho con simetría, que es habitual.

// Algoritmo del eje arbitrario. Devuelve los ejes X e Y del plano OCS.
void arbitraryAxes(Vec3 normal, Vec3& outAxisX, Vec3& outAxisY);

// Transforma un punto de coordenadas OCS a coordenadas de mundo.
Vec3 ocsToWcs(Vec3 point, Vec3 normal);

// --- Splines ---------------------------------------------------------------

// Evalúa una B-spline por el algoritmo de De Boor.
// `degree` es el grado (3 en la mayoría de splines de AutoCAD), `knots` el
// vector de nudos y `t` el parámetro dentro del dominio válido.
Vec2 evaluateBSpline(const std::vector<Vec2>& controlPoints,
                     const std::vector<double>& knots, int degree, double t);

// Tesela una spline en `samples` puntos uniformes sobre su dominio.
std::vector<Vec2> tessellateBSpline(const std::vector<Vec2>& controlPoints,
                                    const std::vector<double>& knots, int degree,
                                    int samples);

}  // namespace dwgcore
