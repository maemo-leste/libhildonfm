/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2009 Christian Dywan <christian@lanedo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include <string.h>
#include <hildon/hildon.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glib/gstdio.h>

#include "hildon-file-system-model.h"
#include "hildon-file-selection.h"
#include "hildon-file-common-private.h"

static void
recurse_folder (const gchar *parent,
                guint        depth)
{
    guint i;

    if (!depth)
        return;
    g_mkdir_with_parents (parent, 0700);

    for (i = 0; i < 5; i++)
    {
        gchar  *folder_name;
        gchar  *folder;
        gchar  *file_name;
        gchar  *file;
        GError *error;

        folder_name = g_strdup_printf ("folder%d", i);
        folder = g_build_path (G_DIR_SEPARATOR_S, parent, folder_name, NULL);
        g_free (folder_name);
        recurse_folder (folder, depth - 1);
        g_free (folder);

        file_name = g_strdup_printf ("file%d", i);
        file = g_build_filename (parent, file_name, NULL);
        g_free (file_name);
        error = NULL;
        g_file_set_contents (file, ".", -1, &error);
        if (error)
            g_error ("Creating test files failed: %s", error->message);
        g_free (file);
    }
}

const gchar *folders[] = {
    "/hildonfmpty",
    "/hildonfmperf/folder2",
    "/hildonfmpty",
    "/hildonfmperf/folder2/folder0",
    "/hildonfmpty",
    "/hildonfmperf/folder2/folder0/folder0",
    "/hildonfmpty",
    "/hildonfmperf/folder2/folder0/folder0/folder0",
    "/hildonfmpty",
    "/hildonfmperf/folder1",
    "/hildonfmpty",
    "/hildonfmperf/folder2/folder1",
    "/hildonfmpty",
    "/hildonfmperf/folder0/folder1/folder2",
    "/hildonfmpty",
    "/hildonfmperf/folder1/folder1/folder2/folder0",
    "/hildonfmpty",
    "/hildonfmperf/folder0/folder1/folder0/folder0",
};

static void
performance_file_system_model (void)
{
    HildonFileSystemModel *model;
    HildonFileSelection *selection;
    gdouble elapsed;
    gchar *start;
    gchar *folder;
    gchar *uri;
    gboolean success;
    guint i;

    g_print ("\n...");

    g_print ("Creating test folders...\n");
    folder = g_build_path (G_DIR_SEPARATOR_S, g_getenv ("MYDOCSDIR"), "hildonfmpty", NULL);
    g_rmdir (folder);
    g_mkdir_with_parents (folder, 0700);
    g_free (folder);
    folder = g_build_path (G_DIR_SEPARATOR_S, g_getenv ("MYDOCSDIR"), "hildonfmperf", NULL);
    recurse_folder (folder, 5);
    g_free (folder);

    g_test_timer_start ();
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv ("MYDOCSDIR"), NULL);
    elapsed = g_test_timer_elapsed ();
    g_print ("%f seconds to create a file system model\n", elapsed);
    g_test_timer_start ();
    selection = g_object_new (HILDON_TYPE_FILE_SELECTION, "model", model, NULL);
    g_object_unref (model);
    elapsed = g_test_timer_elapsed ();
    g_print ("%f seconds to create a file selection\n", elapsed);

    uri = hildon_file_selection_get_current_folder_uri (selection);
    g_assert_cmpstr (uri, ==, NULL);
    start = (gchar *)_hildon_file_selection_get_current_folder_path (selection);

    for (i = 0; i < G_N_ELEMENTS (folders); i++)
    {
        folder = g_strconcat (start, folders[i], NULL);
        g_test_timer_start ();
        success = hildon_file_selection_set_current_folder_uri (selection, folder, NULL);
        elapsed = g_test_timer_elapsed ();
        g_free (folder);
        if (g_strcmp0 (folders[i], "/hildonfmpty"))
            g_print (": %f seconds to switch to %s\n", elapsed, folders[i]);
    }

    g_free (start);

    g_print ("...");
    gtk_widget_destroy (GTK_WIDGET (selection));
}

GtkWidget *
_hildon_file_selection_get_scroll_thumb (HildonFileSelection *self);

static void
performance_file_selection (void)
{
  GtkTreeModel *model;
  GtkWidget *file_selection;
  GtkWidget *window;
  gchar *folder;
  GtkWidget *pannable;
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  guint i;
  gdouble elapsed;

  g_test_timer_start ();
  model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                        "root-dir", g_getenv ("MYDOCSDIR"), NULL);
  file_selection = g_object_new (HILDON_TYPE_FILE_SELECTION, "model", model, NULL);
  hildon_file_selection_show_content_pane (HILDON_FILE_SELECTION (file_selection));
  hildon_file_selection_hide_navigation_pane (HILDON_FILE_SELECTION (file_selection));
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), file_selection);
  gtk_widget_show_all (window);
  g_object_unref (model);
  elapsed = g_test_timer_elapsed ();

  folder = g_build_path (G_DIR_SEPARATOR_S, g_getenv ("MYDOCSDIR"), NULL);
  hildon_file_selection_set_current_folder_uri (HILDON_FILE_SELECTION (file_selection),
                                                folder, NULL);
  g_free (folder);
  pannable = _hildon_file_selection_get_scroll_thumb (HILDON_FILE_SELECTION (file_selection));
  gtk_widget_show (pannable);
  hadj = hildon_pannable_area_get_hadjustment (HILDON_PANNABLE_AREA (pannable));
  vadj = hildon_pannable_area_get_vadjustment (HILDON_PANNABLE_AREA (pannable));
  g_print ("\nlower: %f/%f, upper: %f/%f\n", hadj->lower, vadj->lower, hadj->upper, vadj->upper);
  elapsed = 0;
  for (i = 0; i < 1000; i++)
    {
      g_test_timer_start ();
      hildon_pannable_area_scroll_to (HILDON_PANNABLE_AREA (pannable), hadj->lower, vadj->lower);
      hildon_pannable_area_scroll_to (HILDON_PANNABLE_AREA (pannable), hadj->upper - 1, vadj->upper - 1);
      elapsed += g_test_timer_elapsed ();
    }
  g_print ("scrolling: %f (%f)\n", elapsed / i, elapsed);
  gtk_widget_destroy (window);
}

int
main (int    argc,
      char** argv)
{
    if (!g_thread_supported ())
        g_thread_init (NULL);
    g_assert (gnome_vfs_init ());
    gtk_test_init (&argc, &argv, NULL);

    g_test_add_func ("/performance/file-system-model",
                     performance_file_system_model);
    g_test_add_func ("/performance/file-selection",
                     performance_file_selection);

    return g_test_run ();
}
