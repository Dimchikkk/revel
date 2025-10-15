#include "shape.h"
#include <cairo.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <glib.h>
#include <stdint.h>
#include "model.h"
#include "canvas_core.h"
#include "undo_manager.h"
#include <graphene.h>

typedef struct {
  double x;
  double y;
} BrushPoint;

typedef struct {
  const BrushPoint *points;
  guint8 point_count;
  double width;
  double jitter;
  gboolean closed;
} BrushStroke;

typedef struct {
  gunichar codepoint;
  double advance;
  const BrushStroke *strokes;
  guint8 stroke_count;
} BrushGlyph;

#define BRUSH_STROKE(points_array, width_val, jitter_val, closed_val) \
  { points_array, (guint8)G_N_ELEMENTS(points_array), width_val, jitter_val, closed_val }

#define BRUSH_GLYPH_ENTRY(ch, adv, strokes_array) \
  { ch, adv, strokes_array, (guint8)G_N_ELEMENTS(strokes_array) }

static const BrushPoint BRUSH_A_STROKE1[] = {
  {0.08, 0.98}, {0.28, 0.55}, {0.38, 0.05}
};
static const BrushPoint BRUSH_A_STROKE2[] = {
  {0.92, 0.96}, {0.70, 0.50}, {0.55, 0.05}
};
static const BrushPoint BRUSH_A_STROKE3[] = {
  {0.20, 0.55}, {0.40, 0.48}, {0.75, 0.58}
};
static const BrushStroke BRUSH_A_STROKES[] = {
  BRUSH_STROKE(BRUSH_A_STROKE1, 0.26, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_A_STROKE2, 0.24, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_A_STROKE3, 0.20, 0.10, FALSE)
};

static const BrushPoint BRUSH_B_STROKE1[] = {
  {0.12, 0.02}, {0.12, 0.52}, {0.10, 0.98}
};
static const BrushPoint BRUSH_B_STROKE2[] = {
  {0.10, 0.05}, {0.52, 0.02}, {0.63, 0.20}, {0.18, 0.38}
};
static const BrushPoint BRUSH_B_STROKE3[] = {
  {0.18, 0.44}, {0.60, 0.40}, {0.70, 0.62}, {0.15, 0.94}
};
static const BrushStroke BRUSH_B_STROKES[] = {
  BRUSH_STROKE(BRUSH_B_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_B_STROKE2, 0.22, 0.14, FALSE),
  BRUSH_STROKE(BRUSH_B_STROKE3, 0.22, 0.14, FALSE)
};

static const BrushPoint BRUSH_C_STROKE1[] = {
  {0.78, 0.10}, {0.52, 0.02}, {0.24, 0.18}, {0.12, 0.48}, {0.30, 0.78}, {0.62, 0.92}
};
static const BrushStroke BRUSH_C_STROKES[] = {
  BRUSH_STROKE(BRUSH_C_STROKE1, 0.26, 0.15, FALSE)
};

static const BrushPoint BRUSH_D_STROKE1[] = {
  {0.12, 0.02}, {0.10, 0.98}
};
static const BrushPoint BRUSH_D_STROKE2[] = {
  {0.12, 0.05}, {0.52, 0.10}, {0.72, 0.42}, {0.48, 0.86}, {0.12, 0.95}
};
static const BrushStroke BRUSH_D_STROKES[] = {
  BRUSH_STROKE(BRUSH_D_STROKE1, 0.24, 0.10, FALSE),
  BRUSH_STROKE(BRUSH_D_STROKE2, 0.26, 0.13, FALSE)
};

static const BrushPoint BRUSH_E_STROKE1[] = {
  {0.12, 0.02}, {0.12, 0.98}
};
static const BrushPoint BRUSH_E_STROKE2[] = {
  {0.12, 0.05}, {0.78, 0.08}
};
static const BrushPoint BRUSH_E_STROKE3[] = {
  {0.16, 0.50}, {0.66, 0.48}
};
static const BrushPoint BRUSH_E_STROKE4[] = {
  {0.12, 0.94}, {0.68, 0.90}
};
static const BrushStroke BRUSH_E_STROKES[] = {
  BRUSH_STROKE(BRUSH_E_STROKE1, 0.23, 0.10, FALSE),
  BRUSH_STROKE(BRUSH_E_STROKE2, 0.20, 0.09, FALSE),
  BRUSH_STROKE(BRUSH_E_STROKE3, 0.18, 0.08, FALSE),
  BRUSH_STROKE(BRUSH_E_STROKE4, 0.20, 0.09, FALSE)
};

static const BrushPoint BRUSH_F_STROKE1[] = {
  {0.16, 0.02}, {0.12, 0.98}
};
static const BrushPoint BRUSH_F_STROKE2[] = {
  {0.12, 0.05}, {0.74, 0.10}
};
static const BrushPoint BRUSH_F_STROKE3[] = {
  {0.16, 0.50}, {0.66, 0.45}
};
static const BrushStroke BRUSH_F_STROKES[] = {
  BRUSH_STROKE(BRUSH_F_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_F_STROKE2, 0.20, 0.10, FALSE),
  BRUSH_STROKE(BRUSH_F_STROKE3, 0.18, 0.10, FALSE)
};

static const BrushPoint BRUSH_G_STROKE1[] = {
  {0.82, 0.20}, {0.58, 0.02}, {0.28, 0.15}, {0.10, 0.48}, {0.30, 0.80}, {0.60, 0.92}, {0.78, 0.78}, {0.52, 0.68}
};
static const BrushPoint BRUSH_G_STROKE2[] = {
  {0.58, 0.64}, {0.90, 0.68}, {0.62, 0.98}
};
static const BrushStroke BRUSH_G_STROKES[] = {
  BRUSH_STROKE(BRUSH_G_STROKE1, 0.26, 0.16, FALSE),
  BRUSH_STROKE(BRUSH_G_STROKE2, 0.22, 0.12, FALSE)
};

static const BrushPoint BRUSH_H_STROKE1[] = {
  {0.16, 0.02}, {0.12, 0.98}
};
static const BrushPoint BRUSH_H_STROKE2[] = {
  {0.90, 0.02}, {0.80, 0.98}
};
static const BrushPoint BRUSH_H_STROKE3[] = {
  {0.18, 0.52}, {0.48, 0.48}, {0.78, 0.58}
};
static const BrushStroke BRUSH_H_STROKES[] = {
  BRUSH_STROKE(BRUSH_H_STROKE1, 0.24, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_H_STROKE2, 0.24, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_H_STROKE3, 0.21, 0.12, FALSE)
};

static const BrushPoint BRUSH_I_STROKE1[] = {
  {0.20, 0.05}, {0.82, 0.08}
};
static const BrushPoint BRUSH_I_STROKE2[] = {
  {0.50, 0.05}, {0.44, 0.96}
};
static const BrushPoint BRUSH_I_STROKE3[] = {
  {0.18, 0.92}, {0.78, 0.90}
};
static const BrushStroke BRUSH_I_STROKES[] = {
  BRUSH_STROKE(BRUSH_I_STROKE1, 0.20, 0.09, FALSE),
  BRUSH_STROKE(BRUSH_I_STROKE2, 0.20, 0.08, FALSE),
  BRUSH_STROKE(BRUSH_I_STROKE3, 0.20, 0.09, FALSE)
};

static const BrushPoint BRUSH_J_STROKE1[] = {
  {0.76, 0.05}, {0.32, 0.02}
};
static const BrushPoint BRUSH_J_STROKE2[] = {
  {0.68, 0.05}, {0.64, 0.78}, {0.42, 0.96}, {0.18, 0.80}
};
static const BrushStroke BRUSH_J_STROKES[] = {
  BRUSH_STROKE(BRUSH_J_STROKE1, 0.20, 0.10, FALSE),
  BRUSH_STROKE(BRUSH_J_STROKE2, 0.22, 0.12, FALSE)
};

static const BrushPoint BRUSH_K_STROKE1[] = {
  {0.18, 0.02}, {0.12, 0.98}
};
static const BrushPoint BRUSH_K_STROKE2[] = {
  {0.82, 0.05}, {0.24, 0.52}
};
static const BrushPoint BRUSH_K_STROKE3[] = {
  {0.26, 0.52}, {0.86, 0.98}
};
static const BrushStroke BRUSH_K_STROKES[] = {
  BRUSH_STROKE(BRUSH_K_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_K_STROKE2, 0.22, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_K_STROKE3, 0.22, 0.11, FALSE)
};

static const BrushPoint BRUSH_L_STROKE1[] = {
  {0.18, 0.02}, {0.12, 0.98}, {0.70, 0.92}
};
static const BrushStroke BRUSH_L_STROKES[] = {
  BRUSH_STROKE(BRUSH_L_STROKE1, 0.24, 0.12, FALSE)
};

static const BrushPoint BRUSH_M_STROKE1[] = {
  {0.08, 0.96}, {0.18, 0.05}, {0.40, 0.58}, {0.50, 0.10}, {0.92, 0.98}
};
static const BrushStroke BRUSH_M_STROKES[] = {
  BRUSH_STROKE(BRUSH_M_STROKE1, 0.26, 0.14, FALSE)
};

static const BrushPoint BRUSH_N_STROKE1[] = {
  {0.12, 0.98}, {0.16, 0.08}, {0.16, 0.02}
};
static const BrushPoint BRUSH_N_STROKE2[] = {
  {0.18, 0.10}, {0.24, 0.20}, {0.70, 0.88}, {0.86, 0.98}
};
static const BrushPoint BRUSH_N_STROKE3[] = {
  {0.78, 0.02}, {0.86, 0.24}, {0.90, 0.98}
};
static const BrushStroke BRUSH_N_STROKES[] = {
  BRUSH_STROKE(BRUSH_N_STROKE1, 0.28, 0.14, FALSE),
  BRUSH_STROKE(BRUSH_N_STROKE2, 0.26, 0.15, FALSE),
  BRUSH_STROKE(BRUSH_N_STROKE3, 0.24, 0.13, FALSE)
};

static const BrushPoint BRUSH_N_LOWER_STROKE1[] = {
  {0.16, 0.88}, {0.20, 0.10}, {0.22, 0.04}
};
static const BrushPoint BRUSH_N_LOWER_STROKE2[] = {
  {0.22, 0.15}, {0.34, 0.26}, {0.74, 0.86}, {0.86, 0.94}
};
static const BrushPoint BRUSH_N_LOWER_STROKE3[] = {
  {0.68, 0.02}, {0.78, 0.18}, {0.82, 0.88}
};
static const BrushStroke BRUSH_N_LOWER_STROKES[] = {
  BRUSH_STROKE(BRUSH_N_LOWER_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_N_LOWER_STROKE2, 0.24, 0.13, FALSE),
  BRUSH_STROKE(BRUSH_N_LOWER_STROKE3, 0.22, 0.11, FALSE)
};

static const BrushPoint BRUSH_O_STROKE1[] = {
  {0.48, 0.02}, {0.20, 0.18}, {0.08, 0.50}, {0.24, 0.82}, {0.54, 0.98}, {0.84, 0.74}, {0.94, 0.38}, {0.70, 0.10}, {0.48, 0.02}
};
static const BrushStroke BRUSH_O_STROKES[] = {
  BRUSH_STROKE(BRUSH_O_STROKE1, 0.26, 0.15, TRUE)
};

