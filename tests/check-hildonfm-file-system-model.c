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

#include "hildon-file-system-model.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-dynamic-device.h"
#include "hildon-file-system-local-device.h"
#include "hildon-file-system-obex.h"
#include "hildon-file-system-remote-device.h"
#include "hildon-file-system-root.h"
#include "hildon-file-system-smb.h"
#include "hildon-file-system-upnp.h"
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

/**
 * hildon_uri_to_gtk_tree_iter:
 * @file_system: a #GtkFileSystem
 * @iter: a #GtkTreeIter to store the result.
 * @path: a #gchar to look up from the GtkTreeModel
 *
 * Converts a #GtkFilePath to #GtkTreeIter.
 *
 * Return value: %TRUE, if #iter points to valid location, %FALSE otherwise.
 */
static gboolean
hildon_uri_to_gtk_tree_iter (GtkTreeIter *iter, const gchar *uri,
                             size_t start_length, GtkTreeModel *model)
{
    GtkTreeIter child;
    gboolean result = false;
    gint i = 0;
    gchar *str = NULL, *current = NULL;
    gchar **tokens = NULL;
    gchar *path, *aux;
    size_t length = start_length + 1;

    if (!uri || strlen (uri) < start_length)
        return FALSE;

    aux = g_strdup (uri);
    path = aux;
    path += start_length;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), iter);
    tokens = g_strsplit (path, "/", 0);

    if (!tokens [0])
        {
            g_free (tokens);
            return FALSE;
        }

    if (!strcmp (tokens [0], ""))
        {
            tokens [0] = g_strndup (uri, start_length);
        }
    /*
    while (tokens [i])
        {
            printf ("\"%s\"\n", tokens [i]);
            ++i;
        }

    i = 0;
    */
    current = g_strdup (tokens [0]);

    while( current )
        {
            //printf("%s|", current);
            length = 1;
            gtk_tree_model_get (GTK_TREE_MODEL (model), iter,
                                GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
            if (!g_str_has_prefix (str, "file://"))
                {
                    //printf ("Adding prefix\n");
                    path = g_strconcat ("file://", str, NULL);
                }
            else
                {
                    path = g_strdup (str);
                }
            /*
            printf ("%s\n", path);
            printf ("%d\n",
                    gtk_tree_model_iter_n_children (GTK_TREE_MODEL(model),
                                                    iter));
            */
            if (!strcmp (current, path))
                {
                    g_free (str);
                    g_free (path);

                    if (tokens[i+1] == NULL || *tokens[i+1] == '\0')
                        {
                            result = TRUE;
                            break;
                        }
                    if (gtk_tree_model_iter_children (GTK_TREE_MODEL (model),
                                                      &child, iter ))
                        {
                            *iter = child;
                            gchar *tmp = g_strdup (current);
                            g_free (current);
                            current = g_strconcat (tmp, "/", tokens [++i],
                                                   NULL);
                            g_free (tmp);
                            continue;
                        }
                }
            else
                {
                    g_free (str);
                    g_free (path);

                    if (*current == '\0')
                        {
                            printf ("\'\\0 found\'");
                            break;
                        }
                }

            if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (model), iter))
                break;
        }
    while (current)
        {
            g_free (current);
            current = tokens [++i];
        }

    g_free (tokens);

    return result;
}

/* -------------------- Fixtures -------------------- */

static HildonFileSystemModel *model = NULL;
static HildonFileSelection *fs = NULL;

static void
fx_setup_default_hildonfm_file_system_model ()
{
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv("MYDOCSDIR"),
                          NULL);
    fail_if (!HILDON_IS_FILE_SYSTEM_MODEL(model),
             "File system model creation failed");

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fail_if (!HILDON_IS_FILE_SELECTION (fs),
             "File selection creation failed");
}

static void
fx_teardown_default_hildonfm_file_system_model ()
{

}

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if loading local paths to the file system model works
 */
