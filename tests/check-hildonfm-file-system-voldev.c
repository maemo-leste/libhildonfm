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
#include <hildon/hildon-window.h>

#include "hildon-file-system-voldev.h"
#include "hildon-file-system-model.h"
#include "hildon-file-selection.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if getting the type of a HildonFileSystemVoldev works
 */
START_TEST (test_file_system_voldev_type)
{
    GType type = hildon_file_system_voldev_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_VOLDEV != type,
             "Getting the type of a HildonFileSystemVoldev failed");
}
END_TEST

/**
 * Purpose: Check if creating a new storage dialog works
 */

START_TEST (test_file_system_voldev_find_volume)
{
    gchar *uri = "file:///";
    GnomeVFSVolume *volume = NULL;
    HildonFileSystemModel *model = NULL;
    HildonFileSelection *fs = NULL;

    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL, NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL(model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");

    volume = find_volume (uri);
    /*
    if (volume == NULL)
        printf ("NULL!!!\n");
    */
    fail_if (!GNOME_IS_VFS_VOLUME (volume),
             "Locating a GnomeVFSVolume with 'file:///' as uri failed");
}
END_TEST

/* ------------------ Suite creation ------------------ */

int
main (int    argc,
      char** argv)
{
    if (!g_thread_supported ())
        g_thread_init (NULL);
    gtk_test_init (&argc, &argv, NULL);

    /* Create a test case for general filesystem voldev testing */
    g_test_add_func ("/HildonfmFileSystemVoldev/voldev_type",
                     test_file_system_voldev_type);
    g_test_add_func ("/HildonfmFileSystemVoldev/voldev_type_find_volume",
                     test_file_system_voldev_find_volume);

    return g_test_run ();
}
