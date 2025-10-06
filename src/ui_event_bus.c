#include "ui_event_bus.h"
#include <glib.h>

typedef struct {
  guint id;
  UIEventType type;
  UIEventCallback callback;
  gpointer user_data;
  GDestroyNotify destroy_notify;
} UIEventSubscription;

static GPtrArray *subscription_lists[UI_EVENT_TYPE_COUNT];
static GHashTable *subscription_lookup = NULL;
static guint next_subscription_id = 1;
static gboolean bus_initialized = FALSE;

static void ensure_initialized(void) {
  if (bus_initialized) {
    return;
  }

  for (int i = 0; i < UI_EVENT_TYPE_COUNT; i++) {
    subscription_lists[i] = g_ptr_array_new();
  }

  subscription_lookup = g_hash_table_new(g_direct_hash, g_direct_equal);
  bus_initialized = TRUE;
}

void ui_event_bus_init(void) {
  ensure_initialized();
}

void ui_event_bus_shutdown(void) {
  if (!bus_initialized) {
    return;
  }

  for (int i = 0; i < UI_EVENT_TYPE_COUNT; i++) {
    if (subscription_lists[i]) {
      for (guint idx = 0; idx < subscription_lists[i]->len; idx++) {
        UIEventSubscription *sub = g_ptr_array_index(subscription_lists[i], idx);
        if (sub) {
          if (sub->destroy_notify && sub->user_data) {
            sub->destroy_notify(sub->user_data);
          }
          g_free(sub);
        }
      }
      g_ptr_array_free(subscription_lists[i], TRUE);
      subscription_lists[i] = NULL;
    }
  }

  if (subscription_lookup) {
    g_hash_table_destroy(subscription_lookup);
    subscription_lookup = NULL;
  }

  next_subscription_id = 1;
  bus_initialized = FALSE;
}

guint ui_event_bus_subscribe(UIEventType type,
                             UIEventCallback callback,
                             gpointer user_data,
                             GDestroyNotify destroy_notify) {
  g_return_val_if_fail(callback != NULL, 0);
  g_return_val_if_fail(type >= 0 && type < UI_EVENT_TYPE_COUNT, 0);

  ensure_initialized();

  UIEventSubscription *subscription = g_new0(UIEventSubscription, 1);
  subscription->id = next_subscription_id++;
  subscription->type = type;
  subscription->callback = callback;
  subscription->user_data = user_data;
  subscription->destroy_notify = destroy_notify;

  g_ptr_array_add(subscription_lists[type], subscription);
  g_hash_table_insert(subscription_lookup, GINT_TO_POINTER(subscription->id), subscription);

  return subscription->id;
}

void ui_event_bus_unsubscribe(guint subscription_id) {
  if (!bus_initialized || subscription_id == 0) {
    return;
  }

  UIEventSubscription *subscription = g_hash_table_lookup(subscription_lookup, GINT_TO_POINTER(subscription_id));
  if (!subscription) {
    return;
  }

  GPtrArray *list = subscription_lists[subscription->type];
  if (list) {
    for (guint idx = 0; idx < list->len; idx++) {
      UIEventSubscription *candidate = g_ptr_array_index(list, idx);
      if (candidate == subscription) {
        g_ptr_array_remove_index_fast(list, idx);
        break;
      }
    }
  }

  if (subscription->destroy_notify && subscription->user_data) {
    subscription->destroy_notify(subscription->user_data);
  }

  g_hash_table_remove(subscription_lookup, GINT_TO_POINTER(subscription_id));
  g_free(subscription);
}

gboolean ui_event_bus_emit(const UIEvent *event) {
  g_return_val_if_fail(event != NULL, FALSE);
  g_return_val_if_fail(event->type >= 0 && event->type < UI_EVENT_TYPE_COUNT, FALSE);

  if (!bus_initialized) {
    return FALSE;
  }

  GPtrArray *list = subscription_lists[event->type];
  if (!list) {
    return FALSE;
  }

  gboolean handled = FALSE;

  // Iterate over a snapshot to allow handlers to modify subscriptions
  GPtrArray *snapshot = g_ptr_array_new_full(list->len, NULL);
  for (guint idx = 0; idx < list->len; idx++) {
    g_ptr_array_add(snapshot, g_ptr_array_index(list, idx));
  }

  for (guint idx = 0; idx < snapshot->len; idx++) {
    UIEventSubscription *subscription = g_ptr_array_index(snapshot, idx);
    if (!subscription || subscription->callback == NULL) {
      continue;
    }

    if (subscription->callback(event, subscription->user_data)) {
      handled = TRUE;
      break;
    }
  }

  g_ptr_array_free(snapshot, TRUE);
  return handled;
}
