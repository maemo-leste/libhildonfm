/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2005-2006 Nokia Corporation.  All rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
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

/**
 * SECTION: hildon-file-selection
 * @short_description: Hildon file selection widget
 *
 * This widget displays a list view with the contents of a
 * #HildonFileSystemModel and a "Go to parent folder" button for navigation.
 */

/*
 * Completely rewritten, now data structure is separated to GtkTreeModel
 * compatible object.
 *
 * Issues concerning GtkTreeView::horizontal-separator
 *  * This space is concerned as cell area => alignment 1.0
 *                                         => end of text is clipped)
 *  * This comes also to the last column => Looks wery bad in all trees.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <hildon/hildon.h>
#include <dbus/dbus.h>

#include <gconf/gconf-client.h>

#include "hildon-file-selection.h"

#include "hildon-file-common-private.h"

/* I wonder where does that additional +2 come from.
    Anyway I have to add it to make cell 60 + two
    margin pixels high. Now height is pixel perfect. */
#define THUMBNAIL_CELL_HEIGHT (60 + HILDON_MARGIN_DEFAULT * 2 + 2)
#define THUMBNAIL_CELL_WIDTH (80 + 16)

/* Row height should be 30 and 4 is a mysterious constant.
    I just wonder why the constant is now 4 instead of 2
    used in thumbnail view???  And now it seems to be 3!!! */
#define LIST_CELL_HEIGHT (30 + 3)

/* These sizes are even more confusing. Magical constant is now 1!?# */
#define TREE_CELL_HEIGHT (30 + 1)
G_DEFINE_TYPE(HildonFileSelection, hildon_file_selection,
              GTK_TYPE_CONTAINER)

static GObject *hildon_file_selection_constructor(GType type,
                                                  guint
                                                  n_construct_properties,
                                                  GObjectConstructParam *
                                                  construct_properties);
static void
hildon_file_selection_check_close_load_banner(GtkTreeModel *model,
  GtkTreeIter *iter, gpointer data);
static void hildon_file_selection_close_load_banner(HildonFileSelection *
                                                    self);
static void hildon_file_selection_modified(gpointer object, GtkTreePath *path);
static gboolean view_path_to_main_iter(GtkTreeModel *model,
  GtkTreeIter *iter, GtkTreePath *path);
static void hildon_file_selection_real_row_insensitive(HildonFileSelection *self,
  GtkTreeIter *location);
static void
get_safe_folder_tree_iter(HildonFileSelectionPrivate *priv, GtkTreeIter *iter);

static void
hildon_file_selection_enable_cursor_magic (HildonFileSelection *self,
                                                     GtkTreeModel *model);

static void
hildon_file_selection_disable_cursor_magic (HildonFileSelection *self,
                                            GtkTreeModel *model);

static GtkTreeModel *
hildon_file_selection_create_sort_model(HildonFileSelection *self,
                                        GtkTreeIterCompareFunc sort_function,
                                        GtkTreeModel *parent_model);

static gboolean
hildon_file_selection_select_iter (HildonFileSelection *self,
                                   GtkTreeIter *iter,
                                   GError **error);

GtkWidget *
_hildon_file_selection_get_scroll_list (HildonFileSelection *self);

GtkWidget *
_hildon_file_selection_get_scroll_thumb (HildonFileSelection *self);

static void reload_local_device_folders(HildonFileSelection *selection);

/* Property id:s */
enum {
    PROP_MODEL = 1,
    PROP_LOCAL_ONLY,
    PROP_SHOW_HIDDEN,
    PROP_DND,
    PROP_EMPTY_TEXT,
    PROP_VISIBLE_COLUMNS,
    PROP_SAFE_FOLDER,
    PROP_ACTIVE_PANE,
    PROP_SHOW_UPNP,
    PROP_PANE_POSITION,
    PROP_DRAGGING,
    PROP_SHOW_FILES, /* show or not show the files in the content pane */
    PROP_EDIT_MODE,
    PROP_NAVI_PANE_HIDDEN,
    PROP_SHOW_FOLDERS,
    PROP_SHOW_READONLY
};

enum {
    CURRENT_FOLDER_CHANGED,
    FILE_ACTIVATED,
    FOLDER_ACTIVATED,
    SELECTION_CHANGED,
    NAVIGATION_PANE_CONTEXT_MENU,
    CONTENT_PANE_CONTEXT_MENU,
    URIS_DROPPED,
    LOCATION_INSENSITIVE,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _HildonFileSelectionPrivate {
    GtkWidget *scroll_dir;
    GtkWidget *scroll_list;
    GtkWidget *scroll_thumb;
    GtkWidget *dir_tree;
    GtkWidget *view[4]; /* List, thumbnail, empty, repair */
    int cur_view;
    GtkWidget *hpaned;

    GtkTreeModel *main_model;
    GtkTreeModel *sort_model;   /* HildonFileSystemModel doesn't implement */
    GtkTreeModel *dir_sort;     /* GtkTreeSortable */

    GtkTreeModel *dir_filter;
    GtkTreeModel *view_filter;

    HildonLiveSearch *live_search;

    GtkTreeRowReference *current_folder;
    GtkWidget *view_selector;
    GtkFileFilter *filter;

    HildonFileSelectionMode mode;       /* User requested mode. Actual
                                           mode is either this or an empty
                                           view containing a message */
    guint banner_timeout_id;
    guint banner_close_timeout_id;
    guint content_pane_changed_id;
    guint delayed_select_id;
    guint pane_pos;
    gboolean update_banner;
    gboolean content_pane_last_used;

    gboolean column_headers_visible;

    /* This is set when widget is shown and content pane
       should have focus, but there is nothing yet. We use this to move focus when
       there is some content */
    gboolean force_content_pane;

    /* Set to FALSE when a folder loading starts. If user moves the cursor this
       is set to TRUE. So if it's still FALSE when folder loading is finished,
       we can move the cursor where we want. */
    gboolean user_touched;
    /* Set if user have scrolled bar to specific position. This causes
       automatic scrolling to cursor be disabled when the loading is done */
    gboolean user_scrolled;

    /* Properties */

    HildonFileSelectionVisibleColumns visible_columns;
    gboolean drag_enabled;
    gboolean local_only;
    gboolean show_hidden;
    gboolean show_upnp;
    gboolean currently_dragging;
    GtkFilePath *safe_folder;

    gchar **drag_data_uris;

    gchar *cursor_goal_uri;
    
    /* set this flag to FALSE, files will be filtered out in content pane, used
       in for example, folder chooser dialog and some other similar dialogs */
    gboolean show_files;
    gboolean edit_mode;
    gboolean hide_navi;
    GtkTreeRowReference *current_row; // a row in sort_model of content pane
    gboolean show_folders;
    gboolean show_readonly;

    GtkWidget *label_card;
    GtkWidget *label_device;

    gboolean show_localdevice;
    GVolumeMonitor *monitor;

    guint cursor_idle_id;
    gpointer cursor_idle_data;
};

#if 0
static void
dump_iter (const char *label, GtkTreeModel *model, GtkTreeIter *iter)
{
  char *uri;
  gtk_tree_model_get (model, iter,
                      HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
                      -1);
  fprintf (stderr, "%s: %s\n", label, uri);
  g_free (uri);
}

static void
dump_path (const char *label, GtkTreeModel *model, GtkTreePath *path)
{
  GtkTreeIter iter;
  if (gtk_tree_model_get_iter (model, &iter, path))
    dump_iter (label, model, &iter);
  else
    fprintf (stderr, "%s: <invalid>\n", label);
}

static void
dump_selection (const char *label, GtkTreeView *view)
{
  GtkTreeSelection *sel = gtk_tree_view_get_selection (view);
  GtkTreeModel *model;
  GList *rows = gtk_tree_selection_get_selected_rows (sel, &model);

  fprintf (stderr, "%s:\n", label);
  while (rows)
    {
      dump_path ("", model, rows->data);
      rows = rows->next;
    }
}
#endif

static void
hildon_file_selection_cancel_delayed_select(HildonFileSelectionPrivate *priv)
{
  if (priv->delayed_select_id)
  {
    g_source_remove(priv->delayed_select_id);
    priv->delayed_select_id = 0;
  }
}

static void scroll_to_cursor(GtkTreeView *tree)
{
  GtkTreePath *path;

  gtk_tree_view_get_cursor(tree, &path, NULL);

  if (path)
  {
    gtk_tree_view_scroll_to_cell(tree, path, NULL, FALSE, 0.0f, 0.0f);
    gtk_tree_path_free(path);
  }
}

/* Focuses the given view and scrolls to the cursor. This become
    quite complicated, because the cursor is not neccesarily set.
    We use cursor if it's found then we try selection and finally
    fall back to first item */
static void activate_view(GtkWidget *view)
{
  if (GTK_IS_TREE_VIEW(view))
  {
    if (!GTK_WIDGET_HAS_FOCUS(view))
      gtk_widget_grab_focus(view);

    scroll_to_cursor(GTK_TREE_VIEW(view));
  }
}

static gboolean
hildon_file_selection_content_pane_visible(HildonFileSelectionPrivate *
                                           priv)
{
    return GTK_WIDGET_VISIBLE(priv->view_selector);
}

static GtkWidget *get_current_view(HildonFileSelectionPrivate * priv)
{
    if (hildon_file_selection_content_pane_visible(priv))
      return priv->view[priv->cur_view];

    return NULL;
}

/* If there is no content AND loading is ready we switch to
    information view. Otherwise we show currently asked view */
static gint get_view_to_be_displayed(HildonFileSelectionPrivate *priv)
{
  GtkTreeModel *model, *child_model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gint result;

  model = priv->view_filter;

  /* We have no model => no files/folders */
  if (!model)
    return 2;

  /* We have something => display normal view */
  if (gtk_tree_model_get_iter_first(model, &iter))
      //    return priv->mode;
      return HILDON_FILE_SELECTION_MODE_THUMBNAILS; //Fremantle

  /* We have currently nothing, check if we are loading or if we
     should offer the repair button. */

  g_object_get(model, "child-model", &child_model,
	       "virtual-root", &path, NULL);

  if (gtk_tree_model_get_iter(child_model, &iter, path))
    {
      gboolean ready, is_drive;
      char *uri = NULL;

      gtk_tree_model_get (child_model, &iter,
			  HILDON_FILE_SYSTEM_MODEL_COLUMN_LOAD_READY, &ready,
			  HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_DRIVE, &is_drive,
			  HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
			  -1);
      if (is_drive)
	{
	  /* FIXME */
	if(g_ascii_strcasecmp(uri, "drive:///dev/mmcblk0p3") == 0)  // to be improved, somehow hard coded
	  result = 4;
	else
	  result = 3;
      }
      else if (!ready)
	result = priv->cur_view;
      else
	result = 2;
      g_free(uri);
    }
  else /* Virtual root is almost certainly found, EXCEPT when
          that node is destroyed and row_reference is invalidated... */
    result = 2;

  gtk_tree_path_free(path);
  g_object_unref(child_model);
  return result;
}

static void rebind_models(HildonFileSelectionPrivate *priv)
{
  GtkWidget *current_view = get_current_view(priv);

  if (priv->view[1] == current_view || priv->view[0] == current_view)
  {
     gtk_tree_view_set_model(GTK_TREE_VIEW(priv->view[0]), NULL);
     gtk_tree_view_set_model(GTK_TREE_VIEW(priv->view[1]),
        priv->sort_model);
  }
  else
  {
     gtk_tree_view_set_model(GTK_TREE_VIEW(priv->view[0]), NULL);
     gtk_tree_view_set_model(GTK_TREE_VIEW(priv->view[1]), NULL);
  }
}

/* This method sync selections between listview and thumbnail view */
static void hildon_file_selection_sync_selections(HildonFileSelectionPrivate *priv,
                                                  GtkWidget * source, GtkWidget * target)
{

}

static gboolean expand_cursor_row(GtkTreeView *tree)
{
  GtkTreePath *path;
  gboolean result;

  gtk_tree_view_get_cursor(tree, &path, NULL);
  if (!path)
    return FALSE;

  result = gtk_tree_view_expand_row(tree, path, FALSE);
  gtk_tree_path_free(path);

  return result;
}

static GtkWidget *
view_widget (HildonFileSelectionPrivate *priv, int view)
{
  /* Horrible...
   */
  if (view == 0)
    return priv->scroll_list;
  else if (view == 1)
    return priv->scroll_thumb;
  else
    return priv->view[view];
}

GtkWidget *
_hildon_file_selection_get_scroll_list (HildonFileSelection *self)
{
    g_return_val_if_fail (HILDON_IS_FILE_SELECTION (self), NULL);

    return self->priv->scroll_list;
}

GtkWidget *
_hildon_file_selection_get_scroll_thumb (HildonFileSelection *self)
{
    g_return_val_if_fail (HILDON_IS_FILE_SELECTION (self), NULL);

    return self->priv->scroll_thumb;
}

static void
hildon_file_selection_inspect_view(HildonFileSelectionPrivate *priv)
{
    GtkWidget *view;
    gint current_page, target_page;
    gboolean content_focused = FALSE;
    gboolean virtual_page = FALSE;

    if (!hildon_file_selection_content_pane_visible(priv))
      return;

    current_page = priv->cur_view;
    target_page = get_view_to_be_displayed(priv);
    if(target_page == 4){
      virtual_page = TRUE;
      target_page--;
    }
    else
      virtual_page = FALSE;
    target_page = target_page == 0 ? 1 : target_page; //Fremantle

    view = get_current_view (priv);

    if (current_page != target_page)
    {
      content_focused = GTK_WIDGET_HAS_FOCUS(view) ||
          priv->force_content_pane || priv->content_pane_last_used;
      if (current_page >= 0)
	gtk_widget_hide (view_widget (priv, current_page));
      gtk_widget_show (view_widget (priv, target_page));
      priv->cur_view = target_page;

      if (current_page == HILDON_FILE_SELECTION_MODE_THUMBNAILS &&
        target_page == HILDON_FILE_SELECTION_MODE_LIST)
        hildon_file_selection_sync_selections(priv, priv->view[1],
                                              priv->view[0]);
      else if (current_page == HILDON_FILE_SELECTION_MODE_LIST &&
        target_page == HILDON_FILE_SELECTION_MODE_THUMBNAILS)
        hildon_file_selection_sync_selections(priv, priv->view[0],
                                              priv->view[1]);
      else
        rebind_models(priv);

      view = get_current_view(priv);

      if (content_focused)
      {
        if (!GTK_WIDGET_CAN_FOCUS(view))
          view = priv->dir_tree;
	if((target_page == 3) && virtual_page) {
	  gtk_widget_hide(priv->label_card);
	  gtk_widget_show(priv->label_device);
	}
	else if((target_page == 3) && !virtual_page) {
	  gtk_widget_hide(priv->label_device);
	  gtk_widget_show(priv->label_card);
	}
        activate_view(view);
      }
    }

    if (priv->force_content_pane)
      expand_cursor_row(GTK_TREE_VIEW(priv->dir_tree));
}
static void hildon_file_selection_forall(GtkContainer * self,
                                         gboolean include_internals,
                                         GtkCallback callback,
                                         gpointer callback_data)
{
    g_return_if_fail(callback != NULL);

    if (include_internals) {
        HildonFileSelectionPrivate *priv =
            HILDON_FILE_SELECTION(self)->priv;

        (*callback) (priv->hpaned, callback_data);
    }
}

static void hildon_file_selection_size_request(GtkWidget * self,
                                               GtkRequisition *
                                               requisition)
{
    GtkRequisition req;
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(self)->priv;

    gtk_widget_size_request(GTK_WIDGET(priv->hpaned), &req);
    requisition->width = req.width;
    requisition->height = req.height;
}

static void hildon_file_selection_size_allocate(GtkWidget * self,
                                                GtkAllocation * allocation)
{
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(self)->priv;

    self->allocation = *allocation;
    gtk_widget_size_allocate(priv->hpaned, allocation);
}

/* We need to move to the safe folder if our current folder
   is under the given location */
static void hildon_file_selection_check_location(
  HildonFileSystemModel *model, GtkTreeIter *iter, gpointer data)
{
  HildonFileSelection *self;
  GtkTreeModel *tree_model;
  GtkTreeIter current_iter;
  GtkTreePath *current_path, *device_path;

  self = HILDON_FILE_SELECTION(data);
  tree_model = GTK_TREE_MODEL(model);

  if (hildon_file_selection_get_current_folder_iter(self, &current_iter))
  {
    current_path = gtk_tree_model_get_path(tree_model, &current_iter);
    device_path = gtk_tree_model_get_path(tree_model, iter);

    if (current_path && device_path &&
        (gtk_tree_path_compare(device_path, current_path) == 0 ||
         gtk_tree_path_is_ancestor(device_path, current_path)))
    {
      char *message;

      gtk_tree_model_get(tree_model, iter,
        HILDON_FILE_SYSTEM_MODEL_COLUMN_FAILED_ACCESS_MESSAGE, &message,
        -1);

      if (message) {
        hildon_banner_show_information(GTK_WIDGET(self), NULL, message);
        g_free(message);
      }
    }

    gtk_tree_path_free(current_path);
    gtk_tree_path_free(device_path);
  }
}

static void hildon_file_selection_finalize(GObject * obj)
{
    HildonFileSelection *self = HILDON_FILE_SELECTION(obj);
    HildonFileSelectionPrivate *priv = self->priv;

    if (priv->banner_timeout_id)
      g_source_remove (priv->banner_timeout_id);

    if (priv->banner_close_timeout_id)
      g_source_remove (priv->banner_close_timeout_id);

    if (priv->content_pane_changed_id)
      g_source_remove(priv->content_pane_changed_id);

    if (priv->cursor_idle_id)
      g_source_remove (priv->cursor_idle_id);

    /* We have to remove these by hand */
    g_signal_handlers_disconnect_by_func
        (priv->main_model,
         (gpointer) hildon_file_selection_check_close_load_banner,
         self);
    g_signal_handlers_disconnect_by_func
        (priv->main_model,
         (gpointer) hildon_file_selection_check_location,
         self);
    g_signal_handlers_disconnect_by_func
        (priv->main_model,
        (gpointer) hildon_file_selection_inspect_view,
        priv);
    g_signal_handlers_disconnect_by_func
        (priv->main_model,
        (gpointer) hildon_file_selection_modified,
        self);

    hildon_file_selection_disable_cursor_magic (self, priv->sort_model);
    hildon_file_selection_disable_cursor_magic (self, priv->dir_filter);

    g_free (priv->cursor_goal_uri);
    priv->cursor_goal_uri = NULL;

    hildon_file_selection_cancel_delayed_select(priv);
    g_source_remove_by_user_data(self); /* Banner checking timeout */
    /* This gives warnings to console about unref_tree_helpers */
    /* This have homething to do with content pane filter model */
    gtk_widget_unparent(priv->hpaned);

    /* Works also with NULLs */
    gtk_tree_row_reference_free(priv->current_folder);
    gtk_tree_row_reference_free(priv->current_row);
    g_strfreev(priv->drag_data_uris);

    g_object_unref(priv->dir_filter);

    g_object_unref(priv->dir_sort);

    g_object_unref(priv->sort_model);

    if (priv->view_filter)
    {
      g_object_unref(priv->view_filter);
      priv->view_filter = NULL;
    }

    /* Setting filter don't cause refiltering any more,
        because view_filter is already set to NULL. Failing this setting caused
        segfaults earlier. */
    hildon_file_selection_set_filter(self, NULL);

    if (priv->monitor)
    {
      g_object_unref(priv->monitor);
      priv->monitor = NULL;
    }

    G_OBJECT_CLASS(hildon_file_selection_parent_class)->finalize(obj);
}

/* Previously the folder status was enough to decide whether or not
   to show an item. Now we have also properties for hidden folders
   and non-local folders, so we have to use this filter function
   instead of simple boolean column. */
static gboolean navigation_pane_filter_func(GtkTreeModel *model,
    GtkTreeIter *iter, gpointer data)
{
  gboolean folder, hidden, local, upnp = FALSE;
  char *uri = NULL;
  const char *upnp_root;
  const char *mydocsdir;
  HildonFileSelectionPrivate *priv = data;

  gtk_tree_model_get(model, iter,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER, &folder,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_HAS_LOCAL_PATH, &local,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_HIDDEN, &hidden,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
    -1);

  if (uri != NULL)
  {
      mydocsdir = g_getenv ("MYDOCSDIR");
      upnp_root = g_getenv ("UPNP_ROOT");
      upnp = upnp_root && g_str_has_prefix (uri, upnp_root);
      //Fremantle hack: never filter out root of roots with weird uri
      if (g_str_has_prefix (uri, "files:///"))
          local = TRUE;
      if (mydocsdir && g_str_has_prefix (uri, "file:///") &&
          !priv->show_localdevice && g_str_has_prefix (&uri[7], mydocsdir))
      {
          g_free (uri);
	  return FALSE;
      }
      g_free (uri);
  }

  return (folder &&
         (!priv->local_only || local) &&
         (priv->show_hidden || !hidden) &&
         (priv->show_upnp || !upnp));
}

static gboolean visible_for_live_search (GtkTreeModel *model,
    GtkTreeIter * iter,
    HildonFileSelectionPrivate *priv)
{
    const gchar *needle;
    gchar *needle_stripped;
    gchar *filename = NULL, *display_text = NULL;
    gchar *filename_stripped = NULL, *display_text_stripped = NULL;
    gboolean visible = TRUE;

    /* No live search yet */
    if (!priv->live_search)
        return TRUE;

    needle = hildon_live_search_get_text (priv->live_search);
    /* Live Search is not used at the moment */
    if (needle == NULL || needle[0] == '\0')
      return TRUE;

    gtk_tree_model_get (model, iter,
        HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_NAME, &filename,
        PRIV_COLUMN_DISPLAY_TEXT, &display_text,
        -1);

    needle_stripped = hildon_helper_normalize_string (needle);
    if (filename)
      filename_stripped = hildon_helper_normalize_string (filename);
    if (display_text)
      display_text_stripped = hildon_helper_normalize_string (display_text);

    visible = (filename_stripped != NULL &&
        hildon_helper_smart_match (filename_stripped,
          needle_stripped) != NULL) ||
      (display_text_stripped != NULL &&
        hildon_helper_smart_match (display_text_stripped,
          needle_stripped) != NULL);

    g_free (filename);
    g_free (display_text);
    g_free (filename_stripped);
    g_free (display_text_stripped);
    g_free (needle_stripped);

    return visible;
}

static gboolean filter_func(GtkTreeModel * model, GtkTreeIter * iter,
                            gpointer data)
{
    HildonFileSelectionPrivate *priv = data;
    gboolean is_folder, result, has_local_path, is_hidden;
    GtkFileFilterInfo info;
    char *uri = NULL;
    const char *prefix;
    GtkTreePath *root_path;
    GtkTreePath *tree_path;

    /* Avoid expensive computation for files which are not in the current
     * directory */
    tree_path = gtk_tree_model_get_path (model, iter);
    g_object_get (priv->view_filter, "virtual-root", &root_path, NULL);
    if (!gtk_tree_path_is_descendant (tree_path, root_path)) {
        gtk_tree_path_free (tree_path);
        gtk_tree_path_free (root_path);
        return FALSE;
    }
    if (gtk_tree_path_get_depth (tree_path)
        - gtk_tree_path_get_depth (root_path) != 1) {
        gtk_tree_path_free (tree_path);
        gtk_tree_path_free (root_path);
        return FALSE;
    }
    gtk_tree_path_free (tree_path);
    gtk_tree_path_free (root_path);


    if (visible_for_live_search (model, iter, priv) == FALSE)
      return FALSE;


    if (priv->local_only) {
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_HAS_LOCAL_PATH,
                           &has_local_path, -1);
        if (!has_local_path)
            return FALSE;
    }

