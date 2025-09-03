#include "canvas_actions.h"
#include "canvas_core.h"
#include "canvas_spaces.h"
#include "paper_note.h"
#include "note.h"
#include "space.h"
#include "undo_manager.h"

void canvas_on_add_paper_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    PaperNote *paper_note = paper_note_create(50, 50, 200, 150, "Paper Note", data->next_z_index++, data);
    data->current_space->elements = g_list_append(data->current_space->elements, (Element*)paper_note);

    // Log the action
    undo_manager_push_action(data->undo_manager, ACTION_CREATE_PAPER_NOTE, paper_note, "Create Paper Note");

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_note(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    Note *note = note_create(100, 100, 200, 150, "Note", data->next_z_index++, data);
    data->current_space->elements = g_list_append(data->current_space->elements, (Element*)note);

    // Log the action
    undo_manager_push_action(data->undo_manager, ACTION_CREATE_NOTE, note, "Create Note");

    gtk_widget_queue_draw(data->drawing_area);
}

void canvas_on_add_space(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;

    GtkRoot *root = gtk_widget_get_root(data->drawing_area);
    GtkWindow *window = GTK_WINDOW(root);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Create New Space",
        window,
        GTK_DIALOG_MODAL,
        "Create", GTK_RESPONSE_OK,
        NULL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // Create a grid for better layout
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    // Label
    GtkWidget *label = gtk_label_new("Enter space name:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    // Entry field
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Space name");
    gtk_grid_attach(GTK_GRID(grid), entry, 0, 0, 1, 1);

    gtk_box_append(GTK_BOX(content_area), grid);

    // Store the entry widget in dialog data for easy access
    g_object_set_data(G_OBJECT(dialog), "space_name_entry", entry);

    // Set focus on the entry field
    gtk_widget_grab_focus(entry);

    g_signal_connect(dialog, "response", G_CALLBACK(space_creation_dialog_response), data);

    gtk_window_present(GTK_WINDOW(dialog));
}

void canvas_on_go_back(GtkButton *button, gpointer user_data) {
    CanvasData *data = (CanvasData*)user_data;
    go_back_to_parent_space(data);
}
