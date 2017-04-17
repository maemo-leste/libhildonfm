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
#include <string.h>
#include <stdbool.h>

#include <gtk/gtk.h>
#include <hildon/hildon.h>

#include "hildon-file-details-dialog.h"
#include "hildon-file-system-model.h"
#include "hildon-file-selection.h"
#include "hildon-file-common-private.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

// Enum for gtk file system memory columns
enum
{
  GTK_FILE_SYSTEM_MEMORY_COLUMN_ICON = 0,
  GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME,
  GTK_FILE_SYSTEM_MEMORY_COLUMN_MIME,
  GTK_FILE_SYSTEM_MEMORY_COLUMN_MOD_TIME,
  GTK_FILE_SYSTEM_MEMORY_COLUMN_SIZE,
  GTK_FILE_SYSTEM_MEMORY_COLUMN_IS_HIDDEN,
  GTK_FILE_SYSTEM_MEMORY_COLUMN_IS_FOLDER
};

/* -------------------- Fixtures -------------------- */

static GtkWindow *fdd_window = NULL;
static HildonFileSystemModel *model = NULL;
static HildonFileSelection *fs = NULL;
static GtkWidget *fdd = NULL;

static void
fx_setup_default_hildonfm_file_details_dialog ()
{
    fdd_window = GTK_WINDOW (hildon_window_new ());
    fail_if (!HILDON_IS_WINDOW (fdd_window),
             "Window creation failed.");

    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv("MYDOCSDIR"),
                          NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL(model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");

    fdd = hildon_file_details_dialog_new_with_model(fdd_window, model);
    fail_if (!HILDON_IS_FILE_DETAILS_DIALOG(fdd),
             "File details dialog creation failed");

    //show_all_test_window (GTK_WIDGET (fdd_window));
}

static void
fx_teardown_default_hildonfm_file_details_dialog ()
{
    gtk_widget_destroy (GTK_WIDGET (fdd_window));
    gtk_widget_destroy (GTK_WIDGET (fdd));
}

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if setting & getting file iterators works
 */
START_TEST (test_file_details_dialog_file_iter)
{
    gboolean ret;
    GtkTreeIter iter;
    GtkTreeIter iter2;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    gchar *str = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret,
             "Getting a tree iterator from hildon file system model failed");
    /*
    ret = hildon_uri_to_gtk_tree_iter (iter, folder, strlen(start),
                                       GTK_TREE_MODEL(model));
    fail_if (!ret, "Getting a tree iterator from a file path failed");
    */
    hildon_file_details_dialog_set_file_iter (HILDON_FILE_DETAILS_DIALOG(fdd),
                                              &iter);
    ret = hildon_file_details_dialog_get_file_iter (HILDON_FILE_DETAILS_DIALOG(fdd), &iter2);
    fail_if (!ret, "Setting and getting the file iterator failed");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    free (start);
    if (!g_str_has_prefix (str, "file://"))
        {
            start = g_strconcat ("file://", str, NULL);
            free (str);
        }
    else
        {
            start = str;
        }
    //printf("%s\n%s\n", folder, start);
    fail_if (strcmp(folder, start) != 0, "Uri comparison failed");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if setting & getting file iterators for folders works
 */
START_TEST (test_file_details_dialog_file_iter_folder)
{
    gboolean ret;
    GtkTreeIter iter, iter2;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    gchar *str = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret,
             "Getting a tree iterator from hildon file system model failed");
    /*
    ret = hildon_uri_to_gtk_tree_iter (&iter, folder, strlen(start),
                                       GTK_TREE_MODEL(model));
    fail_if (!ret, "Getting a tree iterator from a file path failed");
    */
    hildon_file_details_dialog_set_file_iter (HILDON_FILE_DETAILS_DIALOG(fdd),
                                              &iter);
    ret = hildon_file_details_dialog_get_file_iter 
        (HILDON_FILE_DETAILS_DIALOG(fdd), &iter2);
    fail_if (!ret, "Setting and getting the file iterator failed");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);

    free (start);

    if (!g_str_has_prefix (str, "file://"))
        {
            start = g_strconcat ("file://", str, NULL);
            free (str);
        }
    else
        {
            start = str;
        }
    //printf("%s\n%s\n", folder, start);

    fail_if (strcmp(folder, start) != 0, "Uri comparison failed");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if getting the type of a HildonFileDetailsDialog works
 */
START_TEST (test_file_details_dialog_type)
{
    GType type = hildon_file_details_dialog_get_type();

    fail_if (HILDON_TYPE_FILE_DETAILS_DIALOG != type,
             "Getting the type of a HildonFileDetailsDialog failed");
}
END_TEST

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_default_hildonfm_file_details_dialog ();
    ((fm_test_func) (func)) ();
    fx_teardown_default_hildonfm_file_details_dialog ();
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

    /* Create a test case for file details dialog iterator tests*/
    g_test_add_data_func ("/HildonfmFileDetailsDialog/file_iter",
        (fm_test_func)test_file_details_dialog_file_iter, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileDetailsDialog/file_iter_folder",
        (fm_test_func)test_file_details_dialog_file_iter_folder, fm_test_setup);

    /* Create a test case for file details dialog general tests */
    g_test_add_data_func ("/HildonfmFileDetailsDialog/type",
        (fm_test_func)test_file_details_dialog_type, fm_test_setup);

    return g_test_run ();
}
