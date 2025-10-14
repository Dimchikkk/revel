#include "ai_chat_dialog.h"

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <graphene.h>
#include <pango/pango.h>

#include "ai/ai_cli.h"
#include "ai/ai_runtime.h"
#include "core/ai_dsl_runner.h"
#include "database.h"

#define AI_MAX_ATTEMPTS 3

typedef struct {
  CanvasData *data;
  GtkWidget *dialog;
  GtkListBox *transcript;
  GtkTextBuffer *prompt_buffer;
  GtkWidget *prompt_view;
  GtkWidget *send_button;
  GtkWidget *cancel_button;
  GtkWidget *provider_combo;
  GtkWidget *settings_button;
  GtkPopover *settings_popover;
  GtkEntry *path_entry;
  GtkSpinButton *timeout_spin;
  GtkSpinButton *context_spin;
  GtkSpinButton *history_spin;
  GtkCheckButton *grammar_check;
  GtkWidget *pending_row;
  GtkLabel *pending_label;
  GtkSpinner *pending_spinner;
  GCancellable *cancellable;
  gboolean busy;
  guint current_attempt;
  guint max_attempts;
  gchar *base_prompt;
} AiChatDialogState;

typedef struct {
  AiChatDialogState *state;
  CanvasData *data;
  gchar *prompt;
  gchar *payload;
  gchar *snapshot;
  gboolean truncated;
  guint attempt;
  guint max_attempts;
} AiChatJob;

static void ai_chat_dialog_refresh_settings(AiChatDialogState *state);
static void on_settings_save(GtkButton *button, gpointer user_data);
static void on_settings_show(GtkPopover *popover, gpointer user_data);

static void ai_chat_dialog_state_free(AiChatDialogState *state) {
  if (!state) return;
  g_clear_object(&state->cancellable);
  g_free(state->base_prompt);
  g_free(state);
}

static void ai_chat_job_free(AiChatJob *job) {
  if (!job) return;
  g_free(job->prompt);
  g_free(job->payload);
  g_free(job->snapshot);
  g_free(job);
}

static void transcript_scroll_to_end(GtkWidget *scrolled) {
  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
  gdouble upper = gtk_adjustment_get_upper(vadj);
  gdouble page_size = gtk_adjustment_get_page_size(vadj);
  gtk_adjustment_set_value(vadj, upper - page_size);
}

static void on_copy_button_clicked(GtkButton *button, gpointer user_data) {
  GtkPopover *popover = GTK_POPOVER(user_data);
  const gchar *text = g_object_get_data(G_OBJECT(popover), "copy-text");
  if (text && *text) {
    GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(button));
    if (clipboard) {
      gdk_clipboard_set_text(clipboard, text);
    }
  }
  gtk_popover_popdown(popover);
}

static void on_copy_popup_pressed(GtkGestureClick *gesture,
                                  int n_press,
                                  double x,
                                  double y,
                                  gpointer user_data) {
  if (n_press != 1) {
    return;
  }

  GtkWidget *widget = GTK_WIDGET(user_data);
  const gchar *text = g_object_get_data(G_OBJECT(widget), "copy-text");
  if (!text || !*text) {
    return;
  }

  GtkWidget *popover = gtk_popover_new();
  gtk_popover_set_has_arrow(GTK_POPOVER(popover), TRUE);
  gtk_popover_set_autohide(GTK_POPOVER(popover), TRUE);
  GtkWidget *root_widget = GTK_WIDGET(gtk_widget_get_root(widget));
  gtk_widget_set_parent(popover, root_widget);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *copy_btn = gtk_button_new_with_label("Copy text");
  gtk_widget_set_focusable(copy_btn, FALSE);
  gtk_box_append(GTK_BOX(box), copy_btn);
  gtk_popover_set_child(GTK_POPOVER(popover), box);

  g_object_set_data_full(G_OBJECT(popover), "copy-text", g_strdup(text), g_free);
  g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_button_clicked), popover);

  graphene_point_t source = GRAPHENE_POINT_INIT(x, y);
  graphene_point_t translated = source;
  if (root_widget && widget) {
    if (!gtk_widget_compute_point(widget, root_widget, &source, &translated)) {
      translated = source;
    }
  }
  GdkRectangle rect = { (int)translated.x, (int)translated.y, 1, 1 };
  gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
  gtk_popover_popup(GTK_POPOVER(popover));
}