static const BrushPoint BRUSH_P_STROKE1[] = {
  {0.12, 0.02}, {0.10, 0.98}
};
static const BrushPoint BRUSH_P_STROKE2[] = {
  {0.12, 0.05}, {0.60, 0.08}, {0.70, 0.32}, {0.20, 0.45}
};
static const BrushStroke BRUSH_P_STROKES[] = {
  BRUSH_STROKE(BRUSH_P_STROKE1, 0.24, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_P_STROKE2, 0.22, 0.13, FALSE)
};

static const BrushPoint BRUSH_Q_STROKE1[] = {
  {0.48, 0.02}, {0.20, 0.18}, {0.08, 0.50}, {0.24, 0.82}, {0.56, 0.98}, {0.86, 0.74}, {0.92, 0.44}, {0.70, 0.16}, {0.48, 0.02}
};
static const BrushPoint BRUSH_Q_STROKE2[] = {
  {0.64, 0.72}, {0.94, 1.05}
};
static const BrushStroke BRUSH_Q_STROKES[] = {
  BRUSH_STROKE(BRUSH_Q_STROKE1, 0.26, 0.15, TRUE),
  BRUSH_STROKE(BRUSH_Q_STROKE2, 0.18, 0.12, FALSE)
};

static const BrushPoint BRUSH_R_STROKE1[] = {
  {0.12, 0.02}, {0.10, 0.98}
};
static const BrushPoint BRUSH_R_STROKE2[] = {
  {0.12, 0.06}, {0.62, 0.08}, {0.70, 0.32}, {0.20, 0.45}
};
static const BrushPoint BRUSH_R_STROKE3[] = {
  {0.26, 0.52}, {0.86, 0.98}
};
static const BrushStroke BRUSH_R_STROKES[] = {
  BRUSH_STROKE(BRUSH_R_STROKE1, 0.24, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_R_STROKE2, 0.22, 0.13, FALSE),
  BRUSH_STROKE(BRUSH_R_STROKE3, 0.22, 0.12, FALSE)
};

static const BrushPoint BRUSH_S_STROKE1[] = {
  {0.78, 0.12}, {0.48, 0.05}, {0.20, 0.20}, {0.40, 0.45}, {0.68, 0.60}, {0.32, 0.80}, {0.12, 0.94}
};
static const BrushStroke BRUSH_S_STROKES[] = {
  BRUSH_STROKE(BRUSH_S_STROKE1, 0.24, 0.14, FALSE)
};

static const BrushPoint BRUSH_T_STROKE1[] = {
  {0.12, 0.08}, {0.90, 0.04}
};
static const BrushPoint BRUSH_T_STROKE2[] = {
  {0.48, 0.02}, {0.42, 0.98}
};
static const BrushStroke BRUSH_T_STROKES[] = {
  BRUSH_STROKE(BRUSH_T_STROKE1, 0.20, 0.10, FALSE),
  BRUSH_STROKE(BRUSH_T_STROKE2, 0.22, 0.12, FALSE)
};

static const BrushPoint BRUSH_U_STROKE1[] = {
  {0.10, 0.05}, {0.20, 0.78}, {0.48, 0.98}, {0.80, 0.70}, {0.86, 0.05}
};
static const BrushStroke BRUSH_U_STROKES[] = {
  BRUSH_STROKE(BRUSH_U_STROKE1, 0.24, 0.13, FALSE)
};

static const BrushPoint BRUSH_V_STROKE1[] = {
  {0.05, 0.05}, {0.40, 0.94}, {0.82, 0.05}
};
static const BrushStroke BRUSH_V_STROKES[] = {
  BRUSH_STROKE(BRUSH_V_STROKE1, 0.26, 0.14, FALSE)
};

static const BrushPoint BRUSH_W_STROKE1[] = {
  {0.04, 0.05}, {0.24, 0.98}
};
static const BrushPoint BRUSH_W_STROKE2[] = {
  {0.26, 0.94}, {0.42, 0.08}, {0.50, 0.40}
};
static const BrushPoint BRUSH_W_STROKE3[] = {
  {0.52, 0.42}, {0.62, 0.08}, {0.70, 0.94}
};
static const BrushPoint BRUSH_W_STROKE4[] = {
  {0.72, 0.92}, {0.92, 0.05}
};
static const BrushStroke BRUSH_W_STROKES[] = {
  BRUSH_STROKE(BRUSH_W_STROKE1, 0.28, 0.13, FALSE),
  BRUSH_STROKE(BRUSH_W_STROKE2, 0.26, 0.14, FALSE),
  BRUSH_STROKE(BRUSH_W_STROKE3, 0.26, 0.14, FALSE),
  BRUSH_STROKE(BRUSH_W_STROKE4, 0.28, 0.13, FALSE)
};

static const BrushPoint BRUSH_W_LOWER_STROKE1[] = {
  {0.08, 0.08}, {0.24, 0.92}, {0.40, 0.16}, {0.52, 0.90}, {0.70, 0.18}, {0.88, 0.94}
};
static const BrushStroke BRUSH_W_LOWER_STROKES[] = {
  BRUSH_STROKE(BRUSH_W_LOWER_STROKE1, 0.28, 0.15, FALSE)
};

static const BrushPoint BRUSH_X_STROKE1[] = {
  {0.10, 0.06}, {0.86, 0.96}
};
static const BrushPoint BRUSH_X_STROKE2[] = {
  {0.86, 0.08}, {0.12, 0.94}
};
static const BrushStroke BRUSH_X_STROKES[] = {
  BRUSH_STROKE(BRUSH_X_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_X_STROKE2, 0.24, 0.12, FALSE)
};

static const BrushPoint BRUSH_Y_STROKE1[] = {
  {0.08, 0.05}, {0.40, 0.40}
};
static const BrushPoint BRUSH_Y_STROKE2[] = {
  {0.90, 0.04}, {0.58, 0.48}, {0.48, 0.98}
};
static const BrushStroke BRUSH_Y_STROKES[] = {
  BRUSH_STROKE(BRUSH_Y_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_Y_STROKE2, 0.24, 0.12, FALSE)
};

static const BrushPoint BRUSH_Z_STROKE1[] = {
  {0.08, 0.08}, {0.90, 0.05}
};
static const BrushPoint BRUSH_Z_STROKE2[] = {
  {0.88, 0.05}, {0.12, 0.95}
};
static const BrushPoint BRUSH_Z_STROKE3[] = {
  {0.10, 0.92}, {0.88, 0.94}
};
static const BrushStroke BRUSH_Z_STROKES[] = {
  BRUSH_STROKE(BRUSH_Z_STROKE1, 0.22, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_Z_STROKE2, 0.22, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_Z_STROKE3, 0.22, 0.11, FALSE)
};

static const BrushPoint BRUSH_ZERO_STROKE1[] = {
  {0.48, 0.02}, {0.20, 0.15}, {0.10, 0.48}, {0.26, 0.88}, {0.60, 0.98}, {0.86, 0.62}, {0.72, 0.18}, {0.48, 0.02}
};
static const BrushStroke BRUSH_ZERO_STROKES[] = {
  BRUSH_STROKE(BRUSH_ZERO_STROKE1, 0.26, 0.15, TRUE)
};

static const BrushPoint BRUSH_ONE_STROKE1[] = {
  {0.32, 0.18}, {0.56, 0.02}, {0.48, 0.96}
};
static const BrushPoint BRUSH_ONE_STROKE2[] = {
  {0.22, 0.92}, {0.68, 0.90}
};
static const BrushStroke BRUSH_ONE_STROKES[] = {
  BRUSH_STROKE(BRUSH_ONE_STROKE1, 0.24, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_ONE_STROKE2, 0.20, 0.10, FALSE)
};

static const BrushPoint BRUSH_TWO_STROKE1[] = {
  {0.18, 0.18}, {0.42, 0.02}, {0.74, 0.18}, {0.60, 0.40}, {0.18, 0.80}, {0.82, 0.92}
};
static const BrushStroke BRUSH_TWO_STROKES[] = {
  BRUSH_STROKE(BRUSH_TWO_STROKE1, 0.24, 0.13, FALSE)
};

static const BrushPoint BRUSH_THREE_STROKE1[] = {
  {0.20, 0.12}, {0.54, 0.02}, {0.80, 0.22}, {0.40, 0.42}, {0.72, 0.60}, {0.30, 0.82}, {0.78, 0.94}
};
static const BrushStroke BRUSH_THREE_STROKES[] = {
  BRUSH_STROKE(BRUSH_THREE_STROKE1, 0.24, 0.14, FALSE)
};

static const BrushPoint BRUSH_FOUR_STROKE1[] = {
  {0.70, 0.05}, {0.24, 0.62}, {0.90, 0.58}
};
static const BrushPoint BRUSH_FOUR_STROKE2[] = {
  {0.72, 0.02}, {0.68, 0.98}
};
static const BrushStroke BRUSH_FOUR_STROKES[] = {
  BRUSH_STROKE(BRUSH_FOUR_STROKE1, 0.22, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_FOUR_STROKE2, 0.22, 0.12, FALSE)
};

static const BrushPoint BRUSH_FIVE_STROKE1[] = {
  {0.76, 0.05}, {0.20, 0.08}, {0.16, 0.42}, {0.58, 0.38}, {0.78, 0.68}, {0.28, 0.94}
};
static const BrushStroke BRUSH_FIVE_STROKES[] = {
  BRUSH_STROKE(BRUSH_FIVE_STROKE1, 0.24, 0.13, FALSE)
};

static const BrushPoint BRUSH_SIX_STROKE1[] = {
  {0.70, 0.12}, {0.40, 0.05}, {0.18, 0.28}, {0.24, 0.60}, {0.55, 0.64}, {0.80, 0.86}, {0.30, 0.94}
};
static const BrushStroke BRUSH_SIX_STROKES[] = {
  BRUSH_STROKE(BRUSH_SIX_STROKE1, 0.24, 0.14, FALSE)
};

static const BrushPoint BRUSH_SEVEN_STROKE1[] = {
  {0.10, 0.08}, {0.88, 0.05}
};
static const BrushPoint BRUSH_SEVEN_STROKE2[] = {
  {0.86, 0.06}, {0.32, 0.98}
};
static const BrushStroke BRUSH_SEVEN_STROKES[] = {
  BRUSH_STROKE(BRUSH_SEVEN_STROKE1, 0.22, 0.11, FALSE),
  BRUSH_STROKE(BRUSH_SEVEN_STROKE2, 0.22, 0.11, FALSE)
};

static const BrushPoint BRUSH_EIGHT_STROKE1[] = {
  {0.52, 0.05}, {0.24, 0.20}, {0.48, 0.42}, {0.72, 0.20}, {0.48, 0.05}
};
static const BrushPoint BRUSH_EIGHT_STROKE2[] = {
  {0.52, 0.48}, {0.20, 0.66}, {0.48, 0.94}, {0.82, 0.70}, {0.52, 0.48}
};
static const BrushStroke BRUSH_EIGHT_STROKES[] = {
  BRUSH_STROKE(BRUSH_EIGHT_STROKE1, 0.24, 0.14, TRUE),
  BRUSH_STROKE(BRUSH_EIGHT_STROKE2, 0.24, 0.14, TRUE)
};

