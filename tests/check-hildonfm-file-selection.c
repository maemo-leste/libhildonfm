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

#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libgnomevfs/gnome-vfs.h>

#include "hildon-file-selection.h"
#include "hildon-file-system-info.h"
#include "hildon-file-system-model.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-settings.h"
#include "hildon-file-common-private.h"

#define START_TEST(name) static void name (void)
#define END_TEST 
#define fail_if(expr, ...) g_assert(!(expr))

#define OBJECTS_IN_TESTFOLDER 6

/* -------------------- Fixtures -------------------- */

static HildonFileSystemModel *model = NULL;
static HildonFileSelection *fs = NULL;
static HildonFileSelection *fs_edit = NULL;

static void
fx_setup_default_hildonfm_file_selection ()
{
    model = g_object_new (HILDON_TYPE_FILE_SYSTEM_MODEL,
                          "root-dir", g_getenv ("MYDOCSDIR"),
                          NULL);

    fs = HILDON_FILE_SELECTION (hildon_file_selection_new_with_model (model));
    fs_edit = g_object_new (HILDON_TYPE_FILE_SELECTION,
                            "model", model,
                            "edit-mode", TRUE,
                            NULL);
    g_object_ref_sink (fs);
    g_object_ref_sink (fs_edit);
}

static void
fx_teardown_default_hildonfm_file_selection ()
{
    g_object_unref (fs);
    g_object_unref (fs_edit);
    g_object_unref (model);
}

/* -------------------- Test cases -------------------- */

/**
 * Purpose: Check if creating a file selection works
 */
START_TEST (test_file_selection_creation)
{
    fail_if(!HILDON_IS_FILE_SELECTION(fs),
            "Creating a file selection failed.");
}
END_TEST

/**
 * Purpose: Check if setting modes for file selection works
 * Case 1: Set a new mode
 * Case 2: Change the mode
 */
START_TEST (test_file_selection_mode)
{
    /* Test 1: Set file selection mode to thumbnails*/
    hildon_file_selection_set_mode (fs, HILDON_FILE_SELECTION_MODE_THUMBNAILS);
    fail_if (hildon_file_selection_get_mode(fs) !=
             HILDON_FILE_SELECTION_MODE_THUMBNAILS,
             "Setting thumbnails mode for a file selection failed");

    /* Test 2: Set file selction mode to list from thumbnails*/
    /* hildon_file_selection_set_mode (fs, HILDON_FILE_SELECTION_MODE_LIST);
    fail_if (hildon_file_selection_get_mode(fs) !=
             HILDON_FILE_SELECTION_MODE_LIST,
             "Setting list mode for a file selection failed"); */
}
END_TEST

/**
 * Purpose: Check if setting different sorting keys works in file selection
 * using GTK_SORT_ASCENDING
 * Case 1: name
 * Case 2: type
 * Case 3: modified
 * Case 4: size
 */
START_TEST (test_file_selection_sort_key_ascending)
{
    GtkSortType order;
    HildonFileSelectionSortKey key;

    /* Test 1: Set sorting to ascending by name */
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_NAME,
                                        GTK_SORT_ASCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_NAME,
             "Setting sorting by size failed with ascending by name");
    fail_if (order != GTK_SORT_ASCENDING,
             "Setting ascending sorting failed with ascending by name");

    /* Test 2: Set sorting to ascending by type */
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_TYPE,
                                        GTK_SORT_ASCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_TYPE,
             "Setting sorting by size failed with ascending by type");
    fail_if (order != GTK_SORT_ASCENDING,
             "Setting ascending sorting failed with ascending by type");

    /* Test 3: Set sorting to ascending by modified */
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_MODIFIED,
                                        GTK_SORT_ASCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_MODIFIED,
             "Setting sorting by size failed with ascending by modified");
    fail_if (order != GTK_SORT_ASCENDING,
             "Setting ascending sorting failed with ascending by modified");

    /* Test 4: Set sorting to ascending by size*/
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_SIZE,
                                        GTK_SORT_ASCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_SIZE,
             "Setting sorting by size failed with ascending by size");
    fail_if (order != GTK_SORT_ASCENDING,
             "Setting ascending sorting failed with ascending by size");
}
END_TEST

/**
 * Purpose: Check if setting different sorting keys works in file selection
 * using GTK_SORT_DESCENDING
 * Case 1: name
 * Case 2: type
 * Case 3: modified
 * Case 4: size
 */
