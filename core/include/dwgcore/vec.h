// Vectores básicos del núcleo geométrico.
// Todo el núcleo trabaja en double: los planos de obra pueden tener coordenadas
// UTM de 6-7 cifras enteras, donde float pierde precisión visible al medir.
#pragma once

#include <cmath>

namespace dwgcore {

struct Vec2 {
  double x = 0.0;
  double y = 0.0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }

inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline double length(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
inline double distance(Vec2 a, Vec2 b) { return length(b - a); }

// Perpendicular a la izquierda (giro de +90°).
inline Vec2 perpLeft(Vec2 a) { return {-a.y, a.x}; }

inline Vec2 normalize(Vec2 a) {
  const double len = length(a);
  return len > 0.0 ? Vec2{a.x / len, a.y / len} : Vec2{0.0, 0.0};
}

struct Vec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }

inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline double length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

inline Vec3 normalize(Vec3 a) {
  const double len = length(a);
  return len > 0.0 ? Vec3{a.x / len, a.y / len, a.z / len} : Vec3{0.0, 0.0, 0.0};
}

}  // namespace dwgcore