static const BrushPoint BRUSH_NINE_STROKE1[] = {
  {0.24, 0.82}, {0.52, 0.98}, {0.82, 0.74}, {0.68, 0.40}, {0.30, 0.36}, {0.12, 0.08}
};
static const BrushStroke BRUSH_NINE_STROKES[] = {
  BRUSH_STROKE(BRUSH_NINE_STROKE1, 0.24, 0.13, FALSE)
};

static const BrushPoint BRUSH_PERIOD_STROKE1[] = {
  {0.45, 0.82}, {0.55, 0.82}, {0.55, 0.92}, {0.45, 0.92}, {0.45, 0.82}
};
static const BrushStroke BRUSH_PERIOD_STROKES[] = {
  BRUSH_STROKE(BRUSH_PERIOD_STROKE1, 0.16, 0.08, TRUE)
};

static const BrushPoint BRUSH_COMMA_STROKE1[] = {
  {0.52, 0.78}, {0.60, 0.95}, {0.40, 1.05}
};
static const BrushStroke BRUSH_COMMA_STROKES[] = {
  BRUSH_STROKE(BRUSH_COMMA_STROKE1, 0.18, 0.09, FALSE)
};

static const BrushPoint BRUSH_QUESTION_STROKE1[] = {
  {0.28, 0.22}, {0.42, 0.05}, {0.70, 0.18}, {0.68, 0.40}, {0.46, 0.52}, {0.44, 0.72}
};
static const BrushStroke BRUSH_QUESTION_STROKES[] = {
  BRUSH_STROKE(BRUSH_QUESTION_STROKE1, 0.22, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_PERIOD_STROKE1, 0.16, 0.08, TRUE)
};

static const BrushStroke BRUSH_SPACE_STROKES[] = {};

static const BrushGlyph brush_glyphs[] = {
  BRUSH_GLYPH_ENTRY('A', 1.05, BRUSH_A_STROKES),
  BRUSH_GLYPH_ENTRY('B', 1.05, BRUSH_B_STROKES),
  BRUSH_GLYPH_ENTRY('C', 1.02, BRUSH_C_STROKES),
  BRUSH_GLYPH_ENTRY('D', 1.08, BRUSH_D_STROKES),
  BRUSH_GLYPH_ENTRY('E', 1.00, BRUSH_E_STROKES),
  BRUSH_GLYPH_ENTRY('F', 0.98, BRUSH_F_STROKES),
  BRUSH_GLYPH_ENTRY('G', 1.06, BRUSH_G_STROKES),
  BRUSH_GLYPH_ENTRY('H', 1.06, BRUSH_H_STROKES),
  BRUSH_GLYPH_ENTRY('I', 0.72, BRUSH_I_STROKES),
  BRUSH_GLYPH_ENTRY('J', 0.96, BRUSH_J_STROKES),
  BRUSH_GLYPH_ENTRY('K', 1.04, BRUSH_K_STROKES),
  BRUSH_GLYPH_ENTRY('L', 0.96, BRUSH_L_STROKES),
  BRUSH_GLYPH_ENTRY('M', 1.20, BRUSH_M_STROKES),
  BRUSH_GLYPH_ENTRY('N', 1.08, BRUSH_N_STROKES),
  BRUSH_GLYPH_ENTRY('n', 0.92, BRUSH_N_LOWER_STROKES),
  BRUSH_GLYPH_ENTRY('O', 1.10, BRUSH_O_STROKES),
  BRUSH_GLYPH_ENTRY('P', 0.98, BRUSH_P_STROKES),
  BRUSH_GLYPH_ENTRY('Q', 1.12, BRUSH_Q_STROKES),
  BRUSH_GLYPH_ENTRY('R', 1.04, BRUSH_R_STROKES),
  BRUSH_GLYPH_ENTRY('S', 1.00, BRUSH_S_STROKES),
  BRUSH_GLYPH_ENTRY('T', 1.00, BRUSH_T_STROKES),
  BRUSH_GLYPH_ENTRY('U', 1.08, BRUSH_U_STROKES),
  BRUSH_GLYPH_ENTRY('V', 1.08, BRUSH_V_STROKES),
  BRUSH_GLYPH_ENTRY('W', 1.28, BRUSH_W_STROKES),
  BRUSH_GLYPH_ENTRY('w', 1.16, BRUSH_W_LOWER_STROKES),
  BRUSH_GLYPH_ENTRY('X', 1.02, BRUSH_X_STROKES),
  BRUSH_GLYPH_ENTRY('Y', 1.02, BRUSH_Y_STROKES),
  BRUSH_GLYPH_ENTRY('Z', 1.02, BRUSH_Z_STROKES),
  BRUSH_GLYPH_ENTRY('0', 1.04, BRUSH_ZERO_STROKES),
  BRUSH_GLYPH_ENTRY('1', 0.82, BRUSH_ONE_STROKES),
  BRUSH_GLYPH_ENTRY('2', 1.00, BRUSH_TWO_STROKES),
  BRUSH_GLYPH_ENTRY('3', 1.00, BRUSH_THREE_STROKES),
  BRUSH_GLYPH_ENTRY('4', 1.04, BRUSH_FOUR_STROKES),
  BRUSH_GLYPH_ENTRY('5', 1.00, BRUSH_FIVE_STROKES),
  BRUSH_GLYPH_ENTRY('6', 1.00, BRUSH_SIX_STROKES),
  BRUSH_GLYPH_ENTRY('7', 1.00, BRUSH_SEVEN_STROKES),
  BRUSH_GLYPH_ENTRY('8', 1.04, BRUSH_EIGHT_STROKES),
  BRUSH_GLYPH_ENTRY('9', 1.00, BRUSH_NINE_STROKES),
  BRUSH_GLYPH_ENTRY('.', 0.52, BRUSH_PERIOD_STROKES),
  BRUSH_GLYPH_ENTRY(',', 0.54, BRUSH_COMMA_STROKES),
  BRUSH_GLYPH_ENTRY('?', 0.96, BRUSH_QUESTION_STROKES),
  BRUSH_GLYPH_ENTRY(' ', 0.55, BRUSH_SPACE_STROKES)
};

static const BrushPoint BRUSH_DEFAULT_STROKE1[] = {
  {0.20, 0.08}, {0.50, 0.50}, {0.30, 0.92}
};
static const BrushPoint BRUSH_DEFAULT_STROKE2[] = {
  {0.78, 0.12}, {0.52, 0.38}, {0.72, 0.86}
};
static const BrushStroke BRUSH_DEFAULT_STROKES[] = {
  BRUSH_STROKE(BRUSH_DEFAULT_STROKE1, 0.22, 0.12, FALSE),
  BRUSH_STROKE(BRUSH_DEFAULT_STROKE2, 0.22, 0.12, FALSE)
};

static const BrushGlyph brush_default_glyph = BRUSH_GLYPH_ENTRY('?', 0.95, BRUSH_DEFAULT_STROKES);

static const BrushGlyph* brush_find_glyph(gunichar codepoint) {
  for (guint i = 0; i < G_N_ELEMENTS(brush_glyphs); i++) {
    if (brush_glyphs[i].codepoint == codepoint) {
      return &brush_glyphs[i];
    }
  }
  return NULL;
}

static const BrushGlyph* brush_get_space_glyph(void) {
  static gboolean initialized = FALSE;
  static const BrushGlyph *space_glyph = NULL;
  if (!initialized) {
    space_glyph = brush_find_glyph(' ');
    if (!space_glyph) {
      space_glyph = &brush_default_glyph;
    }
    initialized = TRUE;
  }
  return space_glyph;
}

static const BrushGlyph* brush_lookup(gunichar ch) {
  if (ch == ' ') {
    return brush_get_space_glyph();
  }
  if (ch == '\t') {
    return brush_get_space_glyph();
  }
  const BrushGlyph *direct = brush_find_glyph(ch);
  if (direct) {
    return direct;
  }
  gunichar upper = g_unichar_toupper(ch);
  const BrushGlyph *glyph = brush_find_glyph(upper);
  if (glyph) {
    return glyph;
  }
  return &brush_default_glyph;
}

static guint32 text_outline_seed(const Shape *shape) {
  guint32 seed = (guint32)(shape->base.x * 73856093u) ^
                 (guint32)(shape->base.y * 19349663u) ^
                 (guint32)(shape->base.width * 83492791u) ^
                 (guint32)(shape->base.height * 2654435761u);
  if (shape->text && *shape->text) {
    seed ^= g_str_hash(shape->text);
  }
  if (seed == 0) seed = 0x9e3779b9u;
  return seed;
}

static inline double text_outline_rand(guint32 *state) {
  *state = (*state * 1664525u) + 1013904223u;
  return ((*state >> 8) & 0xFFFFFF) / (double)0x1000000;
}

static void brush_draw_stroke(cairo_t *cr,
                              const BrushStroke *stroke,
                              double origin_x,
                              double origin_y,
                              double scale,
                              double shear,
                              double base_r,
                              double base_g,
                              double base_b,
                              double base_a,
                              guint32 *seed) {
  if (!stroke || stroke->point_count < 2) {
    return;
  }

  static const double width_multipliers[] = {1.35, 0.95, 0.55};
  static const double alpha_multipliers[] = {0.55, 0.90, 0.70};
  static const double color_lift[] = {0.00, 0.05, 0.10};

  for (int pass = 0; pass < 3; pass++) {
    cairo_new_path(cr);
    for (guint i = 0; i < stroke->point_count; i++) {
      double jitter = stroke->jitter * scale;
      double px = origin_x + stroke->points[i].x * scale + (text_outline_rand(seed) - 0.5) * jitter;
      double py = origin_y + stroke->points[i].y * scale + (text_outline_rand(seed) - 0.5) * jitter;
      double shear_offset = shear * (py - origin_y);
      px += shear_offset;
      if (i == 0) {
        cairo_move_to(cr, px, py);
      } else {
        cairo_line_to(cr, px, py);
      }
    }

    if (stroke->closed) {
      cairo_close_path(cr);
    }

    double width_variance = 0.85 + text_outline_rand(seed) * 0.30;
    double line_width = stroke->width * scale * width_multipliers[pass] * width_variance;
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, MAX(0.5, line_width));

    double shade = MIN(1.0, base_r + color_lift[pass]);
    double shade_g = MIN(1.0, base_g + color_lift[pass]);
    double shade_b = MIN(1.0, base_b + color_lift[pass]);
    cairo_set_source_rgba(cr, shade, shade_g, shade_b, base_a * alpha_multipliers[pass]);

    if (stroke->closed && pass == 0) {
      cairo_fill_preserve(cr);
      cairo_set_line_width(cr, MAX(0.5, line_width * 0.45));
      cairo_set_source_rgba(cr,
                            MIN(1.0, shade + 0.1),
                            MIN(1.0, shade_g + 0.1),
                            MIN(1.0, shade_b + 0.1),
                            base_a * 0.65);
      cairo_stroke(cr);
    } else {
      cairo_stroke(cr);
    }
  }
}