START_TEST (test_file_system_model_load_local_path)
{
    gboolean ret;
    GtkTreeIter iter, iter2;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *str = NULL, *str2 = NULL;
    char *aux;

    folder = g_strconcat (start, end, sub, NULL);

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    //printf("%s\n", (char *)path);
    aux = gtk_file_system_path_to_filename(gtk_file_system, path);
    //printf("%s\n", aux);

    ret = hildon_file_system_model_load_local_path (model, aux, &iter);
    fail_if (!ret, "Loading a local path to hildon file system model failed");

    ret = hildon_uri_to_gtk_tree_iter (&iter2, folder, strlen(start),
                                       GTK_TREE_MODEL(model));
    fail_if (!ret, "Getting a tree iterator to the loaded local path failed: loading to the model failed");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str2, -1);

    fail_if (strcmp (str, str2),
             "Comparison of uris pointed to by the tree iterators failed");

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
    fail_if (strcmp(folder, start) != 0,
             "Original uri differs from uris pointed to by the tree iterators");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if loading uris to the file system model works
 */
START_TEST (test_file_system_model_load_uri)
{
    gboolean ret;
    GtkTreeIter iter, iter2;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *str = NULL, *str2 = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    ret = hildon_uri_to_gtk_tree_iter (&iter2, folder, strlen(start),
                                       GTK_TREE_MODEL(model));
    fail_if (!ret, "Getting a tree iterator to the loaded uri failed: loading to the model failed");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str2, -1);

    fail_if (strcmp (str, str2),
             "Comparison of uris pointed to by the tree iterators failed");

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
    fail_if (strcmp(folder, start) != 0,
             "Original uri differs from uris pointed to by the tree iterators");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if loading GtkFilePaths to the file system model works
 */
START_TEST (test_file_system_model_load_path)
{
    gboolean ret;
    GtkTreeIter iter, iter2;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *str = NULL, *str2 = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    //printf("%s\n", (char *)path);

    ret = hildon_file_system_model_load_path (model, path, &iter);
    fail_if (!ret, "Loading a GtkFilePath to hildon file system model failed");

    ret = hildon_uri_to_gtk_tree_iter (&iter2, folder, strlen(start),
                                       GTK_TREE_MODEL(model));
    fail_if (!ret, "Getting a tree iterator to the loaded GtkFilePath failed: loading to the model failed");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str2, -1);

    fail_if (strcmp (str, str2),
             "Comparison of uris pointed to by the tree iterators failed");

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
    fail_if (strcmp(folder, start) != 0,
             "Original uri differs from uris pointed to by the tree iterators");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if searching an unloaded uri works.
 * Since two of the search functions simply call the third after converting
 * their argument to the proper format, it is necessary to test only one of
 * them.
 */
START_TEST (test_file_system_model_search_uri_not_loaded)
{
    gboolean ret, rec;
    GtkTreeIter iter;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    rec = true;
    ret = hildon_file_system_model_search_uri (model, folder, &iter, NULL, rec);
    fail_if (ret, "Searching file system model worked with an unloaded uri");
}
END_TEST

/**
 * Purpose: Check if searching the file system model for a local path works
 */
START_TEST (test_file_system_model_search_local_path)
{
    gboolean ret, rec;
    GtkTreeIter iter, iter2;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *str = NULL, *str2 = NULL;
    char *aux;

    folder = g_strconcat (start, end, sub, NULL);

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    //printf("%s\n", (char *)path);
    aux = gtk_file_system_path_to_filename(gtk_file_system, path);
    //printf("%s\n", aux);

    ret = hildon_file_system_model_load_local_path (model, aux, &iter);
    fail_if (!ret, "Loading a local path to hildon file system model failed");

    rec = true;
    ret = hildon_file_system_model_search_local_path (model, aux, &iter2, NULL,
                                               rec);
    fail_if (!ret,
             "Searching the file system model failed with a loaded local path");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str2, -1);

    fail_if (strcmp (str, str2),
             "Comparison of uris pointed to by the tree iterators failed");

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
    fail_if (strcmp(folder, start) != 0,
             "Original uri differs from uris pointed to by the tree iterators");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if searching the file system model for a uri works
 */
START_TEST (test_file_system_model_search_uri)
{
    gboolean ret, rec;
    GtkTreeIter iter, iter2;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *str = NULL, *str2 = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    rec = true;
    ret = hildon_file_system_model_search_uri (model, folder, &iter2, NULL,
                                               rec);
    fail_if (!ret,
             "Searching the file system model failed with a loaded uri");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str2, -1);

    fail_if (strcmp (str, str2),
             "Comparison of uris pointed to by the tree iterators failed");

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
    fail_if (strcmp(folder, start) != 0,
             "Original uri differs from uris pointed to by the tree iterators");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if searching the file system model for a GtkFilePath works
 */
