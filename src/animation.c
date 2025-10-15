#include "animation.h"
#include "canvas/canvas_core.h"
#include "elements/element.h"
#include "model.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Bezier cubic ease-in-out control points
#define BEZIER_P1 0.42
#define BEZIER_P2 0.0
#define BEZIER_P3 0.58
#define BEZIER_P4 1.0

// Approximate cubic bezier using simplified calculation
static double bezier_ease(double t) {
    // Simplified cubic bezier approximation for ease-in-out
    return t * t * (3.0 - 2.0 * t);
}

// Ease-in: starts slow, speeds up (quadratic)
static double ease_in(double t) {
    return t * t;
}

// Ease-out: starts fast, slows down
static double ease_out(double t) {
    return t * (2.0 - t);
}

// Bounce: bouncing effect at the end
static double bounce(double t) {
    const double n1 = 7.5625;
    const double d1 = 2.75;

    if (t < 1.0 / d1) {
        return n1 * t * t;
    } else if (t < 2.0 / d1) {
        t -= 1.5 / d1;
        return n1 * t * t + 0.75;
    } else if (t < 2.5 / d1) {
        t -= 2.25 / d1;
        return n1 * t * t + 0.9375;
    } else {
        t -= 2.625 / d1;
        return n1 * t * t + 0.984375;
    }
}

// Elastic: spring-like effect
static double elastic(double t) {
    const double c4 = (2 * M_PI) / 3;

    return t == 0
      ? 0
      : t == 1
      ? 1
      : pow(2, -10 * t) * sin((t * 10 - 0.75) * c4) + 1;
}

// Back: overshoots then returns
static double back(double t) {
    const double c1 = 1.70158;
    const double c3 = c1 + 1;

    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
}

// Parse color string to RGBA components
// Supports formats: (r,g,b,a), #RRGGBB, #RRGGBBAA
static bool parse_color(const char *color_str, double *r, double *g, double *b, double *a) {
    if (!color_str) return false;

    // Handle (r,g,b,a) format
    if (color_str[0] == '(') {
        int count = sscanf(color_str, "(%lf,%lf,%lf,%lf)", r, g, b, a);
        if (count == 4) return true;
        if (count == 3) {
            *a = 1.0;
            return true;
        }
        return false;
    }

    // Handle #RRGGBB or #RRGGBBAA format
    if (color_str[0] == '#') {
        int len = strlen(color_str);
        unsigned int ir, ig, ib, ia = 255;

        if (len == 7) { // #RRGGBB
            if (sscanf(color_str, "#%02x%02x%02x", &ir, &ig, &ib) == 3) {
                *r = ir / 255.0;
                *g = ig / 255.0;
                *b = ib / 255.0;
                *a = 1.0;
                return true;
            }
        } else if (len == 9) { // #RRGGBBAA
            if (sscanf(color_str, "#%02x%02x%02x%02x", &ir, &ig, &ib, &ia) == 4) {
                *r = ir / 255.0;
                *g = ig / 255.0;
                *b = ib / 255.0;
                *a = ia / 255.0;
                return true;
            }
        }
    }

    return false;
}

double animation_interpolate(double t, AnimInterpolationType type) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;

    switch (type) {
        case ANIM_INTERP_IMMEDIATE:
            return 1.0;
        case ANIM_INTERP_LINEAR:
            return t;
        case ANIM_INTERP_BEZIER:
            return bezier_ease(t);
        case ANIM_INTERP_EASE_IN:
            return ease_in(t);
        case ANIM_INTERP_EASE_OUT:
            return ease_out(t);
        case ANIM_INTERP_BOUNCE:
            return bounce(t);
        case ANIM_INTERP_ELASTIC:
            return elastic(t);
        case ANIM_INTERP_BACK:
            return back(t);
        default:
            return t;
    }
}

void animation_engine_init(AnimationEngine *engine, bool cycled) {
    engine->animations = NULL;
    engine->count = 0;
    engine->capacity = 0;
    engine->elapsed_time = 0.0;
    engine->running = false;
    engine->cycled = cycled;
    engine->tick_callback_id = 0;
    engine->widget = NULL;
    engine->user_data = NULL;
    engine->last_frame_time = 0;
}