    if (!priv->show_hidden) {
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_HIDDEN, 
			   &is_hidden, -1);
        if (is_hidden)
            return FALSE;
    }
    gtk_tree_model_get(model, iter,
		       HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri, -1);

    prefix = g_getenv ("UPNP_ROOT");
    if (uri && prefix && g_str_has_prefix (uri, prefix) && !priv->show_upnp) {
       g_free (uri);
       return FALSE;
    }

    prefix = g_getenv("MYDOCSDIR");
    if (uri && prefix && g_str_has_prefix(uri, "file:///") &&
        g_str_has_prefix(&uri[7], prefix) &&
        (priv->show_localdevice == FALSE)) {
      return FALSE;
    }
    if (uri) {
       g_free (uri);
       uri = NULL;
    }

    gtk_tree_model_get(model, iter,
		       HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER,
		       &is_folder, -1);
    if(!priv->show_files) {
       if (!is_folder) {
	/* Files are NOT displayed in for example, 
	   folder chooser dialog..., maybe more etc */
           return FALSE;
       }
    }
    if (is_folder && !priv->show_folders) {
        return FALSE;
    }

    if (!priv->show_readonly)
    {
        gboolean is_readonly;
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_READONLY,
                           &is_readonly, -1);
        if (is_readonly)
            return FALSE;
    }
    /* All files are shown if no filter is present */
    if (!priv->filter) {
        return TRUE;
    }

    if (is_folder)      /* Folders are always displayed */
        return TRUE;

    memset(&info, 0, sizeof(GtkFileFilterInfo));
    info.contains = gtk_file_filter_get_needed(priv->filter);

    if (info.contains & GTK_FILE_FILTER_FILENAME)
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_LOCAL_PATH,
                           &info.filename, -1);
    if (info.contains & GTK_FILE_FILTER_URI)
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &info.uri,
                           -1);
    if (info.contains & GTK_FILE_FILTER_DISPLAY_NAME)
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_NAME,
                           &info.display_name, -1);
    if (info.contains & GTK_FILE_FILTER_MIME_TYPE)
        gtk_tree_model_get(model, iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_MIME_TYPE,
                           &info.mime_type, -1);

/*  g_print("%d\t%s\t%s\t%s\t%s\n",
    info.contains, info.filename, info.uri,
    info.display_name, info.mime_type);*/

    result = gtk_file_filter_filter(priv->filter, &info);

    g_free((char *) info.filename);     /* Casting away const is safe in
                                           this case */
    g_free((char *) info.uri);
    g_free((char *) info.display_name);
    g_free((char *) info.mime_type);

    return result;
}

static gint
sort_function (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
               HildonFileSelectionSortKey key, GtkSortType order)
{
    gint value, weight_a, weight_b, diff;
    char *mime_a, *mime_b;

    gtk_tree_model_get(model, a, HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_WEIGHT,
                       &weight_a, -1);
    gtk_tree_model_get(model, b, HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_WEIGHT,
                       &weight_b, -1);

    /* If the items are in different sorting groups, we can determine
       the order directly by checking the weights. */
    if ((diff = weight_a - weight_b) != 0)
    {
        /* If either of the weights is negative, we need to preserve the
           order independenty of ascending/descending mode */
        if ((weight_a < 0 || weight_b < 0) && order == GTK_SORT_DESCENDING)
          return -diff;

        return diff;
    }

    /* In case of fodlers sort only by name */
    if (weight_a < 0) key = HILDON_FILE_SELECTION_SORT_NAME;

    if (key == HILDON_FILE_SELECTION_SORT_MODIFIED) {
        GtkFileTime time_a, time_b;
		gint retval;

        gtk_tree_model_get(model, a,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_TIME,
                           &time_a, -1);
        gtk_tree_model_get(model, b,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_TIME,
                           &time_b, -1);

        retval = time_a > time_b ? 1 : (time_a == time_b ? 0 : -1);
        if (weight_a < 0 && order == GTK_SORT_ASCENDING) {
	  retval = -retval;
	}
	if (retval != 0) return retval;
	else key = HILDON_FILE_SELECTION_SORT_NAME;
    }

    if (key == HILDON_FILE_SELECTION_SORT_SIZE) {
        gint64 size_a, size_b;
		gint retval;

        gtk_tree_model_get(model, a,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_SIZE,
                           &size_a, -1);
        gtk_tree_model_get(model, b,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_SIZE,
                           &size_b, -1);

        retval = size_a > size_b ? 1 : (size_a == size_b ? 0 : -1);
        if (weight_a < 0 && order == GTK_SORT_ASCENDING)
	  retval = -retval;

	if (retval != 0) return retval;
	else key = HILDON_FILE_SELECTION_SORT_NAME;
    }

    /* Sort by name. This allways applies for directories and also for
       files when name sorting is selected */
    if (key == HILDON_FILE_SELECTION_SORT_NAME) {
        gchar *title_a, *title_b;
        gint value;

        gtk_tree_model_get(model, a,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_KEY,
                           &title_a, -1);
        gtk_tree_model_get(model, b,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_KEY,
                           &title_b, -1);

        value = strcmp(title_a, title_b);

        /* Directories are always sorted alphabetically, so we
            have to reverse order in descending mode. Actually
            we should do a separate sort model for navigation pane,
            but that would be a much bigger change. Temporarily we'll
            do this hack. */
        if (weight_a < 0 && order == GTK_SORT_DESCENDING)
          value = -value;

        g_free(title_a);
        g_free(title_b);
        return value;
    }

    /* Note! Actually we should sort by extension, not by MIME type.
       Getting extension is also related to other problem */
    gtk_tree_model_get(model, a, HILDON_FILE_SYSTEM_MODEL_COLUMN_MIME_TYPE,
                       &mime_a, -1);
    gtk_tree_model_get(model, b, HILDON_FILE_SYSTEM_MODEL_COLUMN_MIME_TYPE,
                       &mime_b, -1);

    value = strcmp(mime_a, mime_b);

    g_free(mime_a);
    g_free(mime_b);
    
    if (value != 0)
      return value;
    else 
      return sort_function (model, a, b, 
			    HILDON_FILE_SELECTION_SORT_NAME, order);
}

static gint
content_pane_sort_function (GtkTreeModel *model,
                            GtkTreeIter *a, GtkTreeIter *b,
                            gpointer data)
{
    GtkSortType order;
    gint key;

    gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE
                                         (data),
                                         &key, &order);


    if (key == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID)
      {
        key = HILDON_FILE_SELECTION_SORT_NAME;
        order = GTK_SORT_ASCENDING;
      }

    return sort_function (model, a, b, key, order);
}

static gint
navigation_pane_sort_function (GtkTreeModel *model,
                               GtkTreeIter *a, GtkTreeIter *b,
                               gpointer data)
{

  return sort_function (model, a, b,
                        HILDON_FILE_SELECTION_SORT_NAME,
                        GTK_SORT_ASCENDING);
}

static void
thumbnail_cell_data_func(GtkTreeViewColumn *col,
			 GtkCellRenderer *renderer,
			 GtkTreeModel *model,
			 GtkTreeIter *iter,
			 HildonFileSelection *selection)
{
  gboolean sensitive;

  /* As the sapwood engine doesn't support insensitive cells which have Pango
     attributes, we set the attributes only here.  This is not even a bit more
     expensive than having it set via gtk_tree_view_column_add_attribute(). */
  g_object_get(renderer, "sensitive", &sensitive, NULL);
  if (sensitive)
    {
      PangoAttrList *display_attrs;

      gtk_tree_model_get(model, iter,
			 PRIV_COLUMN_DISPLAY_ATTRS, &display_attrs,
			 -1);
      g_object_set(renderer, "attributes", display_attrs, NULL);
      pango_attr_list_unref(display_attrs);
    }
  else
    {
      g_object_set(renderer, "attributes", NULL, NULL);
    }
}


