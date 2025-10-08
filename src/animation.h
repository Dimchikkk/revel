#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdbool.h>
#include <gtk/gtk.h>

typedef enum {
    ANIM_INTERP_IMMEDIATE,  // No interpolation, jump to end
    ANIM_INTERP_LINEAR,     // Linear interpolation
    ANIM_INTERP_BEZIER      // Smooth bezier curve
} AnimInterpolationType;

typedef enum {
    ANIM_TYPE_MOVE,
    ANIM_TYPE_RESIZE,
    ANIM_TYPE_COLOR,
    ANIM_TYPE_ROTATE,
    ANIM_TYPE_CREATE,
    ANIM_TYPE_DELETE
} AnimationType;

typedef struct {
    char *element_uuid;  // Element UUID string
    AnimationType type;
    AnimInterpolationType interp;

    // Timing
    double start_time;  // seconds from animation start
    double duration;    // seconds

    // Move animation
    double from_x, from_y;
    double to_x, to_y;

    // Resize animation
    double from_width, from_height;
    double to_width, to_height;

    // Color animation
    char from_color[32];
    char to_color[32];

    // Rotate animation
    double from_rotation;
    double to_rotation;

    bool completed;
} Animation;

typedef struct {
    Animation *animations;
    int count;
    int capacity;

    double elapsed_time;
    bool running;
    bool cycled;

    guint tick_callback_id;
    GtkWidget *widget;   // Widget that owns the tick callback
    gpointer user_data;  // For passing CanvasData to completion callback
    gint64 last_frame_time;  // Track last frame time for delta calculation
} AnimationEngine;

// Initialize/cleanup
void animation_engine_init(AnimationEngine *engine, bool cycled);
void animation_engine_cleanup(AnimationEngine *engine);

// Add animations
void animation_add_move(AnimationEngine *engine, const char *element_uuid,
                       double start_time, double duration,
                       AnimInterpolationType interp,
                       double from_x, double from_y,
                       double to_x, double to_y);

void animation_add_resize(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp,
                         double from_width, double from_height,
                         double to_width, double to_height);

void animation_add_color(AnimationEngine *engine, const char *element_uuid,
                        double start_time, double duration,
                        AnimInterpolationType interp,
                        const char *from_color, const char *to_color);

void animation_add_rotate(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp,
                         double from_rotation, double to_rotation);

void animation_add_create(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp);

void animation_add_delete(AnimationEngine *engine, const char *element_uuid,
                         double start_time, double duration,
                         AnimInterpolationType interp);

// Control
void animation_engine_start(AnimationEngine *engine, GtkWidget *widget, gpointer user_data);
void animation_engine_stop(AnimationEngine *engine);
void animation_engine_reset(AnimationEngine *engine);

// Update (called every frame)
bool animation_engine_tick(AnimationEngine *engine, double delta_time);

// Get animated values for an element
bool animation_engine_get_position(AnimationEngine *engine, const char *element_uuid,
                                  double *out_x, double *out_y);

bool animation_engine_get_size(AnimationEngine *engine, const char *element_uuid,
                              double *out_width, double *out_height);

bool animation_engine_get_color(AnimationEngine *engine, const char *element_uuid,
                               double *out_r, double *out_g, double *out_b, double *out_a);

bool animation_engine_get_rotation(AnimationEngine *engine, const char *element_uuid,
                                   double *out_rotation);

bool animation_engine_get_visibility(AnimationEngine *engine, const char *element_uuid,
                                    double *out_alpha);

// Interpolation helpers
double animation_interpolate(double t, AnimInterpolationType type);

#endif // ANIMATION_H