void animation_engine_cleanup(AnimationEngine *engine) {
    if (engine->animations) {
        // Free UUID strings
        for (int i = 0; i < engine->count; i++) {
            if (engine->animations[i].element_uuid) {
                free(engine->animations[i].element_uuid);
            }
        }
        free(engine->animations);
        engine->animations = NULL;
    }
    engine->count = 0;
    engine->capacity = 0;
    if (engine->tick_callback_id && engine->widget) {
        gtk_widget_remove_tick_callback(engine->widget, engine->tick_callback_id);
        engine->tick_callback_id = 0;
    }
    engine->widget = NULL;
}

static void ensure_capacity(AnimationEngine *engine) {
    if (engine->count >= engine->capacity) {
        engine->capacity = engine->capacity == 0 ? 8 : engine->capacity * 2;
        engine->animations = realloc(engine->animations,
                                    engine->capacity * sizeof(Animation));
    }
}

void animation_add_move(AnimationEngine *engine, const char *element_uuid,
                       double start_time, double duration,
                       AnimInterpolationType interp,
                       double from_x, double from_y,
                       double to_x, double to_y) {
    ensure_capacity(engine);

    Animation *anim = &engine->animations[engine->count++];
    memset(anim, 0, sizeof(Animation));

    anim->element_uuid = strdup(element_uuid);
    anim->type = ANIM_TYPE_MOVE;
    anim->interp = interp;
    anim->start_time = start_time;
    anim->duration = duration;
    anim->from_x = from_x;
    anim->from_y = from_y;
    anim->to_x = to_x;
    anim->to_y = to_y;
    anim->completed = false;
}

void animation_add_resize(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp,
                         double from_width, double from_height,
                         double to_width, double to_height) {
    ensure_capacity(engine);

    Animation *anim = &engine->animations[engine->count++];
    memset(anim, 0, sizeof(Animation));

    anim->element_uuid = strdup(element_uuid);
    anim->type = ANIM_TYPE_RESIZE;
    anim->interp = interp;
    anim->start_time = start_time;
    anim->duration = duration;
    anim->from_width = from_width;
    anim->from_height = from_height;
    anim->to_width = to_width;
    anim->to_height = to_height;
    anim->completed = false;
}

void animation_add_color(AnimationEngine *engine, const char *element_uuid,
                        double start_time, double duration,
                        AnimInterpolationType interp,
                        const char *from_color, const char *to_color) {
    ensure_capacity(engine);

    Animation *anim = &engine->animations[engine->count++];
    memset(anim, 0, sizeof(Animation));

    anim->element_uuid = strdup(element_uuid);
    anim->type = ANIM_TYPE_COLOR;
    anim->interp = interp;
    anim->start_time = start_time;
    anim->duration = duration;
    strncpy(anim->from_color, from_color, sizeof(anim->from_color) - 1);
    strncpy(anim->to_color, to_color, sizeof(anim->to_color) - 1);
    anim->completed = false;
}

void animation_add_rotate(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp,
                         double from_rotation, double to_rotation) {
    ensure_capacity(engine);

    Animation *anim = &engine->animations[engine->count++];
    memset(anim, 0, sizeof(Animation));

    anim->element_uuid = strdup(element_uuid);
    anim->type = ANIM_TYPE_ROTATE;
    anim->interp = interp;
    anim->start_time = start_time;
    anim->duration = duration;
    anim->from_rotation = from_rotation;
    anim->to_rotation = to_rotation;
    anim->completed = false;
}

void animation_add_create(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp) {
    ensure_capacity(engine);

    Animation *anim = &engine->animations[engine->count++];
    memset(anim, 0, sizeof(Animation));

    anim->element_uuid = strdup(element_uuid);
    anim->type = ANIM_TYPE_CREATE;
    anim->interp = interp;
    anim->start_time = start_time;
    anim->duration = duration;
    anim->completed = false;
}

void animation_add_delete(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp) {
    ensure_capacity(engine);

    Animation *anim = &engine->animations[engine->count++];
    memset(anim, 0, sizeof(Animation));

    anim->element_uuid = strdup(element_uuid);
    anim->type = ANIM_TYPE_DELETE;
    anim->interp = interp;
    anim->start_time = start_time;
    anim->duration = duration;
    anim->completed = false;
}

void animation_engine_reset(AnimationEngine *engine) {
    engine->elapsed_time = 0.0;
    engine->last_frame_time = 0;
    for (int i = 0; i < engine->count; i++) {
        engine->animations[i].completed = false;
    }
}

void animation_engine_stop(AnimationEngine *engine) {
    engine->running = false;
    // Note: tick callback will be removed automatically when it returns G_SOURCE_REMOVE
    // We just mark it as 0 to avoid double-removal
    engine->tick_callback_id = 0;
}

static gboolean on_animation_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer user_data);

