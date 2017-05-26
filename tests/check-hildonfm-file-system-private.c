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
#include <hildon-thumbnail/hildon-thumbnail-factory.h>

#include "hildon-file-system-model.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-info.h"
#include "hildon-file-system-common.h"
#include "hildon-file-selection.h"
#include "hildon-file-common-private.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

/* -------------------- Fixtures -------------------- */

static HildonFileSystemModel *model = NULL;
static HildonFileSelection *fs = NULL;

static gchar* get_current_folder_path(HildonFileSelection *_fs)
{
  gchar *rv;
  GFile *file = _hildon_file_selection_get_current_folder_path (_fs);

  rv = g_file_get_uri (file);
  g_object_unref (file);

  return rv;
}

static void
fx_setup_default_hildonfm_file_system_private ()
{
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv ("MYDOCSDIR"),
                          NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL (model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");
}

static void
fx_teardown_default_hildonfm_file_system_private ()
{
    gtk_widget_destroy (GTK_WIDGET (fs));
    g_object_unref (model);
}

typedef struct {
    GFile *file;
    GFileInfo *info;
    GtkFolder *folder;
    GCancellable *cancellable;
    gint pending_adds;
    GdkPixbuf *icon_cache;
    GdkPixbuf *icon_cache_expanded;
    GdkPixbuf *icon_cache_collapsed;
    GdkPixbuf *thumbnail_cache;
    gchar *name_cache;
    gchar *title_cache;
    gchar *key_cache;
    HildonFileSystemModel *model;
    HildonThumbnailRequest* thumbnail_request;
    time_t load_time;
    guint present_flag : 1;
    guint available : 1; /* Set by code */
    guint accessed : 1;  /* Replaces old gateway_accessed from model */
    guint linking : 1; /* whether it's being linked */
    GError *error;      /* Set if cannot get children */
    gchar *thumb_title, *thumb_author, *thumb_album;
    HildonFileSystemSpecialLocation *location;
    /* HildonFileSelection uses display_text and display_attrs in its
     * cellrenderer. */
    gchar *display_text;
    PangoAttrList *display_attrs;
} HildonFileSystemModelNode;

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if comparing a uri of type: file:///folder1/ works
 */
START_TEST (test_file_system_private_compare_uris)
{
    char *uri1, *uri2, *uri3, *uri4, *uri5, *uri6, *uri7, *uri8;

    uri1 = "file:///folder1/";
    uri2 = "file:///folder1";
    uri3 = "/folder1/";
    uri4 = "/folder1";
    uri5 = "file:///folder2/";
    uri6 = "file:///folder2";
    uri7 = "/folder2/";
    uri8 = "/folder2";

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri1, uri1),
             "Comparing uri with it self failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri1, uri2),
             "Comparing uri with last separator removed failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri1, uri3),
             "Comparing uri with 'file://' removed failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri1, uri4),
             "Comparing uri with last separator and 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri1, uri5),
             "Comparing uri with a fake with everything on it failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri1, uri6),
             "Comparing uri with a fake with last separator removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri1, uri7),
             "Comparing uri with a fake with 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri1, uri8),
             "Comparing uri with a fake with last separator and 'file://' removed failed");
}
END_TEST

/**
 * Purpose: Check if comparing a uri of type: file:///folder1 works
 */
START_TEST (test_file_system_private_compare_uris_last)
{
    char *uri1, *uri2, *uri3, *uri4, *uri5, *uri6, *uri7, *uri8;

    uri1 = "file:///folder1/";
    uri2 = "file:///folder1";
    uri3 = "/folder1/";
    uri4 = "/folder1";
    uri5 = "file:///folder2/";
    uri6 = "file:///folder2";
    uri7 = "/folder2/";
    uri8 = "/folder2";

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri2, uri1),
             "Comparing uri with everything on it failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri2, uri2),
             "Comparing uri with it self failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri2, uri3),
             "Comparing uri with 'file://' removed failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri2, uri4),
             "Comparing uri with last separator and 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri2, uri5),
             "Comparing uri with a fake with everything on it failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri2, uri6),
             "Comparing uri with a fake with last separator removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri2, uri7),
             "Comparing uri with a fake with 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri2, uri8),
             "Comparing uri with a fake with last separator and 'file://' removed failed");
}
END_TEST