static void hildon_file_selection_set_property(GObject * object,
                                               guint property_id,
                                               const GValue * value,
                                               GParamSpec * pspec)
{
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(object)->priv;

    switch (property_id) {
    case PROP_MODEL:
        g_assert(priv->main_model == NULL);     /* We come here exactly
                                                   once */
        priv->main_model = GTK_TREE_MODEL(g_value_get_object(value));
        break;
    case PROP_DND:
        priv->drag_enabled = g_value_get_boolean(value);
        break;
    case PROP_LOCAL_ONLY:
    {
        gboolean new_state = g_value_get_boolean(value);

        if (new_state != priv->local_only) {
            priv->local_only = new_state;
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                               (priv->dir_filter));
            if (priv->view_filter) {
                gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                               (priv->view_filter));
                hildon_file_selection_inspect_view(priv);
            }
        }
        break;
    }
    case PROP_SHOW_HIDDEN:
    {
        gboolean new_state = g_value_get_boolean(value);

        if (new_state != priv->show_hidden) {
            priv->show_hidden = new_state;
            gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                               (priv->dir_filter));
            if (priv->view_filter) {
                gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                               (priv->view_filter));
                hildon_file_selection_inspect_view(priv);
            }
        }
        break;
    }
    case PROP_EMPTY_TEXT:
        gtk_label_set_text(GTK_LABEL(priv->view[2]),
                           g_value_get_string(value));
        break;
    case PROP_VISIBLE_COLUMNS:
        priv->visible_columns = g_value_get_int(value);
        break;
    case PROP_SAFE_FOLDER:
        gtk_file_path_free(priv->safe_folder);  /* No need to check NULL */
        priv->safe_folder = g_value_get_boxed(value);
        break;
    case PROP_ACTIVE_PANE:
        activate_view(g_value_get_int(value) == HILDON_FILE_SELECTION_PANE_NAVIGATION ?
          priv->dir_tree : get_current_view(priv));
        break;
    case PROP_SHOW_UPNP:
    {
        gboolean new_state = g_value_get_boolean(value);

        if (new_state != priv->show_upnp) {
            priv->show_upnp = new_state;
            if (priv->dir_filter) {
                    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                               (priv->dir_filter));
            }
        }
        break;
    }
    case PROP_PANE_POSITION:
      gtk_paned_set_position (GTK_PANED (priv->hpaned), 
			      g_value_get_int (value));
      priv->pane_pos = g_value_get_int (value);
      break;
    case PROP_DRAGGING:
      break;
    case PROP_SHOW_FILES:
    {
	gboolean new_state = g_value_get_boolean (value);

	if (new_state  != priv->show_files) {
		priv->show_files = new_state;
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
						  (priv->dir_filter));
		if (priv->view_filter) {
			gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
							(priv->view_filter));
			hildon_file_selection_inspect_view(priv);
		}
	}
      	break;
    }
    case PROP_EDIT_MODE:
      priv->edit_mode = g_value_get_boolean(value);
      break;
    case PROP_NAVI_PANE_HIDDEN:
      priv->hide_navi = g_value_get_boolean(value);
      break;
    case PROP_SHOW_FOLDERS:
      priv->show_folders = g_value_get_boolean(value);
      break;
    case PROP_SHOW_READONLY:
      priv->show_readonly = g_value_get_boolean(value);
      break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void hildon_file_selection_get_property(GObject * object,
                                               guint property_id,
                                               GValue * value,
                                               GParamSpec * pspec)
{
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(object)->priv;

    switch (property_id) {
    case PROP_MODEL:
        g_value_set_object(value, priv->main_model);
        break;
    case PROP_DND:
        g_value_set_boolean(value, priv->drag_enabled);
        break;
    case PROP_LOCAL_ONLY:
        g_value_set_boolean(value, priv->local_only);
        break;
    case PROP_SHOW_HIDDEN:
        g_value_set_boolean(value, priv->show_hidden);
        break;
    case PROP_EMPTY_TEXT:
        g_value_set_string(value,
                           gtk_label_get_text(GTK_LABEL(priv->view[2])));
        break;
    case PROP_VISIBLE_COLUMNS:
        g_value_set_int(value, (gint) priv->visible_columns);
        break;
    case PROP_SAFE_FOLDER:
        g_value_set_boxed(value, priv->safe_folder);
        break;
    case PROP_ACTIVE_PANE:
        g_value_set_int(value, (gint) priv->content_pane_last_used);
        break;
    case PROP_SHOW_UPNP:
        g_value_set_boolean(value, priv->show_upnp);
        break;
    case PROP_PANE_POSITION:
        g_value_set_int (value, 
			 gtk_paned_get_position (GTK_PANED (priv->hpaned)));
	break;
    case PROP_DRAGGING:
	g_value_set_boolean(value, priv->currently_dragging);
	break;
    case PROP_SHOW_FILES:
        g_value_set_boolean(value, priv->show_files);
        break;
    case PROP_EDIT_MODE:
        g_value_set_boolean(value, priv->edit_mode);
        break;
    case PROP_NAVI_PANE_HIDDEN:
        g_value_set_boolean(value, priv->hide_navi);
        break;
    case PROP_SHOW_FOLDERS:
        g_value_set_boolean(value, priv->show_folders);
        break;
    case PROP_SHOW_READONLY:
        g_value_set_boolean(value, priv->show_readonly);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void hildon_file_selection_map(GtkWidget *widget)
{
  GtkWidget *view;
  HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(widget)->priv;

  GTK_WIDGET_CLASS(hildon_file_selection_parent_class)->map(widget);
  view = get_current_view(priv);

  activate_view(priv->dir_tree);

  if (hildon_file_selection_content_pane_visible(priv))
  {
    if (GTK_WIDGET_CAN_FOCUS(view))
    {
      expand_cursor_row(GTK_TREE_VIEW(priv->dir_tree));
      activate_view(view);
    }
    else
      priv->force_content_pane = TRUE;
  }
}

static void hildon_file_selection_class_init(HildonFileSelectionClass *
                                             klass)
{
    GParamSpec *pspec;
    GObjectClass *object;
    GtkWidgetClass *widget;

    hildon_file_selection_parent_class = g_type_class_peek_parent(klass);
    g_type_class_add_private(klass, sizeof(HildonFileSelectionPrivate));
    object = G_OBJECT_CLASS(klass);
    widget = GTK_WIDGET_CLASS(klass);

    object->constructor = hildon_file_selection_constructor;
    object->finalize = hildon_file_selection_finalize;
    object->set_property = hildon_file_selection_set_property;
    object->get_property = hildon_file_selection_get_property;
    widget->size_request = hildon_file_selection_size_request;
    widget->size_allocate = hildon_file_selection_size_allocate;
    widget->map = hildon_file_selection_map;

    /* Don't do actual show all, that would mess things up */
    widget->show_all = gtk_widget_show;
    GTK_CONTAINER_CLASS(klass)->forall = hildon_file_selection_forall;
    klass->location_insensitive = hildon_file_selection_real_row_insensitive;

  /**
   * HildonFileSelection::current-folder-changed:
   * @self: a #HildonFileSelection widget
   *
   * ::current-folder-changed signal is emitted when current folder
   * changes in navigation pane.
   * Use #hildon_file_selection_get_current_folder to receive folder path.
   */
    signals[CURRENT_FOLDER_CHANGED] =
        g_signal_new("current-folder-changed", HILDON_TYPE_FILE_SELECTION,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
                                     current_folder_changed), NULL, NULL,
                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * HildonFileSelection::file-activated:
   * @self: a #HildonFileSelection widget
   *
   * ::file-activated signal is emitted when user selects an item from
   * content pane.
   */
    signals[FILE_ACTIVATED] =
        g_signal_new("file-activated", HILDON_TYPE_FILE_SELECTION,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
				     file_activated), NULL, NULL,
                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * HildonFileSelection::folder-activated:
   * @self: a #HildonFileSelection widget
   *
   * ::folder-activated signal is emitted when user selects an folder item from
   * content pane.
   */
    signals[FOLDER_ACTIVATED] =
        g_signal_new("folder-activated", HILDON_TYPE_FILE_SELECTION,
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * HildonFileSelection::selection-changed:
   * @self: a #HildonFileSelection widget
   *
   * This signal is emitted when selection changes on content pane. Use
   * #hildon_file_selection_get_selected_paths to receive paths.
   */
    signals[SELECTION_CHANGED] =
        g_signal_new("selection-changed", HILDON_TYPE_FILE_SELECTION,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
                                     selection_changed), NULL, NULL,
                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * HildonFileSelection::navigation-pane-context-menu:
   * @self: a #HildonFileSelection widget
   *
   * This signal is emitted when a context menu (if any) should be
   * displayed on navigation pane.
   */
    signals[NAVIGATION_PANE_CONTEXT_MENU] =
        g_signal_new("navigation-pane-context-menu",
                     HILDON_TYPE_FILE_SELECTION, G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
                                     navigation_pane_context_menu), NULL,
                     NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * HildonFileSelection::content-pane-context-menu:
   * @self: a #HildonFileSelection widget
   *
   * This signal is emitted when a context menu (if any) should be
   * displayed on content pane.
   */
    signals[CONTENT_PANE_CONTEXT_MENU] =
        g_signal_new("content-pane-context-menu",
                     HILDON_TYPE_FILE_SELECTION, G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
                                     content_pane_context_menu), NULL,
                     NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * HildonFileSelection::uris-dropped:
   * @self: a #HildonFileSelection widget
   * @destination: Destination URI (this is quaranteed to be a directory).
   * @sources: List of URIs that should be transferred to destination.
   *
   * This signal is emitted when user drags one or more items to some
   * folder. This signal is emitted only if drag'n'drop is enabled
   * during widget creation.
   */
    signals[URIS_DROPPED] =
        g_signal_new("uris-dropped", HILDON_TYPE_FILE_SELECTION,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
                                     uris_dropped), NULL, NULL,
                     gtk_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
                     G_TYPE_STRING, G_TYPE_POINTER);
    /* Not portable to other languages */

  /**
   * HildonFileSelection::location-insensitive:
   * @self: a #HildonFileSelection widget
   * @location: insensitive location.
   *
   * This signal is emitted when user tries to interact with dimmed
   * location.
   */
    signals[LOCATION_INSENSITIVE] =
        g_signal_new("location-insensitive", HILDON_TYPE_FILE_SELECTION,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSelectionClass,
                                     location_insensitive), NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE, 1,
                     GTK_TYPE_TREE_ITER);

    g_object_class_install_property(object, PROP_MODEL,
        g_param_spec_object("model",
                            "Data moodel",
                            "Set the HildonFileSystemModel to display",
                            HILDON_TYPE_FILE_SYSTEM_MODEL,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    pspec = g_param_spec_boolean("drag-enabled", "Drag\'n\'' drop enabled",
                                 "Ask file selection to enable "
                                 "drag\'n\'drop support",
                                 FALSE,
                                 G_PARAM_CONSTRUCT_ONLY |
                                 G_PARAM_READWRITE);
    g_object_class_install_property(object, PROP_DND, pspec);

    pspec = g_param_spec_string("empty-text", "Empty text",
                                "String to use when selected folder "
                                "is empty",
                                NULL, G_PARAM_READWRITE);
    g_object_class_install_property(object, PROP_EMPTY_TEXT, pspec);

    g_object_class_install_property(object, PROP_VISIBLE_COLUMNS,
      g_param_spec_int("visible-columns", "Visible columns",
                                     "Defines the columns shown on content pane",
                                     0, HILDON_FILE_SELECTION_SHOW_ALL,
                                     HILDON_FILE_SELECTION_SHOW_ALL,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object, PROP_SAFE_FOLDER,
      g_param_spec_boxed("safe-folder", "Safe folder",
                                     "Safe folder to use as fallback in various operations",
                                     GTK_TYPE_FILE_PATH, G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_ACTIVE_PANE,
      g_param_spec_int("active-pane", "Active pane",
                                     "Which pane (navigation or content) has (or last time had) active focus.\n"
                                     "This in formation can be used to find which pane should be used to query selection.",
                                     HILDON_FILE_SELECTION_PANE_NAVIGATION,
                                     HILDON_FILE_SELECTION_PANE_CONTENT,
                                     HILDON_FILE_SELECTION_PANE_NAVIGATION, G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_SHOW_HIDDEN,
      g_param_spec_boolean("show-hidden", "Show hidden",
                           "Show hidden files in file selector widgets",
                           FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_LOCAL_ONLY,
      g_param_spec_boolean("local-only", "Local only",
                           "Whether or not all the displayed items should "
                           "have a local file path",
                           FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_SHOW_UPNP,
      g_param_spec_boolean("show-upnp", "Show UPNP",
                           "Whether or not to show UPNP devices",
                           FALSE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_PANE_POSITION,
      g_param_spec_int ("pane-position", "Pane position",
                        "The position of the division between "
			"the both panes",
                        0, INT_MAX, 250,
			G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_DRAGGING,
      g_param_spec_boolean("currently-dragging", "Currently dragging",
                           "Whether or not dragging is ongoing",
                           FALSE, G_PARAM_READABLE));

    g_object_class_install_property(object, PROP_SHOW_FILES,
      g_param_spec_boolean("show-files", "show the files in the content pane",
                           "show the files in the content pane "
                           "if this property is TRUE (TRUE by default)",
                           TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property(object, PROP_EDIT_MODE,
      g_param_spec_boolean("edit-mode", "Edit Mode",
                           "create GtkTreeView in Edit Mode "
                           "if this property is TRUE (FALSE by default)",
                           FALSE, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_NAVI_PANE_HIDDEN,
      g_param_spec_boolean("hide-navi", "Navigation pane hidden",
                           "Hide the left navigation pane "
                           "if this property is TRUE (FALSE by default)",
                           FALSE, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_SHOW_FOLDERS,
      g_param_spec_boolean("show-folders", "show fodlers in the content pane",
                           "show folders in the content pane "
                           "if this property is TRUE (TRUE by default)",
                           TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property(object, PROP_SHOW_READONLY,
      g_param_spec_boolean("show-readonly", "show readonly items in the content pane",
                           "show readonly files and folders in the content pane "
                           "if this property is TRUE (TRUE by default)",
                           TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static gboolean
content_pane_selection_changed_idle(gpointer data)
{
    GDK_THREADS_ENTER();

    g_assert(HILDON_IS_FILE_SELECTION(data));
    g_signal_emit(data, signals[SELECTION_CHANGED], 0);
    HILDON_FILE_SELECTION(data)->priv->content_pane_changed_id = 0;

    GDK_THREADS_LEAVE();

    return FALSE;
}

/* We have to send this signal in idle, because caches contain only
   invalid iterators if this signal is received when we are deleting
   something. */
static void
hildon_file_selection_content_pane_selection_changed (GtkTreeSelection *sel,
                                                      gpointer data)
{
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(data)->priv;

    gtk_tree_row_reference_free(priv->current_row);
    priv->current_row = NULL;

    if (priv->content_pane_changed_id == 0)
    {
      priv->content_pane_changed_id =
        g_idle_add(content_pane_selection_changed_idle, data);
    }
}

/* Checks whether the given path matches current content pane path.
   We have to use view_filter rather than current_folder, since these
   not not neccesarily in sync when this is called */
static gboolean
hildon_file_selection_matches_current_view(HildonFileSelectionPrivate *
                                           priv, GtkTreePath * path)
{
    if (GTK_IS_TREE_MODEL_FILTER(priv->view_filter)) {
        GtkTreePath *current_path;
        gint result = 1;

        g_object_get(priv->view_filter, "virtual-root", &current_path,
                     NULL);

        g_assert(current_path); /* Content pane should always have root
                                   (no local device level allowed) */
        if (!gtk_tree_path_compare(path, current_path) && gtk_tree_row_reference_valid(priv->current_folder)) 
                /* after deleting a folder the new and current (deleted) paths are same 
                   but current_folder is invalid for the current (deleted) row */
                result = 0;
        gtk_tree_path_free(current_path);

        return (result == 0);
    }

    return FALSE;
}

static gboolean hildon_file_selection_check_load_banner(gpointer data)
{
    HildonFileSelection *self;
    GtkTreeIter iter;
    gboolean ready;

    self = HILDON_FILE_SELECTION(data);
    self->priv->update_banner = FALSE;

    /* We only show "updating" banner if
       the filetree itself is visible */
    if (GTK_WIDGET_VISIBLE(data) &&
        hildon_file_selection_get_current_folder_iter(self, &iter))
    {
      gtk_tree_model_get(self->priv->main_model, &iter,
        HILDON_FILE_SYSTEM_MODEL_COLUMN_LOAD_READY, &ready, -1);

      if (!ready)
      {
        GtkWidget *window =
            gtk_widget_get_ancestor(GTK_WIDGET(data), GTK_TYPE_WINDOW);

        if (window) {
            g_debug("Showing update banner");
	    hildon_gtk_window_set_progress_indicator(GTK_WINDOW(window), 1);
	    self->priv->update_banner = TRUE;

        }
      }
    }

    self->priv->banner_timeout_id = 0;
    return FALSE;       /* Don't call again */
}

static gboolean load_banner_timeout(gpointer data)
{
    HildonFileSelection *hfs = data;
    HildonFileSelectionPrivate *priv;

    priv = hfs->priv;

    if (priv->update_banner) {
        GtkWidget *window =
	    gtk_widget_get_ancestor(GTK_WIDGET(data), GTK_TYPE_WINDOW);

        if (window) {
	    hildon_gtk_window_set_progress_indicator(GTK_WINDOW(window), 0);
	}
	priv->update_banner = FALSE;
    }

    if (priv->banner_timeout_id != 0)
    {
        /* should not ever happen? */
        g_source_remove(priv->banner_timeout_id);
        priv->banner_timeout_id = 0;
    }

    priv->banner_close_timeout_id = 0;
    return FALSE;
}

static void hildon_file_selection_close_load_banner(HildonFileSelection *
                                                    self)
{
    HildonFileSelectionPrivate *priv = self->priv;
    GtkWidget *view;

    if (priv->update_banner)
    {
        GtkWidget *window =
	    gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_WINDOW);

        if (window) {
	    hildon_gtk_window_set_progress_indicator(GTK_WINDOW(window), 0);
	}
	priv->update_banner = FALSE;
    }

    if (priv->banner_timeout_id != 0)
    {
        /* cancel the banner */
        g_source_remove(priv->banner_timeout_id);
        priv->banner_timeout_id = 0;
    }

    if (priv->banner_close_timeout_id != 0)
    {
        /* cancel the banner timeout */
        g_source_remove(priv->banner_close_timeout_id);
        priv->banner_close_timeout_id = 0;
    }

    /* Load can finish with no contents */
    hildon_file_selection_inspect_view(priv);

    /* There should always be a valid cursor in a TreeView.  If
       there still isn't one after finishing the loading, we set it
       to the first row.
    */

    view = get_current_view(priv);

    if (view && GTK_IS_TREE_VIEW (view) && !priv->edit_mode)
      {
        GtkTreePath *cursor_path = NULL;

        gtk_tree_view_get_cursor (GTK_TREE_VIEW (view), &cursor_path, NULL);
        if (cursor_path == NULL || priv->user_touched == FALSE)
          {
            GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
            if (gtk_tree_model_iter_n_children (model, NULL) > 0)
              {
                GtkTreePath *path = gtk_tree_path_new_first ();
                gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
                                          NULL, FALSE);
                gtk_tree_path_free (path);
              }
          }
        else
          {
            /* XXX - when the selection mode is MULTIPLE, it can
                     happen that we have a cursor, but the selection
                     is empty.  Thus, we make sure that the cursor row
                     is in the selection.
            */
            gtk_tree_selection_select_path
              (gtk_tree_view_get_selection (GTK_TREE_VIEW (view)),
               cursor_path);

          }

        if (cursor_path)
        {
            gtk_tree_path_free (cursor_path);
            cursor_path = NULL;
        }
      }

    priv->force_content_pane = FALSE;
}

static void
hildon_file_selection_check_close_load_banner(GtkTreeModel *model,
  GtkTreeIter *iter, gpointer data)
{
  GtkTreeIter current_iter;
  HildonFileSelection *self = HILDON_FILE_SELECTION(data);

  if (hildon_file_selection_get_current_folder_iter(self, &current_iter) &&
      iter->user_data == current_iter.user_data)
    hildon_file_selection_close_load_banner(self);
}

static void
get_safe_folder_tree_iter(HildonFileSelectionPrivate *priv, GtkTreeIter *iter)
{
  if (priv->safe_folder == NULL ||
      !hildon_file_system_model_search_path(
        HILDON_FILE_SYSTEM_MODEL(priv->main_model),
        priv->safe_folder, iter, NULL, TRUE))
  { /* We use first node (=device) as fallback */
    gboolean success;
    success = gtk_tree_model_get_iter_first(priv->main_model, iter);
    g_assert(success);  /* This really should work */
    g_debug("No safe folder defined => Using local device root");
  }
}

static gboolean delayed_select_idle(gpointer data)
{
  HildonFileSelection *self;
  HildonFileSelectionPrivate *priv;
  GtkTreeIter main_iter, sort_iter;
  GtkTreePath *sort_path = NULL;
  gboolean found = FALSE;

  GDK_THREADS_ENTER();

  self = HILDON_FILE_SELECTION(data);
  priv = self->priv;

  if (priv->current_folder)
    sort_path = gtk_tree_row_reference_get_path(priv->current_folder);

  if (sort_path)
  {
    /* We have to use sort_path --- not main_path, because there is not
       neccesarily corresponding main node present. */

    /* First try this location */
    if (gtk_tree_model_get_iter(priv->dir_sort, &sort_iter, sort_path))
      found = TRUE;
    else
    {
      /* Then try parent */
      gtk_tree_path_up(sort_path);

      if (gtk_tree_path_get_depth(sort_path) >= 1 &&
          gtk_tree_model_get_iter(priv->dir_sort, &sort_iter, sort_path))
        found = TRUE;
    }

    gtk_tree_path_free(sort_path);
  }

  if (found)
  {
    gtk_tree_model_sort_convert_iter_to_child_iter(
      GTK_TREE_MODEL_SORT(priv->dir_sort), &main_iter, &sort_iter);
    /* It's possible that we are trying to select dimmed location.
       This happens, for example, if root folder of mmc was selected
       in a save dialog and mmc is removed. */
    gtk_tree_model_get(priv->main_model, &main_iter,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE, &found, -1);
  }
  if (!found)
    get_safe_folder_tree_iter(priv, &main_iter);

  /* Ok, now main_iter points to our target location */

  hildon_file_selection_set_current_folder_iter(self, &main_iter);

  /* Keep the focus in right side if possible */
  activate_view(priv->cur_view == 2 ?
                priv->dir_tree : get_current_view(priv));

  /* We have to return TRUE, because cancel removes the handler now */
  hildon_file_selection_cancel_delayed_select(priv);

  GDK_THREADS_LEAVE();

  return TRUE;
}

/* Takes ownership of the given reference */
static void
hildon_file_selection_delayed_select_reference(HildonFileSelection *self,
  GtkTreeRowReference *ref)
{
  if (!ref) return;

  self->priv->user_touched = FALSE;

  if (self->priv->delayed_select_id == 0)
    self->priv->delayed_select_id = g_idle_add(delayed_select_idle, self);

  if (ref != self->priv->current_folder)
  {
    if (self->priv->current_folder)
      gtk_tree_row_reference_free(self->priv->current_folder);

    self->priv->current_folder = ref;
  }
}

static void
hildon_file_selection_delayed_select_path(HildonFileSelection *self,
  GtkTreePath *sort_model_path)
{
  hildon_file_selection_delayed_select_reference(self,
    gtk_tree_row_reference_new(self->priv->dir_sort, sort_model_path));
}

static void hildon_file_selection_row_insensitive(GtkTreeView *tree,
  GtkTreePath *path, gpointer data)
{
  GtkTreeIter iter;

  if (view_path_to_main_iter(gtk_tree_view_get_model(tree), &iter, path))
    g_signal_emit(data, signals[LOCATION_INSENSITIVE], 0, &iter);
}

/* By default, we handle dimmed MMC and gateway. If applications do not want
   this to happen, signal emission can be stopped */
static void hildon_file_selection_real_row_insensitive(HildonFileSelection *self,
  GtkTreeIter *location)
{
#if 0
// no row insensitive banner in Hildon 2.2 style
  gchar *message;

  g_assert(HILDON_IS_FILE_SELECTION(self));

  gtk_tree_model_get(self->priv->main_model, location,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_UNAVAILABLE_REASON, &message, -1);
  hildon_banner_show_information(GTK_WIDGET(self), NULL, message);

  g_free(message);
#endif
}

static void hildon_file_selection_selection_changed(GtkTreeSelection *
                                                    selection,
                                                    gpointer data)
{
    HildonFileSelection *self;
    HildonFileSelectionPrivate *priv;
    GtkTreeModel *model;        /* Navigation pane filter model */
    GtkTreeIter iter;

    g_assert(HILDON_IS_FILE_SELECTION(data));
    self = HILDON_FILE_SELECTION(data);
    priv = self->priv;
    priv->force_content_pane = FALSE;
    priv->user_touched = FALSE;
    priv->user_scrolled = FALSE;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

        GtkTreeIter sort_iter, main_iter;
        GtkTreePath *sort_path, *dir_sort_path;

        g_assert(model == priv->dir_filter
          && GTK_IS_TREE_MODEL_FILTER(model));

        g_free (priv->cursor_goal_uri);
        priv->cursor_goal_uri = NULL;

        gtk_tree_model_filter_convert_iter_to_child_iter
            (GTK_TREE_MODEL_FILTER(priv->dir_filter), &sort_iter, &iter);
        gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT
                                                       (priv->dir_sort),
                                                       &main_iter,
                                                       &sort_iter);
            sort_path =
                gtk_tree_model_get_path(priv->main_model, &main_iter);
            dir_sort_path = gtk_tree_model_get_path(priv->dir_sort, &sort_iter);

            /* Check that we have actually changed the folder */
            if (hildon_file_selection_matches_current_view(priv, sort_path))
            {
                gtk_tree_path_free(sort_path);
                gtk_tree_path_free(dir_sort_path);
                g_debug("Current folder re-selected => Asked to reload (if on gateway)");
                _hildon_file_system_model_queue_reload(
                    HILDON_FILE_SYSTEM_MODEL(priv->main_model),
                    &main_iter, TRUE);

                return;
            }
            else
            {
               _hildon_file_system_model_prioritize_folder
                 (HILDON_FILE_SYSTEM_MODEL(priv->main_model), &main_iter);

               _hildon_file_system_model_queue_reload(
                    HILDON_FILE_SYSTEM_MODEL(priv->main_model),
                    &main_iter, FALSE);
            }

            gtk_tree_row_reference_free (priv->current_folder);
            priv->current_folder = gtk_tree_row_reference_new(priv->dir_sort, dir_sort_path);
            gtk_tree_path_free(dir_sort_path);

        if (hildon_file_selection_content_pane_visible(priv)) {
	    gint sort_column = GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID;
	    GtkSortType sort_order = GTK_SORT_ASCENDING;

            if (priv->sort_model)
              {
		gtk_tree_sortable_get_sort_column_id
		  (GTK_TREE_SORTABLE (priv->sort_model),
		   &sort_column,
		   &sort_order);
                hildon_file_selection_disable_cursor_magic (self,
                                                            priv->sort_model);
                gtk_tree_row_reference_free(priv->current_row);
                priv->current_row = NULL;
                g_object_unref(priv->sort_model);
              }
            if (priv->view_filter)
                g_object_unref(priv->view_filter);

            priv->view_filter =
                gtk_tree_model_filter_new(priv->main_model, sort_path);

            priv->sort_model = hildon_file_selection_create_sort_model
              (self, content_pane_sort_function, priv->view_filter);
	    gtk_tree_sortable_set_sort_column_id 
	      (GTK_TREE_SORTABLE (priv->sort_model),
	       sort_column,
	       sort_order);

            if (!priv->edit_mode)
              hildon_file_selection_enable_cursor_magic (self, priv->sort_model);

            g_assert(priv->view_filter != NULL);

            /* Live search */
            g_assert (priv->live_search);
            hildon_live_search_set_filter (priv->live_search,
                GTK_TREE_MODEL_FILTER (priv->view_filter));
            hildon_live_search_set_text (priv->live_search, "");
            gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER
                                                   (priv->view_filter),
                                                   filter_func, priv,
                                                   NULL);
            gtk_tree_model_filter_refilter (
                GTK_TREE_MODEL_FILTER (priv->view_filter));
            rebind_models(priv);
            g_signal_connect_data(priv->view_filter, "row-has-child-toggled",
                G_CALLBACK
                (hildon_file_selection_inspect_view), priv, NULL,
                G_CONNECT_SWAPPED | G_CONNECT_AFTER);
            hildon_file_selection_inspect_view(priv);
	    g_signal_emit(self, signals[FOLDER_ACTIVATED], 0);

            /* These DON'T affect colums that have AUTOSIZE as sizing type
               ;) */
            gtk_tree_view_columns_autosize(GTK_TREE_VIEW(priv->view[0]));
           /*  gtk_tree_view_columns_autosize(GTK_TREE_VIEW(priv->view[1]));*/

            /* All of the refresh problems seem to happen only with
               matchbox. Some only if we have not enough space, some only
               if we run with the theme */

            /* In problem cases we run just size request, not
               size allocate. With fvwm we run size allocate in all cases.
             */
            /* When there is not set_size_request, or requested -1 then
               this fails. Any positive value works fine */

            /* This was finally a clear Matchbox bug. This was caused by
               unreceived configure event that caused screen freeze. */
        }

        /* It can happen that we still have pending selection. */
        hildon_file_selection_cancel_delayed_select(priv);

        g_signal_emit(data, signals[CURRENT_FOLDER_CHANGED], 0);

        hildon_file_selection_close_load_banner(HILDON_FILE_SELECTION(data));
        if (!priv->update_banner && priv->banner_timeout_id == 0)
        {
            priv->banner_timeout_id =
                g_timeout_add(500, hildon_file_selection_check_load_banner,
                              data);
            priv->banner_close_timeout_id = g_timeout_add(30000,
                                                load_banner_timeout, data);
        }

        /* This can invalidate non-persisting iterators, so we need to do
           mounting as last action. */

        _hildon_file_system_model_mount_device_iter
          (HILDON_FILE_SYSTEM_MODEL(priv->main_model), &main_iter);

#if 0
        /* XXX - mount_device_iter is asyncronous and doesn't return
                 error messages.  We need to find another way to show
                 that message.
        */

        if (!success && error)
        {
            char *message;

            gtk_tree_model_get(priv->main_model, &main_iter,
              HILDON_FILE_SYSTEM_MODEL_COLUMN_FAILED_ACCESS_MESSAGE, &message,
              -1);

            if (message) {
                hildon_banner_show_information(GTK_WIDGET(self), NULL, message);
                g_free(message);
            }

            g_warning("Mount failed (error #%d): %s", error->code, error->message);
            g_error_free(error);
        }
#endif

        gtk_tree_path_free(sort_path);
    }
}

static void hildon_file_selection_row_activated(GtkTreeView * view,
                                                GtkTreePath * path,
                                                GtkTreeViewColumn * col,
                                                gpointer data)
{
	
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreePath *filter_path = NULL, *base_path = NULL, *dir_path = NULL;
    gboolean is_folder, is_available;

    if (HILDON_FILE_SELECTION(data)->priv->edit_mode)
        return;

    model = gtk_tree_view_get_model(view); /* Content pane filter model */

    gtk_tree_row_reference_free(HILDON_FILE_SELECTION(data)->priv->current_row);
    HILDON_FILE_SELECTION(data)->priv->current_row = gtk_tree_row_reference_new(model, path);

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER,
                           &is_folder,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE,
			   &is_available, -1);

        if (is_available) {
          if (is_folder) {
            filter_path = gtk_tree_model_sort_convert_path_to_child_path(
              GTK_TREE_MODEL_SORT(model), path);

            if (filter_path) {
              base_path = gtk_tree_model_filter_convert_path_to_child_path(
                GTK_TREE_MODEL_FILTER(
                  HILDON_FILE_SELECTION(data)->priv->view_filter),
                filter_path);
              gtk_tree_path_free(filter_path);
            }

            if (base_path) {
              dir_path = gtk_tree_model_sort_convert_child_path_to_path(
                GTK_TREE_MODEL_SORT(
                  HILDON_FILE_SELECTION(data)->priv->dir_sort),
                base_path);
              gtk_tree_path_free(base_path);
            }

            if (dir_path) {
              hildon_file_selection_delayed_select_path(
                HILDON_FILE_SELECTION(data), dir_path);
              gtk_tree_path_free(dir_path);
            }
	    g_signal_emit(data, signals[FOLDER_ACTIVATED], 0);
          }
          else { /* When we activate file, let's check if we need to reload */
            GtkTreeIter iter;
            HildonFileSelection *self = HILDON_FILE_SELECTION(data);

            if (hildon_file_selection_get_current_folder_iter(self, &iter))
            {
              _hildon_file_system_model_queue_reload(
                HILDON_FILE_SYSTEM_MODEL(self->priv->main_model),
                &iter, FALSE);
            }
	    g_signal_emit(data, signals[FILE_ACTIVATED], 0);
          }
        }
    }
}

static gboolean hildon_file_selection_user_moved(gpointer object)
{
    HildonFileSelection *self;

    self = HILDON_FILE_SELECTION(object);

    self->priv->user_touched = TRUE;
    self->priv->user_scrolled = FALSE;

    return FALSE;
}

static void hildon_file_selection_check_scroll(GtkWidget *widget, gboolean type, gpointer data)
{
  HildonFileSelectionPrivate *priv;
  GtkWidget *grab_widget;

  priv = HILDON_FILE_SELECTION(widget)->priv;
  grab_widget = gtk_grab_get_current();

  if (grab_widget && GTK_IS_SCROLLBAR(grab_widget) &&
      gtk_widget_is_ancestor(grab_widget, priv->view_selector))
  {
    g_debug("User scrolled the window, cancelling autoscrolling");
    priv->user_scrolled = TRUE;
  }
}

static void hildon_file_selection_keep_cursor_visible(HildonFileSelection *self)
{
    GtkWidget *view;

    view = get_current_view(self->priv);
    if (GTK_IS_TREE_VIEW(view) && !self->priv->user_scrolled)
      scroll_to_cursor(GTK_TREE_VIEW(view));
}

static void hildon_file_selection_modified(gpointer object, GtkTreePath *path)
{
    HildonFileSelection *self;
    GtkTreePath *current_folder;
    GtkTreeIter iter;

    /* Only detect changes inside current folder */
    self = HILDON_FILE_SELECTION(object);

    if (hildon_file_selection_get_current_folder_iter(self, &iter))
    {
      current_folder = gtk_tree_model_get_path(self->priv->main_model, &iter);
      if (current_folder)
      {
        if (gtk_tree_path_is_ancestor(current_folder, path) &&
            gtk_tree_path_get_depth(current_folder) == gtk_tree_path_get_depth(path) - 1)
          hildon_file_selection_keep_cursor_visible(self);
        gtk_tree_path_free(current_folder);
      }
    }
}

static void hildon_file_selection_navigation_pane_context(GtkWidget *
                                                          widget,
                                                          gpointer data)
{
    g_assert(HILDON_IS_FILE_SELECTION(data));
    g_signal_emit(data, signals[NAVIGATION_PANE_CONTEXT_MENU], 0);
}

static void hildon_file_selection_content_pane_context(GtkWidget * widget,
                                                       gpointer data)
{
    g_assert(HILDON_IS_FILE_SELECTION(data));
    g_signal_emit(data, signals[CONTENT_PANE_CONTEXT_MENU], 0);
}

static gboolean hildon_file_selection_on_content_pane_key(GtkWidget *
                                                          widget,
                                                          GdkEventKey *
                                                          event,
                                                          gpointer data)
{
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(data)->priv;

    if (!hildon_file_selection_content_pane_visible(priv))
        return FALSE;

    switch (event->keyval) {
    case GDK_KP_Left:
    case GDK_Left:
    case GDK_leftarrow:
        activate_view(priv->dir_tree);
        return TRUE;
    case GDK_KP_Right:
    case GDK_Right:
    case GDK_rightarrow:
        g_signal_emit_by_name(gtk_widget_get_ancestor
                              (widget, GTK_TYPE_WINDOW), "move-focus",
                              GTK_DIR_TAB_FORWARD);
        return TRUE;
    }

    return FALSE;
}

/* Implement moving from navigation pane to content pane when folder is
   already expanded and right key is pressed */
static gboolean hildon_file_selection_on_navigation_pane_key(GtkWidget *
                                                             widget,
                                                             GdkEventKey *
                                                             event,
                                                             gpointer data)
{
    GtkTreeView *tree;
    GtkWidget *content_view;
    GtkTreePath *path;
    gboolean expanded;
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(data)->priv;
    gboolean result = FALSE;

    content_view = get_current_view(priv);
    tree = GTK_TREE_VIEW(widget);
    gtk_tree_view_get_cursor(tree, &path, NULL);

    if (!path) return FALSE;

    expanded = gtk_tree_view_row_expanded(tree, path);

    switch (event->keyval) {
    case GDK_KP_Left:
    case GDK_Left:
    case GDK_leftarrow:
    {
        if (!expanded) {
            g_signal_emit_by_name(gtk_widget_get_ancestor
                                  (widget, GTK_TYPE_WINDOW), "move-focus",
                                  GTK_DIR_TAB_BACKWARD);
            result = TRUE;
        }
        break;
    }
    case GDK_KP_Right:
    case GDK_Right:
    case GDK_rightarrow:
    {
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_tree_view_get_model(tree);

        /* Well, we also have to check situation if line cannot be
           expanded at all. Also in this case we focus content pane */
        if (expanded
            || (gtk_tree_model_get_iter(model, &iter, path)
                && !gtk_tree_model_iter_has_child(model, &iter))) {

            if (hildon_file_selection_content_pane_visible(priv))
            {
                activate_view(content_view);
            }
            else
            {
            /* If there is no content pane, we focus the next widget */

            g_signal_emit_by_name(gtk_widget_get_ancestor
                                  (widget, GTK_TYPE_WINDOW), "move-focus",
                                  GTK_DIR_TAB_FORWARD);
            }
            result = TRUE;
        }

        break;
    }
    case GDK_KP_Enter:
    case GDK_ISO_Enter:
    case GDK_Return:
        if (expanded)
          gtk_tree_view_collapse_row(tree, path);
        else
          gtk_tree_view_expand_row(tree, path, FALSE);

        result = TRUE;
        break;
    }
    gtk_tree_path_free(path);

    return result;
}

static void
navigation_pane_focus(GObject *object, GParamSpec *pspec, gpointer data)
{
  if (GTK_WIDGET_HAS_FOCUS(object))
  {
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(data)->priv;

    if (priv->content_pane_last_used)
    {
      hildon_file_selection_clear_multi_selection( HILDON_FILE_SELECTION(data) );
      priv->content_pane_last_used = FALSE;
      scroll_to_cursor(GTK_TREE_VIEW(object));
      g_object_notify(data, "active-pane");
    }
  }
}

static void
content_pane_focus(GObject *object, GParamSpec *pspec, gpointer data)
{
  if (GTK_WIDGET_HAS_FOCUS(object))
  {
    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(data)->priv;

    if (!priv->content_pane_last_used)
    {
      priv->content_pane_last_used = TRUE;
      if (!priv->user_scrolled)
        scroll_to_cursor(GTK_TREE_VIEW(object));
      g_object_notify(data, "active-pane");
    }
  }
}

static gboolean
tap_and_hold_query (gpointer self, guint signal_id)
{
  /* Only start the animation when there are actually handlers for our
     "foo-context-menu" signal.
  */
  return !g_signal_has_handler_pending (self, signal_id, 0, FALSE);
}

static gboolean
content_pane_tap_and_hold_query (GtkWidget *widget,
                                 GdkEvent *event,
                                 gpointer self)
{
  return tap_and_hold_query (self, signals[CONTENT_PANE_CONTEXT_MENU]);
}

static gboolean
navigation_pane_tap_and_hold_query (GtkWidget *widget,
                                    GdkEvent *event,
                                    gpointer self)
{
  return tap_and_hold_query (self, signals[NAVIGATION_PANE_CONTEXT_MENU]);
}

/*
 * Used when CSM is launched - we need to know upon which row the tap-and-hold was applied
 */
static gboolean button_press_event_callback(GtkWidget *widget, GdkEventButton *event, gpointer data) {

    HildonFileSelectionPrivate *priv = HILDON_FILE_SELECTION(data)->priv;
    GtkTreePath *path;
    gint cell_x, cell_y;

    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, &cell_x, &cell_y)) {
        if (path) {
            gtk_tree_row_reference_free(priv->current_row);
            priv->current_row = gtk_tree_row_reference_new(priv->sort_model, path);
            gtk_tree_path_free(path);
        }
    }
    return FALSE; //continue propagating the event
}


static void hildon_file_selection_create_thumbnail_view(HildonFileSelection
                                                        * self)
{
    GtkTreeSelection *selection;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkTreeView *tree;

    //    self->priv->view[1] = gtk_tree_view_new();
    if (self->priv->edit_mode) {
        self->priv->view[1] = hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT);
        gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(self->priv->view[1])), GTK_SELECTION_MULTIPLE);
    } else {
        self->priv->view[1] = hildon_gtk_tree_view_new(HILDON_UI_MODE_NORMAL);
        gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(self->priv->view[1])), GTK_SELECTION_NONE);
    }
    
    tree = GTK_TREE_VIEW(self->priv->view[1]);

    gtk_tree_view_set_fixed_height_mode(tree, TRUE);
    gtk_tree_view_set_rules_hint(tree, TRUE);

    col = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col, THUMBNAIL_CELL_WIDTH);
    gtk_cell_renderer_set_fixed_size(renderer,
        THUMBNAIL_CELL_WIDTH, THUMBNAIL_CELL_HEIGHT);

    gtk_tree_view_append_column(tree, col);
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_add_attribute
        (col, renderer, "pixbuf",
         HILDON_FILE_SYSTEM_MODEL_COLUMN_THUMBNAIL);
    gtk_tree_view_column_add_attribute
        (col, renderer, "sensitive",
        HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_expand(col, TRUE);
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ypad", HILDON_MARGIN_DEFAULT,
                           "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_renderer_text_set_fixed_height_from_font(
        GTK_CELL_RENDERER_TEXT(renderer), 2);
    gtk_tree_view_append_column(tree, col);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,
				       "sensitive", HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE);
    gtk_tree_view_column_add_attribute(col, renderer,
				       "text", PRIV_COLUMN_DISPLAY_TEXT);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
					    (GtkTreeCellDataFunc)thumbnail_cell_data_func,
					    self, NULL);

    selection = gtk_tree_view_get_selection(tree);
    g_signal_connect_object
        (selection, "changed",
         G_CALLBACK(hildon_file_selection_content_pane_selection_changed),
         self, 0);
    g_signal_connect_object(tree, "key-press-event",
                     G_CALLBACK(hildon_file_selection_on_content_pane_key),
                     self, 0);

    gtk_widget_tap_and_hold_setup(GTK_WIDGET(tree), NULL, NULL,
                                  GTK_TAP_AND_HOLD_NONE | GTK_TAP_AND_HOLD_NO_INTERNALS);
    g_signal_connect_object (tree, "tap-and-hold-query",
                             G_CALLBACK (content_pane_tap_and_hold_query),
                             self, 0);
    g_signal_connect_object(tree, "tap-and-hold",
                     G_CALLBACK
                     (hildon_file_selection_content_pane_context), self, 0);

    g_signal_connect_object(tree, "notify::has-focus",
                     G_CALLBACK(content_pane_focus), self, 0);

    g_signal_connect_object(GTK_WIDGET(tree), "button-press-event",
                     G_CALLBACK(button_press_event_callback), self, 0);

}

static void hildon_file_selection_create_list_view(HildonFileSelection *
                                                   self)
{

    self->priv->view[0] = gtk_tree_view_new();
    // creating empty dummy  tree view
}

static void hildon_file_selection_create_dir_view(HildonFileSelection *
                                                  self)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;

    /* We really cannot use fixed height mode for hierarchial list, because
        we want that the width of the list can grow dynamically when
        folders are expanded (fixed height forces fixed width */
    self->priv->dir_sort = hildon_file_selection_create_sort_model
      (self, navigation_pane_sort_function, self->priv->main_model);

    self->priv->dir_filter =
        gtk_tree_model_filter_new(self->priv->dir_sort, NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER
        (self->priv->dir_filter),
        navigation_pane_filter_func, self->priv,
        NULL);
    gtk_tree_model_filter_refilter (
        GTK_TREE_MODEL_FILTER (self->priv->dir_filter));

    hildon_file_selection_enable_cursor_magic (self, self->priv->dir_filter);

/*     self->priv->dir_tree = */
/*         gtk_tree_view_new_with_model(self->priv->dir_filter); */
    self->priv->dir_tree = hildon_gtk_tree_view_new_with_model(HILDON_UI_MODE_EDIT, self->priv->dir_filter); //for selection-changed signal to be sent

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_renderer_set_fixed_size(renderer, TREE_CELL_HEIGHT,
                                     TREE_CELL_HEIGHT);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->dir_tree), col);
    gtk_tree_view_column_pack_start(col, renderer, FALSE);

    gtk_tree_view_column_add_attribute
        (col, renderer, "sensitive",
        HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE);
    gtk_tree_view_column_add_attribute
        (col, renderer,
         "pixbuf-expander-closed",
         HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON_COLLAPSED);
    gtk_tree_view_column_add_attribute
        (col, renderer,
         "pixbuf-expander-open",
         HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON_EXPANDED);
    gtk_tree_view_column_add_attribute
        (col, renderer, "pixbuf",
         HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_text_set_fixed_height_from_font(
        GTK_CELL_RENDERER_TEXT(renderer), 1);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute
        (col, renderer, "text",
         HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME);
    gtk_tree_view_column_add_attribute
        (col, renderer, "sensitive",
        HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE);

    /* Selection handling */
    selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(self->priv->dir_tree));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect_object(selection, "changed",
                     G_CALLBACK(hildon_file_selection_selection_changed),
                     self, 0);

    gtk_widget_tap_and_hold_setup(GTK_WIDGET(self->priv->dir_tree), NULL,
                                  NULL, GTK_TAP_AND_HOLD_NONE | GTK_TAP_AND_HOLD_NO_INTERNALS);
    g_signal_connect_object (self->priv->dir_tree, "tap-and-hold-query",
                             G_CALLBACK (navigation_pane_tap_and_hold_query),
                             self, 0);
    g_signal_connect_object(self->priv->dir_tree, "tap-and-hold",
                     G_CALLBACK
                     (hildon_file_selection_navigation_pane_context),
                     self, 0);

    g_signal_connect_object(self->priv->dir_tree, "key-press-event",
                     G_CALLBACK
                     (hildon_file_selection_on_navigation_pane_key), self, 0);
    g_signal_connect_object(self->priv->dir_tree, "notify::has-focus",
                     G_CALLBACK(navigation_pane_focus), self, 0);
}