static void attach_copy_support(GtkWidget *widget, const gchar *text) {
  if (!GTK_IS_WIDGET(widget)) {
    return;
  }
  if (!text) {
    text = "";
  }
  g_object_set_data_full(G_OBJECT(widget), "copy-text", g_strdup(text), g_free);

  GtkGesture *gesture = g_object_get_data(G_OBJECT(widget), "copy-gesture");
  if (!gesture) {
    gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_SECONDARY);
    g_signal_connect(gesture, "pressed", G_CALLBACK(on_copy_popup_pressed), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(gesture));
    g_object_set_data(G_OBJECT(widget), "copy-gesture", gesture);
  }
}

static GtkWidget *transcript_append_message(AiChatDialogState *state,
                                            const char *sender,
                                            const char *body,
                                            gboolean error,
                                            const char *copy_text) {
  if (!state || !state->transcript) {
    return NULL;
  }

  GtkWidget *row = gtk_list_box_row_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
  gtk_widget_set_margin_start(box, 6);
  gtk_widget_set_margin_end(box, 6);
  gtk_widget_set_margin_top(box, 4);
  gtk_widget_set_margin_bottom(box, 4);

  gchar *sender_markup = g_markup_printf_escaped("<b>%s</b>", sender ? sender : "");
  GtkWidget *sender_label = gtk_label_new(NULL);
  gtk_label_set_use_markup(GTK_LABEL(sender_label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(sender_label), 0.0f);
  gtk_label_set_markup(GTK_LABEL(sender_label), sender_markup);
  gtk_box_append(GTK_BOX(box), sender_label);
  g_free(sender_markup);

  GtkWidget *body_label = gtk_label_new(NULL);
  gtk_label_set_wrap(GTK_LABEL(body_label), TRUE);
  gtk_label_set_wrap_mode(GTK_LABEL(body_label), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_xalign(GTK_LABEL(body_label), 0.0f);
  gtk_label_set_selectable(GTK_LABEL(body_label), TRUE);

  gchar *escaped = g_markup_escape_text(body ? body : "", -1);
  if (error) {
    gchar *error_markup = g_strdup_printf("<span foreground=\"orange\">%s</span>", escaped);
    gtk_label_set_use_markup(GTK_LABEL(body_label), TRUE);
    gtk_label_set_markup(GTK_LABEL(body_label), error_markup);
    g_free(error_markup);
  } else {
    gtk_label_set_text(GTK_LABEL(body_label), body ? body : "");
  }
  g_free(escaped);

  gtk_box_append(GTK_BOX(box), body_label);
  attach_copy_support(body_label, copy_text ? copy_text : body);

  gtk_list_box_append(state->transcript, row);
  GtkWidget *scrolled = gtk_widget_get_parent(GTK_WIDGET(state->transcript));
  while (scrolled && !GTK_IS_SCROLLED_WINDOW(scrolled)) {
    scrolled = gtk_widget_get_parent(scrolled);
  }
  if (scrolled && GTK_IS_SCROLLED_WINDOW(scrolled)) {
    transcript_scroll_to_end(scrolled);
  }

  return body_label;
}

static void transcript_set_pending(AiChatDialogState *state, const char *text, gboolean error) {
  if (!state || !state->pending_label) {
    return;
  }
  gtk_label_set_wrap(state->pending_label, TRUE);
  gtk_label_set_wrap_mode(state->pending_label, PANGO_WRAP_WORD_CHAR);
  if (error) {
    gchar *escaped = g_markup_escape_text(text ? text : "", -1);
    gchar *markup = g_strdup_printf("<span foreground=\"orange\">%s</span>", escaped);
    gtk_label_set_use_markup(state->pending_label, TRUE);
    gtk_label_set_markup(state->pending_label, markup);
    g_free(markup);
    g_free(escaped);
  } else {
    gtk_label_set_use_markup(state->pending_label, FALSE);
    gtk_label_set_text(state->pending_label, text ? text : "");
  }
  attach_copy_support(GTK_WIDGET(state->pending_label), text ? text : "");
}

static void transcript_create_pending(AiChatDialogState *state) {
  if (!state || !state->transcript) {
    return;
  }

  GtkWidget *row = gtk_list_box_row_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start(box, 6);
  gtk_widget_set_margin_end(box, 6);
  gtk_widget_set_margin_top(box, 4);
  gtk_widget_set_margin_bottom(box, 4);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

  GtkWidget *spinner = gtk_spinner_new();
  gtk_spinner_start(GTK_SPINNER(spinner));
  gtk_box_append(GTK_BOX(box), spinner);

  GtkWidget *label = gtk_label_new("Waiting for AI response…");
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
  gtk_box_append(GTK_BOX(box), label);

  gtk_list_box_append(state->transcript, row);

  state->pending_row = row;
  state->pending_label = GTK_LABEL(label);
  state->pending_spinner = GTK_SPINNER(spinner);

  GtkWidget *scrolled = gtk_widget_get_parent(GTK_WIDGET(state->transcript));
  while (scrolled && !GTK_IS_SCROLLED_WINDOW(scrolled)) {
    scrolled = gtk_widget_get_parent(scrolled);
  }
  if (scrolled && GTK_IS_SCROLLED_WINDOW(scrolled)) {
    transcript_scroll_to_end(scrolled);
  }
}

static void transcript_clear_pending(AiChatDialogState *state) {
  if (!state || !state->pending_row) {
    return;
  }
  if (state->pending_spinner) {
    gtk_spinner_stop(state->pending_spinner);
  }
  state->pending_row = NULL;
  state->pending_label = NULL;
  state->pending_spinner = NULL;
}

static void set_busy(AiChatDialogState *state, gboolean busy) {
  if (!state) {
    return;
  }
  state->busy = busy;
  gtk_widget_set_sensitive(state->send_button, !busy);
  gtk_widget_set_visible(state->cancel_button, busy);
  gtk_widget_set_sensitive(state->provider_combo, !busy);
  if (state->settings_button) {
    gtk_widget_set_sensitive(state->settings_button, !busy);
  }
}

static void ai_chat_dialog_refresh_settings(AiChatDialogState *state) {
  if (!state || !state->data || !state->data->ai_runtime) {
    return;
  }

  AiRuntime *runtime = state->data->ai_runtime;
  const gchar *provider_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->provider_combo));
  const gchar *override_path = provider_id ? ai_runtime_get_cli_override(runtime, provider_id) : NULL;

  if (state->path_entry) {
    gtk_editable_set_text(GTK_EDITABLE(state->path_entry), override_path ? override_path : "");
  }
  if (state->timeout_spin) {
    gtk_spin_button_set_value(state->timeout_spin, ai_runtime_get_timeout(runtime) / 1000.0);
  }
  if (state->context_spin) {
    gtk_spin_button_set_value(state->context_spin, ai_runtime_get_max_context(runtime));
  }
  if (state->history_spin) {
    gtk_spin_button_set_value(state->history_spin, ai_runtime_get_history_limit(runtime));
  }
  if (state->grammar_check) {
    gtk_check_button_set_active(state->grammar_check, ai_runtime_get_include_grammar(runtime));
  }

}

