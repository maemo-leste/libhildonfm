/*
 * This file is a part of hildon-fm tests
 *
 * Copyright (C) 2008 Nokia Corporation.  All rights reserved.
 *
 * Author: Jukka Kauppinen <jukka.p.kauppinen@nokia.com>
 *
 * Contacts: Richard Sun <richard.sun@nokia.com>
 *           Attila Domokos <attila.domokos@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <hildon/hildon.h>

#include "hildon-file-chooser-dialog.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-model.h"
#include "hildon-file-selection.h"

#define START_TEST(name) static void name (void)
#define END_TEST
#define fail_if(expr, ...) g_assert(!(expr))

/* --------------------- Fixtures --------------------- */

static GtkWidget *fcd_window;
static GtkWidget *fcd;
static GtkFileChooserAction action;

static void
fx_setup_hildonfm_file_chooser_dialog_open ()
{
    HildonFileSystemModel* model;

    fcd_window = hildon_window_new ();

    action = GTK_FILE_CHOOSER_ACTION_SAVE;

    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "ref-widget", fcd_window,
                          "root-dir", g_getenv ("MYDOCSDIR"),
                          NULL);
    fcd = g_object_new (HILDON_TYPE_FILE_CHOOSER_DIALOG,
                        "file-system-model", model,
                        NULL);
}

static void
fx_teardown_hildonfm_file_chooser_dialog ()
{
    gtk_widget_destroy (fcd_window);
    gtk_widget_destroy (fcd);
}

struct _HildonFileChooserDialogPrivate {
    GtkWidget *up_button;
    GtkWidget *path_button;
    GtkWidget *path_label;
    GtkWidget *location_button;

    GtkWidget *action_button;
    GtkWidget *folder_button;
    //GtkWidget *cancel_button;
    HildonFileSelection *filetree;
    HildonFileSystemModel *model;
    GtkSizeGroup *caption_size_group;
    GtkWidget *caption_control_name;
    GtkWidget *caption_control_location;
    GtkWidget *entry_name;
    GtkWidget *eventbox_location;
    GtkWidget *hbox_location, *image_location, *title_location;
    /* horizontal address box containing the up level button
       and the path button */
    GtkWidget *hbox_address;
    GtkWidget *extensions_combo;
    GtkFileChooserAction action;
    GtkWidget *popup;
    GtkWidget *multiple_label, *hbox_items;
    gulong changed_handler;
    gint max_full_path_length;
    gint max_filename_length;
    gboolean popup_protect;
    GtkFileSystemHandle *create_folder_handle;

    /* Popup menu contents */
    GtkWidget *sort_type, *sort_name, *sort_date, *sort_size;

    GtkWidget *mode_list, *mode_thumbnails;
    GSList *filters;
    GtkWidget *filters_separator;
    GSList *filter_menu_items;
    GSList * filter_item_menu_toggle_handlers;

    gchar *stub_name;   /* Set by call to set_current_name */
    gchar *ext_name;
    gboolean autonaming_enabled;
    gboolean edited;
    gboolean should_show_folder_button;
    gboolean should_show_location;
    gboolean show_upnp;
 };

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if setting and getting a safe folder works
 */
START_TEST (test_file_chooser_dialog_safe_folder)
{
    /* gboolean ret;
    gchar *folder, *result, *aux;
    GtkTreeIter iter;

    folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER(fcd));
    folder = (gchar *)realloc (folder, (strlen (folder) + 16) * sizeof (gchar));
    strcat (folder, "/hildonfmtests");

    aux = g_strconcat (folder, "/folder3/file3.txt", NULL);
    ret = hildon_file_system_model_load_uri
        (HILDON_FILE_CHOOSER_DIALOG(fcd)->priv->model, aux, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    hildon_file_chooser_dialog_set_safe_folder_uri
        (HILDON_FILE_CHOOSER_DIALOG(fcd), folder);
    result = hildon_file_chooser_dialog_get_safe_folder
        (HILDON_FILE_CHOOSER_DIALOG(fcd));

    fail_if (result == NULL || strcmp (result, folder) != 0,
             "Setting a safe folder failed at set & get");
    free (aux);
    free (folder);
    free (result); */
}
END_TEST

/**
 * Purpose: Check if setting and getting the show upnp value works
 */