void animation_engine_start(AnimationEngine *engine, GtkWidget *widget, gpointer user_data) {
    if (engine->running) return;

    animation_engine_reset(engine);
    engine->running = true;
    engine->widget = widget;
    engine->user_data = user_data;

    // Add tick callback for animation updates
    engine->tick_callback_id = gtk_widget_add_tick_callback(
        widget, on_animation_tick, engine, NULL);
}

// Returns animation values for a given element_uuid
// Returns true if the element has an active animation
bool animation_engine_get_position(AnimationEngine *engine, const char *element_uuid,
                                  double *out_x, double *out_y) {
    if (!engine->running || !element_uuid) return false;

    Animation *current_anim = NULL;
    Animation *last_completed = NULL;

    // Find the current active animation or the last completed one
    for (int i = 0; i < engine->count; i++) {
        Animation *anim = &engine->animations[i];

        if (!anim->element_uuid || strcmp(anim->element_uuid, element_uuid) != 0) continue;
        if (anim->type != ANIM_TYPE_MOVE) continue;

        double anim_end_time = anim->start_time + anim->duration;

        // Animation hasn't started yet - use first position of first animation
        if (engine->elapsed_time < anim->start_time) {
            if (!current_anim) {
                *out_x = anim->from_x;
                *out_y = anim->from_y;
                return true;
            }
            continue;
        }

        // Animation is currently active
        if (engine->elapsed_time >= anim->start_time && engine->elapsed_time < anim_end_time) {
            current_anim = anim;
            break;
        }

        // Animation has completed - track it
        if (engine->elapsed_time >= anim_end_time) {
            last_completed = anim;
        }
    }

    // Use current animation if found
    if (current_anim) {
        double local_time = engine->elapsed_time - current_anim->start_time;
        double t = local_time / current_anim->duration;
        double interp_t = animation_interpolate(t, current_anim->interp);

        *out_x = current_anim->from_x + (current_anim->to_x - current_anim->from_x) * interp_t;
        *out_y = current_anim->from_y + (current_anim->to_y - current_anim->from_y) * interp_t;
        return true;
    }

    // Use last completed animation's final position
    if (last_completed) {
        *out_x = last_completed->to_x;
        *out_y = last_completed->to_y;
        return true;
    }

    return false;
}

bool animation_engine_get_size(AnimationEngine *engine, const char *element_uuid,
                              double *out_width, double *out_height) {
    if (!engine->running || !element_uuid) return false;

    Animation *current_anim = NULL;
    Animation *last_completed = NULL;

    // Find the current active animation or the last completed one
    for (int i = 0; i < engine->count; i++) {
        Animation *anim = &engine->animations[i];

        if (!anim->element_uuid || strcmp(anim->element_uuid, element_uuid) != 0) continue;
        if (anim->type != ANIM_TYPE_RESIZE) continue;

        double anim_end_time = anim->start_time + anim->duration;

        // Animation hasn't started yet
        if (engine->elapsed_time < anim->start_time) {
            if (!current_anim) {
                *out_width = anim->from_width;
                *out_height = anim->from_height;
                return true;
            }
            continue;
        }

        // Animation is currently active
        if (engine->elapsed_time >= anim->start_time && engine->elapsed_time < anim_end_time) {
            current_anim = anim;
            break;
        }

        // Animation has completed
        if (engine->elapsed_time >= anim_end_time) {
            last_completed = anim;
        }
    }

    if (current_anim) {
        double local_time = engine->elapsed_time - current_anim->start_time;
        double t = local_time / current_anim->duration;
        double interp_t = animation_interpolate(t, current_anim->interp);

        *out_width = current_anim->from_width + (current_anim->to_width - current_anim->from_width) * interp_t;
        *out_height = current_anim->from_height + (current_anim->to_height - current_anim->from_height) * interp_t;
        return true;
    }

    if (last_completed) {
        *out_width = last_completed->to_width;
        *out_height = last_completed->to_height;
        return true;
    }

    return false;
}