START_TEST (test_file_selection_sort_key_descending)
{
    GtkSortType order;
    HildonFileSelectionSortKey key;

    /* Test 1: Set sorting to descending by name */
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_NAME,
                                        GTK_SORT_DESCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_NAME,
             "Setting sorting by size failed with descending by name");
    fail_if (order != GTK_SORT_DESCENDING,
             "Setting descending sorting failed with descending by name");

    /* Test 2: Set sorting to descending by type */
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_TYPE,
                                        GTK_SORT_DESCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_TYPE,
             "Setting sorting by size failed with descending by type");
    fail_if (order != GTK_SORT_DESCENDING,
             "Setting descending sorting failed with descending by type");

    /* Test 3: Set sorting to descending by modified */
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_MODIFIED,
                                        GTK_SORT_DESCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_MODIFIED,
             "Setting sorting by size failed with descending by modified");
    fail_if (order != GTK_SORT_DESCENDING,
             "Setting descending sorting failed with descending by modified");

    /* Test 4: Set sorting to descending by size*/
    hildon_file_selection_set_sort_key (fs, HILDON_FILE_SELECTION_SORT_SIZE,
                                        GTK_SORT_DESCENDING);
    hildon_file_selection_get_sort_key (fs, &key, &order);
    fail_if (key != HILDON_FILE_SELECTION_SORT_SIZE,
             "Setting sorting by size failed with descending by size");
    fail_if (order != GTK_SORT_DESCENDING,
             "Setting descending sorting failed with descending by size");
}
END_TEST

/**
 * Purpose: Check if setting filters in file selection works
 * Case 1: Set a new filter
 * Case 2: Change filter
 */
START_TEST (test_file_selection_filter)
{
    /* Test 1: Test if setting and getting a filter returns the same filter
       that was given as parameter*/
    GtkFileFilter *filter, *filter2, *filter3;
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter, "image/png");
    hildon_file_selection_set_filter (fs, filter);
    filter2 = hildon_file_selection_get_filter (fs);
    fail_if (filter2 != filter, "Filter comparison failed at set & get");

    /* Test 2: Test if changing the filter works*/
    filter3 = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter3, "image/jpg");
    hildon_file_selection_set_filter (fs, filter3);
    filter2 = hildon_file_selection_get_filter (fs);
    fail_if (filter2 == filter,
             "Filter comparison (1&2) failed when changing filters");
    fail_if (filter2 != filter3,
             "Filter comparison (2&3) failed when changing filters");
}
END_TEST

/**
 * Purpose: Check if setting and getting current folder's uri works
 * Case 1: Set a new uri
 * Case 2: Set an already selected uri
 */
START_TEST (test_file_selection_current_folder_uri)
{
    /* Test 1: Test if setting and getting current folder's uri works */
    char *uri, *uri2;
    GtkTreeIter iter;
    gboolean ret;
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char* sub = "/folder2";
    char *folder = NULL;
    char *aux;

    folder = g_strconcat (start, end, NULL);

    /* When at 'root' as now this is suposed to return NULL */
    uri = hildon_file_selection_get_current_folder_uri (fs);
    g_assert_cmpstr (uri, ==, NULL);

    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    if (!ret)
        g_error ("Setting uri of the current folder failed at set & get");
    uri = hildon_file_selection_get_current_folder_uri (fs);
    g_assert_cmpstr (uri, ==, folder);

    ret = hildon_file_selection_get_current_folder_iter (fs, &iter);
    if (!ret)
        g_error ("Getting iterator of the current folder failed at set & get");
    gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
                        HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri2, -1);
    g_assert_cmpstr (uri, ==, uri2);

    /* Test 2: Test if changing the current folder's uri works */
    aux = g_strconcat (folder, sub, NULL);
    g_free (folder);
    folder = aux;

    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    if (!ret)
        g_error ("Setting uri of the current folder failed when changing folder");
    uri = hildon_file_selection_get_current_folder_uri (fs);
    g_assert_cmpstr (uri, ==, folder);

    ret = hildon_file_selection_get_current_folder_iter (fs, &iter);
    fail_if (!ret, "Getting iterator of the current folder failed when changing folder");
    gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
                        HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri2, -1);
    g_assert_cmpstr (uri, ==, uri2);

    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting and unselecting uris in file selection works
 * Case 1: Select a new uri
 * Case 2: Select an already selected uri
 */
START_TEST (test_file_selection_select_uri)
{
    /* Test 1: Test if selecting a uri for file selection works */
    gboolean ret;
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder2";
    char *folder = NULL;
    char *aux = NULL;
    GSList *list;
    gpointer *ptr;
    size_t n = 0;

    folder = g_strconcat (start, end, NULL);

    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    fail_if (!ret, "Setting uri of the current folder failed at select uri: hildonfmtests folder not found");

    aux = g_strconcat (folder, sub, NULL);
    free (folder);
    folder = aux;
    aux = NULL;
    //printf("%s\n", folder);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    fail_if (!ret, "Selecting uri of the subfolder failed at select uri");
    list = hildon_file_selection_get_selected_uris (fs);

    while (n < g_slist_length (list))
        {
            //printf ("LOOP!\n");
            ptr = g_slist_nth_data (list, n);
            if (strcmp ((char *)ptr, folder) == 0)
                {
                    aux = (char *)ptr;
                    break;
                }
            n++;
        }
    n = g_slist_length (list);
    fail_if (g_strcmp0 (folder, aux) != 0,
             "Uri not found in the selected uris list at select uri");

    /* Test 2: Test if selecting a selected uri works*/
    ret = hildon_file_selection_select_uri (fs, folder, error);
    fail_if (!ret, "Selecting an already selected uri failed at select uri");
    list = hildon_file_selection_get_selected_uris (fs);
    fail_if (n != g_slist_length (list),
             "Selected uris list changed size when selecting an already selected uri");

    n = 0;
    aux = "";
    while (n < g_slist_length (list))
        {
            //printf ("LOOP!\n");
            ptr = g_slist_nth_data (list, n);
            if (strcmp ((char *)ptr, folder) == 0)
                {
                    aux = (char *)ptr;
                    break;
                }
            n++;
        }
    fail_if (strcmp (folder, aux) != 0,
             "Uri not found in the selected uris list when selecting already selected uri at select uri");
    free(folder);
}
END_TEST

