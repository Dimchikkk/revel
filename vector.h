#ifndef VECTOR_H
#define VECTOR_H

typedef struct {
  double x, y;
} Vec2;

Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_div(Vec2 v, double scalar);

#endif