static void brush_draw_glyph(cairo_t *cr,
                             const BrushGlyph *glyph,
                             double origin_x,
                             double origin_y,
                             double scale,
                             double shear,
                             double base_r,
                             double base_g,
                             double base_b,
                             double base_a,
                             guint32 *seed) {
  if (!glyph) return;
  if (glyph->stroke_count > 0) {
    guint32 shadow_seed = *seed ^ 0x5f5f5f5f;
    double shadow_offset_x = scale * 0.08;
    double shadow_offset_y = scale * 0.10;
    for (guint i = 0; i < glyph->stroke_count; i++) {
      brush_draw_stroke(cr, &glyph->strokes[i],
                        origin_x + shadow_offset_x,
                        origin_y + shadow_offset_y,
                        scale,
                        shear,
                        0.18,
                        0.18,
                        0.18,
                        base_a * 0.45,
                        &shadow_seed);
    }
  }

  for (guint i = 0; i < glyph->stroke_count; i++) {
    brush_draw_stroke(cr, &glyph->strokes[i], origin_x, origin_y,
                      scale, shear, base_r, base_g, base_b, base_a, seed);
  }
}

#define BRUSH_BASE_HEIGHT 1.0
#define BRUSH_LINE_GAP 0.38
#define BRUSH_TAB_MULTIPLIER 4

static double brush_line_units(const char *line) {
  double units = 0.0;
  if (!line) return units;

  const BrushGlyph *space_glyph = brush_get_space_glyph();
  const double space_advance = space_glyph ? space_glyph->advance : 0.6;

  const char *p = line;
  while (p && *p) {
    gunichar ch = g_utf8_get_char(p);
    if (ch == '\t') {
      units += space_advance * BRUSH_TAB_MULTIPLIER;
    } else {
      const BrushGlyph *glyph = brush_lookup(ch);
      if (glyph == &brush_default_glyph && glyph->stroke_count == 0) {
        units += space_advance;
      } else {
        units += glyph->advance;
      }
    }
    p = g_utf8_next_char(p);
  }

  return units;
}

static void brush_render_text(cairo_t *cr,
                              const char *text,
                              double x,
                              double y,
                              double width,
                              double height,
                              double base_r,
                              double base_g,
                              double base_b,
                              double base_a,
                              double shear,
                              guint32 seed) {
  if (!text || !*text) {
    return;
  }

  char **lines = g_strsplit(text, "\n", -1);
  if (!lines) return;

  GArray *line_units = g_array_new(FALSE, FALSE, sizeof(double));
  double max_units = 0.0;
  int line_count = 0;

  for (int i = 0; lines[i]; i++) {
    double units = brush_line_units(lines[i]);
    if (units <= 0.0) {
      units = brush_get_space_glyph()->advance;
    }
    g_array_append_val(line_units, units);
    if (units > max_units) max_units = units;
    line_count++;
  }

  if (line_count == 0 || max_units <= 0.0) {
    g_array_free(line_units, TRUE);
    g_strfreev(lines);
    return;
  }

  double total_height_units = line_count * BRUSH_BASE_HEIGHT + (line_count - 1) * BRUSH_LINE_GAP;
  if (total_height_units <= 0.0) {
    total_height_units = BRUSH_BASE_HEIGHT;
  }

  double scale_x = width / max_units;
  double scale_y = height / total_height_units;
  double scale = MIN(scale_x, scale_y);
  if (scale <= 0.0) {
    scale = 1.0;
  }

  double used_height = total_height_units * scale;
  double y_offset = (height - used_height) / 2.0;
  if (y_offset < 0.0) y_offset = 0.0;

  const BrushGlyph *space_glyph = brush_get_space_glyph();
  double space_advance = space_glyph ? space_glyph->advance : 0.6;

  for (int line_idx = 0; line_idx < line_count; line_idx++) {
    const char *line_text = lines[line_idx];
    double units = g_array_index(line_units, double, line_idx);
    double line_width = units * scale;
    double x_offset = (width - line_width) / 2.0;
    if (x_offset < 0.0) x_offset = 0.0;

    double origin_y = y + y_offset + line_idx * (BRUSH_BASE_HEIGHT + BRUSH_LINE_GAP) * scale;
    double cursor_x = x + x_offset;

    const char *p = line_text;
    while (p && *p) {
      gunichar ch = g_utf8_get_char(p);
      if (ch == '\t') {
        cursor_x += space_advance * BRUSH_TAB_MULTIPLIER * scale;
      } else {
        const BrushGlyph *glyph = brush_lookup(ch);
        double glyph_scale = scale;
        double glyph_origin_y = origin_y;
        gboolean is_lower = g_unichar_islower(ch);
        if (is_lower) {
          glyph_scale *= 0.78;
          glyph_origin_y += scale * (BRUSH_BASE_HEIGHT - 0.78);
        }
        if (ch == '.' || ch == ',') {
          glyph_scale *= 0.55;
          glyph_origin_y += scale * 0.65;
        } else if (ch == '?') {
          glyph_scale *= 0.9;
          glyph_origin_y += scale * 0.05;
        }
        brush_draw_glyph(cr, glyph, cursor_x, glyph_origin_y,
                         glyph_scale, shear, base_r, base_g, base_b, base_a, &seed);
        cursor_x += glyph->advance * scale;
      }
      p = g_utf8_next_char(p);
    }
  }

  g_array_free(line_units, TRUE);
  g_strfreev(lines);
}

void shape_render_text_outline_sample(cairo_t *cr,
                                      const char *text,
                                      double x,
                                      double y,
                                      double width,
                                      double height,
                                      double stroke_r,
                                      double stroke_g,
                                      double stroke_b,
                                      double stroke_a) {
  guint32 seed = g_str_hash(text ? text : "TXT");
  brush_render_text(cr, text ? text : "TXT",
                    x,
                    y,
                    width,
                    height,
                    stroke_r,
                    stroke_g,
                    stroke_b,
                    stroke_a <= 0.0 ? 1.0 : stroke_a,
                    -0.22,
                    seed ^ 0x9e3779b9u);
}

static void text_outline_draw(Shape *shape, cairo_t *cr) {
  if (!shape->text || shape->text[0] == '\0') {
    return;
  }

  Element *element = (Element*)shape;
  double padding = MAX(6.0, shape->stroke_width * 1.2);
  double content_width = MAX(1.0, element->width - padding * 2.0);
  double content_height = MAX(1.0, element->height - padding * 2.0);
  double base_a = shape->stroke_a;
  if (base_a <= 0.0) base_a = 1.0;

  guint32 seed = text_outline_seed(shape);
  brush_render_text(cr,
                    shape->text,
                    element->x + padding,
                    element->y + padding,
                    content_width,
                    content_height,
                    shape->stroke_r,
                    shape->stroke_g,
                    shape->stroke_b,
                    base_a,
                    -0.22,
                    seed);
}

gboolean shape_on_textview_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
  Shape *shape = (Shape*)user_data;
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
    if (state & GDK_CONTROL_MASK) {
      // Ctrl+Enter inserts a newline
      GtkTextView *text_view = GTK_TEXT_VIEW(shape->text_view);
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
      gtk_text_buffer_insert(buffer, &iter, "\n", 1);

      return TRUE; // Handled - prevent default behavior
    } else {
      // Enter finishes editing
      shape_finish_editing((Element*)shape);
      return TRUE;
    }
  }
  return FALSE;
}

static void shape_update_text_view_position(Shape *shape) {
  if (!shape->scrolled_window || !GTK_IS_WIDGET(shape->scrolled_window)) return;

  int screen_x, screen_y;
  canvas_canvas_to_screen(shape->base.canvas_data,
                          shape->base.x, shape->base.y,
                          &screen_x, &screen_y);
  gtk_widget_set_margin_start(shape->scrolled_window, screen_x - 10);
  gtk_widget_set_margin_top(shape->scrolled_window, screen_y - 10);
  gtk_widget_set_size_request(shape->scrolled_window,
                              shape->base.width + 20,
                              shape->base.height + 20);
}

static void draw_hatch_lines(cairo_t *cr, double cx, double cy, double span, double spacing, double angle) {
  double dir_x = cos(angle);
  double dir_y = sin(angle);
  double perp_x = -dir_y;
  double perp_y = dir_x;
  double half_span = span / 2.0;
  double max_offset = span;

  for (double offset = -max_offset; offset <= max_offset; offset += spacing) {
    double start_x = cx + perp_x * offset - dir_x * half_span;
    double start_y = cy + perp_y * offset - dir_y * half_span;
    double end_x = cx + perp_x * offset + dir_x * half_span;
    double end_y = cy + perp_y * offset + dir_y * half_span;
    cairo_move_to(cr, start_x, start_y);
    cairo_line_to(cr, end_x, end_y);
  }
}

static void build_vertical_cylinder_path(cairo_t *cr, double x, double y, double width, double height) {
  double ellipse_h = height * 0.15;
  double center_x = x + width / 2.0;
  double top_y = y + ellipse_h / 2.0;
  double bottom_y = y + height - ellipse_h / 2.0;

  cairo_rectangle(cr, x, top_y, width, bottom_y - top_y);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, center_x, top_y);
  cairo_scale(cr, width / 2.0, ellipse_h / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, center_x, bottom_y);
  cairo_scale(cr, width / 2.0, ellipse_h / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);
}

static void build_horizontal_cylinder_path(cairo_t *cr, double x, double y, double width, double height) {
  double ellipse_w = width * 0.15;
  double center_y = y + height / 2.0;
  double left_x = x + ellipse_w / 2.0;
  double right_x = x + width - ellipse_w / 2.0;

  cairo_rectangle(cr, left_x, y, right_x - left_x, height);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, left_x, center_y);
  cairo_scale(cr, ellipse_w / 2.0, height / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);

  cairo_new_sub_path(cr);
  cairo_save(cr);
  cairo_translate(cr, right_x, center_y);
  cairo_scale(cr, ellipse_w / 2.0, height / 2.0);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);
}

static void apply_fill(Shape *shape, cairo_t *cr) {
  if (!shape->filled) return;

  cairo_path_t *path = cairo_copy_path(cr);

  double x1, y1, x2, y2;
  cairo_path_extents(cr, &x1, &y1, &x2, &y2);
  double width = MAX(x2 - x1, 1.0);
  double height = MAX(y2 - y1, 1.0);

  if (shape->fill_style == FILL_STYLE_SOLID) {
    cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
    cairo_fill_preserve(cr);
    cairo_path_destroy(path);
    return;
  }

  cairo_save(cr);
  cairo_new_path(cr);
  cairo_append_path(cr, path);
  cairo_clip(cr);

  cairo_set_dash(cr, NULL, 0, 0);
  double spacing = MAX(4.0, shape->stroke_width * 2.0);
  double pattern_alpha = MIN(1.0, shape->base.bg_a);
  double line_width = MAX(1.0, shape->stroke_width * 0.35);
  cairo_set_line_width(cr, line_width);
  cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, pattern_alpha);

  double cx = (x1 + x2) / 2.0;
  double cy = (y1 + y2) / 2.0;
  double span = hypot(width, height) + spacing * 2.0;

  cairo_new_path(cr);
  draw_hatch_lines(cr, cx, cy, span, spacing, G_PI / 4.0);
  cairo_stroke(cr);

  if (shape->fill_style == FILL_STYLE_CROSS_HATCH) {
    cairo_new_path(cr);
    draw_hatch_lines(cr, cx, cy, span, spacing, -G_PI / 4.0);
    cairo_stroke(cr);
  }

  cairo_restore(cr);
  cairo_new_path(cr);
  cairo_append_path(cr, path);
  cairo_path_destroy(path);
}