/**
 * Purpose: Check if comparing a uri of type: /folder1/ works
 */
START_TEST (test_file_system_private_compare_uris_file)
{
    char *uri1, *uri2, *uri3, *uri4, *uri5, *uri6, *uri7, *uri8;

    uri1 = "file:///folder1/";
    uri2 = "file:///folder1";
    uri3 = "/folder1/";
    uri4 = "/folder1";
    uri5 = "file:///folder2/";
    uri6 = "file:///folder2";
    uri7 = "/folder2/";
    uri8 = "/folder2";

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri3, uri1),
             "Comparing uri with everything on it failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri3, uri2),
             "Comparing uri with last separator removed failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri3, uri3),
             "Comparing uri with it self failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri3, uri4),
             "Comparing uri with last separator and 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri3, uri5),
             "Comparing uri with a fake with everything on it failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri3, uri6),
             "Comparing uri with a fake with last separator removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri3, uri7),
             "Comparing uri with a fake with 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri3, uri8),
             "Comparing uri with a fake with last separator and 'file://' removed failed");
}
END_TEST

/**
 * Purpose: Check if comparing a uri of type: /folder1 works
 */
START_TEST (test_file_system_private_compare_uris_last_and_file)
{
    char *uri1, *uri2, *uri3, *uri4, *uri5, *uri6, *uri7, *uri8;

    uri1 = "file:///folder1/";
    uri2 = "file:///folder1";
    uri3 = "/folder1/";
    uri4 = "/folder1";
    uri5 = "file:///folder2/";
    uri6 = "file:///folder2";
    uri7 = "/folder2/";
    uri8 = "/folder2";

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri4, uri1),
             "Comparing uri with everything on it failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri4, uri2),
             "Comparing uri with last separator removed failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri4, uri3),
             "Comparing uri with 'file://' removed failed");

    fail_if (!_hildon_file_system_compare_ignore_last_separator (uri4, uri4),
             "Comparing uri with it self failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri4, uri5),
             "Comparing uri with a fake with everything on it failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri4, uri6),
             "Comparing uri with a fake with last separator removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri4, uri7),
             "Comparing uri with a fake with 'file://' removed failed");

    fail_if (_hildon_file_system_compare_ignore_last_separator (uri4, uri8),
             "Comparing uri with a fake with last separator and 'file://' removed failed");
}
END_TEST

/**
 * Purpose: Check if getting special locations GNode works
 */
START_TEST (test_file_system_get_locations)
{
    GNode *node = _hildon_file_system_get_locations ();

    fail_if (!node, "Getting special locations GNode failed");
    fail_if (!G_NODE_IS_ROOT(node), "Improper GNode returned");
}
END_TEST

/**
 * Purpose: Check if getting a single HildonFileSystemSpecialLocation works
 */
START_TEST (test_file_system_get_special_location)
{
    GFile *file = _hildon_file_selection_get_current_folder_path (fs);
    HildonFileSystemSpecialLocation *location;

    location = _hildon_file_system_get_special_location (file);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
	     "Getting a HildonFileSystemSpecialLocation failed");
}
END_TEST

/**
 * Purpose: Check if getting the GFile from a
 * HildonFileSystemSpecialLocation works
 */
START_TEST (test_file_system_path_for_location)
{
    GFile *file = _hildon_file_selection_get_current_folder_path (fs);
    gchar *start = g_file_get_uri(file);
    HildonFileSystemSpecialLocation *location;
    gchar *result;

    g_object_unref (file);

    location = _hildon_file_system_get_special_location (file);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting a HildonFileSystemSpecialLocation failed");

    file = _hildon_file_system_path_for_location (location);
    result = g_file_get_uri(file);
    g_object_unref (file);

    printf("'%s'|'%s'\n", start, result);
    fail_if (strcmp (start, result), "Getting the GFile failed");

    free (start);
}
END_TEST

