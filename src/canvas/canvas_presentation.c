#include "canvas_presentation.h"
#include "../dsl/dsl_executor.h"

void canvas_presentation_request_auto_next(CanvasData *data) {
  if (!data) return;

  // Don't auto-advance if suppressed (e.g., during manual navigation)
  if (data->presentation_suppress_auto_next) {
    return;
  }

  gboolean in_presentation = canvas_is_presentation_mode(data);
  gboolean has_running_animation = data->anim_engine && data->anim_engine->running;
  if (in_presentation && has_running_animation) {
    data->presentation_auto_next_pending = TRUE;
    return;
  }

  data->presentation_auto_next_pending = FALSE;
  canvas_presentation_next_slide(data);
}

void canvas_on_animation_finished(CanvasData *data) {
  if (!data) return;
  if (!canvas_is_presentation_mode(data)) return;

  if (data->presentation_auto_next_pending) {
    data->presentation_auto_next_pending = FALSE;
    canvas_presentation_next_slide(data);
  }
}