START_TEST (test_file_chooser_dialog_show_upnp)
{
    /* Test 1: Test if setting and gettin the show upnp value works */
    gboolean value1, value2;

    value1 = true;
    hildon_file_chooser_dialog_set_show_upnp (HILDON_FILE_CHOOSER_DIALOG(fcd),
                                              value1);
    value2 = hildon_file_chooser_dialog_get_show_upnp (HILDON_FILE_CHOOSER_DIALOG(fcd));
    fail_if (value1 != value2,
             "Setting the show upnp value failed at set & get");

    /* Test 2: Test if changing the show upnp value works */
    value1 = false;
    hildon_file_chooser_dialog_set_show_upnp (HILDON_FILE_CHOOSER_DIALOG(fcd),
                                              value1);
    value2 = hildon_file_chooser_dialog_get_show_upnp (HILDON_FILE_CHOOSER_DIALOG(fcd));
    fail_if (value1 != value2,
             "Setting the show upnp value failed when changing the value");
}
END_TEST

/**
 * Purpose: Check if getting the type of a HildonFileChooserDialog works
 */
START_TEST (test_file_chooser_dialog_type)
{
    GType type = hildon_file_chooser_dialog_get_type();

    fail_if (HILDON_TYPE_FILE_CHOOSER_DIALOG != type,
             "Getting the type of a HildonFileChooserDialog failed");
}
END_TEST

/**
 * Purpose: Check if creating a file name extensions widget works without
 * using named extensions
 */
START_TEST (test_file_chooser_dialog_add_extensions_combo_nameless)
{
    /* Test 1: Test if creating the GtkWidget works without extension names*/
    GtkWidget *combo;
    GList *list;
    char **ext_names = NULL;
    char **extensions = (char **)malloc (5 * sizeof(char *));
    size_t n = 0;

    extensions[0] = "txt";
    extensions[1] = "rtf";
    extensions[2] = "odf";
    extensions[3] = "doc";
    extensions[4] = NULL;

    list = gtk_container_get_children (GTK_CONTAINER(GTK_BOX
                                                     (GTK_DIALOG(fcd)->vbox)));
    n = g_list_length(list);

    combo = hildon_file_chooser_dialog_add_extensions_combo
        (HILDON_FILE_CHOOSER_DIALOG(fcd), extensions, ext_names);
    fail_if (!GTK_IS_WIDGET(combo),
            "Creating the extensions combo widget failed without names");

    list = gtk_container_get_children (GTK_CONTAINER(GTK_BOX
                                                     (GTK_DIALOG(fcd)->vbox)));
    //printf("%d\n", g_list_length(list));
    fail_if (g_list_length(list) != n + 1, "Adding the created widget failed");

    gtk_widget_destroy (combo);
    free (extensions);
}
END_TEST

/**
 * Purpose: Check if creating a file name extensions widget works using named
 * extensions
 */
START_TEST (test_file_chooser_dialog_add_extensions_combo_named)
{
    /* Test 1: Test if creating the GtkWidget works with extension names*/
    GtkWidget *combo;
    GList *list;
    char **ext_names = (char **)malloc (5 * sizeof (char *));
    char **extensions = (char **)malloc (5 * sizeof (char *));
    size_t n = 0;

    extensions[0] = "txt";
    extensions[1] = "rtf";
    extensions[2] = "odf";
    extensions[3] = "doc";
    extensions[4] = NULL;

    ext_names[0] = "Text";
    ext_names[1] = "Rich text format";
    ext_names[2] = "Open document format";
    ext_names[3] = "Word document";
    ext_names[4] = NULL;

    list = gtk_container_get_children (GTK_CONTAINER(GTK_BOX
                                                     (GTK_DIALOG(fcd)->vbox)));
    n = g_list_length(list);
    //printf("%d\n", n);

    combo = hildon_file_chooser_dialog_add_extensions_combo
        (HILDON_FILE_CHOOSER_DIALOG(fcd), extensions, ext_names);
    fail_if (!GTK_IS_WIDGET(combo),
             "Creating the extensions combo widget failed with names");
    //show_all_test_window (GTK_WIDGET (fcd_window));

    list = gtk_container_get_children (GTK_CONTAINER(GTK_BOX
                                                     (GTK_DIALOG(fcd)->vbox)));
    //printf("%d\n", g_list_length(list));
    fail_if (g_list_length(list) != n + 1, "Adding the created widget failed");
    gtk_widget_destroy (combo);
    free (extensions);
    free (ext_names);
}
END_TEST