START_TEST (test_file_system_model_search_path)
{
    gboolean ret, rec;
    GtkTreeIter iter, iter2;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *str = NULL, *str2 = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    //printf("%s\n", (char *)path);

    ret = hildon_file_system_model_load_path (model, path, &iter);
    fail_if (!ret, "Loading a GtkFilePath to hildon file system model failed");

    rec = true;
    ret = hildon_file_system_model_search_path (model, path, &iter2, NULL, rec);
    fail_if (!ret,
             "Searching the file system model failed with a loaded GtkFilePath");

    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str, -1);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter2,
                        GTK_FILE_SYSTEM_MEMORY_COLUMN_NAME, &str2, -1);

    fail_if (strcmp (str, str2),
             "Comparison of uris pointed to by the tree iterators failed");

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
    fail_if (strcmp(folder, start) != 0,
             "Original uri differs from uris pointed to by the tree iterators");
    free (folder);
    free (start);
}
END_TEST

/**
 * Purpose: Check if creating a new filename works when the default filename is
 * available
 */
START_TEST (test_file_system_model_new_item)
{
    gboolean ret;
    GtkTreeIter iter;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *stub = "file";
    char *ext = ".txt";
    char *folder = NULL;
    char *new_name = NULL;

    folder = g_strconcat (start, end, sub, NULL);
    //printf ("%s\n", folder);
    
    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    folder = g_strconcat (start, end, NULL);
    //printf ("%s\n", folder);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    new_name = hildon_file_system_model_new_item (model, &iter, stub, ext);
    fail_if (new_name == NULL,
             "Getting a new filename failed when stub name is available");
    //printf("%s\n", new_name);

    fail_if (strcmp (new_name, stub),
             "New filename differs from expected when stub is available");
}
END_TEST

/**
 * Purpose: Check if creating a new filename works when the default filename is
 * unavailable
 * Case 1: First alternative is available
 * Case 2: First alternative is unavailable, second one is available
 */
