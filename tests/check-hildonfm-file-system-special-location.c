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

#include "hildon-file-system-special-location.h"
#include "hildon-file-system-model.h"
#include "hildon-file-system-private.h"
#include "hildon-file-selection.h"
#include "hildon-file-common-private.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

/* --------------------- Fixtures --------------------- */

static HildonFileSystemModel *model = NULL;
static HildonFileSelection *fs = NULL;
static HildonFileSystemSpecialLocation *location = NULL;
/*This can be replaced with any other special location in MyDocs*/
static gchar *end = "images";

static gchar* get_current_folder_path(HildonFileSelection *_fs)
{
  gchar *rv;
  GFile *file = _hildon_file_selection_get_current_folder_path (_fs);

  rv = g_file_get_uri (file);
  g_object_unref (file);

  return rv;
}

static void
fx_setup_hildonfm_file_system_special_location ()
{
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv("MYDOCSDIR"),
                          NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL(model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");

    char *start = get_current_folder_path (fs);
    char *folder = NULL;
    GFile *file = NULL;

    folder = g_strconcat (start, "/.", end, NULL);

    file = g_file_new_for_uri (folder);
    fail_if (!file,
             "Getting a file path failed");

    location = _hildon_file_system_get_special_location (file);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting a special location failed");

    free (start);
    free (folder);
    g_object_unref (file);
}

static void
fx_setup_hildonfm_file_system_special_location_mydocs ()
{
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv("MYDOCSDIR"),
                          NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL(model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");

    char *start = get_current_folder_path (fs);
    GFile *file = NULL;

    file = g_file_new_for_uri (start);
    location = _hildon_file_system_get_special_location (file);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting a special location failed");

    free (start);
    g_object_unref (file);
}

static void
fx_teardown_hildonfm_file_system_special_location ()
{
    
}

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if getting a display name for a special location works
 */
START_TEST (test_file_system_special_location_get_display_name)
{
    gchar *result = hildon_file_system_special_location_get_display_name
	(location);
    gchar *expected = NULL;

    expected = g_strconcat ("sfil_li_folder_", end, NULL);

    fail_if (strcmp (result, expected),
             "Getting the display name of a special location failed");
}
END_TEST

/**
 * Purpose: Check if setting a display name for a special location works
 */
START_TEST (test_file_system_special_location_set_display_name)
{
    gchar *name = "Test";
    gchar *original = hildon_file_system_special_location_get_display_name
	(location);

    hildon_file_system_special_location_set_display_name (location, name);
    gchar *result = hildon_file_system_special_location_get_display_name
	(location);

    fail_if (strcmp (name, result),
             "Setting the display name of a special location failed");

    hildon_file_system_special_location_set_display_name (location, original);
    result = hildon_file_system_special_location_get_display_name
	(location);

    fail_if (strcmp (original, result),
             "Resetting the display name of a special location failed");
}
END_TEST

/*
 * Purpose: Check if getting extra info on a special location works
 */
static void test_file_system_special_location_get_extra_info (void)
{
    /* gchar *result = hildon_file_system_special_location_get_extra_info (location);
    g_assert (result); */
}
END_TEST

/**
 * Purpose: Check if creating a child location for a special location works
 */
START_TEST (test_file_system_special_location_child_location)
{
    char *start = get_current_folder_path (fs);
    char *rest = "/hildonfmtests";
    char *folder = g_strconcat (start, rest, NULL);
    HildonFileSystemSpecialLocation *child = NULL;
    GFile *file = NULL;

    file = g_file_new_for_uri (folder);
    child = hildon_file_system_special_location_create_child_location (location,
								       file);
     if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (child))
	g_error ("Creating a new special location failed");
     if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (
	_hildon_file_system_get_special_location (file)))
	g_error ("Getting the newly created special location failed");

    g_object_unref (file);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if getting the type of a HildonFileSystemSpecialLocation works
 */
START_TEST (test_file_system_special_location_type)
{
    GType type = hildon_file_system_special_location_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_SPECIAL_LOCATION != type,
             "Getting the type of a HildonFileSystemSpecialLocation failed");
}
END_TEST

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_hildonfm_file_system_special_location ();
    ((fm_test_func) (func)) ();
    fx_teardown_hildonfm_file_system_special_location ();
}

static void
fm_test_setup2 (gconstpointer func)
{
    fx_setup_hildonfm_file_system_special_location_mydocs ();
    ((fm_test_func) (func)) ();
    fx_teardown_hildonfm_file_system_special_location ();
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

    /* Create a test case for filesystem settings information testing */
    g_test_add_data_func ("/HildonfmFileSystemSpecialLocation/get_display_name",
        (fm_test_func)test_file_system_special_location_get_display_name, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemSpecialLocation/set_display_name",
        (fm_test_func)test_file_system_special_location_set_display_name, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemSpecialLocation/get_extra_info",
        (fm_test_func)test_file_system_special_location_get_extra_info, fm_test_setup);

    /* Create a test case for filesystem settings setting testing */
    g_test_add_data_func ("/HildonfmFileSystemSpecialLocation/child_location",
        (fm_test_func)test_file_system_special_location_child_location, fm_test_setup2);

    /* Create a test case for filesystem settings setting testing */
    g_test_add_data_func ("/HildonfmFileSystemSpecialLocation/type",
        (fm_test_func)test_file_system_special_location_type, fm_test_setup2);

    return g_test_run ();
}