static gboolean path_is_available_folder(GtkTreeView *view, GtkTreeIter *iter, GtkTreePath *path)
{
  GtkTreeModel *model;
  GtkTreeIter temp;
  gboolean is_folder, is_available;

  g_assert(path && gtk_tree_path_get_depth(path) > 0);

  if (!iter)
    iter = &temp;

  model = gtk_tree_view_get_model(view);
  if (!gtk_tree_model_get_iter(model, iter, path))
    return FALSE;

  gtk_tree_model_get(model, iter,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER, &is_folder,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE, &is_available,
    -1);

  return is_folder && is_available;
}

static void on_drag_data_received(GtkWidget * widget,
                                  GdkDragContext * context, int x, int y,
                                  GtkSelectionData * seldata, guint info,
                                  guint time, gpointer userdata)
{
    GtkTreeView *view;
    GtkTreePath *path;
    GtkWidget *nav_pane;
    GtkTreeIter dest_iter;
    char *destination;
    gboolean success = FALSE;
    HildonFileSelection *self;
    HildonFileSelectionPrivate *priv;

    g_assert(GTK_IS_TREE_VIEW(widget));
    g_assert(HILDON_IS_FILE_SELECTION(userdata));

    /* This is needed. Otherwise Gtk will give long description why we
       need this */
    g_signal_stop_emission_by_name(widget, "drag-data-received");
    view = GTK_TREE_VIEW(widget);
    self = HILDON_FILE_SELECTION (userdata);
    priv = self->priv;
    nav_pane = priv->dir_tree;

    /* Dragging from navigation pane to content pane do not work well,
       because content pane always displayes contents of selected folder. */
    if (widget == nav_pane ||
        gtk_drag_get_source_widget(context) != nav_pane)
    {
      if (gtk_tree_view_get_dest_row_at_pos(view, x, y, &path, NULL))
      {
        if (path_is_available_folder(view, &dest_iter, path))
        {
          /* We now have a working iterator to drop destination */

          gchar **uris = gtk_selection_data_get_uris(seldata);

          if (uris)
          {
            gint i;
            GSList *sources = NULL;

            gtk_tree_model_get (gtk_tree_view_get_model(view), &dest_iter,
                                HILDON_FILE_SYSTEM_MODEL_COLUMN_URI,
                                &destination,
                                -1);

            for (i = 0; uris[i]; i++)
              sources = g_slist_append (sources, uris[i]);

            g_signal_emit (userdata, signals[URIS_DROPPED],
                           0, destination, sources);

            g_slist_free (sources);
            g_free (destination);
            g_strfreev (uris);

            success = TRUE;
          }
          else
            g_debug("Dropped data did not contain uri atom signature");
        }

        gtk_tree_path_free(path);
      }
    }

    gtk_drag_finish(context, success, FALSE, time);
}