START_TEST (test_file_system_model_new_item_stub_exists)
{
    /* Test 1: Test if the first alternative name will be offered as new file
       name when the default file name is unavailable */
    gboolean ret;
    GtkTreeIter iter;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *stub = "file1";
    char *stub2 = "file2";
    char *ext = ".txt";
    char *folder = NULL;
    char *new_name = NULL;

    folder = g_strconcat (start, end, sub, NULL);
    //printf ("%s\n", folder);
    
    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    folder = g_strconcat (start, end, NULL);
    //printf ("%s\n", folder);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    new_name = hildon_file_system_model_new_item (model, &iter, stub, ext);
    fail_if (new_name == NULL,
             "Getting a new filename failed when stub name is unavailable");
    //printf("%s\n", new_name);

    fail_if (!strcmp (new_name, stub),
             "New filename doesn't differ from stub when stub is unavailable");
    fail_if (strcmp (new_name, "file1 (1)"),
             "New filename differs from expected when stub is unavailable");

    g_free (new_name);

    /* Test 2: Test if the second alternative name will be offered as new file
       name when both the default file name and the first alternative file name
       are unavailable */
    new_name = hildon_file_system_model_new_item (model, &iter, stub2, ext);
    fail_if (new_name == NULL,
             "Getting a new filename failed when stub & (1) are unavailable");
    //printf("%s\n", new_name);

    fail_if (!strcmp (new_name, stub),
             "New filename doesn't differ from stub when stub & (1)are unavailable");
    fail_if (strcmp (new_name, "file2 (2)"),
             "New filename differs from expected when stub & (1) are unavailable");
    g_free (new_name);
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if getting a new filename from a not loaded location works 
 */
START_TEST (test_file_system_model_new_item_not_loaded)
{
    gboolean ret;
    GtkTreeIter iter;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *stub = "file1";
    char *ext = ".txt";
    char *folder = NULL;
    char *new_name = NULL;

    folder = g_strconcat (start, end, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    fail_if (!ret, "Loading a uri to hildon file system model failed");

    new_name = hildon_file_system_model_new_item (model, &iter, stub, ext);
    fail_if (new_name != NULL,
             "Getting a new filename succeeded when location is not loaded");
}
END_TEST

/**
 * Purpose: Check if creating a new filename works when the default filename is
 * available using autoname_uri
 */
START_TEST (test_file_system_model_autoname_uri)
{
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file.txt";
    char *folder = NULL;
    char *new_name = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    new_name = hildon_file_system_model_autoname_uri (model, folder, error);
    if (new_name == NULL)
        g_error ("Expected NULL but URI is %s", new_name);

    if (g_strcmp0 (folder, new_name))
        g_error ("Expected %s but folder is %s", folder, new_name);
    g_free (new_name);
}
END_TEST

/**
 * Purpose: Check if creating a new filename works when the default filename is
 * unavailable using autoname_uri
 * Case 1: First alternative is available
 * Case 2: First alternative is unavailable, second one is available
 */
START_TEST (test_file_system_model_autoname_uri_stub_exists)
{
    /* Test 1: Test if the first alternative name will be offered as new file
       name when the default file name is unavailable */
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/file1.txt";
    char *sub2 = "/file2.txt";
    char *folder = NULL;
    char *new_name = NULL;

    folder = g_strconcat (start, end, sub, NULL);
    //printf ("%s\n", folder);

    new_name = hildon_file_system_model_autoname_uri (model, folder, error);
    fail_if (new_name == NULL,
             "Getting a new filename failed when stub name is unavailable");
    new_name = hildon_file_system_unescape_string (new_name);
    //printf("%s\n", new_name);

    fail_if (!strcmp (new_name, folder),
             "New filename doesn't differ from stub when stub is unavailable");

    folder = g_strconcat (start, end, "/file1 (1).txt", NULL);
    /*
      This one passes the test but does not comply with documentation
      strcat (folder, "/file1 (1).txt");
    */
    //printf ("%s\n", folder);

    fail_if (strcmp (new_name, folder),
             "New filename differs from expected when stub is unavailable");

    g_free (new_name);

    /* Test 2: Test if the second alternative name will be offered as new file
       name when both the default file name and the first alternative file name
       are unavailable */
    folder = g_strconcat (start, end, sub2, NULL);

    new_name = hildon_file_system_model_autoname_uri (model, folder, error);
    fail_if (new_name == NULL,
             "Getting a new filename failed when stub & (1) are unavailable");
    new_name = hildon_file_system_unescape_string (new_name);
    //printf("%s\n", new_name);

    fail_if (!strcmp (new_name, folder),
             "New filename doesn't differ from stub when stub & (1) are unavailable");

    folder = g_strconcat (start, end, "/file2 (2).txt", NULL);
    /*
      This one passes the test but does not comply with documentation
      strcat (folder, "/file2 (2).txt");
    */
    fail_if (strcmp (new_name, folder),
             "New filename differs from expected when stub & (1) are unavailable");
    g_free (new_name);
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if trying to get a new filename from a not loaded location
 * using autoname_uri
 */
START_TEST (test_file_system_model_autoname_uri_nonexistent_folder)
{
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hilttontests";
    char *sub = "/file1.txt";
    char *folder = NULL;
    char *new_name = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    new_name = hildon_file_system_model_autoname_uri (model, folder, error);
    /* g_assert_cmpstr (new_name, ==, NULL); */
}
END_TEST

/**
 * Purpose: Check if getting a GtkFileSystem from HildonFileSystemModel works
 */
START_TEST (test_file_system_model_get_file_system)
{
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    fail_if (!GTK_IS_FILE_SYSTEM (gtk_file_system),
             "Getting a GtkFileSystem from HildonFileSystemModel failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of a HildonFileSystemModel works
 */
START_TEST (test_file_system_model_type)
{
    GType type = hildon_file_system_model_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_MODEL != type,
             "Getting the type of a HildonFileSystemModel failed");
}
END_TEST

/* ----- Test Cases for functions implemented outside hildon-fm ---- */
/* ---------- These revolve around different hildon types ---------- */

/**
 * Purpose: Check if getting the type of a dynamic device works
 */
START_TEST (test_file_system_dynamic_device)
{
    GType type = hildon_file_system_dynamic_device_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_DYNAMIC_DEVICE != type,
             "Getting the type of a dynamic device failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of a local device works
 */
START_TEST (test_file_system_local_device)
{
    GType type = hildon_file_system_local_device_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_LOCAL_DEVICE != type,
             "Getting the type of a local device failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of an obex device works
 */
START_TEST (test_file_system_obex)
{
    GType type = hildon_file_system_obex_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_OBEX != type,
             "Getting the type of an obex device failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of a remote device works
 */
START_TEST (test_file_system_remote_device)
{
    GType type = hildon_file_system_remote_device_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_REMOTE_DEVICE != type,
             "Getting the type of a remote device failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of a filesystem root works
 */
START_TEST (test_file_system_root)
{
    GType type = hildon_file_system_root_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_ROOT != type,
             "Getting the type of a filesystem root failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of an smb device works
 */
START_TEST (test_file_system_smb)
{
    GType type = hildon_file_system_smb_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_SMB != type,
             "Getting the type of an smb device failed");
}
END_TEST

/**
 * Purpose: Check if getting the type of a upnp device works
 */
START_TEST (test_file_system_upnp)
{
    GType type = hildon_file_system_upnp_get_type();

    fail_if (HILDON_TYPE_FILE_SYSTEM_UPNP != type,
             "Getting the type of a upnp device failed");
}
END_TEST

/* ------------------ Suite creation ------------------ */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_default_hildonfm_file_system_model ();
    ((fm_test_func) (func)) ();
    fx_teardown_default_hildonfm_file_system_model ();
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

    /* Create a test case for loading locations into the file system model*/
    g_test_add_data_func ("/HildonfmFileSystemModel/load_local_path",
        (fm_test_func)test_file_system_model_load_local_path, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/load_uri",
        (fm_test_func)test_file_system_model_load_uri, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/load_path",
        (fm_test_func)test_file_system_model_load_path, fm_test_setup);

    /* Create a test case for searching the file system model for loaded
       locations */
    g_test_add_data_func ("/HildonfmFileSystemModel/search_uri_not_loaded",
        (fm_test_func)test_file_system_model_search_uri_not_loaded, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/search_local_path",
        (fm_test_func)test_file_system_model_search_local_path, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/search_uri",
        (fm_test_func)test_file_system_model_search_uri, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/search_path",
        (fm_test_func)test_file_system_model_search_path, fm_test_setup);

    /* Create a test case for getting unique file names for new files */
    g_test_add_data_func ("/HildonfmFileSystemModel/new_item",
        (fm_test_func)test_file_system_model_new_item, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/new_item_stub_exists",
        (fm_test_func)test_file_system_model_new_item_stub_exists, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/new_item_not_loaded",
        (fm_test_func)test_file_system_model_new_item_not_loaded, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/autoname_uri",
        (fm_test_func)test_file_system_model_autoname_uri, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/autoname_uri_stub_exists",
        (fm_test_func)test_file_system_model_autoname_uri_stub_exists, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/autoname_uri_nonexistent_folder",
        (fm_test_func)test_file_system_model_autoname_uri_nonexistent_folder, fm_test_setup);

    /* Create a test case for testing functions not ment for public use */
    g_test_add_data_func ("/HildonfmFileSystemModel/get_file_system",
        (fm_test_func)test_file_system_model_get_file_system, fm_test_setup);

    /* Create a test case for testing types. Most of the functions being tested
       are implemented outside hildon-fm but declared inside it. */
    g_test_add_data_func ("/HildonfmFileSystemModel/model_type",
        (fm_test_func)test_file_system_model_type, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/dynamic_device",
        (fm_test_func)test_file_system_dynamic_device, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/local_device",
        (fm_test_func)test_file_system_local_device, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/obex",
        (fm_test_func)test_file_system_obex, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/remote_device",
        (fm_test_func)test_file_system_remote_device, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/root",
        (fm_test_func)test_file_system_root, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/smb",
        (fm_test_func)test_file_system_smb, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSystemModel/upnp",
        (fm_test_func)test_file_system_upnp, fm_test_setup);

    return g_test_run ();
}