static gchar *trim_buffer_text(GtkTextBuffer *buffer) {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  gchar *trimmed = g_strstrip(text);
  if (trimmed && *trimmed) {
    return g_strdup(trimmed);
  }
  g_free(text);
  return NULL;
}

static void reset_prompt(AiChatDialogState *state) {
  if (!state || !state->prompt_buffer) {
    return;
  }
  gtk_text_buffer_set_text(state->prompt_buffer, "", -1);
}

static void ai_chat_dialog_prepare_attempt(AiChatDialogState *state);
static void ai_chat_dialog_start_attempt(AiChatDialogState *state);
static gboolean on_prompt_key_pressed(GtkEventControllerKey *controller,
                                      guint keyval,
                                      guint keycode,
                                      GdkModifierType state,
                                      gpointer user_data);

static void ai_chat_dialog_finalize_attempt(AiChatDialogState *state) {
  g_clear_object(&state->cancellable);
  set_busy(state, FALSE);
  transcript_clear_pending(state);
  state->current_attempt = 0;
  g_clear_pointer(&state->base_prompt, g_free);
}

static gboolean schedule_retry_idle(gpointer user_data) {
  AiChatDialogState *state = user_data;
  ai_chat_dialog_start_attempt(state);
  return G_SOURCE_REMOVE;
}

