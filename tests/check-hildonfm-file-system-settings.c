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

#include "hildon-file-system-settings.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

/* --------------------- Fixtures --------------------- */

static HildonFileSystemSettings *fss = NULL;

static void
fx_setup_hildonfm_file_system_settings ()
{
    fss = _hildon_file_system_settings_get_instance ();
}

static void
fx_teardown_hildonfm_file_system_settings ()
{
    
}

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if getting a new instance of HildonFileSystemSettings works
 */
START_TEST (test_file_system_settings_get_instance)
{
    fail_if (!HILDON_IS_FILE_SYSTEM_SETTINGS (fss));
}
END_TEST

/**
 * Purpose: Check if setting and getting user settings in GKeyFiles works
 */
START_TEST (test_file_system_user_settings)
{
    GError **error = NULL;
    GKeyFile *key_file = hildon_file_system_open_user_settings ();
    gchar *group = "test_group";
    gchar *key = "test_key";
    gchar *value = "test_value";

    g_key_file_set_value (key_file, group, key, value);
    fail_if (!g_key_file_has_group (key_file, group),
             "Adding a test group to the GKeyFile failed");
    fail_if (!g_key_file_has_key (key_file, group, key, error),
             "Adding a test key to the GKeyFile failed");
    fail_if (strcmp (g_key_file_get_value (key_file, group, key, error), value),
             "Adding a test value to the GKeyFile failed");

    hildon_file_system_write_user_settings (key_file);
    g_key_file_free (key_file);
    key_file = hildon_file_system_open_user_settings ();

    fail_if (!g_key_file_has_group (key_file, group),
             "Writing and opening user settings failed: group");
    fail_if (!g_key_file_has_key (key_file, group, key, error),
             "Writing and opening user settings failed: key");
    fail_if (strcmp (g_key_file_get_value (key_file, group, key, error), value),
             "Writing and opening user settings failed: value");

    g_key_file_remove_key (key_file, group, key, error);
    g_key_file_remove_group (key_file, group, error);

    hildon_file_system_write_user_settings (key_file);
    g_key_file_free (key_file);
    key_file = hildon_file_system_open_user_settings ();

    fail_if (g_key_file_has_group (key_file, group),
             "Writing and opening user settings failed: removing tests");
    /* This may be used to examine the GKeyFile*/
    /*
    while (i < length_i)
        {
            printf ("%d %s\n", i, groups[i]);
            keys = g_key_file_get_keys (key_file, groups[i], &length_j, error);
            while (j < length_j)
                {
                    value = g_key_file_get_value (key_file, groups[i], keys[j],
                                                  error);
                    printf (" %d %s: '%s'\n", j, keys[j], value);
                    j++;
                }
            i++;
        }
    */
}
END_TEST

/**
 * Purpose: Check if getting the type of a HildonFileSystemSettings works
 */
START_TEST (test_file_system_settings_type)
{
    GType type = hildon_file_system_settings_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_SETTINGS != type,
             "Getting the type of a HildonFileSystemSettings failed");
}
END_TEST

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_hildonfm_file_system_settings ();
    ((fm_test_func) (func)) ();
    fx_teardown_hildonfm_file_system_settings ();
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

    /* Create a test case for general filesystem settings testing */
    g_test_add_data_func ("/HildonfmFileSystemSettings/settings_get_instance",
        (fm_test_func)test_file_system_settings_get_instance, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemSettings/user_settings",
        (fm_test_func)test_file_system_user_settings, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemSettings/settings_type",
        (fm_test_func)test_file_system_settings_type, fm_test_setup);

    return g_test_run ();
}