bool animation_engine_get_color(AnimationEngine *engine, const char *element_uuid,
                               double *out_r, double *out_g, double *out_b, double *out_a) {
    if (!engine->running || !element_uuid) return false;

    Animation *current_anim = NULL;
    Animation *last_completed = NULL;

    for (int i = 0; i < engine->count; i++) {
        Animation *anim = &engine->animations[i];

        if (!anim->element_uuid || strcmp(anim->element_uuid, element_uuid) != 0) continue;
        if (anim->type != ANIM_TYPE_COLOR) continue;

        double anim_end_time = anim->start_time + anim->duration;

        if (engine->elapsed_time < anim->start_time) {
            if (!current_anim) {
                parse_color(anim->from_color, out_r, out_g, out_b, out_a);
                return true;
            }
            continue;
        }

        if (engine->elapsed_time >= anim->start_time && engine->elapsed_time < anim_end_time) {
            current_anim = anim;
            break;
        }

        if (engine->elapsed_time >= anim_end_time) {
            last_completed = anim;
        }
    }

    if (current_anim) {
        double from_r, from_g, from_b, from_a;
        double to_r, to_g, to_b, to_a;

        if (!parse_color(current_anim->from_color, &from_r, &from_g, &from_b, &from_a) ||
            !parse_color(current_anim->to_color, &to_r, &to_g, &to_b, &to_a)) {
            return false;
        }

        double local_time = engine->elapsed_time - current_anim->start_time;
        double t = local_time / current_anim->duration;
        double interp_t = animation_interpolate(t, current_anim->interp);

        *out_r = from_r + (to_r - from_r) * interp_t;
        *out_g = from_g + (to_g - from_g) * interp_t;
        *out_b = from_b + (to_b - from_b) * interp_t;
        *out_a = from_a + (to_a - from_a) * interp_t;
        return true;
    }

    if (last_completed) {
        parse_color(last_completed->to_color, out_r, out_g, out_b, out_a);
        return true;
    }

    return false;
}

bool animation_engine_get_rotation(AnimationEngine *engine, const char *element_uuid,
                                   double *out_rotation) {
    if (!engine->running || !element_uuid) return false;

    Animation *current_anim = NULL;
    Animation *last_completed = NULL;

    for (int i = 0; i < engine->count; i++) {
        Animation *anim = &engine->animations[i];

        if (!anim->element_uuid || strcmp(anim->element_uuid, element_uuid) != 0) continue;
        if (anim->type != ANIM_TYPE_ROTATE) continue;

        double anim_end_time = anim->start_time + anim->duration;

        // Animation hasn't started yet
        if (engine->elapsed_time < anim->start_time) {
            if (!current_anim) {
                *out_rotation = anim->from_rotation;
                return true;
            }
            continue;
        }

        // Animation is currently active
        if (engine->elapsed_time >= anim->start_time && engine->elapsed_time < anim_end_time) {
            current_anim = anim;
            break;
        }

        // Animation has completed
        if (engine->elapsed_time >= anim_end_time) {
            last_completed = anim;
        }
    }

    if (current_anim) {
        double local_time = engine->elapsed_time - current_anim->start_time;
        double t = local_time / current_anim->duration;
        double interp_t = animation_interpolate(t, current_anim->interp);

        *out_rotation = current_anim->from_rotation + (current_anim->to_rotation - current_anim->from_rotation) * interp_t;
        return true;
    }

    if (last_completed) {
        *out_rotation = last_completed->to_rotation;
        return true;
    }

    return false;
}

bool animation_engine_get_visibility(AnimationEngine *engine, const char *element_uuid,
                                    double *out_alpha) {
    if (!engine->running || !element_uuid) return false;

    Animation *current_anim = NULL;
    Animation *last_completed = NULL;

    for (int i = 0; i < engine->count; i++) {
        Animation *anim = &engine->animations[i];

        if (!anim->element_uuid || strcmp(anim->element_uuid, element_uuid) != 0) continue;
        if (anim->type != ANIM_TYPE_CREATE && anim->type != ANIM_TYPE_DELETE) continue;

        double anim_end_time = anim->start_time + anim->duration;

        if (engine->elapsed_time < anim->start_time) {
            if (!current_anim) {
                // Before animation starts
                *out_alpha = (anim->type == ANIM_TYPE_CREATE) ? 0.0 : 1.0;
                return true;
            }
            continue;
        }

        if (engine->elapsed_time >= anim->start_time && engine->elapsed_time < anim_end_time) {
            current_anim = anim;
            break;
        }

        if (engine->elapsed_time >= anim_end_time) {
            last_completed = anim;
        }
    }

    if (current_anim) {
        double local_time = engine->elapsed_time - current_anim->start_time;
        double t = local_time / current_anim->duration;
        double interp_t = animation_interpolate(t, current_anim->interp);

        if (current_anim->type == ANIM_TYPE_CREATE) {
            *out_alpha = interp_t;  // Fade in from 0 to 1
        } else { // ANIM_TYPE_DELETE
            *out_alpha = 1.0 - interp_t;  // Fade out from 1 to 0
        }
        return true;
    }

    if (last_completed) {
        // After animation completes
        *out_alpha = (last_completed->type == ANIM_TYPE_CREATE) ? 1.0 : 0.0;
        return true;
    }

    return false;
}