static void ai_chat_task_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  AiChatJob *job = task_data;
  if (!job || !job->data || !job->data->ai_runtime) {
    g_task_return_new_error(task, g_quark_from_static_string("ai"), 1, "AI runtime unavailable");
    return;
  }

  AiRuntime *runtime = job->data->ai_runtime;
  AiProvider *provider = ai_runtime_get_active_provider(runtime);
  if (!provider) {
    g_task_return_new_error(task, g_quark_from_static_string("ai"), 2, "No AI provider selected");
    return;
  }

  gchar *dsl = NULL;
  gchar *cli_error = NULL;
  guint timeout_ms = ai_runtime_get_timeout(runtime);
  gboolean ok = ai_cli_generate_with_timeout(provider, job->payload, timeout_ms, cancellable, &dsl, &cli_error);
  if (ok) {
    g_task_return_pointer(task, dsl, g_free);
  } else {
    if (!cli_error) {
      cli_error = g_strdup("AI provider failed");
    }
    if (cancellable && g_cancellable_is_cancelled(cancellable)) {
      g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "%s", cli_error);
    } else {
      g_task_return_new_error(task, g_quark_from_static_string("ai"), 3, "%s", cli_error);
    }
    g_free(cli_error);
  }
}

static void append_session_entry(CanvasData *data, const char *prompt, const char *dsl, const char *error_text) {
  if (!data || !data->ai_runtime) {
    return;
  }
  AiConversationEntry *entry = ai_conversation_entry_new(prompt, dsl, error_text);
  ai_session_state_append_entry(data->ai_runtime->session, entry);
}

static void ai_chat_task_finish(GObject *source, GAsyncResult *result, gpointer user_data) {
  GTask *task = G_TASK(result);
  AiChatJob *job = g_task_get_task_data(task);
  AiChatDialogState *state = job ? job->state : NULL;
  CanvasData *data = state ? state->data : NULL;

  if (!state) {
    return;
  }

  GError *error = NULL;
  gchar *dsl = g_task_propagate_pointer(task, &error);

  if (error) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
        g_strcmp0(error->message, "Request cancelled") == 0) {
      transcript_set_pending(state, "Request cancelled.", TRUE);
      g_clear_error(&error);
      ai_chat_dialog_finalize_attempt(state);
      return;
    }

    append_session_entry(data, job->prompt, NULL, error->message);
    if (data && data->model) {
      database_insert_action_log(data->model->db, "ai", job->prompt, NULL, error->message);
    }

    if (job->attempt < job->max_attempts) {
      gchar *retry_msg = g_strdup_printf("Attempt %u failed: %s. Retrying…", job->attempt, error->message);
      transcript_set_pending(state, retry_msg, TRUE);
      g_free(retry_msg);
      state->current_attempt = job->attempt + 1;
      g_clear_error(&error);
      g_idle_add(schedule_retry_idle, state);
      return;
    }

    gchar *message = g_strdup_printf("Attempt %u failed: %s", job->attempt, error->message);
    transcript_set_pending(state, message, TRUE);
    g_free(message);

    if (state->pending_spinner) {
      gtk_spinner_stop(state->pending_spinner);
    }
    transcript_append_message(state, "AI", "Request failed. Please adjust your prompt and try again.", FALSE, "Request failed. Please adjust your prompt and try again.");
    g_clear_error(&error);
    ai_chat_dialog_finalize_attempt(state);
    return;
  }

  AiDslRunnerOptions options = {
    .description = AI_DSL_RUNNER_DEFAULT_LABEL,
    .dry_run = FALSE,
  };
  gboolean applied = FALSE;
  gchar *runner_error = ai_dsl_runner_apply(data, dsl, &options, &applied);

  if (runner_error) {
    append_session_entry(data, job->prompt, dsl, runner_error);
    if (data && data->model) {
      database_insert_action_log(data->model->db, "ai", job->prompt, dsl, runner_error);
    }

    if (job->attempt < job->max_attempts) {
      gchar *retry_msg = g_strdup_printf("Attempt %u invalid: %s. Retrying…", job->attempt, runner_error);
      transcript_set_pending(state, retry_msg, TRUE);
      g_free(retry_msg);
      state->current_attempt = job->attempt + 1;
      g_free(runner_error);
      g_free(dsl);
      g_idle_add(schedule_retry_idle, state);
      return;
    }

    gchar *message = g_strdup_printf("Attempt %u invalid: %s", job->attempt, runner_error);
    transcript_set_pending(state, message, TRUE);
    g_free(message);

    if (state->pending_spinner) {
      gtk_spinner_stop(state->pending_spinner);
    }
    transcript_append_message(state, "AI", "AI response could not be applied. See details above.", FALSE, "AI response could not be applied. See details above.");
    g_free(runner_error);
    g_free(dsl);
    ai_chat_dialog_finalize_attempt(state);
    return;
  }

  append_session_entry(data, job->prompt, dsl, NULL);
  if (data && data->model) {
    database_insert_action_log(data->model->db, "ai", job->prompt, dsl, NULL);
  }

  transcript_set_pending(state, dsl, FALSE);
  if (state->pending_spinner) {
    gtk_spinner_stop(state->pending_spinner);
  }

  if (job->truncated) {
    transcript_append_message(state, "System", "Context was truncated to fit provider limits.", FALSE, "Context was truncated to fit provider limits.");
  }

  if (!applied) {
    transcript_append_message(state,
                              "System",
                              "No changes were applied; ensure the DSL includes complete commands with coordinates and required arguments.",
                              TRUE,
                              "No changes were applied; ensure the DSL includes complete commands with coordinates and required arguments.");
  }

  g_free(dsl);
  ai_chat_dialog_finalize_attempt(state);
}