/**
 * Purpose: Check if selecting nonexistent uris in file selection works
 * Case 1: Current folder has subfolders
 * Case 2: Current folder does not have subfolders
 */
START_TEST (test_file_selection_select_uri_nonexistent)
{
    /* Test 1: Test if selecting nonexistent uris works when current folder has
       subfolders */
    gboolean ret;
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder4";
    char *aux = "/folder1";
    char *tmp = NULL, *tmp2 = NULL;
    char *folder = NULL;
    GSList *list;
    gpointer *ptr;
    size_t n = 0;

    folder = g_strconcat (start, end, NULL);

    /* This must be done in order to ensure that checking the selection after
       failure will be done in the hildonfmtests folder which content is well
       known.*/
    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    if (!ret)
        g_error ("Setting uri of the current folder failed "
                 "at select nonexistent uri: hildonfmtests folder not found");

    tmp = g_strconcat (folder, sub, NULL);
    g_free (folder);
    folder = tmp;
    tmp = NULL;

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (ret)
        g_error ("Selecting a non-existing URI succeeded with subfolders");

    g_free (folder);
    folder = g_strconcat (start, end, aux, NULL);

    list = hildon_file_selection_get_selected_uris (fs);
    /*if (n == g_slist_length (list))
        g_error ("Selecting a non-existing URI failed with subfolders "
                 "but a selection was not added to list of selected URIs. "
                 "Length: %d 0th item: %s",
                 g_slist_length (list), (char *)g_slist_nth_data (list, 0));*/
    n = 0;
    while (n < g_slist_length (list))
        {
            ptr = g_slist_nth_data (list, n);
            if (strcmp ((char *)ptr, folder) == 0)
                {
                    tmp = (char *)ptr;
                    break;
                }
            n++;
        }
    /* if (g_strcmp0 (folder, tmp) != 0)
        g_error ("Proper URI not found in the selected uris list "
                 "when selecting a nonexistent uri"); */
    /* Test 2: Test if selecting nonexistent uris works when current folder does
       not have subfolders */
    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    if (!ret)
        g_error ("Changing URI of the current folder to non-existing "
                 "subfolder %s failed", folder);

    tmp2 = g_strconcat (tmp, sub, NULL);
    free (tmp);
    tmp = tmp2;

    /* ret = hildon_file_selection_select_uri (fs, tmp, error);
    if (ret)
        g_error ("Selecting a nonexistent uri succeeded without subfolders"); */

    /* list = hildon_file_selection_get_selected_uris (fs);
    if (g_slist_length (list) != 0)
        g_error ("Selecting a nonexistent uri failed without subfolders "
                 "but a selection was still added to the list of selected URIs"); */

    g_free (folder);
    g_free (start);
}
END_TEST

/**
 * Purpose: Check if unselecting selected uris in file selection works
 */
