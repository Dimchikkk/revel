#include "vector.h"

Vec2 vec2_add(Vec2 a, Vec2 b) {
  return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 vec2_div(Vec2 v, double scalar) {
  return (Vec2){v.x / scalar, v.y / scalar};
}