static void ai_chat_dialog_start_attempt(AiChatDialogState *state) {
  if (!state || !state->data || !state->data->ai_runtime) {
    transcript_set_pending(state, "AI runtime unavailable", TRUE);
    ai_chat_dialog_finalize_attempt(state);
    return;
  }

  AiRuntime *runtime = state->data->ai_runtime;
  char *snapshot = NULL;
  gboolean truncated = FALSE;
  GError *error = NULL;
  char *payload = ai_runtime_build_payload(runtime, state->data, state->base_prompt,
                                           &snapshot, &truncated, &error);
  if (!payload) {
    transcript_set_pending(state, error ? error->message : "Failed to build context", TRUE);
    if (error) g_error_free(error);
    ai_chat_dialog_finalize_attempt(state);
    return;
  }

  AiChatJob *job = g_new0(AiChatJob, 1);
  job->state = state;
  job->data = state->data;
  job->prompt = g_strdup(state->base_prompt);
  job->payload = payload;
  job->snapshot = snapshot;
  job->truncated = truncated;
  job->attempt = state->current_attempt;
  job->max_attempts = state->max_attempts;

  g_clear_object(&state->cancellable);
  state->cancellable = g_cancellable_new();

  GTask *task = g_task_new(NULL, state->cancellable, ai_chat_task_finish, job);
  g_task_set_task_data(task, job, (GDestroyNotify)ai_chat_job_free);
  g_task_run_in_thread(task, ai_chat_task_thread);
  g_object_unref(task);
}

static void ai_chat_dialog_prepare_attempt(AiChatDialogState *state) {
  if (!state) {
    return;
  }
  state->current_attempt = 1;
  state->max_attempts = AI_MAX_ATTEMPTS;
  transcript_create_pending(state);
  set_busy(state, TRUE);
  ai_chat_dialog_start_attempt(state);
}

static void on_send_clicked(GtkButton *button, gpointer user_data) {
  AiChatDialogState *state = user_data;
  if (!state || state->busy) {
    return;
  }
  gchar *prompt = trim_buffer_text(state->prompt_buffer);
  if (!prompt) {
    return;
  }

  transcript_append_message(state, "You", prompt, FALSE, prompt);
  g_clear_pointer(&state->base_prompt, g_free);
  state->base_prompt = prompt;
  reset_prompt(state);
  ai_chat_dialog_prepare_attempt(state);
}

static void on_cancel_clicked(GtkButton *button, gpointer user_data) {
  AiChatDialogState *state = user_data;
  if (!state || !state->cancellable) {
    return;
  }
  g_cancellable_cancel(state->cancellable);
  transcript_set_pending(state, "Cancelling…", FALSE);
}

