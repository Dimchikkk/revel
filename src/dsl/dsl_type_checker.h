#ifndef DSL_TYPE_CHECKER_H
#define DSL_TYPE_CHECKER_H

#include <glib.h>
#include "canvas.h"

gboolean dsl_type_check_script(CanvasData *data, const gchar *script, const gchar *filename);
gboolean dsl_type_is_number_literal(const gchar *token);

#endif
