#ifndef DSL_UTILS_H
#define DSL_UTILS_H

#include <glib.h>
#include "elements/shape.h"
#include "elements/connection.h"

gchar* trim_whitespace(gchar *str);
gchar** tokenize_line(const gchar *line, int *token_count);

gboolean parse_point(const gchar *str, int *x, int *y);

gboolean parse_float_point(const gchar *str, double *x, double *y);

gboolean parse_shape_type(const gchar *str, int *shape_type);

gboolean parse_color(const gchar *str, double *r, double *g, double *b, double *a);

gboolean parse_color_token(const gchar *token,
                            double *r, double *g, double *b, double *a);

gboolean parse_font_value(const gchar *value, gchar **out_font);

gboolean parse_bool_value(const gchar *token, gboolean *out_value);

gboolean parse_stroke_style_value(const gchar *token, StrokeStyle *out_style);

gboolean parse_fill_style_value(const gchar *token, FillStyle *out_style);

gboolean parse_int_value(const gchar *token, int *out_value);

gboolean parse_double_value(const gchar *token, double *out_value);

#endif
