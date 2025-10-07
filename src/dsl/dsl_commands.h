#ifndef DSL_COMMANDS_H
#define DSL_COMMANDS_H

#include <glib.h>
#include "canvas.h"

gboolean dsl_execute_command_block(CanvasData *data, const gchar *block_source);

#endif