static void shape_get_connection_point(Element *element, int point, int *cx, int *cy) {
  Shape *shape = (Shape*)element;
  int unrotated_x, unrotated_y;

  if (shape->has_bezier_points && (shape->shape_type == SHAPE_BEZIER || shape->shape_type == SHAPE_CURVED_ARROW)) {
    double p0_x = element->x + shape->bezier_p0_u * element->width;
    double p0_y = element->y + shape->bezier_p0_v * element->height;
    double p1_x = element->x + shape->bezier_p1_u * element->width;
    double p1_y = element->y + shape->bezier_p1_v * element->height;
    double p2_x = element->x + shape->bezier_p2_u * element->width;
    double p2_y = element->y + shape->bezier_p2_v * element->height;
    double p3_x = element->x + shape->bezier_p3_u * element->width;
    double p3_y = element->y + shape->bezier_p3_v * element->height;

    switch (point) {
      case 0:
        unrotated_x = (int)round(p0_x);
        unrotated_y = (int)round(p0_y);
        break;
      case 1:
        unrotated_x = (int)round(p1_x);
        unrotated_y = (int)round(p1_y);
        break;
      case 2:
        unrotated_x = (int)round(p2_x);
        unrotated_y = (int)round(p2_y);
        break;
      case 3:
        unrotated_x = (int)round(p3_x);
        unrotated_y = (int)round(p3_y);
        break;
      default:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
        break;
    }
  } else if (shape->has_line_points &&
      (shape->shape_type == SHAPE_LINE || shape->shape_type == SHAPE_ARROW)) {
    double start_x = element->x + shape->line_start_u * element->width;
    double start_y = element->y + shape->line_start_v * element->height;
    double end_x = element->x + shape->line_end_u * element->width;
    double end_y = element->y + shape->line_end_v * element->height;
    double mid_x = (start_x + end_x) / 2.0;
    double mid_y = (start_y + end_y) / 2.0;

    switch (point) {
      case 0:
        unrotated_x = (int)round(start_x);
        unrotated_y = (int)round(start_y);
        break;
      case 1:
        unrotated_x = (int)round(end_x);
        unrotated_y = (int)round(end_y);
        break;
      case 2:
        unrotated_x = (int)round(mid_x);
        unrotated_y = (int)round(mid_y);
        break;
      case 3:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
        break;
      default:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
        break;
    }
  } else {
    switch (point) {
      case 0: // Top
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y;
        break;
      case 1: // Right
        unrotated_x = element->x + element->width;
        unrotated_y = element->y + element->height / 2;
        break;
      case 2: // Bottom
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height;
        break;
      case 3: // Left
        unrotated_x = element->x;
        unrotated_y = element->y + element->height / 2;
        break;
      default:
        unrotated_x = element->x + element->width / 2;
        unrotated_y = element->y + element->height / 2;
    }
  }

  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    double dx = unrotated_x - center_x;
    double dy = unrotated_y - center_y;
    double angle_rad = element->rotation_degrees * M_PI / 180.0;
    *cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
    *cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
  } else {
    *cx = unrotated_x;
    *cy = unrotated_y;
  }
}