static void on_provider_changed(GtkComboBox *combo, gpointer user_data) {
  AiChatDialogState *state = user_data;
  if (!state || !state->data || !state->data->ai_runtime) {
    return;
  }
  const gchar *id = gtk_combo_box_get_active_id(combo);
  if (!id) {
    return;
  }
  ai_runtime_set_active_provider(state->data->ai_runtime, id);
  ai_chat_dialog_refresh_settings(state);
}

static void on_settings_save(GtkButton *button, gpointer user_data) {
  AiChatDialogState *state = user_data;
  if (!state || !state->data || !state->data->ai_runtime) {
    return;
  }

  AiRuntime *runtime = state->data->ai_runtime;
  const gchar *provider_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->provider_combo));
  if (provider_id) {
    const gchar *path_text = state->path_entry ? gtk_editable_get_text(GTK_EDITABLE(state->path_entry)) : NULL;
    ai_runtime_set_cli_override(runtime, provider_id, (path_text && *path_text) ? path_text : NULL);
  }

  guint timeout_sec = state->timeout_spin ? gtk_spin_button_get_value_as_int(state->timeout_spin) : (ai_runtime_get_timeout(runtime) / 1000);
  ai_runtime_set_timeout(runtime, timeout_sec * 1000);

  guint max_context = state->context_spin ? gtk_spin_button_get_value_as_int(state->context_spin) : ai_runtime_get_max_context(runtime);
  ai_runtime_set_max_context(runtime, max_context);

  guint history_limit = state->history_spin ? gtk_spin_button_get_value_as_int(state->history_spin) : ai_runtime_get_history_limit(runtime);
  ai_runtime_set_history_limit(runtime, history_limit);

  gboolean include_grammar = state->grammar_check ? gtk_check_button_get_active(state->grammar_check) : ai_runtime_get_include_grammar(runtime);
  ai_runtime_set_include_grammar(runtime, include_grammar);


  if (state->data && state->data->model) {
    ai_runtime_save_settings(runtime, state->data->model->db);
  }

  ai_chat_dialog_refresh_settings(state);
  if (state->settings_popover) {
    gtk_popover_popdown(state->settings_popover);
  }

}

static void on_settings_show(GtkPopover *popover, gpointer user_data) {
  AiChatDialogState *state = user_data;
  ai_chat_dialog_refresh_settings(state);
}

static gboolean on_dialog_close(GtkWidget *widget, gpointer user_data) {
  AiChatDialogState *state = user_data;
  if (state && state->cancellable) {
    g_cancellable_cancel(state->cancellable);
  }
  gtk_widget_hide(widget);
  if (state && state->data && state->data->ai_toggle_button) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->data->ai_toggle_button))) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state->data->ai_toggle_button), FALSE);
    }
  }
  return TRUE;
}