#define CLIMB_RATE 4
#define MAX_CURSOR_PARTS 10

static void drag_data_get_helper(GtkTreeModel * model, GtkTreePath * path,
                                 GtkTreeIter * iter, gpointer data)
{
  gchar *uri;
  gtk_tree_model_get(model, iter,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri, -1);
  g_ptr_array_add(data, uri);
}

static void drag_begin(GtkWidget * widget, GdkDragContext * drag_context,
                       gpointer user_data)
{
    GdkPixbuf *pixbuf;
    GtkTreeSelection *selection;
    gint column, w, h, dest, count;
    HildonFileSelection *self;
    GList *rows, *row;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GPtrArray *buffer;

    g_assert(GTK_IS_TREE_VIEW(widget));
    g_assert(HILDON_IS_FILE_SELECTION(user_data));

    self = HILDON_FILE_SELECTION(user_data);
    column =
        (widget ==
         self->priv->
         view[1] ? HILDON_FILE_SYSTEM_MODEL_COLUMN_THUMBNAIL :
         HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    if (!rows)
        return;

    /* store drag data for further use in drag_data_get since
       selection is cleared when GtkTreeView gets button-release */
    buffer = g_ptr_array_new();
    gtk_tree_selection_selected_foreach(gtk_tree_view_get_selection
                                        (GTK_TREE_VIEW(widget)),
                                        drag_data_get_helper, buffer);
    g_ptr_array_add(buffer, NULL);
    g_strfreev(self->priv->drag_data_uris);
    self->priv->drag_data_uris = (char **) g_ptr_array_free(buffer, FALSE);

    /* Lets find out how much space we need for drag icon */
    w = h = dest = 0;

    for (row = rows, count = 0; row && count < MAX_CURSOR_PARTS;
         row = row->next, count++)
        if (gtk_tree_model_get_iter(model, &iter, row->data)) {
            GdkPixbuf *buf;
            gint right, bottom;

            gtk_tree_model_get(model, &iter, column, &buf, -1);
            if( !buf )
              return;
            right = gdk_pixbuf_get_width(buf) + dest;
            bottom = gdk_pixbuf_get_height(buf) + dest;
            dest += CLIMB_RATE;

            w = MAX(w, right);
            h = MAX(h, bottom);

            g_object_unref(buf);
        }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    gdk_pixbuf_fill(pixbuf, 0);

    /* Ok, let's combine selected icons to allocated pixbuf */

    dest = 0;


    for (row = rows, count = 0; row && count < MAX_CURSOR_PARTS;
         row = row->next, count++)
        if (gtk_tree_model_get_iter(model, &iter, row->data)) {
            GdkPixbuf *buf;

            gtk_tree_model_get(model, &iter, column, &buf, -1);
            w = gdk_pixbuf_get_width(buf);
            h = gdk_pixbuf_get_height(buf);
            /*   g_assert(gdk_pixbuf_get_has_alpha(buf));*/
            gdk_pixbuf_composite(buf, pixbuf, dest, dest, w, h, dest, dest,
                                 1, 1, GDK_INTERP_NEAREST, 255);
            dest += CLIMB_RATE;

            g_object_unref(buf);
        }



    g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(rows);

    gtk_drag_set_icon_pixbuf(drag_context, pixbuf, w/2, h/2);
    g_object_unref(pixbuf);

    /* We do not want GtkTreeView to create the default drag cursor */
    g_signal_stop_emission_by_name(widget, "drag-begin");

    self->priv->currently_dragging = TRUE;
}

static void drag_data_get(GtkWidget * widget,
                          GdkDragContext * drag_context,
                          GtkSelectionData * selection_data, guint info,
                          guint time, gpointer data)
{
  HildonFileSelection *self;

  self = HILDON_FILE_SELECTION(data);

  /* We use only uri-list internally, but somebody else
     can use text format as well. */
  if (!self->priv->drag_data_uris
      || !gtk_selection_data_set_uris(selection_data,
				      self->priv->drag_data_uris))
  {
    gchar *plain = g_strjoinv("\n", self->priv->drag_data_uris);
    gtk_selection_data_set_text(selection_data, plain, -1);
    g_free(plain);
  }

  g_strfreev(self->priv->drag_data_uris);
  self->priv->drag_data_uris = NULL;

  /* We do not want to use GtkTreeView's default implementation,
     because it does not support dragging multiple rows. */
  g_signal_stop_emission_by_name(widget, "drag-data-get");
}

static gboolean drag_motion(GtkWidget * widget,
                            GdkDragContext * drag_context, gint x, gint y,
                            guint time, gpointer userdata)
{
    HildonFileSelectionPrivate *priv;
    GtkTreeView *view;
    GtkWidgetClass *klass;
    GtkTreePath *path;
    GtkTreeViewDropPosition pos;
    gboolean valid_location = FALSE;

    g_assert(GTK_IS_TREE_VIEW(widget));
    g_assert(HILDON_IS_FILE_SELECTION(userdata));

    view = GTK_TREE_VIEW(widget);
    priv = HILDON_FILE_SELECTION(userdata)->priv;

#if 0
    We rely on the default handler of GtkTreeView to do most of the work
    and then adjust the result when the handler returns. We want to make
    sure that two additional restrictions apply:
      * Drop position is new _BEFORE_ OR _AFTER_, but always
        INTO_OR_BEFORE or INTO_OR_AFTER, since we do not like
        those horizontal lines that are drawn by default.
      * We do not allow drops to all locations (dimmed folders + files).

    GtkTreeView contains only the following to decide whether
    a row is a valid target (in set_destination_row):

    if (TRUE /* FIXME if the location droppable predicate */)
    {
      can_drop = TRUE;
    }

    This is likely because in Gtk, cursor can be moved to insensitive
    tree items as well, but this is not true in our case.

    To be able to apply these conditions, we must use a hack and
    call default handler manually and then do our special cases.
    We cannot use "after" type of signals, since the default handler
    terminates signal emission on success.
#endif

    klass = GTK_WIDGET_CLASS(G_OBJECT_GET_CLASS(widget));
    valid_location = klass->drag_motion(widget, drag_context, x, y, time);

    if (valid_location)
    {
      gtk_tree_view_get_drag_dest_row(view, &path, &pos);

      /* We need to adjust drop position type after we default handler.
         The reason is that we do not like those horizontal lines
         that come from BEFORE and AFTER types. */
      if (path)
      {
        /* Dragging from navigation pane to content pane do not work well,
           because content pane always displayes contents of selected folder.
         */
        if ((widget == priv->dir_tree ||
             gtk_drag_get_source_widget(drag_context) != priv->dir_tree) &&
            (path_is_available_folder(view, NULL, path)))
        {
          if (pos == GTK_TREE_VIEW_DROP_BEFORE)
            gtk_tree_view_set_drag_dest_row(view, path,
              GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
          else if (pos == GTK_TREE_VIEW_DROP_AFTER)
            gtk_tree_view_set_drag_dest_row(view, path,
              GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
        }
        else
        {
          /* Treeview would allow to drop here, but we do not */
          gtk_tree_view_set_drag_dest_row(view, NULL, 0);
          gdk_drag_status(drag_context, 0, time);
          valid_location = FALSE;
        }

        gtk_tree_path_free(path);
      }
    }

    g_signal_stop_emission_by_name(widget, "drag-motion");

    return valid_location;
}

static void drag_end(GtkWidget *widget, GdkDragContext *context,
                     gpointer user_data)
{
  HildonFileSelectionPrivate *priv;
  priv = HILDON_FILE_SELECTION(user_data)->priv;
  priv->currently_dragging = FALSE;
}

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context,
  gint x, gint y, guint time, gpointer user_data)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target == GDK_NONE)
  {
    gtk_drag_finish (context, FALSE, FALSE, time);
    return TRUE;
  }
  else
    gtk_drag_get_data (widget, context, target, time);

  return TRUE;
}

/* Target types for DnD from the FileSelection widget */
static GtkTargetEntry hildon_file_selection_source_targets[] = {
  { "text/uri-list", 0, 0 }
};

static void hildon_file_selection_setup_dnd_view(HildonFileSelection *
                                                 self, GtkWidget * view)
{
    gtk_drag_dest_set(view, 0, NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(view);

    g_signal_connect_object(view, "drag-data-received",
                     G_CALLBACK(on_drag_data_received), self, 0);

    /* We cannot use "add_uri_targets" or similar, since GtkTreeView
       only accepts raw table */
    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(view),
      GDK_BUTTON1_MASK, hildon_file_selection_source_targets,
      G_N_ELEMENTS(hildon_file_selection_source_targets), GDK_ACTION_COPY);

    g_signal_connect_object(view, "drag-begin", G_CALLBACK(drag_begin), self,0 );
    g_signal_connect_object(view, "drag-data-get", G_CALLBACK(drag_data_get),
                     self, 0);
    g_signal_connect_object(view, "drag-motion", G_CALLBACK(drag_motion), self, 0);

    /* We want to connect after, because we want to give GtkTreeView a choice
       to clear drag timers. GtkTreeView never accepts the drag, since
       our model does not implement DragDest iface. */
    g_signal_connect_object(view, "drag-drop", G_CALLBACK(drag_drop),
      self, G_CONNECT_AFTER);

    g_signal_connect_object(view, "drag-end", G_CALLBACK(drag_end), self, 0);

    /* We don't connect "drag-data-delete", because file system
       notifications tell us about what to update */
    /* g_signal_connect(view, "drag-data-delete",
       G_CALLBACK(drag_data_delete), self); */
}

static void
trigger_repair (const char *device)
{
  DBusConnection *conn;
  DBusMessage *message;
  dbus_bool_t ret;
  const char *empty_string = "";

  conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (conn == NULL)
    {
      g_warning("dbus_bus_get failed");
      return;
    }
  
  message = dbus_message_new_method_call ("com.nokia.ke_recv",
					  "/com/nokia/ke_recv/repair_card",
					  "com.nokia.ke_recv",
					  "repair_card");
  if (message == NULL)
    {
      g_warning("dbus_message_new_method_call failed");
      return;
    }

  ret = dbus_message_append_args (message,
				  DBUS_TYPE_STRING, &device,
				  DBUS_TYPE_STRING, &empty_string, /* label */
				  DBUS_TYPE_INVALID);

  if (!ret)
    {
      g_warning("dbus_message_append_args failed");
      dbus_message_unref(message);
      return;
    }

  if (!dbus_connection_send(conn, message, NULL))
    {
      g_warning("dbus_connection_send failed");
      dbus_message_unref(message);
      return;
    }
  
  dbus_connection_flush(conn);
  dbus_message_unref(message);
}

static void
repair_button_clicked (GtkWidget *button, HildonFileSelection *self)
{
  HildonFileSelectionPrivate *priv = self->priv;
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dir_tree));
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      char *uri, *device;

      gtk_tree_model_get (model, &iter,
			  HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
			  HILDON_FILE_SYSTEM_MODEL_COLUMN_EXTRA_INFO, &device,
			  -1);

      g_debug ("REPAIR %s %s\n", uri, device);

      if (device)
	trigger_repair (device);
	
      g_free (uri);
      g_free (device);
    }
}

static void
update_local_device_visibility (GMount              *mount,
                                gboolean             mounted,
                                HildonFileSelection *selection)
{
  HildonFileSelectionPrivate *priv = selection->priv;
  GFile *root = g_mount_get_root (mount);
  gchar* uri = g_file_get_uri(root);

  g_object_unref (root);

  if (!(uri && g_str_has_prefix (uri, "file:///home/"))) {
    g_free (uri);
    return;
  }

  /* Special-case scratchbox */
  if (g_file_test ("/scratchbox/", G_FILE_TEST_EXISTS))
  {
    reload_local_device_folders(selection);
    priv->show_localdevice = TRUE;
    g_free (uri);
    return;
  }

  if (g_str_equal (&uri[7], g_getenv ("MYDOCSDIR")))
    {
      priv->show_localdevice = mounted;
      if (priv->dir_filter) {
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->dir_filter));
      }
      if (priv->view_filter) {
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                               (priv->view_filter));
	hildon_file_selection_inspect_view(priv);
      }
#if someone_explains_this
      reload_local_device_folders(selection);
#endif
   }

  g_free (uri);
}

static void
hildon_file_selection_mounted_added_cb (GVolumeMonitor        *volume_monitor,
                                        GMount                *mount,
                                        HildonFileSelection   *selection)
{
  update_local_device_visibility (mount, TRUE, selection);
}

static void
hildon_file_selection_mounted_removed_cb (GVolumeMonitor      *volume_monitor,
                                          GMount              *mount,
                                          HildonFileSelection *selection)
{
  update_local_device_visibility (mount, FALSE, selection);
}