/**
 * Purpose: Check if getting the GtkFileSystemVolume from a 
 * HildonFileSystemSpecialLocation works
 */
START_TEST (test_file_system_get_volume_for_location)
{
    const char *start = g_getenv("MYDOCSDIR");
    GtkFileSystem *system = _hildon_file_system_model_get_file_system (model);
    GFile *file = g_file_new_for_commandline_arg (start);
    GtkFileSystemVolume *volume = NULL;
    HildonFileSystemSpecialLocation *location;

    location = _hildon_file_system_get_special_location (file);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting a HildonFileSystemSpecialLocation failed");

    volume = _hildon_file_system_get_volume_for_location (system, location);
    fail_if (volume == NULL, "Getting the GtkFileSystemVolume failed");
}
END_TEST

/**
 * Purpose: Check if creating file names works with all arguments
 */
START_TEST (test_file_system_create_file_name)
{
    gboolean ret;
    GtkTreeIter iter;
    GFile *file = _hildon_file_selection_get_current_folder_path (fs);
    char *start = g_file_get_uri(file);
    char *end = "/.images";//This can be replaced with any other special locat.
    char *folder = NULL;
    char *result = NULL;
    GFile *path;
    GNode *node;
    HildonFileSystemModelNode *model_node;
    GFileInfo *info;
    HildonFileSystemSpecialLocation *location;

    g_object_unref (file);

    folder = g_strconcat (start, end, NULL);
    path = g_file_new_for_uri (folder);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    node = iter.user_data;
    model_node = node->data;
    fail_if (model_node == NULL, "Getting a HildonFileSystemModelNode failed");

    info = model_node->info;
    fail_if (info == NULL, "Getting a GtkFileInfo failed");

    location = _hildon_file_system_get_special_location (path);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting HildonFileSystemSpecialLocation failed");

    result = _hildon_file_system_create_file_name (path, location, info);
    printf("%s\n", result);

    free (folder);
    folder = g_strconcat ("sfil_li_folder_", end + 2, NULL);
    printf("%s\n", folder);

    fail_if (strcmp (folder, result),
             "Creating a file name with all arguments failed");

    g_object_unref (file);

    g_free (folder);
    g_free (start);
}

END_TEST

/**
 * Purpose: Check if creating file names works without a GtkFileInfo
 */