static void shape_draw(Element *element, cairo_t *cr, gboolean is_selected) {
  Shape *shape = (Shape*)element;

  if (shape->editing) {
    shape_update_text_view_position(shape);
  }

  // Save cairo state and apply rotation if needed
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Set stroke style (dashed, dotted, or solid)
  if (shape->stroke_style == STROKE_STYLE_DASHED) {
    double dashes[] = {12.0, 8.0};
    cairo_set_dash(cr, dashes, 2, 0);
  } else if (shape->stroke_style == STROKE_STYLE_DOTTED) {
    double dashes[] = {2.0, 5.0};
    cairo_set_dash(cr, dashes, 2, 0);
  } else {
    cairo_set_dash(cr, NULL, 0, 0);  // Solid line
  }

  cairo_set_line_width(cr, shape->stroke_width);
  cairo_new_path(cr);

  switch (shape->shape_type) {
    case SHAPE_CIRCLE:
      {
        // Draw circle centered in the element's bounding box
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;
        double radius = MIN(element->width, element->height) / 2.0;

        cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
        apply_fill(shape, cr);

        // Handle stroke
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_RECTANGLE:
      {
        cairo_rectangle(cr, element->x, element->y, element->width, element->height);
        apply_fill(shape, cr);

        // Handle stroke
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_ROUNDED_RECTANGLE:
      {
        double radius = MIN(element->width, element->height) * 0.2;
        if (radius < 8.0) radius = 8.0;
        double x = element->x;
        double y = element->y;
        double width = element->width;
        double height = element->height;

        double right = x + width;
        double bottom = y + height;

        cairo_new_sub_path(cr);
        cairo_arc(cr, right - radius, y + radius, radius, -G_PI_2, 0);
        cairo_arc(cr, right - radius, bottom - radius, radius, 0, G_PI_2);
        cairo_arc(cr, x + radius, bottom - radius, radius, G_PI_2, G_PI);
        cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI_2);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_TRIANGLE:
      {
        // Draw equilateral triangle
        double center_x = element->x + element->width / 2.0;
        double top_y = element->y;
        double bottom_y = element->y + element->height;

        cairo_move_to(cr, center_x, top_y);
        cairo_line_to(cr, element->x, bottom_y);
        cairo_line_to(cr, element->x + element->width, bottom_y);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_TEXT_OUTLINE:
      {
        cairo_save(cr);
        text_outline_draw(shape, cr);
        cairo_restore(cr);
      }
      break;
    case SHAPE_CYLINDER_VERTICAL:
      {
        // Draw vertical cylinder (complete ellipses at top and bottom, connected by lines)
        double ellipse_w = element->width;
        double ellipse_h = element->height * 0.15; // 15% of height for ellipse
        double center_x = element->x + element->width / 2.0;
        double top_y = element->y + ellipse_h / 2.0;
        double bottom_y = element->y + element->height - ellipse_h / 2.0;

        // Fill the cylinder body if filled
        if (shape->filled) {
          if (shape->fill_style == FILL_STYLE_SOLID) {
            cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
            cairo_rectangle(cr, element->x, top_y, element->width, bottom_y - top_y);
            cairo_fill(cr);
          } else {
            cairo_new_path(cr);
            build_vertical_cylinder_path(cr, element->x, element->y, element->width, element->height);
            apply_fill(shape, cr);
            cairo_new_path(cr);
          }
        }

        // Draw top ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, center_x, top_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);

        // Draw side lines
        cairo_move_to(cr, element->x, top_y);
        cairo_line_to(cr, element->x, bottom_y);
        cairo_move_to(cr, element->x + element->width, top_y);
        cairo_line_to(cr, element->x + element->width, bottom_y);
        cairo_stroke(cr);

        // Draw bottom ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, center_x, bottom_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_CYLINDER_HORIZONTAL:
      {
        // Draw horizontal cylinder (complete ellipses at left and right, connected by lines)
        double ellipse_w = element->width * 0.15; // 15% of width for ellipse
        double ellipse_h = element->height;
        double center_y = element->y + element->height / 2.0;
        double left_x = element->x + ellipse_w / 2.0;
        double right_x = element->x + element->width - ellipse_w / 2.0;

        // Fill the cylinder body if filled
        if (shape->filled) {
          if (shape->fill_style == FILL_STYLE_SOLID) {
            cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
            cairo_rectangle(cr, left_x, element->y, right_x - left_x, element->height);
            cairo_fill(cr);
          } else {
            cairo_new_path(cr);
            build_horizontal_cylinder_path(cr, element->x, element->y, element->width, element->height);
            apply_fill(shape, cr);
            cairo_new_path(cr);
          }
        }

        // Draw left ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, left_x, center_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);

        // Draw top and bottom lines
        cairo_move_to(cr, left_x, element->y);
        cairo_line_to(cr, right_x, element->y);
        cairo_move_to(cr, left_x, element->y + element->height);
        cairo_line_to(cr, right_x, element->y + element->height);
        cairo_stroke(cr);

        // Draw right ellipse (complete)
        cairo_save(cr);
        cairo_translate(cr, right_x, center_y);
        cairo_scale(cr, ellipse_w / 2.0, ellipse_h / 2.0);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        if (shape->filled && shape->fill_style == FILL_STYLE_SOLID) {
          cairo_set_source_rgba(cr, shape->base.bg_r, shape->base.bg_g, shape->base.bg_b, shape->base.bg_a);
          cairo_fill_preserve(cr);
        }
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_DIAMOND:
      {
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;

        cairo_move_to(cr, center_x, element->y);
        cairo_line_to(cr, element->x + element->width, center_y);
        cairo_line_to(cr, center_x, element->y + element->height);
        cairo_line_to(cr, element->x, center_y);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_TRAPEZOID:
      {
        // Draw trapezoid with top edge narrower than bottom edge
        double top_inset = element->width * 0.2;  // Top is 60% of width
        double x = element->x;
        double y = element->y;
        double width = element->width;
        double height = element->height;

        cairo_move_to(cr, x + top_inset, y);
        cairo_line_to(cr, x + width - top_inset, y);
        cairo_line_to(cr, x + width, y + height);
        cairo_line_to(cr, x, y + height);
        cairo_close_path(cr);

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_LINE:
    case SHAPE_ARROW:
      {
        double width = MAX(element->width, 1);
        double height = MAX(element->height, 1);
        double start_u = shape->has_line_points ? shape->line_start_u : 0.0;
        double start_v = shape->has_line_points ? shape->line_start_v : 0.0;
        double end_u = shape->has_line_points ? shape->line_end_u : 1.0;
        double end_v = shape->has_line_points ? shape->line_end_v : 1.0;

        double start_x = element->x + start_u * width;
        double start_y = element->y + start_v * height;
        double end_x = element->x + end_u * width;
        double end_y = element->y + end_v * height;

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);

        cairo_move_to(cr, start_x, start_y);
        cairo_line_to(cr, end_x, end_y);
        cairo_stroke(cr);

        if (shape->shape_type == SHAPE_ARROW) {
          double angle = atan2(end_y - start_y, end_x - start_x);
          double arrow_length = MAX(shape->stroke_width * 3.0, 12.0);
          double arrow_angle = 160.0 * G_PI / 180.0; // 160 degrees

          double back_x = end_x - arrow_length * cos(angle);
          double back_y = end_y - arrow_length * sin(angle);

          double left_x = back_x + arrow_length * cos(angle - arrow_angle);
          double left_y = back_y + arrow_length * sin(angle - arrow_angle);
          double right_x = back_x + arrow_length * cos(angle + arrow_angle);
          double right_y = back_y + arrow_length * sin(angle + arrow_angle);

          cairo_move_to(cr, end_x, end_y);
          cairo_line_to(cr, left_x, left_y);
          cairo_move_to(cr, end_x, end_y);
          cairo_line_to(cr, right_x, right_y);
          cairo_stroke(cr);
        }
      }
      break;
    case SHAPE_CUBE:
      {
        double x = element->x;
        double y = element->y;
        double width = element->width;
        double height = element->height;
        double offset = MIN(width, height) * 0.35;

        if (width < 10 || height < 10) { // too small to draw a cube
            cairo_rectangle(cr, x, y, width, height);
        } else {
            // Front face
            cairo_rectangle(cr, x, y + offset, width - offset, height - offset);
            // Top face
            cairo_move_to(cr, x, y + offset);
            cairo_line_to(cr, x + offset, y);
            cairo_line_to(cr, x + width, y);
            cairo_line_to(cr, x + width - offset, y + offset);
            cairo_close_path(cr);
            // Side face
            cairo_move_to(cr, x + width - offset, y + offset);
            cairo_line_to(cr, x + width, y);
            cairo_line_to(cr, x + width, y + height - offset);
            cairo_line_to(cr, x + width - offset, y + height);
            cairo_close_path(cr);
        }

        apply_fill(shape, cr);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_BEZIER:
      {
        double width = MAX(element->width, 1);
        double height = MAX(element->height, 1);

        // Default control points if not set
        double p0_u = shape->has_bezier_points ? shape->bezier_p0_u : 0.0;
        double p0_v = shape->has_bezier_points ? shape->bezier_p0_v : 0.5;
        double p1_u = shape->has_bezier_points ? shape->bezier_p1_u : 0.33;
        double p1_v = shape->has_bezier_points ? shape->bezier_p1_v : 0.0;
        double p2_u = shape->has_bezier_points ? shape->bezier_p2_u : 0.67;
        double p2_v = shape->has_bezier_points ? shape->bezier_p2_v : 1.0;
        double p3_u = shape->has_bezier_points ? shape->bezier_p3_u : 1.0;
        double p3_v = shape->has_bezier_points ? shape->bezier_p3_v : 0.5;

        double p0_x = element->x + p0_u * width;
        double p0_y = element->y + p0_v * height;
        double p1_x = element->x + p1_u * width;
        double p1_y = element->y + p1_v * height;
        double p2_x = element->x + p2_u * width;
        double p2_y = element->y + p2_v * height;
        double p3_x = element->x + p3_u * width;
        double p3_y = element->y + p3_v * height;

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);

        // Draw the bezier curve
        cairo_move_to(cr, p0_x, p0_y);
        cairo_curve_to(cr, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_CURVED_ARROW:
      {
        double width = MAX(element->width, 1);
        double height = MAX(element->height, 1);

        // Default control points if not set - creates curved arrow from bottom-left to top-right
        double p0_u = shape->has_bezier_points ? shape->bezier_p0_u : 0.0;
        double p0_v = shape->has_bezier_points ? shape->bezier_p0_v : 1.0;
        double p1_u = shape->has_bezier_points ? shape->bezier_p1_u : 0.25;
        double p1_v = shape->has_bezier_points ? shape->bezier_p1_v : 0.5;
        double p2_u = shape->has_bezier_points ? shape->bezier_p2_u : 0.75;
        double p2_v = shape->has_bezier_points ? shape->bezier_p2_v : 0.5;
        double p3_u = shape->has_bezier_points ? shape->bezier_p3_u : 1.0;
        double p3_v = shape->has_bezier_points ? shape->bezier_p3_v : 0.0;

        double p0_x = element->x + p0_u * width;
        double p0_y = element->y + p0_v * height;
        double p1_x = element->x + p1_u * width;
        double p1_y = element->y + p1_v * height;
        double p2_x = element->x + p2_u * width;
        double p2_y = element->y + p2_v * height;
        double p3_x = element->x + p3_u * width;
        double p3_y = element->y + p3_v * height;

        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);

        // Draw the bezier curve
        cairo_move_to(cr, p0_x, p0_y);
        cairo_curve_to(cr, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y);
        cairo_stroke(cr);

        // Calculate the tangent at the end of the curve for the arrowhead
        // The tangent direction at p3 is along the vector from p2 to p3
        double dx = p3_x - p2_x;
        double dy = p3_y - p2_y;
        double angle = atan2(dy, dx);

        double arrow_length = MAX(shape->stroke_width * 3.0, 12.0);
        double arrow_angle = 160.0 * G_PI / 180.0; // 160 degrees

        double back_x = p3_x - arrow_length * cos(angle);
        double back_y = p3_y - arrow_length * sin(angle);

        double left_x = back_x + arrow_length * cos(angle - arrow_angle);
        double left_y = back_y + arrow_length * sin(angle - arrow_angle);
        double right_x = back_x + arrow_length * cos(angle + arrow_angle);
        double right_y = back_y + arrow_length * sin(angle + arrow_angle);

        cairo_move_to(cr, p3_x, p3_y);
        cairo_line_to(cr, left_x, left_y);
        cairo_move_to(cr, p3_x, p3_y);
        cairo_line_to(cr, right_x, right_y);
        cairo_stroke(cr);
      }
      break;
    case SHAPE_PLOT:
      {
        // Parse multi-line plot data: each line can be "line \"Name\" x,y x,y ..." or simple "x,y" format
        if (!shape->text || strlen(shape->text) == 0) {
          // Draw empty plot with axes
          double margin = 20.0;
          cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
          cairo_set_line_width(cr, 1.0);

          // Y-axis
          cairo_move_to(cr, element->x + margin, element->y + margin);
          cairo_line_to(cr, element->x + margin, element->y + element->height - margin);
          // X-axis
          cairo_line_to(cr, element->x + element->width - margin, element->y + element->height - margin);
          cairo_stroke(cr);
        } else {
          // Data structure for multi-line support
          typedef struct {
            gchar *label;
            GArray *x_values;
            GArray *y_values;
          } PlotLine;

          GArray *plot_lines = g_array_new(FALSE, FALSE, sizeof(PlotLine));

          // Parse input text
          gchar **lines = g_strsplit(shape->text, "\n", -1);
          PlotLine *current_line = NULL;

          for (int i = 0; lines[i] != NULL; i++) {
            gchar *line = g_strstrip(lines[i]);
            if (strlen(line) == 0) continue;

            // Check if line starts with "line " keyword
            if (g_str_has_prefix(line, "line ")) {
              // New line definition: line "Name" x,y x,y ... or line Name x,y x,y ...
              const char *rest = line + 5; // skip "line "

              // Parse label (with or without quotes)
              gchar *label = NULL;
              const char *data_start = rest;

              if (*rest == '"') {
                // Quoted label
                rest++; // skip opening quote
                const char *end_quote = strchr(rest, '"');
                if (end_quote) {
                  label = g_strndup(rest, end_quote - rest);
                  data_start = end_quote + 1;
                }
              } else {
                // Unquoted label (read until space or comma)
                const char *label_end = rest;
                while (*label_end && !g_ascii_isspace(*label_end) && *label_end != ',') {
                  label_end++;
                }
                if (label_end > rest) {
                  label = g_strndup(rest, label_end - rest);
                  data_start = label_end;
                }
              }

              // Create new plot line
              PlotLine new_line;
              new_line.label = label ? label : g_strdup("Series");
              new_line.x_values = g_array_new(FALSE, FALSE, sizeof(double));
              new_line.y_values = g_array_new(FALSE, FALSE, sizeof(double));
              g_array_append_val(plot_lines, new_line);
              current_line = &g_array_index(plot_lines, PlotLine, plot_lines->len - 1);

              // Parse data points from the rest of the line
              gchar **points = g_strsplit_set(data_start, " \t", -1);
              for (int j = 0; points[j] != NULL; j++) {
                gchar *point = g_strstrip(points[j]);
                if (strlen(point) == 0) continue;

                // Parse x,y pair
                gchar **coords = g_strsplit(point, ",", 2);
                if (coords[0] && coords[1]) {
                  double x = g_strtod(g_strstrip(coords[0]), NULL);
                  double y = g_strtod(g_strstrip(coords[1]), NULL);
                  g_array_append_val(current_line->x_values, x);
                  g_array_append_val(current_line->y_values, y);
                }
                g_strfreev(coords);
              }
              g_strfreev(points);

            } else {
              // Legacy format: simple x,y pairs or single values
              if (plot_lines->len == 0) {
                // Create default line if none exists
                PlotLine new_line;
                new_line.label = g_strdup("Data");
                new_line.x_values = g_array_new(FALSE, FALSE, sizeof(double));
                new_line.y_values = g_array_new(FALSE, FALSE, sizeof(double));
                g_array_append_val(plot_lines, new_line);
                current_line = &g_array_index(plot_lines, PlotLine, 0);
              }

              // Parse as comma or space-separated values
              gchar **parts = g_strsplit_set(line, ", \t", -1);
              int value_count = 0;
              double values[2] = {0.0, 0.0};

              for (int j = 0; parts[j] != NULL && value_count < 2; j++) {
                gchar *trimmed = g_strstrip(parts[j]);
                if (strlen(trimmed) > 0) {
                  char *endptr;
                  double val = g_strtod(trimmed, &endptr);
                  if (*endptr == '\0' || g_ascii_isspace(*endptr)) {
                    values[value_count++] = val;
                  }
                }
              }
              g_strfreev(parts);

              if (value_count >= 2) {
                g_array_append_val(current_line->x_values, values[0]);
                g_array_append_val(current_line->y_values, values[1]);
              } else if (value_count == 1) {
                // If only one value, use index as x
                double x_val = (double)current_line->x_values->len;
                g_array_append_val(current_line->x_values, x_val);
                g_array_append_val(current_line->y_values, values[0]);
              }
            }
          }
          g_strfreev(lines);

          if (plot_lines->len > 0) {
            // Find global min/max for scaling across all lines
            double min_x = G_MAXDOUBLE;
            double max_x = -G_MAXDOUBLE;
            double min_y = G_MAXDOUBLE;
            double max_y = -G_MAXDOUBLE;

            for (guint line_idx = 0; line_idx < plot_lines->len; line_idx++) {
              PlotLine *pline = &g_array_index(plot_lines, PlotLine, line_idx);
              for (guint i = 0; i < pline->x_values->len; i++) {
                double x = g_array_index(pline->x_values, double, i);
                double y = g_array_index(pline->y_values, double, i);
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
              }
            }

            // Force axes to start from 0 (don't auto-scale away from zero)
            if (min_x > 0) min_x = 0;
            if (min_y > 0) min_y = 0;

            // Add 10% padding to max values for better visualization
            double x_padding = (max_x - min_x) * 0.1;
            double y_padding = (max_y - min_y) * 0.1;
            max_x += x_padding;
            max_y += y_padding;

            double x_range = max_x - min_x;
            double y_range = max_y - min_y;
            if (x_range < 0.001) x_range = 1.0;
            if (y_range < 0.001) y_range = 1.0;

            double margin_left = 50.0;
            double margin_bottom = 30.0;
            double margin_top = 20.0;
            double margin_right = 20.0;
            double plot_width = element->width - margin_left - margin_right;
            double plot_height = element->height - margin_top - margin_bottom;

            // Draw grid lines and labels
            cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.15);
            cairo_set_line_width(cr, 0.5);

            // Calculate nice grid intervals
            int num_y_ticks = 5;
            int num_x_ticks = 5;
            double y_tick_interval = y_range / num_y_ticks;
            double x_tick_interval = x_range / num_x_ticks;

            // Draw horizontal grid lines and Y labels
            PangoLayout *layout = pango_cairo_create_layout(cr);
            pango_layout_set_font_description(layout, pango_font_description_from_string("Sans 8"));

            for (int i = 0; i <= num_y_ticks; i++) {
              double y_val = min_y + i * y_tick_interval;
              double y_pos = element->y + margin_top + plot_height * (1.0 - (double)i / num_y_ticks);

              // Grid line (skip the one at the bottom which is the x-axis)
              if (i > 0) {
                cairo_move_to(cr, element->x + margin_left, y_pos);
                cairo_line_to(cr, element->x + margin_left + plot_width, y_pos);
                cairo_stroke(cr);
              }

              // Y-axis label
              cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.7);
              gchar *label = g_strdup_printf("%.0f", y_val);
              pango_layout_set_text(layout, label, -1);
              cairo_move_to(cr, element->x + 5, y_pos - 6);
              pango_cairo_show_layout(cr, layout);
              g_free(label);
              cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.15);
            }

            // Draw vertical grid lines and X labels
            for (int i = 0; i <= num_x_ticks; i++) {
              double x_val = min_x + i * x_tick_interval;
              double x_pos = element->x + margin_left + plot_width * (double)i / num_x_ticks;

              // Grid line (skip the one at the left which is the y-axis)
              if (i > 0) {
                cairo_move_to(cr, x_pos, element->y + margin_top);
                cairo_line_to(cr, x_pos, element->y + margin_top + plot_height);
                cairo_stroke(cr);
              }

              // X-axis label
              cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.7);
              gchar *label = g_strdup_printf("%.0f", x_val);
              pango_layout_set_text(layout, label, -1);
              int text_width, text_height;
              pango_layout_get_pixel_size(layout, &text_width, &text_height);
              cairo_move_to(cr, x_pos - text_width / 2, element->y + element->height - margin_bottom + 5);
              pango_cairo_show_layout(cr, layout);
              g_free(label);
              cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.15);
            }

            g_object_unref(layout);

            // Draw main axes (darker)
            cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.5);
            cairo_set_line_width(cr, 1.5);

            // Y-axis
            cairo_move_to(cr, element->x + margin_left, element->y + margin_top);
            cairo_line_to(cr, element->x + margin_left, element->y + element->height - margin_bottom);
            // X-axis
            cairo_line_to(cr, element->x + element->width - margin_right, element->y + element->height - margin_bottom);
            cairo_stroke(cr);

            // Color palette for multiple lines
            typedef struct { double r, g, b; } Color;
            Color colors[] = {
              {0.23, 0.51, 0.96}, // Blue
              {0.94, 0.27, 0.27}, // Red
              {0.13, 0.70, 0.29}, // Green
              {0.60, 0.35, 0.71}, // Purple
              {0.95, 0.61, 0.07}, // Orange
              {0.00, 0.74, 0.83}, // Cyan
              {0.91, 0.12, 0.39}, // Pink
              {0.55, 0.63, 0.10}, // Lime
            };
            int num_colors = sizeof(colors) / sizeof(colors[0]);

            // Draw each plot line with different color
            cairo_set_line_width(cr, shape->stroke_width);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

            for (guint line_idx = 0; line_idx < plot_lines->len; line_idx++) {
              PlotLine *pline = &g_array_index(plot_lines, PlotLine, line_idx);

              // Select color (use shape's color for single line, palette for multiple)
              Color line_color;
              if (plot_lines->len == 1) {
                line_color.r = shape->stroke_r;
                line_color.g = shape->stroke_g;
                line_color.b = shape->stroke_b;
              } else {
                line_color = colors[line_idx % num_colors];
              }

              cairo_set_source_rgba(cr, line_color.r, line_color.g, line_color.b, shape->stroke_a);

              // Draw line
              for (guint i = 0; i < pline->x_values->len; i++) {
                double x = g_array_index(pline->x_values, double, i);
                double y = g_array_index(pline->y_values, double, i);

                double norm_x = (x - min_x) / x_range;
                double norm_y = 1.0 - ((y - min_y) / y_range);

                double screen_x = element->x + margin_left + norm_x * plot_width;
                double screen_y = element->y + margin_top + norm_y * plot_height;

                if (i == 0) {
                  cairo_move_to(cr, screen_x, screen_y);
                } else {
                  cairo_line_to(cr, screen_x, screen_y);
                }
              }
              cairo_stroke(cr);

              // Draw points
              for (guint i = 0; i < pline->x_values->len; i++) {
                double x = g_array_index(pline->x_values, double, i);
                double y = g_array_index(pline->y_values, double, i);

                double norm_x = (x - min_x) / x_range;
                double norm_y = 1.0 - ((y - min_y) / y_range);

                double screen_x = element->x + margin_left + norm_x * plot_width;
                double screen_y = element->y + margin_top + norm_y * plot_height;

                cairo_arc(cr, screen_x, screen_y, shape->stroke_width + 1.0, 0, 2 * M_PI);
                cairo_fill(cr);
              }
            }

            // Draw legend if multiple lines
            if (plot_lines->len > 1) {
              double legend_x = element->x + element->width - margin_right - 120;
              double legend_y = element->y + margin_top + 10;
              double legend_line_height = 18;

              PangoLayout *legend_layout = pango_cairo_create_layout(cr);
              pango_layout_set_font_description(legend_layout, pango_font_description_from_string("Sans 9"));

              for (guint line_idx = 0; line_idx < plot_lines->len; line_idx++) {
                PlotLine *pline = &g_array_index(plot_lines, PlotLine, line_idx);
                Color line_color = colors[line_idx % num_colors];

                double y_pos = legend_y + line_idx * legend_line_height;

                // Draw color box
                cairo_set_source_rgba(cr, line_color.r, line_color.g, line_color.b, shape->stroke_a);
                cairo_rectangle(cr, legend_x, y_pos, 12, 12);
                cairo_fill(cr);

                // Draw label text
                cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a * 0.9);
                pango_layout_set_text(legend_layout, pline->label, -1);
                cairo_move_to(cr, legend_x + 18, y_pos);
                pango_cairo_show_layout(cr, legend_layout);
              }

              g_object_unref(legend_layout);
            }
          }

          // Clean up plot_lines
          for (guint i = 0; i < plot_lines->len; i++) {
            PlotLine *pline = &g_array_index(plot_lines, PlotLine, i);
            g_free(pline->label);
            g_array_free(pline->x_values, TRUE);
            g_array_free(pline->y_values, TRUE);
          }
          g_array_free(plot_lines, TRUE);
        }
      }
      break;
    case SHAPE_OVAL:
      {
        double center_x = element->x + element->width / 2.0;
        double center_y = element->y + element->height / 2.0;
        double radius_x = element->width / 2.0;
        double radius_y = element->height / 2.0;

        cairo_save(cr);
        cairo_translate(cr, center_x, center_y);
        cairo_scale(cr, radius_x, radius_y);
        cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore(cr);

        apply_fill(shape, cr);

        cairo_set_source_rgba(cr, shape->stroke_r, shape->stroke_g, shape->stroke_b, shape->stroke_a);
        cairo_stroke(cr);
      }
      break;
  }

  // Restore cairo state before drawing selection UI
  cairo_restore(cr);

  if (is_selected) {
    // Draw selection outline (with rotation)
    cairo_save(cr);
    if (element->rotation_degrees != 0.0) {
      double center_x = element->x + element->width / 2.0;
      double center_y = element->y + element->height / 2.0;
      cairo_translate(cr, center_x, center_y);
      cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
      cairo_translate(cr, -center_x, -center_y);
    }
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, element->x, element->y, element->width, element->height);
    cairo_stroke(cr);
    cairo_restore(cr);

    // Draw control lines for bezier curves
    if ((shape->shape_type == SHAPE_BEZIER || shape->shape_type == SHAPE_CURVED_ARROW) && shape->has_bezier_points) {
      int p0_cx, p0_cy, p1_cx, p1_cy, p2_cx, p2_cy, p3_cx, p3_cy;
      shape_get_connection_point(element, 0, &p0_cx, &p0_cy);
      shape_get_connection_point(element, 1, &p1_cx, &p1_cy);
      shape_get_connection_point(element, 2, &p2_cx, &p2_cy);
      shape_get_connection_point(element, 3, &p3_cx, &p3_cy);

      // Draw control lines
      cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5);
      cairo_set_line_width(cr, 1);
      double dashes[] = {6.0, 6.0};
      cairo_set_dash(cr, dashes, 2, 0);
      cairo_move_to(cr, p0_cx, p0_cy);
      cairo_line_to(cr, p1_cx, p1_cy);
      cairo_move_to(cr, p2_cx, p2_cy);
      cairo_line_to(cr, p3_cx, p3_cy);
      cairo_stroke(cr);
      cairo_set_dash(cr, NULL, 0, 0);
    }

    // Draw connection points (with rotation applied)
    for (int i = 0; i < 4; i++) {
      int cx, cy;
      shape_get_connection_point(element, i, &cx, &cy);
      cairo_arc(cr, cx, cy, 7, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.2, 0.2, 0.9, 0.6);
      cairo_fill(cr);
      cairo_arc(cr, cx, cy, 7, 0, 2 * G_PI);
      cairo_set_source_rgba(cr, 0.1, 0.1, 0.7, 0.8);
      cairo_set_line_width(cr, 2);
      cairo_stroke(cr);
    }

    // Draw rotation handle (without rotation)
    element_draw_rotation_handle(element, cr);
  }

  // Save state again for text
  cairo_save(cr);
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, element->rotation_degrees * M_PI / 180.0);
    cairo_translate(cr, -center_x, -center_y);
  }

  // Draw text if not editing and text exists (but not for plots - their text is data)
  if (!shape->editing && shape->text && strlen(shape->text) > 0 && shape->shape_type != SHAPE_PLOT && shape->shape_type != SHAPE_TEXT_OUTLINE) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_from_string(shape->font_description);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, shape->text, -1);
    pango_layout_set_width(layout, (element->width - 20) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, element_get_pango_alignment(shape->alignment));

    // Apply strikethrough if enabled
    if (shape->strikethrough) {
      PangoAttrList *attrs = pango_attr_list_new();
      PangoAttribute *strike_attr = pango_attr_strikethrough_new(TRUE);
      pango_attr_list_insert(attrs, strike_attr);
      pango_layout_set_attributes(layout, attrs);
      pango_attr_list_unref(attrs);
    }

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    cairo_set_source_rgba(cr, shape->text_r, shape->text_g, shape->text_b, shape->text_a);

    // Position text based on alignment
    int padding = 10;
    int available_height = element->height - (2 * padding);

    // Calculate horizontal position based on alignment
    int text_x;
    PangoAlignment pango_align = element_get_pango_alignment(shape->alignment);
    if (pango_align == PANGO_ALIGN_LEFT) {
      text_x = element->x + padding;
    } else if (pango_align == PANGO_ALIGN_RIGHT) {
      text_x = element->x + padding;  // Same as left because we use pango_layout_set_width
    } else {
      text_x = element->x + padding;
    }

    // Calculate vertical position based on alignment
    int text_y;
    VerticalAlign valign = element_get_vertical_alignment(shape->alignment);
    if (valign == VALIGN_TOP) {
      text_y = element->y + padding;
    } else if (valign == VALIGN_BOTTOM) {
      text_y = element->y + element->height - padding - text_height;
    } else {
      text_y = element->y + padding + (available_height - text_height) / 2;
    }

    // Ensure text doesn't go outside bounds
    if (text_y < element->y + padding) {
      text_y = element->y + padding;
    }

    if (text_height <= available_height) {
      cairo_move_to(cr, text_x, text_y);
      pango_cairo_show_layout(cr, layout);
    } else {
      pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
      pango_layout_set_height(layout, available_height * PANGO_SCALE);
      cairo_move_to(cr, text_x, element->y + padding);
      pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);
  }

  // Restore cairo state at end
  cairo_restore(cr);
}