static void hildon_file_selection_init(HildonFileSelection * self)
{
    self->priv =
        G_TYPE_INSTANCE_GET_PRIVATE(self, HILDON_TYPE_FILE_SELECTION,
                                    HildonFileSelectionPrivate);
    self->priv->mode = HILDON_FILE_SELECTION_MODE_THUMBNAILS;
    GTK_WIDGET_SET_FLAGS(GTK_WIDGET(self), GTK_NO_WINDOW);

    self->priv->show_files = TRUE;
    self->priv->scroll_dir = hildon_pannable_area_new();
    self->priv->scroll_list = hildon_pannable_area_new();
    self->priv->scroll_thumb = hildon_pannable_area_new();
    if(!(HILDON_IS_PANNABLE_AREA(self->priv->scroll_dir) &&
	 HILDON_IS_PANNABLE_AREA(self->priv->scroll_dir) &&
	 HILDON_IS_PANNABLE_AREA(self->priv->scroll_dir))){
      self->priv->scroll_dir = gtk_scrolled_window_new(NULL, NULL);
      self->priv->scroll_list = gtk_scrolled_window_new(NULL, NULL);
      self->priv->scroll_thumb = gtk_scrolled_window_new(NULL, NULL);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				     (self->priv->scroll_dir),
				     GTK_POLICY_AUTOMATIC,
				     GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				     (self->priv->scroll_list),
				     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				     (self->priv->scroll_thumb),
				     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

      gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
					  (self->priv->scroll_dir),
					  GTK_SHADOW_NONE);
      gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
					  (self->priv->scroll_list),
					  GTK_SHADOW_NONE);
      gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
					  (self->priv->scroll_thumb),
					  GTK_SHADOW_NONE);
    }
        
    self->priv->view_selector = gtk_vbox_new (FALSE, 0);

    self->priv->column_headers_visible = FALSE;

    /* This needs to exist before set properties are called */
    self->priv->view[2] = gtk_label_new (_("hfil_li_no_files_folders_to_show"));
    gtk_misc_set_alignment(GTK_MISC(self->priv->view[2]), 0.5f, 0.0f);
    gtk_box_pack_start (GTK_BOX (self->priv->view_selector),
			self->priv->view[2], TRUE, TRUE, 0);

    {
      GtkWidget *vbox, *button_label, *button;

      self->priv->label_card = gtk_label_new (dgettext("ke-recv", 
					   _("card_ib_unknown_format_card")));
      self->priv->label_device = gtk_label_new (dgettext("ke-recv", 
      					     _("card_ib_unknown_format_device")));

#if 0
      gtk_label_set_justify (GTK_LABEL (self->priv->label_card), GTK_JUSTIFY_CENTER);
      gtk_label_set_max_width_chars (GTK_LABEL (self->priv->label_card), 20);
      gtk_label_set_line_wrap (GTK_LABEL (self->priv->label_card), TRUE);
#endif
      gtk_misc_set_padding (GTK_MISC (self->priv->label_card), 10, 10);
      gtk_widget_show (self->priv->label_card);

      gtk_misc_set_padding (GTK_MISC (self->priv->label_device), 10, 10);
      gtk_widget_show (self->priv->label_device);
      
      button_label = gtk_label_new (_("sfil_bd_repair_memory_card"));
      gtk_widget_show (button_label);
      gtk_misc_set_padding (GTK_MISC (button_label), 20, 15);

      button = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (button), button_label);
      g_signal_connect_object (button, "clicked",
			       G_CALLBACK (repair_button_clicked), self,
			       0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), self->priv->label_card, FALSE, FALSE, 10);
      gtk_box_pack_start (GTK_BOX (vbox), self->priv->label_device, FALSE, FALSE, 10);
      
      self->priv->view[3] = vbox;
      gtk_box_pack_start (GTK_BOX (self->priv->view_selector),
			self->priv->view[3], TRUE, TRUE, 0);
    }
    self->priv->hpaned = gtk_hpaned_new();

    gtk_container_set_border_width(GTK_CONTAINER(self->priv->scroll_dir),
        HILDON_MARGIN_DEFAULT);
    gtk_container_set_border_width(GTK_CONTAINER(self->priv->view_selector),
        HILDON_MARGIN_DEFAULT);

    gtk_paned_pack1(GTK_PANED(self->priv->hpaned),
                    GTK_WIDGET(self->priv->scroll_dir), TRUE, FALSE);

    gtk_paned_pack2(GTK_PANED(self->priv->hpaned),
                    GTK_WIDGET(self->priv->view_selector), TRUE, FALSE);

    gtk_widget_set_parent(self->priv->hpaned, GTK_WIDGET(self));

    g_signal_connect(self, "grab-notify",
      G_CALLBACK(hildon_file_selection_check_scroll), NULL);
}

static GObject *hildon_file_selection_constructor(GType type,
                                                  guint
                                                  n_construct_properties,
                                                  GObjectConstructParam *
                                                  construct_properties)
{
    GObject *obj;
    HildonFileSelection *self;
    HildonFileSelectionPrivate *priv;
    GtkTreePath *temp_path;

    obj =
        G_OBJECT_CLASS(hildon_file_selection_parent_class)->
        constructor(type, n_construct_properties, construct_properties);

    /* Now construction parameters (=backend) are applied. Let's finalize
       construction */
    self = HILDON_FILE_SELECTION(obj);
    priv = self->priv;
  /*priv->sort_model =   <SNIP> */

    priv->monitor = g_volume_monitor_get ();

    /* Special-case scratchbox */
    if (g_file_test ("/scratchbox/", G_FILE_TEST_EXISTS))
        self->priv->show_localdevice = TRUE;
    else {
        const gchar* path = g_getenv ("MYDOCSDIR");

        if (path) {
	    GFile *file = g_file_new_for_path (path);
            GMount* mount = g_file_find_enclosing_mount (file, NULL, NULL);

            g_object_unref (file);

            if (mount) {
                GFile *root = g_mount_get_root (mount);

                if (root) {
                  gchar *uri = g_file_get_uri(root);

                  g_object_unref(root);

                  if (g_str_has_prefix (uri, "file:///")) {
                    if (g_str_equal (&uri[7], g_getenv ("MYDOCSDIR")))
                      hildon_file_selection_mounted_added_cb (priv->monitor,
                                                              mount, self);
                    else
                      hildon_file_selection_mounted_removed_cb (priv->monitor,
                                                                mount, self);
                  }

                  g_free (uri);
                }

                g_object_unref (mount);
            }
        }
    }

    /* we need to create view models here, even if dummy ones */

    temp_path = gtk_tree_path_new_from_string("0");
    priv->view_filter = gtk_tree_model_filter_new(priv->main_model, temp_path);
    gtk_tree_path_free (temp_path);

    gtk_tree_model_filter_set_visible_func
      (GTK_TREE_MODEL_FILTER (priv->view_filter), filter_func, priv, NULL);
    gtk_tree_model_filter_refilter (
        GTK_TREE_MODEL_FILTER (priv->view_filter));

    priv->sort_model = hildon_file_selection_create_sort_model
      (self, content_pane_sort_function, priv->view_filter);
    if (!priv->edit_mode)
      hildon_file_selection_enable_cursor_magic (self, priv->sort_model);
    hildon_file_selection_create_dir_view(self);
    hildon_file_selection_create_list_view(self);
    hildon_file_selection_create_thumbnail_view(self);

    /* Live search */
    priv->live_search = HILDON_LIVE_SEARCH (hildon_live_search_new ());
    hildon_live_search_set_filter (priv->live_search,
        GTK_TREE_MODEL_FILTER (priv->view_filter));

    hildon_live_search_widget_hook (priv->live_search,
        GTK_WIDGET (priv->view_selector),
        priv->view[1]);

    gtk_container_add(GTK_CONTAINER(priv->scroll_dir), priv->dir_tree);
    gtk_container_add(GTK_CONTAINER(priv->scroll_list), priv->view[0]);
    gtk_container_add(GTK_CONTAINER(priv->scroll_thumb), priv->view[1]);

    gtk_box_pack_start (GTK_BOX (self->priv->view_selector),
		      self->priv->scroll_list, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (self->priv->view_selector),
		      self->priv->scroll_thumb, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (self->priv->view_selector),
                      GTK_WIDGET(self->priv->live_search), FALSE, FALSE, 0);

    /* Also the views of the navigation pane are trees (and this is
       needed). Let's deny expanding */
    g_signal_connect(priv->view[0], "test-expand-row",
                     G_CALLBACK(gtk_true), NULL);
    g_signal_connect(priv->view[1], "test-expand-row",
                     G_CALLBACK(gtk_true), NULL);
    g_signal_connect_swapped(priv->view[0], "button-press-event",
                             G_CALLBACK(hildon_file_selection_user_moved),
                             self);
    g_signal_connect_swapped(priv->view[1], "button-press-event",
                             G_CALLBACK(hildon_file_selection_user_moved),
                             self);
    g_signal_connect_swapped(priv->view[0], "move-cursor",
                             G_CALLBACK(hildon_file_selection_user_moved),
                             self);
    g_signal_connect_swapped(priv->view[1], "move-cursor",
                             G_CALLBACK(hildon_file_selection_user_moved),
                             self);
    g_signal_connect_after(priv->view[0], "row-activated",
                           G_CALLBACK(hildon_file_selection_row_activated),
                           self);
    g_signal_connect_after(priv->view[1], "row-activated",
                           G_CALLBACK(hildon_file_selection_row_activated),
                           self);
    g_signal_connect_after(priv->view[0], "row-insensitive",
                           G_CALLBACK(hildon_file_selection_row_insensitive),
                           self);
    g_signal_connect_after(priv->view[1], "row-insensitive",
                           G_CALLBACK(hildon_file_selection_row_insensitive),
                           self);
    g_signal_connect_after(priv->dir_tree, "row-insensitive",
                           G_CALLBACK(hildon_file_selection_row_insensitive),
                           self);
    g_signal_connect_object(priv->main_model, "row-inserted",
                            G_CALLBACK(hildon_file_selection_modified), self,
                            G_CONNECT_AFTER | G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->main_model, "row-deleted",
                            G_CALLBACK(hildon_file_selection_modified), self,
                            G_CONNECT_AFTER | G_CONNECT_SWAPPED);

    /* Use sort model when possible, because we own it. In last case we
       have to use main model. This handler has to be removed manually,
       because main_model can live long after we are destroyed
       We have to connect after, otherwise iterator stamps invalidate
       when cursor is set. */
    g_signal_connect_data(priv->view_filter, "row-has-child-toggled",
                             G_CALLBACK
                             (hildon_file_selection_inspect_view), priv, NULL,
                              G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    g_signal_connect_object(priv->main_model, "finished-loading",
                             G_CALLBACK
                             (hildon_file_selection_check_close_load_banner),
                             self, 0);
    g_signal_connect_object(priv->main_model, "device-disconnected",
      G_CALLBACK(hildon_file_selection_check_location), self, 0);

    g_signal_connect_object(priv->monitor, "mount-added",
                            G_CALLBACK(hildon_file_selection_mounted_added_cb),
                            self, 0);
    g_signal_connect_object(priv->monitor, "mount-removed",
                            G_CALLBACK(hildon_file_selection_mounted_removed_cb),
                            self, 0);

    hildon_file_selection_set_select_multiple(self, FALSE);

    /* GtkTreeModelSort implements just GtkDragSource, not
       GtkDragDest. We have to implement from scratch. */
    if (priv->drag_enabled) {
        hildon_file_selection_setup_dnd_view(self, priv->view[0]);
        hildon_file_selection_setup_dnd_view(self, priv->view[1]);
        hildon_file_selection_setup_dnd_view(self, priv->dir_tree);
    }
    if (priv->hide_navi) {
      gtk_widget_show (priv->hpaned);
      gtk_widget_show_all (priv->view_selector);
      gtk_widget_hide (GTK_WIDGET(priv->live_search));
    }
    else {
      gtk_widget_show_all (priv->hpaned);
      gtk_widget_hide (GTK_WIDGET(priv->live_search));
    }
    priv->cur_view = -1;
    gtk_widget_hide (priv->scroll_list);
    gtk_widget_hide (priv->scroll_thumb);
    gtk_widget_hide (priv->view[2]);
    gtk_widget_hide (priv->view[3]);

    hildon_file_selection_inspect_view (priv);
    return obj;
}

static void
get_selected_uris_helper (GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data)
{
  char *file_uri;
  GSList **list;

  list = data;
  gtk_tree_model_get (model, iter,
                      HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &file_uri,
                      -1);
  *list = g_slist_append (*list, file_uri);   /* file_uri is already a
                                                 copy */
}

/*** Public API **********************************************************/

/**
 * hildon_file_selection_new_with_model:
 * @model: a #HildonFileSystemModel to display.
 *
 * Creates a new #HildonFileSelection using given model.
 *
 * Returns: a new #HildonFileSelection
 */
GtkWidget *hildon_file_selection_new_with_model(HildonFileSystemModel *
                                                model)
{
    return
        GTK_WIDGET(g_object_new
                   (HILDON_TYPE_FILE_SELECTION, "model", model, NULL));
}

/**
 * hildon_file_selection_set_empty_text:
 * @file_selection: the #HildonFileSelection
 * @empty_text: a label string
 *
 * Adapts the string displayed if there are no files or folders in the
 * current location.
 *
 * For example, a sound application may want to display "No sound files".
 *
 * Since: 2.13
 */
void hildon_file_selection_set_empty_text (HildonFileSelection *file_selection,
                                           const gchar         *empty_text)
{
    g_return_if_fail (HILDON_IS_FILE_SELECTION (file_selection));

    gtk_label_set_label (GTK_LABEL (file_selection->priv->view[2]), empty_text);
}

/**
 * hildon_file_selection_set_mode:
 * @self: a pointer to #HildonFileSelection
 * @mode: New mode for current folder.
 *
 * Swithces file selection between list and thumbnail modes. Note that
 * this function works only after widget is shown because of #GtkNotebook
 * implementation.
 *
 * Deprecated: 2.22: #HildonFileSelection supports only the
 *   %HILDON_FILE_SELECTION_MODE_THUMBNAILS mode.
 */
void hildon_file_selection_set_mode(HildonFileSelection * self,
                                    HildonFileSelectionMode mode)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    g_return_if_fail(/* mode == HILDON_FILE_SELECTION_MODE_LIST || //Fremantle*/
                     mode == HILDON_FILE_SELECTION_MODE_THUMBNAILS);

    if (self->priv->mode != mode) {
        self->priv->mode = mode;
        hildon_file_selection_inspect_view(self->priv);
    }
}

/**
 * hildon_file_selection_get_mode:
 * @self: a pointer to #HildonFileSelection
 *
 * Gets Current view mode for file selection widget. If widget is not
 * shown this will return an invalid mode (-1).
 * This is because of #GtkNotebook implementation.
 *
 * Returns: Current view mode.
 */
HildonFileSelectionMode hildon_file_selection_get_mode(HildonFileSelection
                                                       * self)
{
    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self),
                         HILDON_FILE_SELECTION_MODE_LIST);
    return self->priv->mode;
}

/**
 * hildon_file_selection_set_sort_key:
 * @self: a pointer to #HildonFileSelection
 * @key: New sort key.
 * @order: New sort order.
 *
 * Changes sort settings for views. Key only affects content page,
 * navigation pane is always sorted by name.
 */
void hildon_file_selection_set_sort_key(HildonFileSelection * self,
                                        HildonFileSelectionSortKey key,
                                        GtkSortType order)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE
                                         (self->priv->sort_model),
                                         (gint) key, order);
}

/**
 * hildon_file_selection_get_sort_key:
 * @self: a pointer to #HildonFileSelection
 * @key: a place to store sort key.
 * @order: a place to store sort order.
 *
 * Currently active sort settings are stored to user provided pointers.
 */
void hildon_file_selection_get_sort_key(HildonFileSelection * self,
                                        HildonFileSelectionSortKey * key,
                                        GtkSortType * order)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE
                                         (self->priv->sort_model),
                                         (gint *) key, order);
}

/**
 * hildon_file_selection_set_current_folder_iter:
 * @self: a #HildonFileSelection object
 * @main_iter: a #GtkTreeIter in the underlying model
 *
 * Sets the current folder displayed in @self to the folder specified by
 * @main_iter.
 */
void
hildon_file_selection_set_current_folder_iter(HildonFileSelection * self,
                                              GtkTreeIter * main_iter)
{
    GtkTreeIter sort_iter, filter_iter;
    GtkTreeView *view;
    GtkTreePath *treepath;
    gboolean res;
    GtkTreeIter temp_iter;

    g_assert(HILDON_IS_FILE_SELECTION(self));

    self->priv->force_content_pane = FALSE;

    /* Sanity check that iterator points to a folder. Otherwise take the parent instead */
    gtk_tree_model_get(self->priv->main_model, main_iter,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER, &res, -1);

    if (!res)
    {
      res = gtk_tree_model_iter_parent(self->priv->main_model, &temp_iter, main_iter);
      g_assert(res);
      main_iter = &temp_iter;
    }

/*     gtk_tree_model_get(self->priv->main_model, main_iter, */
/*                        HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri, */
/*                        -1); */
/*     if(g_ascii_strcasecmp (uri, "file:///") == 0) { */
/*       free(uri); */
/*       return; */
/*     } */
/*     free(uri); */
    gtk_tree_model_sort_convert_child_iter_to_iter(GTK_TREE_MODEL_SORT
                                                   (self->priv->
                                                    dir_sort),
                                                   &sort_iter, main_iter);
    gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER
                                                     (self->priv->
                                                      dir_filter),
                                                     &filter_iter,
                                                     &sort_iter);
    view = GTK_TREE_VIEW(self->priv->dir_tree);
    treepath =
        gtk_tree_model_get_path(self->priv->dir_filter, &filter_iter);

    gtk_tree_view_expand_to_path(view, treepath);

    gtk_tree_view_set_cursor(view, treepath, NULL, FALSE);

    gtk_tree_path_free(treepath);
}

/**
 * hildon_file_selection_set_current_folder_uri:
 * @self: a pointer to #HildonFileSelection
 * @folder: a new folder.
 * @error: a place to store possible error.
 *
 * Changes the content pane to display the given folder.
 *
 * Returns: %TRUE if directory change was succesful,
 *          %FALSE if error occurred.
 */
gboolean
hildon_file_selection_set_current_folder_uri (HildonFileSelection *self,
                                              const char *folder,
                                              GError **error)
{
    HildonFileSystemModel *file_system_model;
    GtkTreeIter main_iter;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);
    g_return_val_if_fail(folder != NULL, FALSE);

    g_debug("Setting folder to %s", (const char *) folder);

    file_system_model = HILDON_FILE_SYSTEM_MODEL(self->priv->main_model);

    if (hildon_file_system_model_load_uri (file_system_model,
                                           folder, &main_iter))
    {
        /* Save dialogs are currently smart enough to rerun autonaming
           when folder is loaded. No need to block here until children
           are loaded. */
        hildon_file_selection_set_current_folder_iter(self, &main_iter);
        activate_view(self->priv->dir_tree);
        g_debug("Directory changed successfully");
        return TRUE;
    }

    g_debug("Directory change failed");
    return FALSE;
}