START_TEST (test_file_selection_unselect_uri)
{
    gboolean ret;
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char* sub = "/folder2";
    char *folder = NULL;
    GSList *list;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (!ret)
        g_error ("Selecting uri of the subfolder failed at select uri");

    hildon_file_selection_unselect_uri (fs, folder);
    list = hildon_file_selection_get_selected_uris (fs);
    /* if (g_slist_length (list) != 0)
        g_error ("Unselecting the selected uri failed in file selection"); */
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting not selected uris in file selection works
 */
START_TEST (test_file_selection_unselect_uri_not_selected)
{
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char* sub = "/folder2";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    //printf("( %d | ", n);
    hildon_file_selection_unselect_uri (fs, folder);
    list = hildon_file_selection_get_selected_uris (fs);
    //printf("%d )\n", g_slist_length (list));
    fail_if(n != g_slist_length (list),
            "Unselecting the not selected uri changed selections unexpectedly in file selection");
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting an uri will change current folder as expected
 * Case 1: Making a new selection
 * Case 2: Changing the selection
 */
START_TEST (test_file_selection_select_uri_and_current_folder_uri)
{
    /* Test 1: Test if making a new selection changes current folder */
    gboolean ret;
    GError **error = NULL;
    char* orig = hildon_file_selection_get_current_folder_uri (fs);
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder3";
    char *subsub = "/subfolder";
    char *test_folder = NULL;
    char *folder = NULL;
    char *subfolder = NULL;
    char *aux;

    g_assert (orig == NULL);

    test_folder = g_strconcat (start, end, NULL);
    folder = g_strconcat (start, end, sub, NULL);
    subfolder = g_strconcat (start, end, sub, subsub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    fail_if (!ret, "Selecting uri of the subfolder failed at select uri");
    aux = hildon_file_selection_get_current_folder_uri (fs);
    g_assert (aux != NULL);
    g_assert_cmpstr (test_folder, ==, aux);

    /* Test 2: Test if changing the selection changes current folder */
    ret = hildon_file_selection_select_uri (fs, subfolder, error);
    fail_if (!ret, "Selecting uri of the subfolder failed at select uri");
    aux = hildon_file_selection_get_current_folder_uri (fs);
    g_assert (aux != NULL);
    g_assert_cmpstr (folder, ==, aux);

    free (orig);
    free (start);
    free (folder);
    free (test_folder);
    free (subfolder);
}
END_TEST

/**
 * Purpose: Check if column header visibility setting works
 * Case 1: Set value
 * Case 2: Change value
 */
START_TEST (test_file_selection_column_headers_visible)
{
    /* Test 1: Test if setting the value works */
    gboolean bool1, bool2, bool3;

    bool1 = true;
    hildon_file_selection_set_column_headers_visible (fs, bool1);
    bool2 = hildon_file_selection_get_column_headers_visible (fs);
    fail_if (bool1 != bool2,
             "Setting column headers visible failed at set & get");

    /* Test 2: test if changing the value works*/
    bool3 = false;
    hildon_file_selection_set_column_headers_visible (fs, bool3);
    bool2 = hildon_file_selection_get_column_headers_visible (fs);
    fail_if (bool1 == bool2, "Changing column header value failed (1&2)");
    fail_if (bool2 != bool3, "Changing column header value failed (2&3)");
}
END_TEST

/**
 * Purpose: Check if setting select multiple value works properly
 * Case 1: Set value
 * Case 2: Change value
 */
START_TEST (test_file_selection_select_multiple)
{
    /* Test 1: Test if setting the value in normal mode is ignored */
    gboolean bool1, bool2;

    bool1 = true;
    hildon_file_selection_set_select_multiple (fs, bool1);
    bool2 = hildon_file_selection_get_select_multiple (fs);
    g_assert_cmpint (bool1, !=, bool2);

    /* Test 2: Test if setting the value in edit mode is ignored */
    bool1 = true;
    hildon_file_selection_set_select_multiple (fs_edit, bool1);
    bool2 = hildon_file_selection_get_select_multiple (fs_edit);
    g_assert_cmpint (bool1, !=, bool2);
}
END_TEST

/**
 * Purpose: Check if selecting all works when something is already selected
 */
START_TEST (test_file_selection_select_all_something_selected)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at select all");

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_select_all (fs);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);

    g_assert_cmpint (n, ==, 1);
    g_free (start);
    g_free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting all works when something is already selected
 */
static void test_file_selection_select_all_something_selected_edit (void)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs_edit);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs_edit, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at select all");

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_select_all (fs_edit);

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);

    /* g_assert_cmpint (n, ==, OBJECTS_IN_TESTFOLDER); */
    g_free (start);
    g_free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting all works when nothing is selected beforehand
 */
START_TEST (test_file_selection_select_all_nothing_selected)
{
    gboolean ret;
    GError **error = NULL;
    GtkTreeIter iter;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *folder = NULL;
    char *aux;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, NULL);

    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    if (!ret)
        g_error ("Setting current folder uri failed at select all");

    aux = g_strconcat (folder, "/folder1", NULL);
    ret = hildon_file_system_model_load_uri (model, aux, &iter);
    if (!ret)
        g_error ("Loading a uri to hildon file system model failed");
    hildon_file_selection_unselect_all (fs);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 0);

    hildon_file_selection_select_all (fs);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);

    g_assert_cmpint (n, ==, 1);
    g_free (aux);
    g_free (start);
    g_free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting all works when nothing is selected beforehand
 */
START_TEST (test_file_selection_select_all_nothing_selected_edit)
{
    gboolean ret;
    GError **error = NULL;
    GtkTreeIter iter;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs_edit);
    char *end = "/hildonfmtests";
    char *folder = NULL;
    char *aux;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, NULL);

    ret = hildon_file_selection_set_current_folder_uri (fs_edit, folder, error);
    if (!ret)
        g_error ("Setting current folder uri failed at select all");

    aux = g_strconcat (folder, "/folder1", NULL);
    ret = hildon_file_system_model_load_uri (model, aux, &iter);
    if (!ret)
        g_error ("Loading a uri to hildon file system model failed");
    hildon_file_selection_unselect_all (fs_edit);

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 0);

    hildon_file_selection_select_all (fs_edit);

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);

    /* g_assert_cmpint (n, ==, OBJECTS_IN_TESTFOLDER); */
    g_free (aux);
    g_free (start);
    g_free (folder);
}

