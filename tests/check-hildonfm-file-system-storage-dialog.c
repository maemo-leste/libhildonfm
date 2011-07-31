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

#include "hildon-file-system-storage-dialog.h"
#include "hildon-file-system-model.h"
#include "hildon-file-selection.h"
#include "hildon-file-common-private.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_FILE_SYSTEM_STORAGE_DIALOG, HildonFileSystemStorageDialogPriv))

/* --------------------- Fixtures --------------------- */
static HildonFileSystemModel *model = NULL;
static HildonFileSelection *fs = NULL;
static GtkWindow *fssd_window = NULL;
static GtkWidget *fssd = NULL;
static gchar *start = NULL;

static void
fx_setup_hildonfm_file_system_storage_dialog ()
{
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL, NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL(model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");

    start = (gchar *)_hildon_file_selection_get_current_folder_path (fs);

    fssd_window = GTK_WINDOW (hildon_window_new ());
    fail_if (!HILDON_IS_WINDOW (fssd_window),
             "Window creation failed.");

    fssd = hildon_file_system_storage_dialog_new (fssd_window, start);
    fail_if (!HILDON_IS_FILE_SYSTEM_STORAGE_DIALOG (fssd), 
             "File system storage dialog creation failed");

    //show_all_test_window (GTK_WIDGET (fssd_window));
}

static void
fx_teardown_hildonfm_file_system_storage_dialog ()
{

}

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if getting the type of a HildonFileSystemStorageDialog works
 */
START_TEST (test_file_system_storage_dialog_type)
{
    GType type = hildon_file_system_storage_dialog_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_STORAGE_DIALOG != type,
             "Getting the type of a HildonFileSystemStorageDialog failed");
}
END_TEST

/**
 * Purpose: Check if creating a new storage dialog works
 */
START_TEST (test_file_system_storage_dialog_new)
{

}
END_TEST

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_hildonfm_file_system_storage_dialog ();
    ((fm_test_func) (func)) ();
    fx_teardown_hildonfm_file_system_storage_dialog ();
}

int
main (int    argc,
      char** argv)
{
    if (!g_thread_supported ())
        g_thread_init (NULL);
    gtk_test_init (&argc, &argv, NULL);

    /* Create a test case for general filesystem settings testing */
    g_test_add_data_func ("/HildonfmFileSystemStorageDialog/type",
        (fm_test_func)test_file_system_storage_dialog_type, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemStorageDialog/new",
        (fm_test_func)test_file_system_storage_dialog_new, fm_test_setup);

    return g_test_run ();
}
