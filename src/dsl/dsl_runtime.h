#ifndef DSL_RUNTIME_H
#define DSL_RUNTIME_H

#include <glib.h>
#include "canvas.h"

typedef struct _DSLRuntime DSLRuntime;

typedef struct {
  gchar *script;
} DSLCommandBlock;

typedef enum {
  DSL_VAR_UNSET = 0,
  DSL_VAR_INT,
  DSL_VAR_REAL,
  DSL_VAR_BOOL,
  DSL_VAR_STRING,
} DSLVarType;

typedef struct {
  DSLVarType type;
  double numeric_value;
  gchar *string_value;
  gchar *expression;
  gboolean evaluating;
  gboolean is_global;
} DSLVariable;

DSLRuntime* dsl_runtime_get(CanvasData *data);
void dsl_runtime_reset(CanvasData *data);
DSLVariable* dsl_runtime_lookup_variable(CanvasData *data, const gchar *name);
DSLVariable* dsl_runtime_ensure_variable(CanvasData *data, const gchar *name);

gboolean dsl_runtime_set_variable(CanvasData *data, const gchar *name, double value, gboolean trigger_watchers);
gboolean dsl_runtime_set_string_variable(CanvasData *data, const gchar *name, const gchar *value, gboolean trigger_watchers);

gboolean dsl_runtime_recompute_expressions(CanvasData *data);

void dsl_runtime_seed_global_types(CanvasData *data, GHashTable *dest);

void dsl_runtime_register_element(CanvasData *data, const gchar *id, ModelElement *element);
ModelElement* dsl_runtime_lookup_element(CanvasData *data, const gchar *id);
const gchar* dsl_runtime_lookup_element_id(CanvasData *data, ModelElement *element);

void dsl_runtime_add_click_handler(CanvasData *data, const gchar *element_id, gchar *block_source);
void dsl_runtime_add_variable_handler(CanvasData *data, const gchar *var_name, gchar *block_source);
gboolean dsl_runtime_handle_click(CanvasData *data, const gchar *element_id);

void dsl_runtime_prepare_animation_engine(CanvasData *data);
void dsl_runtime_add_move_animation(CanvasData *data, ModelElement *model_element,
                                    int from_x, int from_y, int to_x, int to_y,
                                    double start_time, double duration, AnimInterpolationType interp);
void dsl_runtime_add_resize_animation(CanvasData *data, ModelElement *model_element,
                                      int from_w, int from_h, int to_w, int to_h,
                                      double start_time, double duration, AnimInterpolationType interp);
void dsl_runtime_text_update(CanvasData *data, ModelElement *model_element, const gchar *new_text);

void dsl_runtime_notify_variable(CanvasData *data, const gchar *var_name);
void dsl_runtime_flush_notifications(CanvasData *data);

gboolean dsl_evaluate_expression(CanvasData *data, const gchar *expr, double *out_value);

gchar* dsl_interpolate_text(CanvasData *data, const gchar *input);
gchar* dsl_resolve_numeric_token(CanvasData *data, const gchar *token);

gboolean dsl_parse_point_token(CanvasData *data, const gchar *token, int *out_x, int *out_y);

gboolean dsl_parse_double_token(CanvasData *data, const gchar *token, double *out_value);

gchar* dsl_unescape_text(const gchar *str);

void dsl_runtime_inline_text_updated(CanvasData *data, Element *element, const gchar *text);

void dsl_runtime_register_text_binding(CanvasData *data, const gchar *element_id, const gchar *var_name);

void dsl_runtime_element_moved(CanvasData *data, ModelElement *model_element);
void dsl_runtime_register_position_binding(CanvasData *data, const gchar *element_id, const gchar *var_name);

void dsl_runtime_register_auto_next(CanvasData *data, const gchar *var_name, gboolean is_string, const gchar *expected_str, double expected_value);

#endif