/**
 * Purpose: Check if unselecting all works with multiple selections
 */
START_TEST (test_file_selection_unselect_all_multiple)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at unselect all");

    hildon_file_selection_select_all (fs);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_unselect_all (fs);
    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    /* g_assert_cmpint (n, ==, 0); */
    g_free (start);
    g_free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting all works with multiple selections
 */
START_TEST (test_file_selection_unselect_all_multiple_edit)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs_edit);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs_edit, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at unselect all");

    hildon_file_selection_select_all (fs_edit);

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    /* g_assert_cmpint (n, ==, OBJECTS_IN_TESTFOLDER); */

    hildon_file_selection_unselect_all (fs_edit);
    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 0);
    g_free (start);
    g_free (folder);
}

/**
 * Purpose: Check if unselecting all works with a single selection
 */
START_TEST (test_file_selection_unselect_all_single)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at unselect all");

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_unselect_all (fs);
    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);

    /* g_assert_cmpint (n, ==, 0); */
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting all works without selections
 */
START_TEST (test_file_selection_unselect_all_none)
{
    GSList *list;
    size_t n = 0;

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    fail_if (n != 0,
             "There should be no selections by default yet there is atleast 1");

    hildon_file_selection_unselect_all (fs);
    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    //printf("%d\n", n);
    fail_if(n != 0,
            "Unselect all added selections or did something really weird");
}
END_TEST

/**
 * Purpose: Check if unselecting all works with multiple selections
 */
START_TEST (test_file_selection_clear_multi_selection_multiple)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at clear multi selection");

    hildon_file_selection_select_all (fs);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_clear_multi_selection (fs);
    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);

    g_assert_cmpint (n, ==, 1);
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting all works with multiple selections
 */
static void test_file_selection_clear_multi_selection_multiple_edit (void)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs_edit);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs_edit, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at clear multi selection");

    hildon_file_selection_select_all (fs_edit);

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    /* g_assert_cmpint (n, ==, OBJECTS_IN_TESTFOLDER); */

    hildon_file_selection_clear_multi_selection (fs_edit);
    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);

    /* g_assert_cmpint (n, ==, 1); */
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting all works with a single selection
 */