// Returns true if all animations completed (and not cycled)
bool animation_engine_tick(AnimationEngine *engine, double delta_time) {
    if (!engine->running) return true;

    engine->elapsed_time += delta_time;

    bool all_completed = true;

    for (int i = 0; i < engine->count; i++) {
        Animation *anim = &engine->animations[i];

        // Check if animation should start
        if (engine->elapsed_time < anim->start_time) {
            all_completed = false;
            continue;
        }

        // Calculate progress
        double local_time = engine->elapsed_time - anim->start_time;

        if (local_time >= anim->duration) {
            // Animation finished
            if (!anim->completed) {
                anim->completed = true;
                CanvasData *data = (CanvasData *)engine->user_data;
                if (data && anim->element_uuid) {
                    ModelElement *model_element = g_hash_table_lookup(data->model->elements, anim->element_uuid);
                    if (model_element) {
                        int current_z = model_element->position ? model_element->position->z :
                                         (model_element->visual_element ? model_element->visual_element->z : 0);
                        gboolean needs_sync = FALSE;
                        switch (anim->type) {
                          case ANIM_TYPE_MOVE:
                            model_update_position(data->model, model_element,
                                                  (int)anim->to_x, (int)anim->to_y, current_z);
                            if (model_element->visual_element) {
                              int z = model_element->position ? model_element->position->z : model_element->visual_element->z;
                              element_update_position(model_element->visual_element,
                                                      (int)anim->to_x, (int)anim->to_y, z);
                            }
                            needs_sync = TRUE;
                            break;
                          case ANIM_TYPE_RESIZE:
                            model_update_size(data->model, model_element,
                                              (int)anim->to_width, (int)anim->to_height);
                            if (model_element->visual_element) {
                              element_update_size(model_element->visual_element,
                                                  (int)anim->to_width, (int)anim->to_height);
                            }
                            needs_sync = TRUE;
                            break;
                          case ANIM_TYPE_COLOR:
                            if (model_element->bg_color) {
                              model_element->bg_color->r = anim->to_color[0];
                              model_element->bg_color->g = anim->to_color[1];
                              model_element->bg_color->b = anim->to_color[2];
                              model_element->bg_color->a = anim->to_color[3];
                            }
                            needs_sync = TRUE;
                            break;
                          case ANIM_TYPE_ROTATE:
                            model_update_rotation(data->model, model_element, anim->to_rotation);
                            if (model_element->visual_element) {
                              model_element->visual_element->rotation_degrees = anim->to_rotation;
                            }
                            needs_sync = TRUE;
                            break;
                          default:
                            break;
                        }
                        if (needs_sync) {
                          canvas_sync_with_model(data);
                          gtk_widget_queue_draw(data->drawing_area);
                        }
                    }
                }
            }
        } else {
            all_completed = false;
        }
    }

    // Check if we should cycle
    if (all_completed && engine->cycled) {
        animation_engine_reset(engine);
        return false;
    }

    return all_completed;
}

// GTK tick callback
static gboolean on_animation_tick(GtkWidget *widget, GdkFrameClock *clock,
                                 gpointer user_data) {
    AnimationEngine *engine = (AnimationEngine *)user_data;

    gint64 current_time = gdk_frame_clock_get_frame_time(clock);

    if (engine->last_frame_time == 0) {
        engine->last_frame_time = current_time;
        return G_SOURCE_CONTINUE;
    }

    double delta = (current_time - engine->last_frame_time) / 1000000.0; // Convert to seconds
    engine->last_frame_time = current_time;

    bool completed = animation_engine_tick(engine, delta);

    if (completed && !engine->cycled && engine->count > 0) {
        CanvasData *data = (CanvasData *)engine->user_data;
        if (data) {
            extern gboolean canvas_is_presentation_mode(CanvasData *data);
            extern void canvas_on_animation_finished(CanvasData *data);
            if (!canvas_is_presentation_mode(data)) {
                canvas_show_notification(data, "Animation completed");
            }
            canvas_on_animation_finished(data);
        }
        animation_engine_stop(engine);
        return G_SOURCE_REMOVE;
    }

    // Trigger redraw
    gtk_widget_queue_draw(widget);

    return G_SOURCE_CONTINUE;
}