static GtkWidget *build_dialog(CanvasData *data) {
  GtkWidget *dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "AI Assistant");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 600);

  AiChatDialogState *state = g_new0(AiChatDialogState, 1);
  state->data = data;

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_start(vbox, 12);
  gtk_widget_set_margin_end(vbox, 12);
  gtk_widget_set_margin_top(vbox, 12);
  gtk_widget_set_margin_bottom(vbox, 12);
  gtk_box_append(GTK_BOX(content), vbox);

  GtkWidget *provider_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *provider_label = gtk_label_new("Provider:");
  gtk_label_set_xalign(GTK_LABEL(provider_label), 0.0f);
  gtk_box_append(GTK_BOX(provider_box), provider_label);

  GtkWidget *provider_combo = gtk_combo_box_text_new();
  if (data && data->ai_runtime && data->ai_runtime->providers) {
    for (guint i = 0; i < data->ai_runtime->providers->len; i++) {
      AiProvider *provider = g_ptr_array_index(data->ai_runtime->providers, i);
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(provider_combo), provider->id, provider->label);
    }
    AiProvider *active = ai_runtime_get_active_provider(data->ai_runtime);
    if (active) {
      gtk_combo_box_set_active_id(GTK_COMBO_BOX(provider_combo), active->id);
    }
  }
  gtk_widget_set_hexpand(provider_combo, TRUE);
  gtk_box_append(GTK_BOX(provider_box), provider_combo);

  GtkWidget *settings_button = gtk_menu_button_new();
  GtkWidget *settings_icon = gtk_image_new_from_icon_name("emblem-system");
  gtk_menu_button_set_child(GTK_MENU_BUTTON(settings_button), settings_icon);
  gtk_box_append(GTK_BOX(provider_box), settings_button);

  GtkWidget *popover = gtk_popover_new();
  GtkWidget *settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top(settings_box, 8);
  gtk_widget_set_margin_bottom(settings_box, 8);
  gtk_widget_set_margin_start(settings_box, 12);
  gtk_widget_set_margin_end(settings_box, 12);

  GtkWidget *path_label = gtk_label_new("CLI override path");
  gtk_label_set_xalign(GTK_LABEL(path_label), 0.0f);
  GtkWidget *path_entry = gtk_entry_new();
  gtk_widget_set_hexpand(path_entry, TRUE);
  gtk_box_append(GTK_BOX(settings_box), path_label);
  gtk_box_append(GTK_BOX(settings_box), path_entry);

  GtkWidget *timeout_label = gtk_label_new("Timeout (seconds)");
  gtk_label_set_xalign(GTK_LABEL(timeout_label), 0.0f);
  GtkWidget *timeout_spin = gtk_spin_button_new_with_range(10, 600, 5);
  gtk_box_append(GTK_BOX(settings_box), timeout_label);
  gtk_box_append(GTK_BOX(settings_box), timeout_spin);

  GtkWidget *context_label = gtk_label_new("Max context bytes");
  gtk_label_set_xalign(GTK_LABEL(context_label), 0.0f);
  GtkWidget *context_spin = gtk_spin_button_new_with_range(1024, 65536, 1024);
  gtk_box_append(GTK_BOX(settings_box), context_label);
  gtk_box_append(GTK_BOX(settings_box), context_spin);

  GtkWidget *history_label = gtk_label_new("History exchanges");
  gtk_label_set_xalign(GTK_LABEL(history_label), 0.0f);
  GtkWidget *history_spin = gtk_spin_button_new_with_range(1, 10, 1);
  gtk_box_append(GTK_BOX(settings_box), history_label);
  gtk_box_append(GTK_BOX(settings_box), history_spin);

  GtkWidget *grammar_check = gtk_check_button_new_with_label("Include DSL grammar snippet");
  gtk_box_append(GTK_BOX(settings_box), grammar_check);


  GtkWidget *save_button = gtk_button_new_with_label("Save settings");
  gtk_widget_add_css_class(save_button, "suggested-action");
  gtk_box_append(GTK_BOX(settings_box), save_button);

  gtk_popover_set_child(GTK_POPOVER(popover), settings_box);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(settings_button), popover);

  gtk_box_append(GTK_BOX(vbox), provider_box);

  GtkWidget *scrolled = gtk_scrolled_window_new();
  gtk_widget_set_hexpand(scrolled, TRUE);
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  GtkWidget *list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
  gtk_widget_set_valign(list, GTK_ALIGN_END);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), list);
  gtk_box_append(GTK_BOX(vbox), scrolled);

  GtkWidget *prompt_frame = gtk_frame_new(NULL);
  gtk_frame_set_label(GTK_FRAME(prompt_frame), "Prompt");
  GtkWidget *prompt_view = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(prompt_view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(prompt_view), 6);
  gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(prompt_view), 6);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(prompt_view), 6);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(prompt_view), 6);
  gtk_frame_set_child(GTK_FRAME(prompt_frame), prompt_view);
  gtk_box_append(GTK_BOX(vbox), prompt_frame);

  GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
  gtk_widget_set_visible(cancel_button, FALSE);
  GtkWidget *send_button = gtk_button_new_with_label("Send");
  gtk_widget_set_hexpand(send_button, FALSE);
  gtk_box_append(GTK_BOX(action_box), cancel_button);
  gtk_box_append(GTK_BOX(action_box), send_button);
  gtk_box_append(GTK_BOX(vbox), action_box);

  state->dialog = dialog;
  state->transcript = GTK_LIST_BOX(list);
  state->prompt_view = prompt_view;
  state->prompt_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(prompt_view));
  state->send_button = send_button;
  state->cancel_button = cancel_button;
  state->provider_combo = provider_combo;
  state->settings_button = settings_button;
  state->settings_popover = GTK_POPOVER(popover);
  state->path_entry = GTK_ENTRY(path_entry);
  state->timeout_spin = GTK_SPIN_BUTTON(timeout_spin);
  state->context_spin = GTK_SPIN_BUTTON(context_spin);
  state->history_spin = GTK_SPIN_BUTTON(history_spin);
  state->grammar_check = GTK_CHECK_BUTTON(grammar_check);
  state->busy = FALSE;

  g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), state);
  g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), state);
  g_signal_connect(provider_combo, "changed", G_CALLBACK(on_provider_changed), state);
  g_signal_connect(save_button, "clicked", G_CALLBACK(on_settings_save), state);
  g_signal_connect(popover, "show", G_CALLBACK(on_settings_show), state);
  g_signal_connect(dialog, "close-request", G_CALLBACK(on_dialog_close), state);
  GtkEventController *key_controller = gtk_event_controller_key_new();
  g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_prompt_key_pressed), state);
  gtk_widget_add_controller(prompt_view, key_controller);

  g_object_set_data_full(G_OBJECT(dialog), "ai-state", state, (GDestroyNotify)ai_chat_dialog_state_free);

  if (data && data->ai_runtime && data->ai_runtime->session) {
    GPtrArray *log = ai_session_state_get_log(data->ai_runtime->session);
    if (log && log->len > 0) {
      guint start_idx = log->len > 10 ? log->len - 10 : 0;
      for (guint i = start_idx; i < log->len; i++) {
        AiConversationEntry *entry = g_ptr_array_index(log, i);
        if (!entry) continue;
        if (entry->prompt) {
          transcript_append_message(state, "You", entry->prompt, FALSE, entry->prompt);
        }
        if (entry->dsl) {
          transcript_append_message(state, "AI", entry->dsl, FALSE, entry->dsl);
        } else if (entry->error) {
          transcript_append_message(state, "AI", entry->error, TRUE, entry->error);
        }
      }
    }
  }

  ai_chat_dialog_refresh_settings(state);

  return dialog;
}