START_TEST (test_file_system_create_file_name_without_info)
{
    char *start = "file:///";
    GFile *path = g_file_new_for_uri (start);
    HildonFileSystemSpecialLocation *location;
    char *result;

    location = _hildon_file_system_get_special_location (path);
    fail_if (!HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting a HildonFileSystemSpecialLocation failed");

    result = _hildon_file_system_create_file_name (path, location,
						 NULL);

    fail_if (strcmp (start + 7, result),
             "Creating a file name without info failed");
}
END_TEST

/**
 * Purpose: Check if creating file names works with all arguments
 */
START_TEST (test_file_system_create_file_name_without_location)
{
    gboolean ret;
    GtkTreeIter iter;
    char *start = get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *folder = NULL;
    char *result = NULL;
    GFile *path;
    GNode *node;
    HildonFileSystemModelNode *model_node;
    GFileInfo *info;

    folder = g_strconcat (start, end, NULL);
    path = g_file_new_for_uri(folder);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    node = iter.user_data;
    model_node = node->data;
    fail_if (model_node == NULL, "Getting a HildonFileSystemModelNode failed");

    info = model_node->info;
    fail_if (info == NULL, "Getting a GtkFileInfo failed");

    result = _hildon_file_system_create_file_name (path, NULL, info);
    printf("%s\n", result);

    fail_if (strcmp (++end, result),
             "Creating a file name without location failed");

    free (folder);
    free (start);
}

END_TEST

/**
 * Purpose: Check if creating file names works without a GtkFileInfo and a
 * HildonFileSystemSpecialLocation
 */
START_TEST (test_file_system_create_file_name_without_info_and_location)
{
    GFile *file = _hildon_file_selection_get_current_folder_path (fs);
    char *start = g_file_get_uri (file);
    GFile *path = g_file_new_for_uri (start);
    size_t length = strlen (start) - strlen ("MyDocs");
    char *result;

    g_object_unref (file);
    result = _hildon_file_system_create_file_name (path, NULL, NULL);
    g_object_unref (path);
    printf ("%s\n", result);
    fail_if (strcmp (start + length, result),
             "Creating a file name without info and location failed");
    g_free (result);
    g_free (start);
}
END_TEST

/**
 * Purpose: Check if creating display names works with folders
 * There is no point in calling this without info because then this only calls
 * create_file_name and nothing else
 */
START_TEST (test_file_system_create_display_name_folder)
{
    gboolean ret;
    GtkTreeIter iter;
    GFile *file = _hildon_file_selection_get_current_folder_path (fs);
    char *start = g_file_get_uri(file);
    char *end = "/hildonfmtests";
    char *folder = NULL;
    char *result = NULL;
    GFile *path;
    GNode *node;
    HildonFileSystemModelNode *model_node;
    GFileInfo *info;
    HildonFileSystemSpecialLocation *location;

    g_object_unref (file);

    folder = g_strconcat (start, end, NULL);
    path = g_file_new_for_uri (folder);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    node = iter.user_data;
    model_node = node->data;
    fail_if (model_node == NULL, "Getting a HildonFileSystemModelNode failed");

    info = model_node->info;
    fail_if(info == NULL, "Getting a GtkFileInfo failed");

    location = _hildon_file_system_get_special_location (path);
    fail_if (HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting HildonFileSystemSpecialLocation succeeded unexpectedly");

    result = _hildon_file_system_create_display_name (path, location,
                                                      info);

    g_object_unref (path);

    printf("%s\n", result);
    fail_if (strcmp (end + 1, result),
             "Creating a display name for a file failed");

    free (result);
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if creating display names works with files
 * There is no point in calling this without info because then this only calls
 * create_file_name and nothing else
 */
START_TEST (test_file_system_create_display_name_file)
{
    gboolean ret;
    GtkTreeIter iter;
    GFile *file = _hildon_file_selection_get_current_folder_path (fs);
    char *start = g_file_get_uri (file);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *result = NULL;
    GFile *path;
    GNode *node;
    HildonFileSystemModelNode *model_node;
    GFileInfo *info;
    HildonFileSystemSpecialLocation *location;

    g_object_unref (file);

    folder = g_strconcat (start, end, sub, NULL);
    path = g_file_new_for_uri (folder);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    node = iter.user_data;
    model_node = node->data;
    fail_if (model_node == NULL, "Getting a HildonFileSystemModelNode failed");

    info = model_node->info;
    fail_if (info == NULL, "Getting a GtkFileInfo failed");

    location = _hildon_file_system_get_special_location (path);
    fail_if (HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION (location),
             "Getting HildonFileSystemSpecialLocation succeeded unexpectedly");

    result = _hildon_file_system_create_display_name (path, location,
                                                      info);

    /* g_assert_cmpstr (sub + 1, ==, result); */

    g_object_unref (path);
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if identifying known extensions works
 */
START_TEST (test_file_system_is_known_extension)
{
    g_assert (_hildon_file_system_is_known_extension (".deb"));
    g_assert (!_hildon_file_system_is_known_extension (".mdup"));
}
END_TEST

/**
 * Purpose: Check if searching a filename for the extension works
 * Case 1: Extension is not required to be known
 * Case 2: Extension is required to be known but it is not
 * Case 3: Extension is required to be known and it is
 */
START_TEST (test_file_system_search_extension)
{
    /* Test 1 */
    gchar *name = "file:///tmp/file.txt";
    gchar *res;
    gboolean only_known = false, is_folder = false;

    res = _hildon_file_system_search_extension (name, only_known, is_folder);
    fail_if (strcmp (res, ".txt"), "Searching for an extension failed");

    /* Test 2 */
    /* only_known = true;

    res = _hildon_file_system_search_extension (name, only_known, is_folder);
    fail_if (res != NULL,
             "Searching for an unknown extension amongst the known worked"); */

    /* Test 3 */
    name = "file:///tmp/file.deb";

    res = _hildon_file_system_search_extension (name, only_known, is_folder);
    fail_if (strcmp (res, ".deb"), "Searching for a known extension failed");
}
END_TEST

/**
 * Purpose: Check if searching a folder's name for an extension works
 * Case 1: It is known that the name being searched is a folder
 * Case 2: It is not known that the name being searched is a folder which
 * makes it identical to searching a file without an extension
 */
START_TEST (test_file_system_search_extension_folder)
{
    /* Test 1 */
    gchar *name = "file:///tmp";
    gchar *res;
    gboolean only_known = false, is_folder = true;

    res = _hildon_file_system_search_extension (name, only_known, is_folder);
    fail_if (res != NULL,
             "Searching for an extension from a folder worked unexpectedly");

    /* Test 2 */
    is_folder = false;

    res = _hildon_file_system_search_extension (name, only_known, is_folder);
    fail_if (res != NULL,
             "Searching for an extension from a folder worked unexpectedly");
}
END_TEST

/**
 * Purpose: Check if parsing the autonumbers works
 * Case 1: A valid autonumber
 * Case 2: An invalid autonumber
 */
START_TEST (test_file_system_parse_autonumber)
{
    /* Test 1 */
    char *valid1 = "(0)", *valid2 = " (1) ", *valid3 = " ( 5 ) ";
    char *invalid1 = "1", *invalid2 = "(A)", *invalid3 = "()";
    long res;

    res = _hildon_file_system_parse_autonumber (valid1);
    fail_if (res != 0,
             "Parsing an autonumber for a valid autonumber failed");

    res = _hildon_file_system_parse_autonumber (valid2);
    fail_if (res != 1,
             "Parsing an autonumber for a valid autonumber failed");

    res = _hildon_file_system_parse_autonumber (valid3);
    fail_if (res != 5,
             "Parsing an autonumber for a valid autonumber failed");

    /* Test 2 */
    res = _hildon_file_system_parse_autonumber (invalid1);
    fail_if (res >= 0,
             "Parsing an autonumber for a valid autonumber failed");

    res = _hildon_file_system_parse_autonumber (invalid2);
    fail_if (res >= 0,
             "Parsing an autonumber for a valid autonumber failed");

    res = _hildon_file_system_parse_autonumber (invalid3);
    fail_if (res >= 0,
             "Parsing an autonumber for a valid autonumber failed");
}
END_TEST

/**
 * Purpose: Check if removing an autonumber from a string works
 * This calls parse_autonumber to determine whether or not the autonumber is
 * valid. Comprehensive valid and invalid input testing is done in the test for
 * that function.
 */
START_TEST (test_file_system_remove_autonumber)
{
    char *start = "file";
    char *valid = " (5)";
    char *invalid = "(-6)";
    char *name, *aux;

    name = g_strconcat (start, valid, NULL);

    _hildon_file_system_remove_autonumber (name);
    fail_if (strcmp (start, name), "Removing a valid autonumber failed");

    free (name);
    name = g_strconcat (start, invalid, NULL);
    aux = g_strdup (name);

    _hildon_file_system_remove_autonumber (name);
    fail_if (strcmp (aux, name), "Removing a valid autonumber failed");

    free (name);
    free (aux);
}
END_TEST

/**
 * Purpose: Check if unescaping strings works
 */
START_TEST (test_file_system_unescape_string)
{
    char *str1 = "test", *str2 = "\%20test", *str3 = "test\%20";
    char *str4 = "test\%20test", *str5 = "\%20test\%20test\%20";
    char *esc1 = "test", *esc2 = " test", *esc3 = "test ";
    char *esc4 = "test test", *esc5 = " test test ";
    char *res1, *res2, *res3, *res4, *res5;

    res1 = hildon_file_system_unescape_string (str1);
    fail_if (strcmp(esc1, res1),
             "Unescaping a string without escape characters caused changes");

    res2 = hildon_file_system_unescape_string (str2);
    fail_if (strcmp(esc2, res2),
             "Unescaping a string with a preceding escape character failed");

    res3 = hildon_file_system_unescape_string (str3);
    fail_if (strcmp(esc3, res3),
             "Unescaping a string with a following escape character failed");

    res4 = hildon_file_system_unescape_string (str4);
    fail_if (strcmp(esc4, res4),
             "Unescaping a string with a middle escape character failed");

    res5 = hildon_file_system_unescape_string (str5);
    fail_if (strcmp(esc5, res5),
             "Unescaping a string with multiple escape characters failed");
}
END_TEST

/* ---- Test Cases for hildon-file-system-common.h ---- */

/**
 * Purpose: Check if creating a filesystem backend works
 */
START_TEST (test_file_system_create_backend)
{
    GtkFileSystem *gtk_file_system = hildon_file_system_create_backend ("gnome-vfs",
                                                                        TRUE);
    fail_if (!GTK_IS_FILE_SYSTEM (gtk_file_system),
             "Creating a filesystem backend failed");
}
END_TEST

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_default_hildonfm_file_system_private ();
    ((fm_test_func) (func)) ();
    fx_teardown_default_hildonfm_file_system_private ();
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

    /* Create a test case for comparing uris with optional pre/postfixes*/
    g_test_add_data_func ("/HildonfmFileSystemPrivate/private_compare_uris",
        (fm_test_func)test_file_system_private_compare_uris, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/private_compare_uris_last",
        (fm_test_func)test_file_system_private_compare_uris_last, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/private_compare_uris_file",
        (fm_test_func)test_file_system_private_compare_uris_file, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/private_compare_uris_last_and_file",
        (fm_test_func)test_file_system_private_compare_uris_last_and_file, fm_test_setup);

    /* Create a test case for special locations */
    g_test_add_data_func ("/HildonfmFileSystemPrivate/get_locations",
        (fm_test_func)test_file_system_get_locations, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/get_special_location",
        (fm_test_func)test_file_system_get_special_location, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/path_for_location",
        (fm_test_func)test_file_system_path_for_location, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/get_volume_for_location",
        (fm_test_func)test_file_system_get_volume_for_location, fm_test_setup);

    /* Create a test case for creating filenames and display names */
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_file_name",
        (fm_test_func)test_file_system_create_file_name, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_file_name_without_info",
        (fm_test_func)test_file_system_create_file_name_without_info, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_file_name_without_location",
        (fm_test_func)test_file_system_create_file_name_without_location, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_file_name_without_info_and_location",
        (fm_test_func)test_file_system_create_file_name_without_info_and_location, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_display_name_folder",
        (fm_test_func)test_file_system_create_display_name_folder, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_display_name_file",
        (fm_test_func)test_file_system_create_display_name_file, fm_test_setup);

    /* Create a test case for extension handling */
    g_test_add_data_func ("/HildonfmFileSystemPrivate/is_known_extension",
        (fm_test_func)test_file_system_is_known_extension, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/search_extension",
        (fm_test_func)test_file_system_search_extension, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/search_extension_folder",
        (fm_test_func)test_file_system_search_extension_folder, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/parse_autonumber",
        (fm_test_func)test_file_system_parse_autonumber, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/remove_autonumber",
        (fm_test_func)test_file_system_remove_autonumber, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemPrivate/unescape_string",
        (fm_test_func)test_file_system_unescape_string, fm_test_setup);

    /* Create a test case for functions declared in file-system-common */
    g_test_add_data_func ("/HildonfmFileSystemPrivate/create_backend",
        (fm_test_func)test_file_system_create_backend, fm_test_setup);

    return g_test_run ();
}