static int shape_pick_resize_handle(Element *element, int x, int y) {
  // Apply inverse rotation to mouse coordinates if element is rotated
  double rotated_cx = x;
  double rotated_cy = y;
  if (element->rotation_degrees != 0.0) {
    double center_x = element->x + element->width / 2.0;
    double center_y = element->y + element->height / 2.0;
    double dx = x - center_x;
    double dy = y - center_y;
    double angle_rad = -element->rotation_degrees * M_PI / 180.0;
    rotated_cx = center_x + dx * cos(angle_rad) - dy * sin(angle_rad);
    rotated_cy = center_y + dx * sin(angle_rad) + dy * cos(angle_rad);
  }

  int size = 8;
  struct { int px, py; } handles[4] = {
    {element->x, element->y},
    {element->x + element->width, element->y},
    {element->x + element->width, element->y + element->height},
    {element->x, element->y + element->height}
  };

  // For small elements (< 50px), only show bottom-right handle (index 2)
  gboolean is_small = (element->width < 50 || element->height < 50);

  for (int i = 0; i < 4; i++) {
    if (is_small && i != 2) continue; // Skip all but bottom-right for small elements

    if (abs(rotated_cx - handles[i].px) <= size && abs(rotated_cy - handles[i].py) <= size) {
      return i;
    }
  }
  return -1;
}

