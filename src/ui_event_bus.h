#ifndef UI_EVENT_BUS_H
#define UI_EVENT_BUS_H

#include <gtk/gtk.h>

typedef struct _CanvasData CanvasData;

typedef enum {
  UI_EVENT_POINTER_PRIMARY_PRESS,
  UI_EVENT_POINTER_PRIMARY_RELEASE,
  UI_EVENT_POINTER_SECONDARY_PRESS,
  UI_EVENT_POINTER_SECONDARY_RELEASE,
  UI_EVENT_POINTER_MOTION,
  UI_EVENT_POINTER_LEAVE,
  UI_EVENT_SCROLL,
  UI_EVENT_KEY_PRESS,
  UI_EVENT_DRAG_BEGIN,
  UI_EVENT_DRAG_UPDATE,
  UI_EVENT_DRAG_END,
  UI_EVENT_TYPE_COUNT
} UIEventType;

typedef struct {
  double x;
  double y;
  int n_press;
  GdkModifierType modifiers;
} UIPointerEventData;

typedef struct {
  guint keyval;
  guint keycode;
  GdkModifierType modifiers;
} UIKeyEventData;

typedef struct {
  double dx;
  double dy;
  GdkModifierType modifiers;
} UIScrollEventData;

typedef struct {
  double start_x;
  double start_y;
  double offset_x;
  double offset_y;
  GdkModifierType modifiers;
} UIDragEventData;

typedef struct {
  UIEventType type;
  CanvasData *canvas;
  GdkEvent *gdk_event;
  union {
    UIPointerEventData pointer;
    UIKeyEventData key;
    UIScrollEventData scroll;
    UIDragEventData drag;
  } data;
} UIEvent;

typedef gboolean (*UIEventCallback)(const UIEvent *event, gpointer user_data);

void ui_event_bus_init(void);
void ui_event_bus_shutdown(void);
guint ui_event_bus_subscribe(UIEventType type,
                             UIEventCallback callback,
                             gpointer user_data,
                             GDestroyNotify destroy_notify);
void ui_event_bus_unsubscribe(guint subscription_id);
gboolean ui_event_bus_emit(const UIEvent *event);

#endif