static gboolean scroll_after_realize(gpointer user_data) {
  GtkWidget *scrolled = user_data;
  if (scrolled && GTK_IS_SCROLLED_WINDOW(scrolled)) {
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
    if (vadj) {
      gdouble upper = gtk_adjustment_get_upper(vadj);
      gtk_adjustment_set_value(vadj, upper);
    }
  }
  return G_SOURCE_REMOVE;
}

static void on_dialog_map(GtkWidget *widget, gpointer user_data) {
  GtkWidget *scrolled = user_data;
  g_timeout_add(100, scroll_after_realize, scrolled);
}

void ai_chat_dialog_present(CanvasData *data) {
  if (!data) {
    return;
  }
  if (!data->ai_dialog) {
    data->ai_dialog = build_dialog(data);
    AiChatDialogState *state = g_object_get_data(G_OBJECT(data->ai_dialog), "ai-state");
    if (state && state->transcript) {
      GtkWidget *scrolled = gtk_widget_get_parent(GTK_WIDGET(state->transcript));
      while (scrolled && !GTK_IS_SCROLLED_WINDOW(scrolled)) {
        scrolled = gtk_widget_get_parent(scrolled);
      }
      if (scrolled) {
        g_signal_connect(data->ai_dialog, "map", G_CALLBACK(on_dialog_map), scrolled);
      }
    }
  }
  gtk_window_present(GTK_WINDOW(data->ai_dialog));
}

void ai_chat_dialog_toggle(GtkToggleButton *button, gpointer user_data) {
  CanvasData *data = user_data;
  if (!data) {
    return;
  }
  gboolean active = gtk_toggle_button_get_active(button);
  if (active) {
    ai_chat_dialog_present(data);
  } else if (data->ai_dialog) {
    gtk_widget_hide(data->ai_dialog);
  }
}

void ai_chat_dialog_close(CanvasData *data) {
  if (!data || !data->ai_dialog) {
    return;
  }
  gtk_widget_hide(data->ai_dialog);
}
static gboolean on_prompt_key_pressed(GtkEventControllerKey *controller,
                                      guint keyval,
                                      guint keycode,
                                      GdkModifierType state,
                                      gpointer user_data) {
  AiChatDialogState *dialog_state = user_data;
  if (!dialog_state || dialog_state->busy) {
    return GDK_EVENT_PROPAGATE;
  }

  if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
    on_send_clicked(GTK_BUTTON(dialog_state->send_button), dialog_state);
    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}