gboolean
_hildon_file_selection_set_current_folder_path (HildonFileSelection *self,
                                               const GtkFilePath *folder,
                                               GError **error)
{
    HildonFileSystemModel *file_system_model;
    GtkTreeIter main_iter;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);
    g_return_val_if_fail(folder != NULL, FALSE);

    g_debug("Setting folder to %s", (const char *) folder);
    
    file_system_model = HILDON_FILE_SYSTEM_MODEL(self->priv->main_model);
    if (hildon_file_system_model_load_path (file_system_model,
                                            folder, &main_iter))
    {
        /* Save dialogs are currently smart enough to rerun autonaming
           when folder is loaded. No need to block here until children
           are loaded. */
        hildon_file_selection_set_current_folder_iter(self, &main_iter);
        activate_view(self->priv->dir_tree);
        g_debug("Directory changed successfully");
        return TRUE;
    }
    g_debug("Directory change failed");
    return FALSE;
}

/**
 * hildon_file_selection_get_current_folder_uri:
 * @self: a pointer to #HildonFileSelection
 *
 * Gets the URI of the currently active folder (the folder which is
 * displayed in the content pane). You have to release the returned string
 * with #g_free.
 *
 * Returns: a string.
 */
char *
hildon_file_selection_get_current_folder_uri (HildonFileSelection *self)
{
  GtkTreeIter iter;

  if (hildon_file_selection_get_current_folder_iter(self, &iter)) {
    char *uri;

    gtk_tree_model_get(self->priv->main_model, &iter,
                       HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
                       -1);
    return uri;    /* This is already a copy */
  }

  return NULL;
}

GFile *
_hildon_file_selection_get_current_folder_path (HildonFileSelection *self)
{
  GtkTreeIter iter;

  if (self->priv->cursor_goal_uri)
    return g_file_new_for_uri (self->priv->cursor_goal_uri);
 
  if (hildon_file_selection_get_current_folder_iter(self, &iter)) {
    GFile *file;

    gtk_tree_model_get(self->priv->main_model, &iter,
                       HILDON_FILE_SYSTEM_MODEL_COLUMN_GTK_PATH_INTERNAL,
		       &file,
                       -1);
    return file;    /* This is already a copy */
  }

  return NULL;
}

/**
 * hildon_file_selection_set_filter:
 * @self: a pointer to #HildonFileSelection
 * @filter: a new #GtkFileFilter.
 *
 * Only the files matching the filter will be displayed in content pane.
 * Use %NULL to remove filtering.
 */
void hildon_file_selection_set_filter(HildonFileSelection * self,
                                      GtkFileFilter * filter)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    g_return_if_fail(filter == NULL || GTK_IS_FILE_FILTER(filter));

    if (filter != self->priv->filter) {
        if (self->priv->filter)
            g_object_unref(self->priv->filter);

        if (filter) {
            g_object_ref(filter);
            g_object_ref_sink (GTK_OBJECT(filter));
        }

        self->priv->filter = filter;

        if (self->priv->view_filter) {
            /* gtk_tree_model_filter_refilter displays many critical
               warnings. This is because of Gtk bug: When refiltering is
               asked gtk uses foreach for THE WHOLE child model (not just
               for pats under the virtual root). In respective callback
               it displays errors when such an item found that has
               greater depth than virtual root but is form different
               branch. We don't need to fear these warnings, though. */

        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER
                                           (self->priv->view_filter));
        hildon_file_selection_inspect_view(self->priv);
      }
    }
}

/**
 * hildon_file_selection_get_filter:
 * @self: a pointer to #HildonFileSelection
 *
 * Get currently active filter set by #hildon_file_selection_set filter.
 * Can be %NULL if no filter is set.
 *
 * Returns: a #GtkFileFilter or %NULL.
 */
GtkFileFilter *hildon_file_selection_get_filter(HildonFileSelection * self)
{
    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), NULL);

    return self->priv->filter;
}

/**
 * hildon_file_selection_set_select_multiple:
 * @self: a pointer to #HildonFileSelection
 * @select_multiple: either %TRUE or %FALSE.
 *
 * If multiple selection is enabled, checkboxes will appear to the last
 * item to the content pane. Multiple selection must be enabled if one
 * wants to call #hildon_file_selection_select_all.
 *
 * Since 2.1.4 this function has no effect. Create a separate
 * #HildonFileSelection with "edit-mode" set to %TRUE instead.
 */
void hildon_file_selection_set_select_multiple(HildonFileSelection * self,
                                               gboolean select_multiple)
{

}

/**
 * hildon_file_selection_get_select_multiple:
 * @self: a pointer to #HildonFileSelection
 *
 * Gets state of multiple selection.
 *
 * Returns: %TRUE If multiple selection is currently enabled.
 */
gboolean hildon_file_selection_get_select_multiple(HildonFileSelection *
                                                   self)
{
    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);

    return
        gtk_tree_selection_get_mode(gtk_tree_view_get_selection
                                    (GTK_TREE_VIEW(self->priv->view[0])))
        == GTK_SELECTION_MULTIPLE;
}

/**
 * hildon_file_selection_select_all:
 * @self: a pointer to #HildonFileSelection
 *
 * Selects the first row in the content pane.
 */
void hildon_file_selection_select_all(HildonFileSelection * self)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

    gtk_tree_row_reference_free(self->priv->current_row);
    self->priv->current_row = gtk_tree_row_reference_new(self->priv->sort_model, gtk_tree_path_new_first());
}

/**
 * hildon_file_selection_unselect_all:
 * @self: a pointer to #HildonFileSelection
 *
 * Clears current selection from content pane.
 */
void hildon_file_selection_unselect_all(HildonFileSelection * self)
{
    GtkWidget *view;

    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

    view = get_current_view(self->priv);
    if (GTK_IS_TREE_VIEW(view))
    {
      GtkTreeSelection *sel;
      sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
      gtk_tree_selection_unselect_all(sel);
    }
}

/**
 * hildon_file_selection_clear_multi_selection:
 * @self: a pointer to #HildonFileSelection
 *
 * Otherwise similar to #hildon_file_selection_unselect_all,
 * but keeps the node with cursor selected. Thus,
 * this function don't have any efect in single selection mode.
 */
void hildon_file_selection_clear_multi_selection(HildonFileSelection * self)
{
    GtkWidget *view;

    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

    view = get_current_view(self->priv);
    if (GTK_IS_TREE_VIEW(view))
    {
      GtkTreePath *path;
      GtkTreeSelection *sel;

      sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

      if (gtk_tree_selection_get_mode(sel) == GTK_SELECTION_MULTIPLE)
      {
        gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &path, NULL);
        gtk_tree_selection_unselect_all(sel);

        if (path)
        {
          gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, NULL, FALSE);
          gtk_tree_path_free(path);
        }
      }
    }
}

/**
 * hildon_file_selection_get_selected_uris:
 * @self: a pointer to #HildonFileSelection
 *
 * Gets list of selected URIs from content pane. You have to release
 * the returned list with #g_free for the individual URIs and
 * g_slist_free for the list nodes.  If you are interested in item
 * that (probably) has active focus, you have to first get the active
 * pane and then call either #hildon_file_selection_get_selected_uris
 * or #hildon_file_selection_get_current_folder_uri.
 *
 * Returns: a #GSList containing strings.
 */
GSList *
hildon_file_selection_get_selected_uris (HildonFileSelection *self)
{
  GtkWidget *view;

  g_return_val_if_fail (HILDON_IS_FILE_SELECTION(self), NULL);

  view = get_current_view (self->priv);

  if (GTK_IS_TREE_VIEW (view))
    {
        GSList *uris = NULL;
        if (self->priv->edit_mode) {
            /* in Fremantle tree selection exists only in edit mode */
            gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection
                                                 (GTK_TREE_VIEW(view)),
                                                 get_selected_uris_helper,
                                                 &uris);
      
        } else {
            /* and in normal mode we use the last activated item (current_row) */
            GtkTreeModel *model;
            gchar *file_uri;

            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));

            if (gtk_tree_row_reference_valid(self->priv->current_row)) {
                GtkTreeIter iter;
                GtkTreePath *current_row_path =
                  gtk_tree_row_reference_get_path(self->priv->current_row);
                if (gtk_tree_model_get_iter(model, &iter, current_row_path)) {
                    gtk_tree_model_get(model, &iter, HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &file_uri, -1);
                    uris = g_slist_append(NULL, file_uri);
                }
                gtk_tree_path_free (current_row_path);
            }

        }
      return uris;
    }

  return NULL;
}

/* Used by select/unselect path for selections */
static void
hildon_file_selection_select_unselect_main_iter(HildonFileSelectionPrivate
                                                * priv, GtkTreeIter * iter,
                                                gboolean select,
                                                gboolean keep_current)
{
    GtkWidget *view = get_current_view(priv);

    if (GTK_IS_TREE_VIEW(view)) {
        GtkTreeIter sort_iter, filter_iter;
        GtkTreeView *treeview = GTK_TREE_VIEW(view);

        gtk_tree_model_filter_convert_child_iter_to_iter
          (GTK_TREE_MODEL_FILTER(priv->view_filter),
           &filter_iter, iter);

        gtk_tree_model_sort_convert_child_iter_to_iter
          (GTK_TREE_MODEL_SORT(priv->sort_model),
           &sort_iter, &filter_iter);

        if (select)
        {
          GtkTreePath *path;

          /* Setting cursor is a hack, but it ensures that we do not
             end up having old cursor value selected as well
             if we are in the multiselection mode... */
          path = gtk_tree_model_get_path(priv->sort_model, &sort_iter);

          if (path)
          {
            gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
            gtk_tree_row_reference_free(priv->current_row);
            priv->current_row = gtk_tree_row_reference_new(priv->sort_model, path);
            gtk_tree_path_free(path);
          }

        }
    }
}

/**
 * hildon_file_selection_select_uri:
 * @self: a pointer to #HildonFileSelection
 * @uri: a file to select.
 * @error: a place to store possible error.
 *
 * Selects the given file. If the URI doesn't point to current folder the
 * folder is changed accordingly. If multiple selection is disabled then
 * the previous selection will dissappear.
 *
 * Returns: %TRUE if folder was succesfully selected,
 *          %FALSE if the path doesn't contain a valid folder.
 */
gboolean
hildon_file_selection_select_uri (HildonFileSelection *self,
                                  const char *uri,
                                  GError **error)
{
  gboolean found;
  GtkTreeIter iter;

  g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);
  g_return_val_if_fail(uri != NULL, FALSE);

  found =
    hildon_file_system_model_load_uri (HILDON_FILE_SYSTEM_MODEL
                                       (self->priv->main_model), uri,
                                       &iter);

  if (found)
    return hildon_file_selection_select_iter (self, &iter, error);

  return FALSE;
}

gboolean
_hildon_file_selection_select_path (HildonFileSelection *self,
                                    const GtkFilePath *path,
                                    GError **error)
{
  gboolean found;
  GtkTreeIter iter;

  g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);

  found =
    hildon_file_system_model_load_path (HILDON_FILE_SYSTEM_MODEL
                                       (self->priv->main_model), path,
                                       &iter);

  if (found)
    return hildon_file_selection_select_iter (self, &iter, error);

  return FALSE;
}

gboolean
hildon_file_selection_select_iter (HildonFileSelection *self,
                                   GtkTreeIter *iter,
                                   GError **error)
{
  GtkTreeIter nav_iter, old_iter;
  gboolean dir_changed;
  GtkWidget *view;

  /* We set the nav. pane to contain parent of the item found */
  if (gtk_tree_model_iter_parent(self->priv->main_model, &nav_iter, iter))
    {
      dir_changed =
        hildon_file_selection_get_current_folder_iter(self, &old_iter) &&
        old_iter.user_data != nav_iter.user_data;

      if (dir_changed)
        hildon_file_selection_set_current_folder_iter(self, &nav_iter);

      /* Had to move this after setting folder. Otherwise it'll be
         cleared... */
      self->priv->user_touched = TRUE;
      hildon_file_selection_select_unselect_main_iter
        (self->priv, iter, TRUE, !dir_changed);

      view = get_current_view(self->priv);
      if (GTK_IS_TREE_VIEW(view))
        activate_view(view);
      else
        activate_view(self->priv->dir_tree);

      return TRUE;
    }

  /* If we do not get the parent then we fall through and the next
     statement will just change the directory. */

  /* We didn't find the match, but we did find the parent */
  if (iter->user_data != NULL)
    {
      hildon_file_selection_set_current_folder_iter (self, iter);
      return TRUE;
    }

  return FALSE;
}

/**
 * hildon_file_selection_unselect_uri:
 * @self: a pointer to #HildonFileSelection
 * @uri: file to unselect
 *
 * Unselects a currently selected filename. If the filename is not in
 * the current directory, does not exist, or is otherwise not currently
 * selected, does nothing.
 */
void
hildon_file_selection_unselect_uri (HildonFileSelection *self,
                                     const char *uri)
{
  GtkTreeIter iter;

  g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

  if (hildon_file_system_model_search_uri
      (HILDON_FILE_SYSTEM_MODEL(self->priv->main_model), uri, &iter,
       NULL, TRUE))
    hildon_file_selection_select_unselect_main_iter
      (self->priv, &iter, FALSE, FALSE);
}

void
_hildon_file_selection_unselect_path (HildonFileSelection *self,
                                      const GtkFilePath *path)
{
  GtkTreeIter iter;

  g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

  if (hildon_file_system_model_search_path
      (HILDON_FILE_SYSTEM_MODEL(self->priv->main_model), path, &iter,
       NULL, TRUE))
    hildon_file_selection_select_unselect_main_iter
      (self->priv, &iter, FALSE, FALSE);
}

/**
 * hildon_file_selection_hide_content_pane:
 * @self: a pointer to #HildonFileSelection
 *
 * Hides the content pane. This is used in certain file management dialogs.
 */
void hildon_file_selection_hide_content_pane(HildonFileSelection * self)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    gtk_widget_hide(self->priv->view_selector);
}

/**
 * hildon_file_selection_hide_navigation_pane:
 * @self: a pointer to #HildonFileSelection
 *
 * Hides the content pane. This is used in certain file management dialogs.
 */
void hildon_file_selection_hide_navigation_pane(HildonFileSelection * self)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    gtk_widget_hide(self->priv->scroll_dir);
}

/**
 * hildon_file_selection_show_content_pane:
 * @self: a pointer to #HildonFileSelection
 *
 * Shows the content pane. This is used in certain file management dialogs.
 * The content pane is shown by default. Calling this is needed only if
 * you have hidden
 */
void hildon_file_selection_show_content_pane(HildonFileSelection * self)
{
    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));
    gtk_widget_show (self->priv->view_selector);
    hildon_file_selection_selection_changed
        (gtk_tree_view_get_selection(GTK_TREE_VIEW(self->priv->dir_tree)),
         self);    /* Update content pane */
}

/* Path is a location in content pane filter model - convert it to main model
   iterator if possible */
static gboolean view_path_to_main_iter(GtkTreeModel *model,
  GtkTreeIter *iter, GtkTreePath *path)
{
  GtkTreeModel *child_model;
  GtkTreeIter filter_iter, sort_iter;

  if (gtk_tree_model_get_iter(model, &filter_iter, path))
  {
    if(GTK_IS_TREE_MODEL_SORT(model))
    {
      child_model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(model));

      gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT
                                                       (model),
                                                       &sort_iter, &filter_iter);
      gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER
                                                     (child_model), iter, &sort_iter);
    }
    else
    {
      child_model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

      gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER
                                                       (model),
                                                       &sort_iter, &filter_iter);

      gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT
                                                     (child_model), iter, &sort_iter);
    }

    return TRUE;
  }
  return FALSE;
}

/**
 * hildon_file_selection_get_current_content_iter:
 * @self: a #HildonFileSelection
 * @iter: a #GtkTreeIter for the result.
 *
 * Similar to #hildon_file_selection_get_current_folder_iter but this
 * works with content pane. However, this function fails also
 * if the content pane is not currently visible, more than one item
 * is selected or if the current folder is empty.
 *
 * Deprecated: You probably want to use
 *   #hildon_file_selection_get_active_content_iter() instead
 * if you are intrested in single item.
 *
 * Returns: %TRUE, if the given iterator is set, %FALSE otherwise.
 */
#ifndef HILDON_DISABLE_DEPRECATED
gboolean hildon_file_selection_get_current_content_iter(HildonFileSelection
                                                        * self,
                                                        GtkTreeIter * iter)
{
    GtkWidget *view;
    GList *selected_paths;
    gboolean result = FALSE;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);

    if (!hildon_file_selection_content_pane_visible(self->priv))
        return FALSE;

    view = get_current_view(self->priv);
    if (!GTK_IS_TREE_VIEW(view))
        return FALSE;

    if (self->priv->current_row) {
        selected_paths = g_list_append(NULL, gtk_tree_row_reference_get_path(self->priv->current_row));
        if (selected_paths) {
            result = view_path_to_main_iter(self->priv->sort_model, iter, selected_paths->data);
            g_list_foreach(selected_paths, (GFunc) gtk_tree_path_free, NULL);
            g_list_free(selected_paths);
        }
    }
    return result;
}
#endif
/**
 * hildon_file_selection_get_active_content_iter:
 * @self: a #HildonFileSelection
 * @iter: a #GtkTreeIter for the result.
 *
 * Gets an iterator to the item with active focus in content pane
 * (cursor in #GtkTreeView). Returned item need not to be selected,
 * it just has the active focus. If there is no focus on the content pane,
 * this function fails.
 *
 * This function differs from
 * #hildon_file_selection_get_current_content_iter,
 * because  this function uses cursor, not GtkTreeSelection.
 *
 * Deprecated: After checkboxes removal, there is no reason for this
 *  function to be used.
 *
 * Returns: %TRUE, if the given iterator is set, %FALSE otherwise.
 */
#ifndef HILDON_DISABLE_DEPRECATED
gboolean
hildon_file_selection_get_active_content_iter(HildonFileSelection *self,
  GtkTreeIter *iter)
{
    GtkWidget *view;
    GtkTreePath *path;
    gboolean result;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);

    if (!hildon_file_selection_content_pane_visible(self->priv))
        return FALSE;

    view = get_current_view(self->priv);
    if (!GTK_IS_TREE_VIEW(view) || !self->priv->content_pane_last_used)
        return FALSE;

    gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &path, NULL);
    if (!path)
      return FALSE;

    result = view_path_to_main_iter(self->priv->sort_model, iter, path);
    gtk_tree_path_free(path);
    return result;
}
#endif
/**
 * hildon_file_selection_content_iter_is_selected:
 * @self: a #HildonFileSelection
 * @iter: a #GtkTreeIter for the result.
 *
 * Checks if the given iterator is selected in the content pane.
 *
 * Deprecated: There is no much use for this function.
 *
 * Returns: %TRUE, if the iterator is selected, %FALSE otherwise.
 */