/**
 * Purpose: Check if adding extra widgets works
 */
START_TEST (test_file_chooser_dialog_add_extra)
{
    GtkWidget *widget = hildon_date_editor_new();
    GList *list;
    size_t n = 0;

    list = gtk_container_get_children (GTK_CONTAINER(GTK_BOX
                                                     (GTK_DIALOG(fcd)->vbox)));
    n = g_list_length(list);

    hildon_file_chooser_dialog_add_extra (HILDON_FILE_CHOOSER_DIALOG(fcd),
                                          widget);

    list = gtk_container_get_children (GTK_CONTAINER(GTK_BOX
                                                     (GTK_DIALOG(fcd)->vbox)));
    fail_if (g_list_length(list) != n + 1, "Adding the extra widget failed");
    gtk_widget_destroy (widget);
}
END_TEST

/**
 * Purpose: Check if extension setting and getting works
 */
START_TEST (test_file_chooser_dialog_extension)
{
    /* Test 1: Test if setting and getting an extension works*/
    gchar *extension1, *extension2;

    extension1 = "odt";
    hildon_file_chooser_dialog_set_extension (HILDON_FILE_CHOOSER_DIALOG(fcd),
                                              extension1);
    extension2 = hildon_file_chooser_dialog_get_extension
        (HILDON_FILE_CHOOSER_DIALOG(fcd));
    fail_if(strcmp(extension1, extension2) != 0,
            "Extention comparison failed at set & get");
    free (extension2);

    /* Test 2: Test if changing the extension works*/
    extension1 = "txt";
    hildon_file_chooser_dialog_set_extension (HILDON_FILE_CHOOSER_DIALOG(fcd),
                                              extension1);
    extension2 = hildon_file_chooser_dialog_get_extension
        (HILDON_FILE_CHOOSER_DIALOG(fcd));
    fail_if(strcmp(extension1, extension2) != 0,
            "Extention comparison failed when changing extension setting");
    free (extension2);
}
END_TEST

static void
test_file_chooser_dialog_set_folder (void)
{
    /* GtkWidget *dialog;

    dialog = hildon_file_chooser_dialog_new_with_properties (NULL,
        "action", GTK_FILE_CHOOSER_ACTION_SAVE, NULL);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "filename.jpg");
    if (gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                             "/home/kalikiana/MyDocs/.documents"))
        g_error ("Error setting folder");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog); */
    g_print ("FIXME: SKIPPED ");
}

/* static void
button_clicked_cb (GtkButton *button,
                GtkWindow *window)
{
    GtkWidget *dialog;

    dialog = hildon_file_chooser_dialog_new_with_properties (NULL,
        "action", GTK_FILE_CHOOSER_ACTION_SAVE, NULL);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "filename.jpg");
    if (gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                             "/home/kalikiana/MyDocs/.documents"))
        g_error ("Error setting folder");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static gboolean
button_click_idle_cb (gpointer data)
{
    GtkWidget *button = GTK_WIDGET (data);
    gtk_test_widget_click (button, 1, 0);
    return FALSE;
} */

static void
test_file_chooser_dialog_set_folder_idle (void)
{
    /* GtkWidget *window;
    GtkWidget *button;
    HildonProgram *hildon_program;

    hildon_program = hildon_program_get_instance ();
    window = hildon_window_new ();
    hildon_program_add_window (hildon_program, HILDON_WINDOW (window));
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    button = gtk_button_new_with_label ("Save As");
    g_signal_connect (button, "clicked", G_CALLBACK (button_clicked_cb), window);
    gtk_container_add (GTK_CONTAINER (window), button);
    gtk_widget_show_all (window);
    g_idle_add (button_click_idle_cb, button);

    gtk_main (); */
    g_print ("FIXME: SKIPPED ");
}

/* static void
window_map_event_cb (GtkWidget *window) { g_print ("%s\n", G_STRFUNC); }

static void
window_realize_cb (GtkWidget *window) { g_print ("%s\n", G_STRFUNC); }

static void
window_screen_changed_cb (GtkWidget *window) { g_print ("%s\n", G_STRFUNC); }

static void
window_show_cb (GtkWidget *window) { g_print ("%s\n", G_STRFUNC); }

static void
window_set_focus_cb (GtkWidget *window) { g_print ("%s\n", G_STRFUNC); } */