START_TEST (test_file_selection_clear_multi_selection_single)
{
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_selection_select_uri (fs, folder, error);
    fail_if (!ret, "Selecting uri failed at clear multi selection");

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    fail_if (n != 1,
             "Select uri succeeded but selected uris list is of wrong size");

    hildon_file_selection_clear_multi_selection (fs);
    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    //printf ("%d\n", n);
    fail_if (n != 1, "Clear multi selection failed with a single selection");
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting all works without selections
 */
START_TEST (test_file_selection_clear_multi_selection_none)
{
    GSList *list;
    size_t n = 0;

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    fail_if (n != 0,
             "There should be no selections by default yet there is atleast 1");

    hildon_file_selection_clear_multi_selection (fs);
    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    //printf("%d\n", n);
    fail_if(n != 0,
            "Clear multi selection added selections or did something really weird");
}
END_TEST

/* --- Test cases for functions declared in hildon-file-common-private.h --- */

/**
 * Purpose: Check if setting and getting paths in file selection works
 * Case 1: Set a new path
 * Case 2: Change the path
 */
START_TEST (test_file_selection_current_folder_path)
{
    /* Test 1: Test if setting and getting current folder's path works */
    gchar *uri, *uri2;
    GtkFilePath *path, *path2;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system(model);
    gboolean ret;
    GError **error = NULL;
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char* sub = "/folder2";
    char *folder = NULL;
    char *aux = NULL;

    folder = g_strconcat (start, end, NULL);

    path = gtk_file_system_uri_to_path(gtk_file_system, folder);
    ret = _hildon_file_selection_set_current_folder_path (fs, path, error);
    fail_if (!ret, "Setting path of the current folder failed at set & get");

    uri = hildon_file_selection_get_current_folder_uri (fs);
    g_assert_cmpstr (uri, ==, folder);

    path2 = _hildon_file_selection_get_current_folder_path (fs);
    uri2 = gtk_file_system_path_to_uri (gtk_file_system, path2);
    fail_if (strcmp (uri, uri2) != 0,
             "Path comparison failed at set & get");

    /* Test 2: Test if changing the current folder's path works */
    aux = g_strconcat (folder, sub, NULL);
    free (folder);
    folder = aux;

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    ret = _hildon_file_selection_set_current_folder_path (fs, path, error);
    fail_if (!ret,
             "Setting path of the current folder failed when changing folder");

    uri = hildon_file_selection_get_current_folder_uri (fs);
    fail_if (strcmp (uri, folder) != 0,
             "Changing current folder's path succeeded but wrong path set");

    path2 = _hildon_file_selection_get_current_folder_path (fs);
    uri2 = gtk_file_system_path_to_uri (gtk_file_system, path2);
    fail_if (strcmp (uri, uri2) != 0,
             "Path comparison failed when changing folder");

    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting a GtkFilePath works
 * Case 1: Select a path
 * Case 2: Select an already selected path
 */
START_TEST (test_file_selection_select_path)
{
    /* Test 1: Test if selecting a path works*/
    gboolean ret;
    GError **error = NULL;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    char *aux = NULL;
    GSList *list;
    gpointer *ptr;
    size_t n = 0;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    GtkFilePath *path = NULL;

    folder = g_strconcat (start, end, sub, NULL);

    path = gtk_file_system_uri_to_path(gtk_file_system, folder);
    ret = _hildon_file_selection_select_path (fs, path, error);
    fail_if (!ret,
             "Selecting a path failed at select path");

    list = hildon_file_selection_get_selected_uris (fs);

    while (n < g_slist_length (list))
        {
            //printf ("LOOP!\n");
            ptr = g_slist_nth_data (list, n);
            if (strcmp ((char *)ptr, folder) == 0)
                {
                    aux = (char *)ptr;
                    break;
                }
            n++;
        }
    n = g_slist_length (list);
    fail_if (strcmp (folder, aux) != 0,
             "Path not found in the selected uris list at select path");

    /* Test 2: Test if selecting an already selected path works*/
    ret = _hildon_file_selection_select_path (fs, path, error);
    fail_if (!ret,
             "Selecting a path failed at reselect path");

    list = hildon_file_selection_get_selected_uris (fs);

    while (n < g_slist_length (list))
        {
            //printf ("LOOP!\n");
            ptr = g_slist_nth_data (list, n);
            if (strcmp ((char *)ptr, folder) == 0)
                {
                    aux = (char *)ptr;
                    break;
                }
            n++;
        }
    n = g_slist_length (list);
    fail_if (strcmp (folder, aux) != 0,
             "Path not found in the selected uris list when reselecting");

    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting nonexistent paths in file selection works
 * Case 1: Current folder has subfolders
 * Case 2: Current folder does not have subfolders
 */
START_TEST (test_file_selection_select_path_nonexistent)
{
    /* Test 1: Test if selecting nonexistent paths works when current folder has
       subfolders */
    gboolean ret;
    GError **error = NULL;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system(model);
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder4";
    char *aux = "/folder1";
    char *tmp = NULL, *tmp2 = NULL;
    char *folder = NULL;
    GSList *list;
    gpointer *ptr;
    size_t n = 0;

    folder = g_strconcat (start, end, NULL);

    /* This must be done in order to ensure that checking the selection after
       failure will be done in the hildonfmtests folder which content is well
       known.*/
    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    ret = _hildon_file_selection_set_current_folder_path (fs, path, error);
    if (!ret)
        g_error ("Setting path of the current folder failed "
                 "at select nonexistent path: hildonfmtests folder not found");

    tmp = g_strconcat (folder, sub, NULL);
    free (folder);
    folder = tmp;
    tmp = NULL;
    path = gtk_file_system_uri_to_path (gtk_file_system, folder);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    ret = _hildon_file_selection_select_path (fs, path, error);
    if (ret)
        g_error ("Selecting a nonexistent path succeeded with subfolders");

    list = hildon_file_selection_get_selected_uris (fs);
    /* g_assert_cmpint (n, !=, g_slist_length (list)); */

    g_free (folder);
    folder = g_strconcat (start, end, aux, NULL);

    n = 0;
    while (n < g_slist_length (list))
        {
            ptr = g_slist_nth_data (list, n);
            if (strcmp ((char *)ptr, folder) == 0)
                {
                    tmp = (char *)ptr;
                    break;
                }
            n++;
        }
    /* g_assert_cmpstr (folder, ==, tmp); */

    /* Test 2: Test if selecting nonexistent paths works when current folder
       does not have subfolders */
    ret = hildon_file_selection_set_current_folder_uri (fs, folder, error);
    if (!ret)
        g_error ("Changing path of the current folder to subfolders "
                 "failed at select nonexistent path: hildonfmtests folder not found");

    tmp2 = g_strconcat (tmp, sub, NULL);
    free (tmp);
    tmp = tmp2;

    /* path = gtk_file_system_uri_to_path (gtk_file_system, tmp);
    ret = _hildon_file_selection_select_path (fs, path, error);
    if (ret)
        g_error ("Selecting a nonexistent path succeeded without subfolders");

    list = hildon_file_selection_get_selected_uris (fs);
    g_assert_cmpint (g_slist_length (list), ==, 0); */
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting selected paths in file selection works
 */
START_TEST (test_file_selection_unselect_path)
{
    gboolean ret;
    GError **error = NULL;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char* sub = "/folder2";
    char *folder = NULL;
    GSList *list;

    folder = g_strconcat (start, end, sub, NULL);

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    ret = _hildon_file_selection_select_path (fs, path, error);
    fail_if (!ret, "Selecting path of the subfolder failed at select path");

    _hildon_file_selection_unselect_path (fs, path);
    list = hildon_file_selection_get_selected_uris (fs);
    /* g_assert_cmpint (g_slist_length (list), ==, 0); */
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if unselecting not selected paths in file selection works
 */
START_TEST (test_file_selection_unselect_path_not_selected)
{
    char *start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char* sub = "/folder2";
    char *folder = NULL;
    GtkFilePath *path;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);

    path = gtk_file_system_uri_to_path(gtk_file_system, folder);
    _hildon_file_selection_unselect_path (fs, path);
    list = hildon_file_selection_get_selected_uris (fs);
    fail_if(n != g_slist_length (list),
            "Unselecting the not selected path changed selections unexpectedly");
    free (start);
    free (folder);
}
END_TEST

/**
 * Purpose: Check if selecting an path will change current folder as expected
 * Case 1: Making a new selection
 * Case 2: Changing a selection
 */
START_TEST (test_file_selection_select_path_and_current_folder_path)
{
    /* Test 1: Test if making a new path selection changes current folder */
    gboolean ret;
    GError **error = NULL;
    GtkFilePath *path, *path2;
    GtkFileSystem *gtk_file_system = _hildon_file_system_model_get_file_system (model);
    char* orig = hildon_file_selection_get_current_folder_uri (fs);
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder3";
    char *subsub = "/subfolder";
    char *test_folder = NULL;
    char *folder = NULL;
    char *subfolder = NULL;
    char *aux = NULL;

    g_assert (!orig);

    test_folder = g_strconcat (start, end, NULL);
    folder = g_strconcat (start, end, sub, NULL);
    subfolder = g_strconcat (start, end, sub, subsub, NULL);

    path = gtk_file_system_uri_to_path (gtk_file_system, folder);
    ret = _hildon_file_selection_select_path (fs, path, error);
    fail_if (!ret, "Selecting uri of the subfolder failed at select uri");
    path2 = _hildon_file_selection_get_current_folder_path (fs);
    g_assert (path2);
    aux = gtk_file_system_path_to_uri(gtk_file_system, path2);
    fail_if (strcmp(test_folder, aux) != 0,
             "Selecting a uri didn't change the current folder properly");

    /* Test 2: Test if changing the selection changes current folder */
    path = gtk_file_system_uri_to_path (gtk_file_system, subfolder);
    ret = _hildon_file_selection_select_path (fs, path, error);
    fail_if (!ret, "Selecting uri of the subfolder failed at select uri");
    path2 = _hildon_file_selection_get_current_folder_path (fs);
    aux = gtk_file_system_path_to_uri(gtk_file_system, path2);
    fail_if (!aux ,
             "(char *)_hildon_file_selection_get_current_folder_path returned NULL");
    fail_if (strcmp(folder, aux) != 0,
             "Selecting a uri didn't change the current folder properly");

    free (orig);
    free (start);
    free (folder);
    free (test_folder);
    free (subfolder);
}
END_TEST

/**
 * Purpose: Check if getting just the selected files works when folders are also
 * selected
 */
START_TEST(test_file_selection_get_selected_files)
{
    gboolean ret;
    GError **error = NULL;
    GtkTreeIter iter;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    if (!ret)
        g_error ("Loading a uri to hildon file system model failed");

    ret = hildon_file_selection_select_uri (fs, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at select all");

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_select_all (fs);

    list = hildon_file_selection_get_selected_uris (fs);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    list = _hildon_file_selection_get_selected_files (fs);
    /* g_assert_cmpint (g_slist_length (list), ==, 3); */

    g_free (start);
    g_free (folder);
}
END_TEST

/**
 * Purpose: Check if getting just the selected files works when folders are also
 * selected
 */
static void test_file_selection_get_selected_files_edit (void)
{
    gboolean ret;
    GError **error = NULL;
    GtkTreeIter iter;
    char* start = (char *)_hildon_file_selection_get_current_folder_path (fs_edit);
    char *end = "/hildonfmtests";
    char *sub = "/folder1";
    char *folder = NULL;
    GSList *list;
    size_t n = 0;

    folder = g_strconcat (start, end, sub, NULL);

    ret = hildon_file_system_model_load_uri (model, folder, &iter);
    if (!ret)
        g_error ("Loading a uri to hildon file system model failed");

    ret = hildon_file_selection_select_uri (fs_edit, folder, error);
    if (!ret)
        g_error ("Selecting uri failed at select all");

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    g_assert_cmpint (n, ==, 1);

    hildon_file_selection_select_all (fs_edit);

    list = hildon_file_selection_get_selected_uris (fs_edit);
    n = g_slist_length (list);
    /* g_assert_cmpint (n, ==, OBJECTS_IN_TESTFOLDER); */

    list = _hildon_file_selection_get_selected_files (fs_edit);
    /* g_assert_cmpint (g_slist_length (list), ==, 3); */

    g_free (start);
    g_free (folder);
}

/**
 * Purpose: Check if getting the type of a HildonFileSelection works
 */
START_TEST (test_file_selection_type)
{
    GType type = hildon_file_selection_get_type();

    fail_if (HILDON_TYPE_FILE_SELECTION != type,
             "Getting the type of a HildonFileSelection failed");
}
END_TEST

/* ---------- Suite creation ---------- */

typedef void (*fm_test_func) (void);

static void
fm_test_setup (gconstpointer func)
{
    fx_setup_default_hildonfm_file_selection ();
    ((fm_test_func) (func)) ();
    fx_teardown_default_hildonfm_file_selection ();
}

int
main (int    argc,
      char** argv)
{
    if (!g_thread_supported ())
        g_thread_init (NULL);
    g_assert(gnome_vfs_init());
    gtk_test_init (&argc, &argv, NULL);

    /* Create test case for file selection modes */
    g_test_add_data_func ("/HildonfmFileSelection/creation",
        (fm_test_func)test_file_selection_creation, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/mode",
        (fm_test_func)test_file_selection_mode, fm_test_setup);

    /* Create test case for file selection sorting */
    g_test_add_data_func ("/HildonfmFileSelection/sort_key_ascending",
        (fm_test_func)test_file_selection_sort_key_ascending, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/sort_key_descending",
        (fm_test_func)test_file_selection_sort_key_descending, fm_test_setup);

    /* Create test case for file selection filters */
    g_test_add_data_func ("/HildonfmFileSelection/filter",
        (fm_test_func)test_file_selection_filter, fm_test_setup);

    /* Create test case for file selection uris */
    g_test_add_data_func ("/HildonfmFileSelection/current_folder_uri",
        (fm_test_func)test_file_selection_current_folder_uri, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_uri",
        (fm_test_func)test_file_selection_select_uri, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_uri_nonexistent",
        (fm_test_func)test_file_selection_select_uri_nonexistent, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_uri",
        (fm_test_func)test_file_selection_unselect_uri, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_uri_not_selected",
        (fm_test_func)test_file_selection_unselect_uri_not_selected, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_uri_and_current_folder_uri",
        (fm_test_func)test_file_selection_select_uri_and_current_folder_uri, fm_test_setup);

    /* Create test case for file selection boolean settings */
    g_test_add_data_func ("/HildonfmFileSelection/column_headers_visible",
        (fm_test_func)test_file_selection_column_headers_visible, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_multiple",
        (fm_test_func)test_file_selection_select_multiple, fm_test_setup);

    /* Create test case for file selection multiple uri functions */
    g_test_add_data_func ("/HildonfmFileSelection/select_all_nothing_selected",
        (fm_test_func)test_file_selection_select_all_nothing_selected, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_all_nothing_selected_edit",
        (fm_test_func)test_file_selection_select_all_nothing_selected_edit, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_all_something_selected",
        (fm_test_func)test_file_selection_select_all_something_selected, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_all_something_selected_edit",
        (fm_test_func)test_file_selection_select_all_something_selected_edit, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_all_multiple",
        (fm_test_func)test_file_selection_unselect_all_multiple, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_all_multiple_edit",
        (fm_test_func)test_file_selection_unselect_all_multiple_edit, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_all_single",
        (fm_test_func)test_file_selection_unselect_all_single, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/nselect_all_none",
        (fm_test_func)test_file_selection_unselect_all_none, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/clear_multi_selection_multiple",
        (fm_test_func)test_file_selection_clear_multi_selection_multiple, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/clear_multi_selection_multiple_edit",
        (fm_test_func)test_file_selection_clear_multi_selection_multiple_edit, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/clear_multi_selection_single",
        (fm_test_func)test_file_selection_clear_multi_selection_single, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/clear_multi_selection_none",
        (fm_test_func)test_file_selection_clear_multi_selection_none, fm_test_setup);

    /* Create test case for file selection general testing */
    g_test_add_data_func ("/HildonfmFileSelection/type",
        (fm_test_func)test_file_selection_type, fm_test_setup);

    /* Create test case for file selection filters */
    g_test_add_data_func ("/HildonfmFileSelection/current_folder_path",
        (fm_test_func)test_file_selection_current_folder_path, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_path",
        (fm_test_func)test_file_selection_select_path, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_path_nonexistent",
        (fm_test_func)test_file_selection_select_path_nonexistent, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_path",
        (fm_test_func)test_file_selection_unselect_path, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/unselect_path_not_selected",
        (fm_test_func)test_file_selection_unselect_path_not_selected, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/select_path_and_current_folder_path",
        (fm_test_func)test_file_selection_select_path_and_current_folder_path, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/get_selected_files",
        (fm_test_func)test_file_selection_get_selected_files, fm_test_setup);
    g_test_add_data_func ("/HildonfmFileSelection/get_selected_files_edit",
        (fm_test_func)test_file_selection_get_selected_files_edit, fm_test_setup);

    return g_test_run ();
}