#ifndef HILDON_DISABLE_DEPRECATED
gboolean
hildon_file_selection_content_iter_is_selected(HildonFileSelection *self,
  GtkTreeIter *iter)
{
    GtkWidget *view;
    GList *selected_paths;
    GtkTreePath *main_path, *sort_path, *filter_path;
    gboolean result = FALSE;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);

    if (!hildon_file_selection_content_pane_visible(self->priv))
        return FALSE;

    view = get_current_view(self->priv);
    if (!GTK_IS_TREE_VIEW(view))
        return FALSE;

    main_path = gtk_tree_model_get_path(self->priv->main_model, iter);

    if (main_path)
    {
      sort_path = gtk_tree_model_sort_convert_child_path_to_path(
          GTK_TREE_MODEL_SORT(self->priv->sort_model), main_path);

      if (sort_path)
      {
        filter_path = gtk_tree_model_filter_convert_child_path_to_path(
            GTK_TREE_MODEL_FILTER(self->priv->view_filter), sort_path);

        if (filter_path)
        {
          /* Ok, we now need to check if filter path is present in selection */
          selected_paths = gtk_tree_selection_get_selected_rows(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), NULL);

          result = g_list_find_custom(selected_paths,
            filter_path, (GCompareFunc) gtk_tree_path_compare) != NULL;

          g_list_foreach(selected_paths, (GFunc) gtk_tree_path_free, NULL);
          g_list_free(selected_paths);

          gtk_tree_path_free(filter_path);
        }
        gtk_tree_path_free(sort_path);
      }
      gtk_tree_path_free(main_path);
    }

    return result;
}
#endif
/**
 * hildon_file_selection_get_current_folder_iter:
 * @self: a #HildonFileSelection
 * @iter: a #GtkTreeIter for the result.
 *
 * Fills the given iterator to match currently selected item in the
 * navigation pane. Internally this gets the selection from the
 * tree and then the current iterator from selection and converts
 * the result accordingly.
 *
 * Returns: %TRUE, if the given iterator is set, %FALSE otherwise.
 */
gboolean hildon_file_selection_get_current_folder_iter(HildonFileSelection
                                                       * self,
                                                       GtkTreeIter * iter)
{
    GtkTreeSelection *selection;
    GtkTreeIter filter_iter, sort_iter;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), FALSE);

    selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(self->priv->dir_tree));
    if (!gtk_tree_selection_get_selected(selection, NULL, &filter_iter))
        return gtk_tree_model_get_iter_first(self->priv->main_model, iter);

    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER
                                                     (self->priv->
                                                      dir_filter),
                                                     &sort_iter,
                                                     &filter_iter);
    gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT
                                                   (self->priv->
                                                    dir_sort), iter,
                                                   &sort_iter);

    return TRUE;
}

/* Similar to get_selected_paths, but returns only files. This still uses
   content_pane_last_used, so we do not need to change dialog at all */
/** 
 * _hildon_file_selection_get_selected_files:
 * @self:  a #HildonFileSelection.
 *
 * This function always returns a list of one file or NULL.
 */
GSList *_hildon_file_selection_get_selected_files(HildonFileSelection* self)
{
    GtkWidget *view;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gboolean folder;
    GFile *file;

    g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self), NULL);

    view = get_current_view(self->priv);

    if (GTK_IS_TREE_VIEW(view) && self->priv->content_pane_last_used) {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_row_reference_valid(self->priv->current_row)) {
            GtkTreePath *path = gtk_tree_row_reference_get_path(self->priv->current_row);
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                gtk_tree_model_get(model, &iter,
                                   HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER,
                                   &folder, -1);
                if (!folder) {
                    gtk_tree_model_get(model, &iter,
                                       HILDON_FILE_SYSTEM_MODEL_COLUMN_GTK_PATH_INTERNAL,
                                       &file, -1);
                    gtk_tree_path_free(path);
                    return g_slist_append(NULL, file);
                }
            }
            gtk_tree_path_free(path);
        }
    }
    return NULL;
}

/**
 * hildon_file_selection_dim_current_selection:
 * @self: a #HildonFileSelection.
 *
 * Appends currently selected paths to set of dimmed paths.
 * Note that dimmed paths cannot be selected, so selection
 * no longer contains the same paths after this function.
 */
void hildon_file_selection_dim_current_selection(HildonFileSelection *self)
{
    HildonFileSystemModel *model;
    GtkWidget *view;
    GtkTreeSelection *sel;
    GList *paths, *path;
    GtkTreeIter iter;

    g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

    view = get_current_view(self->priv);

    if (GTK_IS_TREE_VIEW(view))
    {
        model = HILDON_FILE_SYSTEM_MODEL(self->priv->main_model);
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
        paths = gtk_tree_selection_get_selected_rows(sel, NULL);
        gtk_tree_selection_unselect_all(sel);

        for (path = paths; path; path = path->next)
          if (view_path_to_main_iter(self->priv->sort_model, &iter, path->data))
            hildon_file_system_model_iter_available(model, &iter, FALSE);

        g_list_foreach(paths, (GFunc) gtk_tree_path_free, NULL);
        g_list_free(paths);
    }
}

/**
 * hildon_file_selection_undim_all:
 * @self: a #HildonFileSelection.
 *
 * Undims all from model that are dimmed by code. Simply calls
 * #hildon_file_system_model_reset_available for underlying model.
 */
void hildon_file_selection_undim_all(HildonFileSelection *self)
{
  g_return_if_fail(HILDON_IS_FILE_SELECTION(self));

  hildon_file_system_model_reset_available(
    HILDON_FILE_SYSTEM_MODEL(self->priv->main_model));
}

/**
 * hildon_file_selection_get_active_pane:
 * @self: a #HildonFileSelection.
 *
 * Gets the pane that either has active focus or
 * (in case of no pane has it) last time had it.
 *
 * Returns: Currently active pane.
 */
HildonFileSelectionPane
hildon_file_selection_get_active_pane(HildonFileSelection *self)
{
  g_return_val_if_fail(HILDON_IS_FILE_SELECTION(self),
    HILDON_FILE_SELECTION_PANE_NAVIGATION);

  return (HildonFileSelectionPane) self->priv->content_pane_last_used;
}

/* This tiny helper is needed by file chooser dialog. See
   hildon-file-chooser-dialog.c for description */
void _hildon_file_selection_realize_help(HildonFileSelection *self)
{
  gtk_widget_realize(self->priv->dir_tree);
}


/**
 * hildon_file_selection_set_column_headers_visible:
 * @self: a #HildonFileSelection object
 * @visible: whether column headers should be visible
 *
 * Shown/hides column headers from list view.
 */
void
hildon_file_selection_set_column_headers_visible(HildonFileSelection *self, gboolean visible)
{
  if(visible) {
    if(!self->priv->column_headers_visible) {
      self->priv->column_headers_visible = visible;
      gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self->priv->view[0]), TRUE);
    }
  } else {
    if(self->priv->column_headers_visible) {
      self->priv->column_headers_visible = visible;
      gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self->priv->view[0]), FALSE);
    }
  }
}

/**
 * hildon_file_selection_get_column_headers_visible
 * @self: a #HildonFileSelection.
 *
 * Returns: Whether column headers are shown or not.
 */
gboolean
hildon_file_selection_get_column_headers_visible(HildonFileSelection *self)
{
  return self->priv->column_headers_visible;
}

/* Set the cursor of VIEW to be at PATH, or somehwere near if PATH is
   not valid for the model.
*/
static void
hildon_file_selection_set_cursor_stubbornly (HildonFileSelection *self,
					     GtkTreeView *view,
                                             GtkTreePath *path)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (view);

  /* We try setting thr cursor to PATH, then the previous item of
     PATH, then the parent of PATH.
  */

  if (model == NULL || gtk_tree_model_iter_n_children (model, NULL) <= 0)
    return;

  if (gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
  else
    {
      GtkTreePath *p = gtk_tree_path_copy (path);

      if (!gtk_tree_path_prev (p))
        {
          gtk_tree_path_free (p);
          p = gtk_tree_path_copy (path);
          if (!gtk_tree_path_up (p))
            {
              gtk_tree_path_free (p);
              return;
            }
        }

      gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), p, NULL, FALSE);
      gtk_tree_path_free (p);
    }

  g_signal_emit(self, signals[CURRENT_FOLDER_CHANGED], 0);
}

struct idle_cursor_data {
  HildonFileSelection *self;
  GtkTreeView *view;
  GtkTreePath *path;
  gboolean stubbornly;
};

static gboolean
set_cursor_idle_handler (gpointer data)
{
  struct idle_cursor_data *c = (struct idle_cursor_data *)data;

  GDK_THREADS_ENTER ();

  gtk_tree_view_expand_to_path (c->view, c->path);

  if (c->stubbornly)
    hildon_file_selection_set_cursor_stubbornly (c->self, c->view, c->path);
  else
    gtk_tree_view_set_cursor (c->view, c->path, NULL, FALSE);

  gtk_tree_path_free (c->path);

  c->self->priv->cursor_idle_id = 0;

  GDK_THREADS_LEAVE ();
  g_free (c);

  return FALSE;
}

static void
set_cursor_when_idle (HildonFileSelection *self,
		      GtkTreeView *view, GtkTreePath *path,
                      gboolean stubbornly)
{
  struct idle_cursor_data *c;
  GtkTreePath *p = gtk_tree_path_copy (path);

  if (self->priv->cursor_idle_id)
    {
      c = self->priv->cursor_idle_data;
      /*
	If we already have idle set and path is a parent of what we already have
	and we stubbornly set the cursor, then the path points to a folder that
	is being deleted, so set the cursor to the *parent* of path.
       */
      if (stubbornly && gtk_tree_path_is_descendant (c->path, p))
	gtk_tree_path_up (p);

      g_source_remove (self->priv->cursor_idle_id);
      gtk_tree_path_free (c->path);
      g_free (c);
    }

  c = g_new (struct idle_cursor_data, 1);
  c->self = self;
  c->view = GTK_TREE_VIEW (view);
  c->path = p;
  c->stubbornly = stubbornly;

  self->priv->cursor_idle_data = c;
  self->priv->cursor_idle_id = g_idle_add (set_cursor_idle_handler, c);
}

static GtkTreeView *
get_view_for_model (HildonFileSelection *self, GtkTreeModel *model)
{
  HildonFileSelectionPrivate *priv = self->priv;

  /* XXX - figuring out the view from the model is too closely tied to
           our setup.
  */
  if (model == priv->dir_filter)
    return GTK_TREE_VIEW (priv->dir_tree);
  else
    {
      GtkWidget *view;
      GtkTreeModel *active_model;

      view = get_current_view (priv);
      if (!GTK_IS_TREE_VIEW (view))
        return NULL;

      active_model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      if (model != active_model)
        return NULL;

      return GTK_TREE_VIEW (view);
    }
}

static void
hildon_file_selection_row_deleted (GtkTreeModel *model,
                                   GtkTreePath *path,
                                   gpointer data)
{
  HildonFileSelection *self = (HildonFileSelection *)data;
  GtkTreeView *view;

  /* Check the view to see if we need to move the cursor so that it is
     always set.
  */

  view = get_view_for_model (self, model);
  if (view)
    {
      GtkTreePath *cursor_path;

      /* We want to move the cursor when its row is deleted.  However,
         the TreeView is also watching the model for deleted rows and
         updates its cursor accordingly.  Thus, when we check the
         cursor here, it might or might not already have been unset,
         depending whose handler is run first.  Thus, when the cursor
         is not set here, we want to set it again.  Except when we
         have a goal uri for the cursor.  In that case, we leave the
         cursor unset.
      */

      gtk_tree_view_get_cursor (GTK_TREE_VIEW (view), &cursor_path, NULL);

      if (cursor_path == NULL && self->priv->cursor_goal_uri != NULL)
        {
          /* We have a goal uri. Let's continue waiting for it.
           */
          return;
        }

      if (cursor_path && gtk_tree_path_compare (path, cursor_path) != 0 &&
	  !gtk_tree_path_is_descendant (cursor_path, path))
        {
          /* We have a cursor but it is not on/inside the row that is being
             deleted.  Do nothing.
          */
          gtk_tree_path_free (cursor_path);
          return;
        }

      if (cursor_path &&
	  (gtk_tree_path_compare (path, cursor_path) == 0 ||
	   gtk_tree_path_is_descendant (cursor_path, path)))
	{
	  /* We are deleting a row that is parent of the current cursor,
	     set the cursor to the deleted row's parent;
	   */
	  GtkTreePath *p = gtk_tree_path_copy (path);

	  if (gtk_tree_path_up (p) && gtk_tree_path_get_depth(p))
	    {
	      gboolean use_idle = TRUE;
	      GtkTreePath *root = gtk_tree_path_new_first();

	      if (gtk_tree_path_compare (p, root) == 0)
		  use_idle = FALSE;

	      gtk_tree_path_free (root);

	      /* XXX - A mount is removed, we have to set the cursor here, not
		 in an idle func as there model is no longer available
	       */
	      if (use_idle)
		set_cursor_when_idle (self, view, p, TRUE);
	      else
		hildon_file_selection_set_cursor_stubbornly (self, view, p);
	    }

	  gtk_tree_path_free (p);
	  gtk_tree_path_free (cursor_path);
	  return;
	}

      gtk_tree_path_free (cursor_path);
      set_cursor_when_idle (self, view, path, TRUE);
    }
}

static void
hildon_file_selection_row_inserted (GtkTreeModel *model,
                                    GtkTreePath *path, GtkTreeIter *iter,
                                    gpointer data)
{
  HildonFileSelection *self = (HildonFileSelection *)data;
  HildonFileSelectionPrivate *priv = self->priv;
  GtkTreeSelection *selection;
  GtkTreeView *view;
  char *uri;

  if (priv->cursor_goal_uri == NULL)
    return;

  view = get_view_for_model (self, model);
  if (view == NULL)
    return;

  selection = gtk_tree_view_get_selection (view);
  gtk_tree_model_get (model, iter,
                      HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
                      -1);

  if (g_str_equal (uri, priv->cursor_goal_uri) &&
      !gtk_tree_selection_get_selected (selection, NULL, NULL))
    {

      gtk_tree_view_expand_to_path (view, path);
      set_cursor_when_idle (self, view, path, FALSE);

      g_free (priv->cursor_goal_uri);
      priv->cursor_goal_uri = NULL;
    }

  g_free (uri);
}

/* Setup things so that the view that shows MODEL (at any time) will
   not lose its cursor when rows are deleted from the model.  Also,
   the model will start tracking the cursor_goal_uri as explained for
   hildon_file_selection_move_cursor_to_uri.
*/

static void
hildon_file_selection_enable_cursor_magic (HildonFileSelection *self,
                                           GtkTreeModel *model)
{
  g_signal_connect (model, "row-deleted",
                    G_CALLBACK (hildon_file_selection_row_deleted),
                    self);
  g_signal_connect (model, "row-inserted",
                    G_CALLBACK (hildon_file_selection_row_inserted),
                    self);
}

static void
hildon_file_selection_disable_cursor_magic (HildonFileSelection *self,
                                            GtkTreeModel *model)
{
  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (hildon_file_selection_row_deleted), self);
  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (hildon_file_selection_row_inserted), self);
}

/**
 * hildon_file_selection_move_cursor_to_uri:
 * @self: a #HildonFileSelection object
 * @uri: an uri
 *
 * Moves the cursor to the given @uri in the file selection widget.
 */
void
hildon_file_selection_move_cursor_to_uri (HildonFileSelection * self,
                                          const gchar *uri)
{
  HildonFileSelectionPrivate *priv = self->priv;
  GtkTreeIter iter;
  gboolean loaded;

  loaded = hildon_file_system_model_load_uri
      (HILDON_FILE_SYSTEM_MODEL (priv->main_model), uri, &iter);
  if (loaded)
    {
      GtkTreeIter filter_iter, sort_iter;
      GtkTreePath *path;

      /* Now find the view corresponding to ITER and set its cursor.
      */

      if (priv->content_pane_last_used)
        {
          GtkTreeView *view = get_view_for_model (self, priv->sort_model);
          if (view == NULL)
            return;

          gtk_tree_model_filter_convert_child_iter_to_iter
            (GTK_TREE_MODEL_FILTER(priv->view_filter),
             &filter_iter, &iter);

          gtk_tree_model_sort_convert_child_iter_to_iter
            (GTK_TREE_MODEL_SORT(priv->sort_model),
             &sort_iter, &filter_iter);

          path = gtk_tree_model_get_path (priv->sort_model, &sort_iter);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
          gtk_tree_path_free (path);
        }
      else
        {
          gtk_tree_model_sort_convert_child_iter_to_iter
            (GTK_TREE_MODEL_SORT(priv->dir_sort),
             &sort_iter, &iter);

          gtk_tree_model_filter_convert_child_iter_to_iter
            (GTK_TREE_MODEL_FILTER(priv->dir_filter),
             &filter_iter, &sort_iter);

          path = gtk_tree_model_get_path (priv->dir_filter, &filter_iter);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->dir_tree), path,
                                    NULL, FALSE);
          gtk_tree_path_free (path);
        }
    }
  else
    {
      GtkWidget *view;
      GtkTreeSelection *selection;

      g_free (priv->cursor_goal_uri);
      priv->cursor_goal_uri = g_strdup (uri);

      if (priv->content_pane_last_used)
        view = get_current_view (priv);
      else
        view = priv->dir_tree;

      if (view == NULL)
        return;

      /* XXX - we only want to unset the cursor...
       */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      gtk_tree_selection_unselect_all (selection);
    }
}

static GtkTreeModel *
hildon_file_selection_create_sort_model (HildonFileSelection *self,
                                         GtkTreeIterCompareFunc sort_function,
                                         GtkTreeModel *parent_model)
{
  GtkTreeModel *ret;
  GtkTreeSortable *sortable;


  if(!parent_model) {
    return NULL;
  }

  ret = gtk_tree_model_sort_new_with_model(parent_model);

  sortable = GTK_TREE_SORTABLE(ret);
  gtk_tree_sortable_set_sort_func(sortable,
                                  HILDON_FILE_SELECTION_SORT_NAME,
                                  sort_function, sortable, NULL);
  gtk_tree_sortable_set_sort_func(sortable,
                                  HILDON_FILE_SELECTION_SORT_TYPE,
                                  sort_function, sortable, NULL);
  gtk_tree_sortable_set_sort_func(sortable,
                                  HILDON_FILE_SELECTION_SORT_MODIFIED,
                                  sort_function, sortable, NULL);
  gtk_tree_sortable_set_sort_func(sortable,
                                  HILDON_FILE_SELECTION_SORT_SIZE,
                                  sort_function, sortable, NULL);
  gtk_tree_sortable_set_default_sort_func(sortable, sort_function,
                                          sortable, NULL);
  gtk_tree_sortable_set_sort_column_id(sortable,
                                       (gint)
                                       HILDON_FILE_SELECTION_SORT_NAME,
                                       GTK_SORT_ASCENDING);

  return ret;
}

static void reload_local_device_folders(HildonFileSelection *selection)
{
    HildonFileSystemModel *model;
    
    g_return_if_fail(HILDON_IS_FILE_SELECTION(selection));
    g_object_get(selection, "model", &model, NULL);
    g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));
    rescan_local_device_folders(model);
}