static int shape_pick_connection_point(Element *element, int x, int y) {
  // Hide connection points for small elements (< 100px on either dimension)
  if (element->width < 100 || element->height < 100) {
    return -1;
  }

  for (int i = 0; i < 4; i++) {
    int px, py;
    shape_get_connection_point(element, i, &px, &py);
    int dx = x - px, dy = y - py;
    int dist_sq = dx * dx + dy * dy;
    if (dist_sq < 100) {
      return i;
    }
  }
  return -1;
}



static void shape_start_editing(Element *element, GtkWidget *overlay) {
  Shape *shape = (Shape*)element;
  shape->editing = TRUE;

  if (!shape->text_view) {
    // Create scrolled window
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

    // Create text view
    shape->text_view = gtk_text_view_new();

    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), shape->text_view);

    // Set size with some padding for scrollbars
    gtk_widget_set_size_request(scrolled_window, element->width + 20, element->height + 20);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), scrolled_window);
    gtk_widget_set_halign(scrolled_window, GTK_ALIGN_START);
    gtk_widget_set_valign(scrolled_window, GTK_ALIGN_START);

    // Convert canvas coordinates to screen coordinates
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, element->x, element->y, &screen_x, &screen_y);
    gtk_widget_set_margin_start(scrolled_window, screen_x - 10); // Adjust for padding
    gtk_widget_set_margin_top(scrolled_window, screen_y - 10);   // Adjust for padding

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(shape_on_textview_key_press), shape);
    gtk_widget_add_controller(shape->text_view, key_controller);

    // Store the scrolled window reference if needed for later access
    shape->scrolled_window = scrolled_window;
  }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(shape->text_view));
  gtk_text_buffer_set_text(buffer, shape->text, -1);

  gtk_widget_show(shape->scrolled_window ? shape->scrolled_window : shape->text_view);
  gtk_widget_grab_focus(shape->text_view);
}

void shape_finish_editing(Element *element) {
  Shape *shape = (Shape*)element;
  if (!shape->text_view) return;

  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(shape->text_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);

  char *new_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  char* old_text = g_strdup(shape->text);
  g_free(shape->text);
  shape->text = new_text;

  Model* model = shape->base.canvas_data->model;
  ModelElement* model_element = model_get_by_visual(model, element);
  undo_manager_push_text_action(shape->base.canvas_data->undo_manager, model_element, old_text, new_text);
  model_update_text(model, model_element, new_text);

  shape->editing = FALSE;

  // Hide the scrolled window instead of the text view
  if (shape->scrolled_window) {
    gtk_widget_hide(shape->scrolled_window);
  } else {
    gtk_widget_hide(shape->text_view);
  }

  // Queue redraw using the stored canvas data
  if (shape->base.canvas_data && shape->base.canvas_data->drawing_area) {
    canvas_sync_with_model(shape->base.canvas_data);
    gtk_widget_queue_draw(shape->base.canvas_data->drawing_area);
    gtk_widget_grab_focus(shape->base.canvas_data->drawing_area);
  }
}

static void shape_update_position(Element *element, int x, int y, int z) {
  Shape *shape = (Shape*)element;
  element->x = x;
  element->y = y;
  element->z = z;
  if (shape->scrolled_window) {
    int screen_x, screen_y;
    canvas_canvas_to_screen(element->canvas_data, x, y, &screen_x, &screen_y);
    gtk_widget_set_margin_start(shape->scrolled_window, screen_x - 10);
    gtk_widget_set_margin_top(shape->scrolled_window, screen_y - 10);
  }
  if (shape->editing) {
    shape_update_text_view_position(shape);
  }
}

static void shape_update_size(Element *element, int width, int height) {
  Shape *shape = (Shape*)element;
  element->width = width;
  element->height = height;
  if (shape->scrolled_window) {
    gtk_widget_set_size_request(shape->scrolled_window, width + 20, height + 20);
  }
  if (shape->editing) {
    shape_update_text_view_position(shape);
  }
}

void shape_free(Element *element) {
  Shape *shape = (Shape*)element;
  if (shape->text) g_free(shape->text);
  if (shape->font_description) g_free(shape->font_description);
  if (shape->alignment) g_free(shape->alignment);
  if (shape->scrolled_window && GTK_IS_WIDGET(shape->scrolled_window) &&
      gtk_widget_get_parent(shape->scrolled_window)) {
    gtk_widget_unparent(shape->scrolled_window);
  }
  g_free(element);
}

static ElementVTable shape_vtable = {
  .draw = shape_draw,
  .get_connection_point = shape_get_connection_point,
  .pick_resize_handle = shape_pick_resize_handle,
  .pick_connection_point = shape_pick_connection_point,
  .start_editing = shape_start_editing,
  .update_position = shape_update_position,
  .update_size = shape_update_size,
  .free = shape_free,
};

Shape* shape_create(ElementPosition position,
                   ElementSize size,
                   ElementColor color,
                   int stroke_width,
                   ShapeType shape_type,
                   gboolean filled,
                   ElementText text,
                   ElementShape shape_config,
                   const ElementDrawing *drawing_config,
                   CanvasData *data) {
  Shape *shape = g_new0(Shape, 1);

  shape->base.type = ELEMENT_SHAPE;
  shape->base.vtable = &shape_vtable;
  shape->base.x = position.x;
  shape->base.y = position.y;
  shape->base.z = position.z;
  shape->base.width = size.width;
  shape->base.height = size.height;
  shape->base.bg_r = color.r;
  shape->base.bg_g = color.g;
  shape->base.bg_b = color.b;
  shape->base.bg_a = color.a;

  shape->shape_type = shape_config.shape_type;
  shape->stroke_width = shape_config.stroke_width;
  shape->filled = shape_config.filled;
  shape->stroke_style = shape_config.stroke_style;
  shape->fill_style = shape_config.fill_style;
  shape->stroke_r = shape_config.stroke_color.r;
  shape->stroke_g = shape_config.stroke_color.g;
  shape->stroke_b = shape_config.stroke_color.b;
  shape->stroke_a = shape_config.stroke_color.a;
  shape->base.canvas_data = data;

  shape->text = g_strdup(text.text);
  shape->text_r = text.text_color.r;
  shape->text_g = text.text_color.g;
  shape->text_b = text.text_color.b;
  shape->text_a = text.text_color.a;
  shape->font_description = g_strdup(text.font_description);
  shape->strikethrough = text.strikethrough;
  shape->alignment = g_strdup(text.alignment ? text.alignment : "center");
  shape->text_view = NULL;
  shape->scrolled_window = NULL;
  shape->editing = FALSE;
  shape->has_line_points = FALSE;
  shape->line_start_u = 0.0;
  shape->line_start_v = 0.0;
  shape->line_end_u = 1.0;
  shape->line_end_v = 1.0;
  shape->has_bezier_points = FALSE;
  shape->bezier_p0_u = 0.0;
  shape->bezier_p0_v = 0.5;
  shape->bezier_p1_u = 0.33;
  shape->bezier_p1_v = 0.0;
  shape->bezier_p2_u = 0.67;
  shape->bezier_p2_v = 1.0;
  shape->bezier_p3_u = 1.0;
  shape->bezier_p3_v = 0.5;
  shape->dragging_control_point = FALSE;
  shape->dragging_control_point_index = -1;

  // Set has_bezier_points for bezier and curved arrow shapes by default
  if (shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) {
    shape->has_bezier_points = TRUE;

    // Use different default control points for curved arrow
    if (shape_type == SHAPE_CURVED_ARROW) {
      shape->bezier_p0_u = 0.0;
      shape->bezier_p0_v = 1.0;  // bottom-left
      shape->bezier_p1_u = 0.25;
      shape->bezier_p1_v = 0.5;  // control point
      shape->bezier_p2_u = 0.75;
      shape->bezier_p2_v = 0.5;  // control point
      shape->bezier_p3_u = 1.0;
      shape->bezier_p3_v = 0.0;  // top-right
    }
  }

  if (drawing_config && drawing_config->drawing_points && drawing_config->drawing_points->len >= 2) {
    DrawingPoint *points = (DrawingPoint*)drawing_config->drawing_points->data;
    if ((shape_type == SHAPE_BEZIER || shape_type == SHAPE_CURVED_ARROW) && drawing_config->drawing_points->len >= 4) {
      // For bezier curves, we expect 4 points
      shape->bezier_p0_u = points[0].x;
      shape->bezier_p0_v = points[0].y;
      shape->bezier_p1_u = points[1].x;
      shape->bezier_p1_v = points[1].y;
      shape->bezier_p2_u = points[2].x;
      shape->bezier_p2_v = points[2].y;
      shape->bezier_p3_u = points[3].x;
      shape->bezier_p3_v = points[3].y;
      shape->has_bezier_points = TRUE;
    } else if (shape_type == SHAPE_LINE || shape_type == SHAPE_ARROW) {
      // For lines and arrows, we expect 2 points
      shape->line_start_u = points[0].x;
      shape->line_start_v = points[0].y;
      shape->line_end_u = points[1].x;
      shape->line_end_v = points[1].y;
      shape->has_line_points = TRUE;
    }
  }

  return shape;
}