static void
test_file_chooser_dialog_set_folder_show (void)
{
    /* GtkWidget *window;
    GtkWidget *button;
    HildonProgram *hildon_program;

    hildon_program = hildon_program_get_instance ();
    window = hildon_window_new ();
    hildon_program_add_window (hildon_program, HILDON_WINDOW (window));
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    button = gtk_button_new_with_label ("Save As");
    g_signal_connect (button, "clicked", G_CALLBACK (button_clicked), window);
    gtk_container_add (GTK_CONTAINER (window), button);
    g_signal_connect (window, "map-event",
        G_CALLBACK (window_map_event_cb), button);
    g_signal_connect (window, "realize",
        G_CALLBACK (window_realize_cb), button);
    g_signal_connect (window, "screen-changed",
        G_CALLBACK (window_screen_changed_cb), button);
    g_signal_connect (window, "show",
        G_CALLBACK (window_show_cb), button);
    g_signal_connect (window, "set-focus",
        G_CALLBACK (window_set_focus_cb), button);
    gtk_widget_show_all (window);

    gtk_main (); */
    g_print ("FIXME: SKIPPED ");
}

/* static void
window_create_idle_cb (gpointer data)
{
    GtkWidget *window;
    GtkWidget *button;
    HildonProgram *hildon_program;

    hildon_program = hildon_program_get_instance ();
    window = hildon_window_new ();
    hildon_program_add_window (hildon_program, HILDON_WINDOW (window));
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    button = gtk_button_new_with_label ("Save As");
    g_signal_connect (button, "clicked", G_CALLBACK (button_clicked_cb), window);
    gtk_container_add (GTK_CONTAINER (window), button);
    g_signal_connect (window, "map-event", G_CALLBACK (window_map_event_cb), button);
    g_signal_connect (window, "realize", G_CALLBACK (window_realize_cb), button);
    g_signal_connect (window, "screen-changed", G_CALLBACK (window_screen_changed_cb), button);
    g_signal_connect (window, "show", G_CALLBACK (window_show_cb), button);
    g_signal_connect (window, "set-focus", G_CALLBACK (window_set_focus_cb), button);
    gtk_widget_show_all (window);
} */

static void
test_file_chooser_dialog_set_folder_deferred (void)
{
    /* g_idle_add (window_create_idle_cb, NULL);

    gtk_main (); */
    g_print ("FIXME: SKIPPED ");
}

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_hildonfm_file_chooser_dialog_open ();
    ((fm_test_func) (func)) ();
    fx_teardown_hildonfm_file_chooser_dialog ();
}

int
main (int    argc,
      char** argv)
{
#if !GLIB_CHECK_VERSION(2,32,0)
#ifdef	G_THREADS_ENABLED
    if (!g_thread_supported ())
        g_thread_init (NULL);
#endif
#endif

    gtk_test_init (&argc, &argv, NULL);
    hildon_init ();

    /* Create a test case for file chooser dialog safe folder*/
    g_test_add_data_func ("/HildonfmFileChooserDialog/safe_folder",
        (fm_test_func)test_file_chooser_dialog_safe_folder, fm_test_setup);
    /* Create a test case for file chooser dialog boolean values*/
    g_test_add_data_func ("/HildonfmFileChooserDialog/show_upnp",
        (fm_test_func)test_file_chooser_dialog_show_upnp, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/type",
        (fm_test_func)test_file_chooser_dialog_type, fm_test_setup);
    /* Create a test case for file chooser dialog extensions*/
    g_test_add_data_func ("/HildonfmFileChooserDialog/add_extensions_combo_nameless",
        (fm_test_func)test_file_chooser_dialog_add_extensions_combo_nameless, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/add_extensions_combo_named",
        (fm_test_func)test_file_chooser_dialog_add_extensions_combo_named, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/add_extra",
        (fm_test_func)test_file_chooser_dialog_add_extra, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/extension",
        (fm_test_func)test_file_chooser_dialog_extension, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/set_folder",
        (fm_test_func)test_file_chooser_dialog_set_folder, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/set_folder_idle",
        (fm_test_func)test_file_chooser_dialog_set_folder_idle, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/set_folder_show",
        (fm_test_func)test_file_chooser_dialog_set_folder_show, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileChooserDialog/set_folder_deferred",
        (fm_test_func)test_file_chooser_dialog_set_folder_deferred, fm_test_setup);

    return g_test_run ();
}
