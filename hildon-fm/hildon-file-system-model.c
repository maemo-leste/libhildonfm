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
 * SECTION: hildon-file-system-model
 * @short_description: a #GtkTreeModel-compatible file system model
 *
 * This is the model used by #HildonFileSelection.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <hildon-thumbnail-factory.h>
#include <hildon-albumart-factory.h>

#include "hildon-file-system-model.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-settings.h"
#include "hildon-file-system-voldev.h"
#include <glib/gprintf.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <errno.h>
#include <hildon-mime.h>
#ifdef UPSTREAM_DISABLED
#include <tracker.h>
#endif
#include "hildon-file-common-private.h"
#include "hildon-file-system-special-location.h"
#include "hildon-file-system-root.h"
#include "hildon-file-system-local-device.h"
#include "hildon-file-details-dialog.h"

/*#define DEBUG*/

/*  Reload contents of removable devices after this amount of seconds */
#define RELOAD_THRESHOLD 30
#define THUMBNAIL_WIDTH 80      /* For images inside thumbnail folder */
#define THUMBNAIL_HEIGHT 60
#define THUMBNAIL_ICON 48       /* Size for icon theme icons used in
                                   thumbnail mode. Using the value 60 made
                                   icons to have size 60x51!!! */
#define DEFAULT_MAX_CACHE 50
#define MIN_CACHE 20

#define MAX_BATCH 20

static const char *EXPANDED_EMBLEM_NAME = "qgn_list_gene_fldr_exp";
static const char *COLLAPSED_EMBLEM_NAME = "qgn_list_gene_fldr_clp";

static GQuark hildon_file_system_model_quark = 0;

typedef struct {
    GtkFilePath *path;
    GtkFileInfo *info;
    GtkFileFolder *folder;
    GtkFileSystemHandle *get_folder_handle;
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

typedef struct {
    GNode *parent_node;
    GtkFileFolder *folder;
    GSList *children;
    GSList *iter;
} delayed_list_type;

struct _HildonFileSystemModelPrivate {
    GNode *roots;
    GType column_types[NUM_COLUMNS];
    gint stamp;

    GtkFileSystem *filesystem;
    GtkWidget *ref_widget;      /* Any widget on the same screen, needed
                                   to return correct icons */

    /* We have to keep references to emblems ourselves. They are used only
       while composed image is made, so our new cache approach would free
       them immediately after composed image is ready */
    GdkPixbuf *expanded_emblem, *collapsed_emblem;
    guint timeout_id;
#ifdef UPSTREAM_DISABLED
    TrackerClient *tracker_client;
#endif

    /* Properties */
    gchar *backend_name;
    gchar *alternative_root_dir;
    gboolean multiroot;

    gulong volumes_changed_handler;
    gulong style_changed_handler;
    gulong hour24_changed_handler;

    /* This is set to true when all GnomeVFS devices have been
       enumerated at least once.
    */
   gboolean first_root_scan_completed;
};

typedef struct {
    HildonFileSystemModel *model;
    GNode *node;
} HandleData;

/* Property id:s */
enum {
    PROP_BACKEND = 1,
    PROP_BACKEND_OBJECT,
    PROP_THUMBNAIL_CALLBACK,
    PROP_REF_WIDGET,
    PROP_ROOT_DIR,
    PROP_MULTI_ROOT
};

enum {
    FINISHED_LOADING,
    DEVICE_DISCONNECTED,
    VOLDEV_MOUNTED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void hildon_file_system_model_iface_init(GtkTreeModelIface * iface);
static void
hildon_file_system_model_drag_source_iface_init(GtkTreeDragSourceIface *iface);

static void hildon_file_system_model_init(HildonFileSystemModel * self);
static void hildon_file_system_model_class_init(HildonFileSystemModelClass
                                                * klass);
static GObject *
hildon_file_system_model_constructor(GType type,
                                     guint n_construct_properties,
                                     GObjectConstructParam *
                                     construct_properties);

static GNode *
hildon_file_system_model_add_node(GtkTreeModel * model,
                                  GNode * parent_node,
                                  GtkFileFolder * parent_folder,
                                  const GtkFilePath * path,
                                  gboolean with_search);
static void hildon_file_system_model_remove_node_list(GtkTreeModel * model,
                                                      GNode * parent_node,
                                                      GSList * children);
static void hildon_file_system_model_change_node_list(GtkTreeModel * model,
                                                      GNode * parent_node,
                                                      GtkFileFolder *
                                                      folder,
                                                      GSList * children);
static gboolean is_node_loaded (GNode * node);
static GNode *
hildon_file_system_model_kick_node(GNode *node, gpointer data);
static void
clear_model_node_caches(HildonFileSystemModelNode *model_node);
static void unlink_file_folder(GNode *node);
static gboolean
link_file_folder(GNode *node, const GtkFilePath *path);
static void
hildon_file_system_model_folder_finished_loading(GtkFileFolder *monitor,
  gpointer data);
static void emit_node_changed(GNode *node);
static void
location_changed(HildonFileSystemSpecialLocation *location, GNode *node);
static void
location_connection_state_changed(HildonFileSystemSpecialLocation *location,
    GNode *node);
static void
location_rescan (HildonFileSystemSpecialLocation *location, GNode *node);
static void setup_node_for_location(GNode *node);
static void
hildon_file_system_model_reload_node (HildonFileSystemModel *model,
                                            GNode *node,
                                            gboolean force);
static void
_hildon_file_system_model_load_children(HildonFileSystemModel *model,
                                        GtkTreeIter *parent_iter);

static GtkTreePath *hildon_file_system_model_get_path(GtkTreeModel * model,
                                                      GtkTreeIter * iter);
static GNode *
hildon_file_system_model_search_path_internal (GNode *parent_node,
                                               const GtkFilePath *path,
                                               gboolean recursively);
static gboolean
model_node_is_folder(HildonFileSystemModelNode *model_node);


#define CAST_GET_PRIVATE(o) \
    ((HildonFileSystemModelPrivate *) HILDON_FILE_SYSTEM_MODEL(o)->priv)
#define MODEL_FROM_NODE(n) ((HildonFileSystemModelNode *) n->data)->model

/* Note! G_IMPLEMENT_INTERFACE macros together form the 5th parameter for
   G_DEFINE_TYPE_EXTENDED */
G_DEFINE_TYPE_EXTENDED(HildonFileSystemModel, hildon_file_system_model,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                           hildon_file_system_model_iface_init)
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_DRAG_SOURCE,
                           hildon_file_system_model_drag_source_iface_init))

static void
handle_finished_node (GNode *node)
{
  GtkTreeIter iter;
  HildonFileSystemModel *model = MODEL_FROM_NODE(node);
  GNode *child_node;

  child_node = g_node_first_child(node);
  while (child_node)
    {
      HildonFileSystemModelNode *model_node = child_node->data;

      /* We do not want to ever kick permanent special locations. */
      
      if (model_node->present_flag
	  || (model_node->location  && model_node->location->permanent)||
	  model_node->linking)
	  child_node = g_node_next_sibling(child_node);
      else
	child_node = hildon_file_system_model_kick_node(child_node, model);
    }

  emit_node_changed (node);

  iter.stamp = model->priv->stamp;
  iter.user_data = node;
  g_signal_emit (model, signals[FINISHED_LOADING], 0, &iter);
}

/* This default handler is activated when device tree (mmc/gateway)
   is automatically removed. This handler removes the tree, but the
   root node stays in the tree. Thus iter provided as parameter will
   stay valid. */
static void hildon_file_system_model_real_device_disconnected(
  HildonFileSystemModel *self, GtkTreeIter *iter)
{
  GNode *node, *child;
  HildonFileSystemModelNode *model_node;

  g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(self));
  g_return_if_fail(iter->stamp == self->priv->stamp);

  node = iter->user_data;
  unlink_file_folder(node);

  child = node->children;
  while (child)
    child = hildon_file_system_model_kick_node(child, self);

  model_node = node->data;
  g_return_if_fail(model_node != NULL);

  clear_model_node_caches(model_node);

  if (model_node->info)
  {
    gtk_file_info_free (model_node->info);
    model_node->info = NULL;
  }

  if (model_node->location
      && (model_node->location->compatibility_type ==
          HILDON_FILE_SYSTEM_MODEL_MMC))
    {
      /* When a MMC is disconnected, we assume that the next it gets
         connected it is a different MMC.  Thus, we need to reset
         load_time and accessed.
      */
      model_node->load_time = 0;
      model_node->accessed = FALSE;
    }
}

static void send_device_disconnected(GNode *node)
{
  HildonFileSystemModel *model;
  GtkTreeIter iter;

  g_return_if_fail(node != NULL);

  model = MODEL_FROM_NODE(node);
  iter.stamp = model->priv->stamp;
  iter.user_data = node;
  g_signal_emit(model, signals[DEVICE_DISCONNECTED], 0, &iter);
}

/* Returns the device node that is the parent of the given node. Fills
   the type pointer by node type if given */
static GNode *get_device_for_node(GNode *node)
{
  while (node)
  {
    HildonFileSystemModelNode *model_node = node->data;

    if (model_node &&
        model_node->location != NULL)
      return node;

    node = node->parent;
  }

  return NULL;
}

static void handle_load_error(GNode *node)
{
  HildonFileSystemModelNode *model_node;

  model_node = node->data;

  g_return_if_fail(model_node != NULL);
  g_return_if_fail(model_node->error != NULL);

  g_warning("%s", model_node->error->message);

  /* We failed to connect to device before the call expired.
     We want disconnect the whole device in question, not
     just to kick of the individual node that caused problems

     XXX - Gtk+ 2.10 doesn't have ERROR_TIMEOUT anymore.  Is
           ERROR_FAILED equivalent?
  */
  if (g_error_matches (model_node->error, GTK_FILE_SYSTEM_ERROR,
                       GTK_FILE_SYSTEM_ERROR_FAILED))
  {
    GNode *device_node = get_device_for_node(node);
    if (device_node)
    {
      node = device_node;
      model_node = device_node->data;
    }
  }

  /* We do not kick of devices because of errors. Those ones that want to
     be removed are kicked on when their parent is refreshed. */
  if (model_node->location)
  {
    // g_clear_error(&model_node->error);
    send_device_disconnected(node);
    emit_node_changed(node);
  }
  else if (g_error_matches(model_node->error,
      GTK_FILE_SYSTEM_ERROR, GTK_FILE_SYSTEM_ERROR_NONEXISTENT))
    /* No longer present, we remove this node totally */
    hildon_file_system_model_kick_node(node, model_node->model);
  else /* Some other error, we represent this as disabled */
    emit_node_changed(node);
}


static gboolean
node_needs_reload (HildonFileSystemModel *model, GNode *node,
                   gboolean force)
{
  HildonFileSystemModelNode *model_node;
  time_t current_time;
  gboolean removable;

  model_node = node->data;
  g_assert(model_node != NULL);

  /* Check if we really need to load children. We don't want to reload
     if not needed and we don't want to restart existing async
     loadings. We also don't try to access gateway if not accessed
     yet.
  */

  if (model_node->location
      && !model_node->accessed
      && (hildon_file_system_special_location_requires_access
          (model_node->location))
      && model_node->error == NULL)
    {
      /* Accessing this node is expensive and the user has not tried
         to do it explicitly yet.  We don't reload it even if forced.
      */
      g_debug ("TOO EXPENSIVE\n");
      return FALSE;
    }

  if (model_node->get_folder_handle != NULL
      || (model_node->folder
	  && gtk_file_folder_is_finished_loading (model_node->folder)))
    {
      /* This node is being loaded right now, just let it finish.
       */
      return FALSE;
    }

  if (force)
    {
      /* Explicit user action will trigger a reload even if the
         RELOAD_THRESHOLD timeout has not expired yet.
      */
      return TRUE;
    }

  /* If none of the rules above apply, we reload a node if it hasn't
     been loaded yet, or if it is a node that we don't receive change
     notifications for and it has been loaded too long ago.

     We assume that we don't receive change notifications for
     non-'local' locations and locations that had an error.
  */

  current_time = time(NULL);
  removable = !gtk_file_system_path_is_local (model->priv->filesystem,
                                              model_node->path);

  return (model_node->load_time == 0
          || ((abs(current_time - model_node->load_time) > RELOAD_THRESHOLD)
              && (removable || model_node->error)));
}


static GNode *get_node(HildonFileSystemModelPrivate * priv,
                       GtkTreeIter * iter)
{
    if (iter) {
        g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
        return iter->user_data;
    } else
        return priv->roots;
}

/**********************************************/
/* Start of GTK_TREE_MODEL interface methods */
/**********************************************/

static GtkTreeModelFlags hildon_file_system_model_get_flags(GtkTreeModel *
                                                            model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint hildon_file_system_model_get_n_columns(GtkTreeModel * model)
{
    return NUM_COLUMNS;
}

static GType hildon_file_system_model_get_column_type(GtkTreeModel * model,
                                                      gint index)
{
    g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), G_TYPE_NONE);
    g_return_val_if_fail(0 <= index
                         && index < NUM_COLUMNS,
                         G_TYPE_NONE);

    return CAST_GET_PRIVATE(model)->column_types[index];
}

/* The following method is mostly borrowed from GtkTreeStore */
static gboolean hildon_file_system_model_get_iter(GtkTreeModel * model,
                                                  GtkTreeIter * iter,
                                                  GtkTreePath * path)
{
    gint *indices;
    gint i, depth;

    indices = gtk_tree_path_get_indices(path);
    depth = gtk_tree_path_get_depth(path);

    g_return_val_if_fail(depth > 0, FALSE);

    if (!gtk_tree_model_iter_nth_child(model, iter, NULL, indices[0]))
        return FALSE;

    for (i = 1; i < depth; i++) {
        GtkTreeIter parent_iter = *iter;

        if (!gtk_tree_model_iter_nth_child
            (model, iter, &parent_iter, indices[i]))
            return FALSE;
    }

    return TRUE;
}

static GtkTreePath *hildon_file_system_model_get_path(GtkTreeModel * model,
                                                      GtkTreeIter * iter)
{
    GNode *node, *parent;
    GtkTreePath *path;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    node = iter->user_data;
    path = gtk_tree_path_new();

    g_assert(node != NULL);

    while (!G_NODE_IS_ROOT(node)) {     /* Don't take take fake root into
                                           account */
        parent = node->parent;
        gtk_tree_path_prepend_index(path,
                                    g_node_child_position(parent, node));
        node = parent;
    }

    return path;
}

inline static GdkPixbuf
    *hildon_file_system_model_create_image(HildonFileSystemModelPrivate *
                                           priv,
                                           HildonFileSystemModelNode *
                                           model_node, gint size)
{
    return _hildon_file_system_create_image (priv->filesystem,
                                             priv->ref_widget,
                                             model_node->info,
                                             model_node->location,
                                             size);
}

/* Creates a new pixbuf conatining normal image and given emblem */
static GdkPixbuf
    *hildon_file_system_model_create_composite_image
    (HildonFileSystemModelPrivate * priv,
     HildonFileSystemModelNode * model_node, GdkPixbuf *emblem)
{
    GdkPixbuf *plain, *result;

    plain =
        hildon_file_system_model_create_image(priv, model_node,
                                              TREE_ICON_SIZE);
    if (!plain) return NULL;
    if (!emblem) return plain;

    result = gdk_pixbuf_copy(plain);
    if (!result)  /* Not an assert anymore */
      return plain;

    /* This causes read errors according to valgrind. I wonder why is that
     */
    gdk_pixbuf_composite(emblem, result, 0, 0,
      MIN(gdk_pixbuf_get_width(emblem), gdk_pixbuf_get_width(result)),
      MIN(gdk_pixbuf_get_height(emblem), gdk_pixbuf_get_height(result)),
      0, 0, 1, 1, GDK_INTERP_NEAREST, 255);
    g_object_unref(plain);

    return result;
}

static gboolean
is_drive (HildonFileSystemModelNode *m)
{
  return g_str_has_prefix (gtk_file_path_get_string (m->path), "drive://");
}

static gboolean
is_node_loaded (GNode * node)
{
  HildonFileSystemModelNode *model_node = node->data;

  /* Only folders need to be loaded.
   */
  if (!model_node_is_folder(model_node))
    return TRUE;

  return (model_node->error 
	  || is_drive (model_node)
	  || (model_node->folder
	      && gtk_file_folder_is_finished_loading (model_node->folder)
	      && model_node->pending_adds == 0)); /* this is the only place pending_adds is checked, thus no need to know the exact amount, just equality to 0 */
}

static void emit_node_changed(GNode *node)
{
  HildonFileSystemModelNode *model_node;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_assert(node != NULL);

  model_node = node->data;
  model = GTK_TREE_MODEL(model_node->model);
  iter.stamp = CAST_GET_PRIVATE(model)->stamp;
  iter.user_data = node;
  path = hildon_file_system_model_get_path(model, &iter);
  if (gtk_tree_path_get_depth (path) > 0)
    gtk_tree_model_row_changed(model, path, &iter);
  gtk_tree_path_free(path);
}

static void
thumbnail_request_pixbuf_cb(HildonThumbnailFactory *factory,
                            GdkPixbuf              *thumbnail,
                            GError                 *error,
                            gpointer                user_data)
{
  GNode *node = user_data;
  HildonFileSystemModelNode *model_node = node->data;

  g_assert(model_node != NULL);

  if (model_node->thumbnail_request == NULL) //in case hildon_thumbnail_request_unqueue() was called already
      return;

  g_object_unref (model_node->thumbnail_request);
  model_node->thumbnail_request = NULL;

  if (error != NULL)
  {
      //if thumbnailer couldn't generate a thumbnail, let's set an icon "unknown file"
      if (model_node->thumbnail_cache)
          g_object_unref(model_node->thumbnail_cache);
      model_node->thumbnail_cache = _hildon_file_system_load_icon_cached(gtk_icon_theme_get_default(), "filemanager_unknown_file", THUMBNAIL_ICON);
      emit_node_changed(node);
      return;
  }

  g_return_if_fail (GDK_IS_PIXBUF (thumbnail));

  if (model_node->thumbnail_cache)
      g_object_unref(model_node->thumbnail_cache);

   model_node->thumbnail_cache = g_object_ref(thumbnail);
   emit_node_changed(node);
   g_object_unref(model_node->model);
}

static GdkPixbuf *get_expanded_emblem(HildonFileSystemModelPrivate *priv)
{
  if (!priv->expanded_emblem)
    priv->expanded_emblem = _hildon_file_system_load_icon_cached(
      gtk_icon_theme_get_default(), EXPANDED_EMBLEM_NAME, TREE_ICON_SIZE);

  return priv->expanded_emblem;
}

static GdkPixbuf *get_collapsed_emblem(HildonFileSystemModelPrivate *priv)
{
  if (!priv->collapsed_emblem)
    priv->collapsed_emblem = _hildon_file_system_load_icon_cached(
      gtk_icon_theme_get_default(), COLLAPSED_EMBLEM_NAME, TREE_ICON_SIZE);

  return priv->collapsed_emblem;
}

static gboolean
path_is_readonly(GtkFileSystem *file_system,
                 GtkFilePath   *path)
{
  GFileInfo *info;
  gboolean retval = FALSE;
  gchar *uri = gtk_file_system_path_to_uri(file_system, path);
  GFile *file;

  if (!uri)
    return TRUE;

  file = g_file_new_for_uri (uri);
  g_free (uri);
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                            G_FILE_QUERY_INFO_NONE, NULL, NULL);
  g_object_unref (file);

  if (!info)
      return TRUE;

  retval =
      !g_file_info_get_attribute_boolean (info,
                                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
  g_object_unref (info);

  return retval;
}

/* Returns whether model_node is considered to be a folder (by
 * HildonFileSystemModel's definitions). */
static gboolean
model_node_is_folder(HildonFileSystemModelNode *model_node)
{
    return model_node->folder
	|| model_node->location
	|| model_node->get_folder_handle
	|| (model_node->info && gtk_file_info_get_is_folder(model_node->info));
}

static gboolean
model_node_invalidate_display_props(GNode *node, gpointer _)
{
  HildonFileSystemModelNode *model_node;

  model_node = node->data;
  g_free(model_node->display_text);
  model_node->display_text = NULL;
  pango_attr_list_unref(model_node->display_attrs);
  model_node->display_attrs = NULL;
  return FALSE;
}

/* Clears ->display_text and ->display_attrs of all nodes.  Used when style or
   time-format changes.  We use g_signal_connect_swapped(), as then it is
   possible to pass this function as handler. */
static void
invalidate_display_props(HildonFileSystemModel *self)
{
  g_node_traverse(self->priv->roots, G_POST_ORDER, G_TRAVERSE_ALL, -1,
		  model_node_invalidate_display_props, NULL);
}

/* Formats file_time as expected by specs.  NOTE: return value points to a
 * static buffer, don't try to free it. */
static const char *
get_date_string(GtkFileTime file_time)
{
    static char buf[128];
    gboolean format24h;
    time_t time_val;
    struct tm *time_struct;
    size_t ds, ts;

    if (file_time == 0)
	return "-";

    time_val = (time_t) file_time;
    time_struct = localtime(&time_val);

    ds = strftime(buf, sizeof(buf), dgettext("hildon-libs", "wdgt_va_date"), time_struct);
    if (ds == 0)
	return "-";

    g_object_get(_hildon_file_system_settings_get_instance(),
		 "hour24", &format24h, NULL);

    buf[ds] = ' ';
    ts = strftime(buf + ds + 1, sizeof(buf) - ds - 1,
		  dgettext("hildon-libs",
			   format24h ? "wdgt_va_24h_time"
		                     : time_struct->tm_hour > 11 ? "wdgt_va_12h_time_pm"
		   	                                         : "wdgt_va_12h_time_am"),
		  time_struct);
    if (ts == 0)
	buf[ds] = '\0';

    g_assert(ds + 1 + ts + 1 <= sizeof(buf));

    return buf;
}

/* Generates properties used by HildonFileSelection's cell renderer, cached in
   model_node->display_{text,attrs}. */
static void
generate_display_text_and_attrs(HildonFileSystemModel *model, GtkTreeIter *iter)
{
    HildonFileSystemModelNode *model_node;
    gchar *title;
    gint64 time;
    const gchar *mime;
    guint row1len;
    PangoAttrList *alist;
    GdkColor color1, color2;
    GString *text;

    model_node = ((GNode *)iter->user_data)->data;
    /* We have to get title via gtk_tree_model_get_value, because it triggers
     * loading of next level.  Sad. */
    gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
		       HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME, &title,
		       -1);

    /* type    1st row         2nd row
     * ------  -----------     --------
     * folder  title           -
     * audio   track/title     author/-
     * image   title           date
     * other   title           date, size
     */
    text = g_string_sized_new(48);
    mime = "";
    time = 0;
    if (model_node->info) {
	mime = gtk_file_info_get_mime_type(model_node->info);
	time = gtk_file_info_get_modification_time(model_node->info);
    }

    if (model_node_is_folder(model_node)) {
	g_string_append(text, title);
	row1len = text->len;
    } else if (g_str_has_prefix(mime, "audio/")) {
	gchar *track, *author;

	gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
			   HILDON_FILE_SYSTEM_MODEL_COLUMN_TITLE, &track,
			   HILDON_FILE_SYSTEM_MODEL_COLUMN_AUTHOR, &author,
			   -1);
	if (track[0])
	    g_string_append(text, track);
	else
	    g_string_append(text, title);
	row1len = text->len;

	if (author[0]) {
	    g_string_append_c(text, '\n');
	    g_string_append(text, author);
	}
	g_free(track);
	g_free(author);
    } else if (g_str_has_prefix(mime, "image/") || g_str_has_prefix(mime, "video/")) {
	g_string_append(text, title);
	row1len = text->len;
	g_string_append_c(text, '\n');
	g_string_append(text, get_date_string(time));
    } else {
	gchar *fsize;

	g_string_append(text, title);
	row1len = text->len;
	g_string_append_c(text, '\n');
	g_string_append(text, get_date_string(time));

	g_string_append(text, ", ");
	fsize = hildon_format_file_size_for_display(model_node->info ?
						    gtk_file_info_get_size(model_node->info)
						    : 0);
	g_string_append(text, fsize);
	g_free(fsize);

    }
    g_free(title);
    model_node->display_text = g_string_free(text, FALSE);

    alist = NULL;
    if (model->priv->ref_widget
	&& gtk_style_lookup_color(model->priv->ref_widget->style,
				  "DefaultTextColor", &color1)
	&& gtk_style_lookup_color(model->priv->ref_widget->style,
				  "SecondaryTextColor", &color2)) {
	PangoAttribute *row1, *row2;

	alist = pango_attr_list_new();
	row1 = pango_attr_foreground_new(color1.red, color1.green, color1.blue);
	row1->start_index = 0;
	row1->end_index = row1len;
	row2 = pango_attr_foreground_new(color2.red, color2.green, color2.blue);
	row2->start_index = row1len + 1;
	pango_attr_list_insert(alist, row1);
	pango_attr_list_insert(alist, row2);
    }
    model_node->display_attrs = alist;
}

static void hildon_file_system_model_get_value(GtkTreeModel * model,
                                               GtkTreeIter * iter,
                                               gint column, GValue * value)
{
    GNode *node;
    GtkFileInfo *info;
    GtkFilePath *path;
    GtkFileSystem *fs;
    HildonFileSystemModelNode *model_node;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    g_return_if_fail(iter && priv->stamp == iter->stamp);
#if 0  /* Enable the following to get statistics which info is requested.
          Actually TYPE and SORT_KEY are the two most often asked fields */
    {
      static gint columns[NUM_COLUMNS] = { 0 };
      gint i;

      columns[column]++;
      for (i = 0; i < NUM_COLUMNS; i++)
        g_debug("%d ", columns[i]);
    }
#endif
    g_value_init(value, priv->column_types[column]);

    node = iter->user_data;
    g_assert(node != NULL);
    model_node = node->data;
    g_assert(model_node != NULL);

    info = model_node->info;
    path = model_node->path;
    fs = priv->filesystem;
    g_assert(path != NULL);

    switch (column) {
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_GTK_PATH_INTERNAL:
        g_value_set_boxed(value, path);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_LOCAL_PATH:
        g_value_take_string
            (value, gtk_file_system_path_to_filename(fs, path));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_URI:
        g_value_take_string(value,
                           gtk_file_system_path_to_uri(fs, path));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_NAME:
        /* Gtk+'s display name contains also extension */
        if (model_node->name_cache == NULL)
          model_node->name_cache = _hildon_file_system_create_file_name(fs,
                                       path, model_node->location, info);
        g_value_set_string(value, model_node->name_cache);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME:
        if (!model_node->title_cache)
	  {
	    model_node->title_cache = 
	      _hildon_file_system_create_display_name (fs,
						       path,
						       model_node->location,
						       info);

	    /* We load this node if this is the first time someone
	       asks for its display name and if it is a folder and it
	       has not been loaded yet.
	    */

	    if (model_node->load_time == 0
		&& model_node->error == NULL
		&& (model_node->location ||
		    (info && gtk_file_info_get_is_folder (info))))
	      {
		unlink_file_folder (node);
		link_file_folder (node, model_node->path);
	      }
	  }

        g_value_set_string(value, model_node->title_cache);

        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_KEY:
        /* We cannot just use display_key from GtkFileInfo, because it is
         * case sensitive */
        if (!model_node->key_cache)
        {
          gchar *name, *casefold;

          name = _hildon_file_system_create_file_name(fs, path,
                            model_node->location, info);
          casefold = g_utf8_casefold(name, -1);
          model_node->key_cache = g_utf8_collate_key_for_filename(casefold, -1);
          g_free(casefold);
          g_free(name);
        }

        g_value_set_string(value, model_node->key_cache);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_MIME_TYPE:
        /* get_mime_type do not make a duplicate */
        g_value_set_string(value, info ? gtk_file_info_get_mime_type(info) : "");
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_SIZE:
        g_value_set_int64(value, info ? gtk_file_info_get_size(info) : 0);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_TIME:
        g_value_set_int64(value,
                          info ? gtk_file_info_get_modification_time(info) : 0);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER:
        g_value_set_boolean(value, model_node_is_folder(model_node));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE:
      g_value_set_boolean(value, model_node->available &&
            /* Folders that cause access errors are dimmed. Devices are not */
            (model_node->location ?
             hildon_file_system_special_location_is_available(model_node->location) :
             !model_node->error));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_READONLY:
        g_value_set_boolean(value, path ? path_is_readonly(fs, path) : FALSE);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_HAS_LOCAL_PATH:
        g_value_set_boolean(value,
            path ? gtk_file_system_path_is_local(fs, path) : FALSE);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_TYPE:
        g_value_set_int(value, model_node->location ?
            model_node->location->compatibility_type :
            (gtk_file_info_get_is_folder(info) ?
                HILDON_FILE_SYSTEM_MODEL_FOLDER :
                HILDON_FILE_SYSTEM_MODEL_FILE));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON:
      if (!model_node->icon_cache)
        model_node->icon_cache =
          hildon_file_system_model_create_image(priv, model_node,
                                                TREE_ICON_SIZE);

      g_value_set_object(value, model_node->icon_cache);
      break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON_COLLAPSED:
        if (!model_node->icon_cache_collapsed)
            model_node->icon_cache_collapsed =
                hildon_file_system_model_create_composite_image
                    (priv, model_node, get_collapsed_emblem(priv));

        g_value_set_object(value, model_node->icon_cache_collapsed);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON_EXPANDED:
        if (!model_node->icon_cache_expanded)
            model_node->icon_cache_expanded =
                hildon_file_system_model_create_composite_image
                    (priv, model_node, get_expanded_emblem(priv));

        g_value_set_object(value, model_node->icon_cache_expanded);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_THUMBNAIL:
        if (!model_node->thumbnail_cache)
        {
            gchar *uri = NULL;
            const gchar *mime_type = NULL;
            gboolean is_image = FALSE;
            gboolean is_audio = FALSE;

            if (path)
              uri = gtk_file_system_path_to_uri(priv->filesystem, path);
            if (info)
            {
              mime_type = gtk_file_info_get_mime_type(info);
	      is_image = mime_type && (g_str_has_prefix (mime_type, "image/")
				       || g_str_has_prefix (mime_type, "sketch/png"));
	      is_audio = mime_type && g_str_has_prefix (mime_type, "audio/");
            }

            if (is_image && hildon_thumbnail_is_cached (uri,
              THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, TRUE))
            {
              GError *error = NULL;
              gchar *thumb_uri = hildon_thumbnail_get_uri(uri, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, TRUE);
              gchar *thumb_file = g_filename_from_uri(thumb_uri, NULL, NULL);
              g_free(thumb_uri);
              model_node->thumbnail_cache = gdk_pixbuf_new_from_file_at_size(thumb_file, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, &error);
              g_free(thumb_file);
              if (error == NULL)
              {
                  g_free(uri);
                  break;
              }
              g_debug("Failed to load cached thumbnail: %s", error->message);
              g_error_free(error);
            }

            if (is_image)
            {
              if (!model_node->thumbnail_request)
              {  /* This can fail with GtkFileSystemUnix if the
                           name contains invalid UTF-8 */
                HildonThumbnailFactory *factory;

		g_object_ref(model_node->model);
                factory = hildon_thumbnail_factory_get_instance();
                model_node->thumbnail_request =
                    hildon_thumbnail_factory_request_pixbuf(factory,
                        uri, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, TRUE,
                        gtk_file_info_get_mime_type(info),
                        thumbnail_request_pixbuf_cb, node, NULL);
                g_object_unref(factory);
              }

              /* the following if clause handles the hourglass icon */
              if (!model_node->thumbnail_cache)
              {
                HildonMimeCategory cat =
                    hildon_mime_get_category_for_mime_type(mime_type);

                if (cat == HILDON_MIME_CATEGORY_IMAGES)
                  model_node->thumbnail_cache =
                      _hildon_file_system_load_icon_cached(
                        gtk_icon_theme_get_default(),
                        "filemanager_file_loading", THUMBNAIL_ICON);
              }
            }

            /* Tracker get the albumart and stores it according to the spec
             * http://live.gnome.org/MediaArtStorageSpec
             * Then, it generates a thumbnail. We use that thumbnail instead
             * of the generic music icon.
             *
             * FIXME: if there is several audio files from the same albums, we
             * load the same thumbnail for each of them. It would be better to
             * load it only one time.
             * FIXME: if Tracker generates the albumart and thumbnail later, it
             * does not update the icon.
             */
            if (is_audio)
            {
              gchar *album;
              gchar *album_art;
              gchar *album_art_uri;
              gchar *thumbnail_uri;
              gchar *thumbnail_file;

              /* We have to get album via gtk_tree_model_get_value, because
               * it triggers loading of next level.  Sad. */
              gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
                  HILDON_FILE_SYSTEM_MODEL_COLUMN_ALBUM, &album,
                  -1);

              /* Fremantle does not use 'artist' to find the albumart. The
               * decision to drop the artist from the name effectively was
               * made based on requests from mafw/media player. This was
               * needed to support albums with variable artists. Hence the
               * NULL parameter.
               */
              album_art = hildon_albumart_get_path (NULL, album, "album");
              album_art_uri = g_filename_to_uri (album_art, NULL, NULL);
              thumbnail_uri = hildon_thumbnail_get_uri (album_art_uri,
                  THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, TRUE);
              thumbnail_file = g_filename_from_uri(thumbnail_uri, NULL, NULL);

              model_node->thumbnail_cache = gdk_pixbuf_new_from_file_at_size
                (thumbnail_file, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, NULL);

              g_free (thumbnail_uri);
              g_free (thumbnail_file);
              g_free (album_art_uri);
              g_free (album_art);
            }

            g_free(uri);

            if (!model_node->thumbnail_cache)
              model_node->thumbnail_cache =
                 hildon_file_system_model_create_image(priv, model_node,
                                                       THUMBNAIL_ICON);
        }

        g_value_set_object(value, model_node->thumbnail_cache);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_LOAD_READY:
        g_value_set_boolean(value, is_node_loaded(node));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_FREE_SPACE:
        g_warning("USING FREE SPACE COLUMN IS DEPRECATED");
        g_value_set_int64(value, 0);
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_TITLE:
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_AUTHOR:
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_ALBUM:
    {
#ifdef UPSTREAM_DISABLED
        const gchar *keys [] =  {
            "Audio:Artist",
            "Audio:Title",
            "Audio:Album",
            NULL
        };
        gchar **arr;
        GError *error=NULL;

        if (!model_node->thumb_author)
        {
            gchar *str = gtk_file_system_path_to_filename(priv->filesystem, path);
            arr = tracker_metadata_get(priv->tracker_client, SERVICE_MUSIC, str, keys, &error);
            g_free(str);

            if (G_UNLIKELY(error ))
            {
                g_error_free(error);
                model_node->thumb_author = g_strdup("");
                model_node->thumb_title = g_strdup("");
                model_node->thumb_album = g_strdup("");
            }
            else if (arr != NULL && arr[0] != NULL
                && arr[1] != NULL && arr[2] != NULL)
            {
                model_node->thumb_author = g_strdup(arr[0]);
                model_node->thumb_title = g_strdup(arr[1]);
                model_node->thumb_album = g_strdup(arr[2]);
            }
            else
            {
                model_node->thumb_author = g_strdup("");
                model_node->thumb_title = g_strdup("");
                model_node->thumb_album = g_strdup("");
            }
            g_strfreev(arr);
        }

        if (column == HILDON_FILE_SYSTEM_MODEL_COLUMN_AUTHOR)
          g_value_set_string(value, model_node->thumb_author);
        else if (column == HILDON_FILE_SYSTEM_MODEL_COLUMN_TITLE)
          g_value_set_string(value, model_node->thumb_title);
        else if (column == HILDON_FILE_SYSTEM_MODEL_COLUMN_ALBUM)
          g_value_set_string(value, model_node->thumb_album);
        else
          g_assert_not_reached ();
        break;
#else
      if (!model_node->thumb_author)
      {
          g_warning("Tracker support not implemented, using dummy values");
          model_node->thumb_author = g_strdup("Author");
          model_node->thumb_title = g_strdup("Title");
          model_node->thumb_album = g_strdup("Album");
      }
      if (column == HILDON_FILE_SYSTEM_MODEL_COLUMN_AUTHOR)
        g_value_set_string(value, model_node->thumb_author);
      else if (column == HILDON_FILE_SYSTEM_MODEL_COLUMN_TITLE)
        g_value_set_string(value, model_node->thumb_title);
      else if (column == HILDON_FILE_SYSTEM_MODEL_COLUMN_ALBUM)
        g_value_set_string(value, model_node->thumb_album);
      else
        g_assert_not_reached ();
      break;
#endif
    }
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_HIDDEN:
    {
      gboolean result;

      if (model_node->location)
        result = !hildon_file_system_special_location_is_visible(model_node->location, g_node_first_child(node) != NULL);
      else if (!info)
        result = FALSE;
      else
        result = gtk_file_info_get_is_hidden(info);

      if (result == TRUE)
        {
          /* When this item is actually hidden, and it is a special
             location, we queue it for reload if it hasn't been loaded
             at all yet.  Special locations can become visible when
             they have children, and we need to scan them to figure
             this out.
          */

          if (model_node->location
              && model_node->load_time == 0
              && (!hildon_file_system_special_location_requires_access
                  (model_node->location)))
            {
              g_debug ("SCANNING FOR VISIBILITY: %s\n", (char*) model_node->path);
              _hildon_file_system_model_queue_reload
                (HILDON_FILE_SYSTEM_MODEL(model), iter, FALSE);
            }
        }

      g_value_set_boolean(value, result);
      break;
    }
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_UNAVAILABLE_REASON:
        g_value_take_string(value, model_node->location ?
            hildon_file_system_special_location_get_unavailable_reason(model_node->location) :
            g_strdup(_("sfil_ib_opening_not_allowed")));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_FAILED_ACCESS_MESSAGE:
        if (model_node->location && model_node->location->failed_access_message)
        {
          if (!model_node->title_cache)
            model_node->title_cache = _hildon_file_system_create_display_name(
                fs, path, model_node->location, info);
          g_value_take_string(value,
                g_strdup_printf(model_node->location->failed_access_message,
                model_node->title_cache));
        }
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_WEIGHT:
        /* Temporary version of the weight calculation */
        g_value_set_int(value, model_node->location ?
                model_node->location->sort_weight : (
            (info && gtk_file_info_get_is_folder(info)) ?
                SORT_WEIGHT_FOLDER : SORT_WEIGHT_FILE));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_EXTRA_INFO:
        if (model_node->location)
            g_value_take_string(value,
                hildon_file_system_special_location_get_extra_info(model_node->location));
        break;
    case HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_DRIVE:
        g_value_set_boolean (value, is_drive (model_node));
        break;
    case PRIV_COLUMN_DISPLAY_TEXT:
	if (!model_node->display_text)
	    generate_display_text_and_attrs(HILDON_FILE_SYSTEM_MODEL(model), iter);
	g_value_set_string(value, model_node->display_text);
	break;
    case PRIV_COLUMN_DISPLAY_ATTRS:
	if (!model_node->display_attrs)
	    generate_display_text_and_attrs(HILDON_FILE_SYSTEM_MODEL(model), iter);
	g_value_set_boxed(value, model_node->display_attrs);
	break;
    default:
        g_assert_not_reached();
    };
}

static gboolean hildon_file_system_model_iter_next(GtkTreeModel * model,
                                                   GtkTreeIter * iter)
{
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    g_return_val_if_fail(iter != NULL, FALSE);
    g_return_val_if_fail(priv->stamp == iter->stamp, FALSE);
    iter->user_data = g_node_next_sibling((GNode *) iter->user_data);

    return iter->user_data != NULL;
}

static gint hildon_file_system_model_iter_n_children(GtkTreeModel * model,
                                                     GtkTreeIter * iter)
{
    HildonFileSystemModel *self;
    HildonFileSystemModelPrivate *priv;

    g_assert(HILDON_IS_FILE_SYSTEM_MODEL(model));
    self = HILDON_FILE_SYSTEM_MODEL(model);
    priv = self->priv;

    if (iter == NULL)   /* Roots are always in tree, we don't need to ask
                           loading */
        return g_node_n_children(priv->roots);

    g_return_val_if_fail(priv->stamp == iter->stamp, 0);

    return g_node_n_children(iter->user_data);
}

static gboolean hildon_file_system_model_iter_has_child(GtkTreeModel *
                                                        model,
                                                        GtkTreeIter * iter)
{
    return (hildon_file_system_model_iter_n_children(model, iter) > 0);
}

static gboolean hildon_file_system_model_iter_nth_child(GtkTreeModel *
                                                        model,
                                                        GtkTreeIter * iter,
                                                        GtkTreeIter *
                                                        parent, gint n)
{
    GNode *node;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    if (parent) {
        g_return_val_if_fail(parent->stamp == priv->stamp, FALSE);
        node = g_node_nth_child(parent->user_data, n);
    } else
        node = g_node_nth_child(priv->roots, n);

    iter->stamp = priv->stamp;
    iter->user_data = node;

    return node != NULL;
}

static gboolean hildon_file_system_model_iter_children(GtkTreeModel *
                                                       model,
                                                       GtkTreeIter * iter,
                                                       GtkTreeIter *
                                                       parent)
{
    return hildon_file_system_model_iter_nth_child(model, iter, parent, 0);
}

static gboolean hildon_file_system_model_iter_parent(GtkTreeModel * model,
                                                     GtkTreeIter * iter,
                                                     GtkTreeIter * child)
{
    GNode *node;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    g_return_val_if_fail(child->stamp == priv->stamp, FALSE);

    node = child->user_data;
    iter->stamp = priv->stamp;
    iter->user_data = node->parent;

    return node->parent != NULL && node->parent != priv->roots;
}

/*********************************************/
/* End of GTK_TREE_MODEL interface methods */
/*********************************************/

static gint path_compare_helper(gconstpointer a, gconstpointer b)
{
    /* This is really a macro, so we cannot pass it directly to search
       function */
    if (a == NULL || b == NULL)
      return 1;

    return gtk_file_path_compare((GtkFilePath *) a, (GtkFilePath *) b);
}

static GNode
    *hildon_file_system_model_search_folder(GtkFileFolder * folder)
{
    return (GNode *) g_object_get_qdata(G_OBJECT(folder),
                                        hildon_file_system_model_quark);
}

static void hildon_file_system_model_files_added (GtkFileFolder * monitor,
						  GSList * paths,
						  gpointer data);

typedef struct {
  GtkFileFolder *monitor;
  GSList *paths;
  gpointer data;
  GSList *next_path;
  gboolean all_new;
} dfa_clos;

static gboolean
dfa_run (gpointer data)
{
  GNode *node;
  HildonFileSystemModelNode *model_node;

  dfa_clos *c = (dfa_clos *)data;
  
  GDK_THREADS_ENTER ();
  
  node = hildon_file_system_model_search_folder (c->monitor);
  if (node)
    {
      gint i;
      model_node = node->data;
/*       model_node->pending_adds -= g_slist_length (c->paths); */
      model_node->pending_adds = 0; //no need to count
      i = 0;
      while (c->next_path && i < MAX_BATCH)
	  {
              hildon_file_system_model_add_node (GTK_TREE_MODEL (model_node->model),
                                                 node,
                                                 c->monitor,
                                                 (GtkFilePath *) c->next_path->data,
                                                 TRUE);
              c->next_path = c->next_path->next;
              i++;
	  }
      model_node->pending_adds = (c->next_path != NULL)? 1 : 0;
    }

  GDK_THREADS_LEAVE ();

  if (node) {
      emit_node_changed (node);

      if (is_node_loaded (node))
          handle_finished_node (node);
  }
  if (c->next_path)
    return TRUE;

  g_object_unref (c->monitor);
  gtk_file_paths_free (c->paths);
  g_object_unref (c->data);
  g_free (c);

  return FALSE;
}

static void
delay_files_added (GtkFileFolder * monitor,
		   GSList * paths,
		   gpointer data,
                   gboolean all_new)
{
  GNode *node;
  HildonFileSystemModelNode *model_node;

  node = hildon_file_system_model_search_folder (monitor);
  if (node)
    {
      dfa_clos *c = g_new0 (dfa_clos, 1);
      
      g_object_ref (monitor);
      paths = gtk_file_paths_copy (paths);
      g_object_ref (data);
      
      model_node = node->data;
/*       model_node->pending_adds += g_slist_length (paths); */
      model_node->pending_adds = 1; // it is faster than counting all items

      c->monitor = monitor;
      c->paths = paths;
      c->data = data;
      c->next_path = paths;
      c->all_new = all_new;
      
      g_idle_add (dfa_run, c);
    }
}

static void hildon_file_system_model_files_added(GtkFileFolder * monitor,
                                                 GSList * paths,
                                                 gpointer data)
{
  /* The files identified by PATHS have been created.  Normally, they
     are children of MONITOR, but it might happen that the file
     corresponding to MONITOR is among PATHS, too.  This happens when
     you start monitoring a filename before it exists.

     XXX - maybe this situation should be filtered out by the GnomeVFS
           GtkFileSystem backend, depending on what guarantees the
           "files-added" signal makes.
  */

  if (paths != NULL)
  {
    GNode *node;
    HildonFileSystemModelNode *model_node;

    node = hildon_file_system_model_search_folder(monitor);
    if (node != NULL)
      {
	gint i;
	GtkTreeModel *model;
        GtkFilePath *real_path;
        gboolean all_new = TRUE;

	model_node = node->data;
	model = GTK_TREE_MODEL (model_node->model);

	i = 0;
	while (paths && i < MAX_BATCH)
            {
                if (model_node->location)
                    real_path = hildon_file_system_special_location_rewrite_path (model_node->location, CAST_GET_PRIVATE(model)->filesystem, paths->data);
                else
                    real_path = gtk_file_path_copy (paths->data);
              
                //search whether this node already exists
                if (hildon_file_system_model_search_path_internal (node, real_path, FALSE) &&
		    !g_str_has_prefix (gtk_file_path_get_string(real_path), "upnpav://") &&
		    !g_str_has_prefix (gtk_file_path_get_string(real_path), "file:///media/")) {
                    //node already exists no need to add
                    all_new = FALSE;
                } 
		else {
                    hildon_file_system_model_add_node (model,
                                                       node,
                                                       monitor,
                                                       (GtkFilePath *) paths->data,
                                                       TRUE);
                }
                gtk_file_path_free(real_path);
                paths = paths->next;
                i++;
            }

	emit_node_changed (node);

	if (paths)
            delay_files_added (monitor, paths, data, all_new);

	if (is_node_loaded (node))
	  handle_finished_node (node);
      }
    else
      g_warning("Data destination not found!");
  }
}

static void hildon_file_system_model_files_removed(GtkFileFolder * monitor,
                                                   GSList * paths,
                                                   gpointer data)
{
  if (paths != NULL)
  {
    GNode *node;

    g_debug("Removing files (monitor = %p)", (void *) monitor);

    node = hildon_file_system_model_search_folder(monitor);
    if (node != NULL)
        hildon_file_system_model_remove_node_list(data, node, paths);
    else
        g_warning("Data destination not found!");
  }
}

static void hildon_file_system_model_dir_removed(GtkFileFolder * monitor,
                                                 gpointer data)
{
    g_warning("Dir removed callback called, but this method is not "
               "implemented (and probably there is no need to implement "
               "it either).");
}

static void hildon_file_system_model_files_changed(GtkFileFolder * monitor,
                                                   GSList * paths,
                                                   gpointer data)
{
  if (paths != NULL)
  {
    GNode *node;

    g_debug("Files changed (monitor = %p)", (void *) monitor);

    node = hildon_file_system_model_search_folder(monitor);
    if (node != NULL)
        hildon_file_system_model_change_node_list(data, node, monitor,
                                                  paths);
    else
        g_warning("Data destination not found!");
  }
}

static void hildon_file_system_model_folder_finished_loading(GtkFileFolder *monitor, gpointer data)
{
  GNode *node = hildon_file_system_model_search_folder(monitor);
  if (node)
    {
      g_debug("Finished loading (monitor = %p)", (void *) monitor);
      if (is_node_loaded (node))
	handle_finished_node (node);
    }
}

static GNode *
hildon_file_system_model_search_path_internal (GNode *parent_node,
                                               const GtkFilePath *path,
                                               gboolean recursively)
{
    const gchar *folder_string, *test_string;
    GNode *node;
    HildonFileSystemModelNode *model_node;

    g_assert(parent_node != NULL && path != NULL);

    /* Not allocated dynamically */
    folder_string = gtk_file_path_get_string(path);

    model_node = parent_node->data;

    /* First consider the parent itself.
     */
    if (model_node)
      {
        test_string = gtk_file_path_get_string (model_node->path);
        if (_hildon_file_system_compare_ignore_last_separator (folder_string,
                                                               test_string))
          return parent_node;
      }

    for (node = g_node_first_child(parent_node); node;
         node = g_node_next_sibling(node)) {
        model_node = node->data;

        test_string = gtk_file_path_get_string(model_node->path);

        if (_hildon_file_system_compare_ignore_last_separator (folder_string,
                                                               test_string))
            return node;

        if (recursively) {
            /* Allways peek into devices, since they can include different
               base locations within them */
          gint test_len = strlen(test_string);

          if (model_node->location ||
                (g_ascii_strncasecmp(folder_string, test_string, test_len) == 0
                 && (folder_string[test_len] == G_DIR_SEPARATOR)))
          {
                GNode *result =
                      hildon_file_system_model_search_path_internal(node,
                                                                    path,
                                                                    TRUE);
                if (result) return result;
          }
        }
    }

    return NULL;
}

static void hildon_file_system_model_send_has_child_toggled(GtkTreeModel *
                                                            model,
                                                            GNode *
                                                            parent_node)
{
    GtkTreePath *tree_path;
    GtkTreeIter iter;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    iter.stamp = priv->stamp;
    iter.user_data = parent_node;
    tree_path = hildon_file_system_model_get_path(model, &iter);
    gtk_tree_model_row_has_child_toggled(model, tree_path, &iter);
    gtk_tree_path_free(tree_path);
}

static void
unlink_file_folder(GNode *node)
{
  HildonFileSystemModelNode *model_node = node->data;
  g_assert(model_node != NULL);

  if (model_node->get_folder_handle)
    {
      GtkFileSystemHandle *handle = model_node->get_folder_handle;
      if (handle->file_system)
	gtk_file_system_cancel_operation (model_node->get_folder_handle);
      else
	{
	  /* This is a special handle created by one of our special
	     locations.  It is not associated with any GtkFileSystem
	     and the operation can not be cancelled.  But since the
	     node might be going away, we set the cancelled flag
	     directly so that get_folder_callback does the right
	     thing.
	  */
	  handle->cancelled = TRUE;
	}
      model_node->get_folder_handle = NULL;
    }

  if (model_node->folder)
    {
      g_object_set_qdata(G_OBJECT(model_node->folder),
			 hildon_file_system_model_quark, NULL);
      
      g_signal_handlers_disconnect_by_func
        (model_node->folder,
         (gpointer) hildon_file_system_model_dir_removed,
         model_node->model);
      g_signal_handlers_disconnect_by_func
        (model_node->folder,
         (gpointer) hildon_file_system_model_files_added,
         model_node->model);
      g_signal_handlers_disconnect_by_func
        (model_node->folder,
         (gpointer) hildon_file_system_model_files_removed,
         model_node->model);
      g_signal_handlers_disconnect_by_func
        (model_node->folder,
         (gpointer) hildon_file_system_model_files_changed,
         model_node->model);
      g_signal_handlers_disconnect_by_func
        (model_node->folder,
         (gpointer) hildon_file_system_model_folder_finished_loading,
         model_node->model);

      g_object_unref(model_node->folder);
      model_node->folder = NULL;
    }
}

static void
free_handle_data (HandleData *handle_data)
{
    g_object_unref (handle_data->model);
    g_slice_free (HandleData, handle_data);
}

static void
get_folder_callback (GtkFileSystemHandle *handle,
                     GtkFileFolder *folder,
                     const GError *error,
                     gpointer data)
{
  gboolean cancelled = handle->cancelled;
  HildonFileSystemModelNode *model_node;
  HildonFileSystemModel *model;
  HandleData *handle_data = (HandleData *) data;
  GNode *node;

  g_object_unref (handle);

  /* When the operation has been cancelled, handle_data->node is no
     longer valid.
   */
  if (cancelled)
    {
      g_debug ("LINK CANCELLED\n");
      free_handle_data (handle_data);
      return;
    }

  node = handle_data->node;
  model_node = (HildonFileSystemModelNode *) node->data;
  model = model_node->model;

  model_node->get_folder_handle = NULL;
  model_node->folder = folder;
  model_node->error = error? g_error_copy (error) : NULL;
  model_node->linking = FALSE;

  if (folder == NULL)
    {
      g_warning("Failed to create monitor for path %s",
                 gtk_file_path_get_string (model_node->path));
      if (model_node->error == NULL)
	model_node->error = g_error_new (G_FILE_ERROR, G_FILE_ERROR_FAILED,
					 "failure");
    }

  g_debug ("LINK DONE %s %s %p\n",
       (char *)model_node->path, error? error->message : "(success)",
       folder);
  
  if (model_node->error)
    {
      handle_finished_node (node);
      handle_load_error (node);
      free_handle_data (handle_data);
      return;
    }

  g_signal_connect_object
    (model_node->folder, "deleted",
     G_CALLBACK(hildon_file_system_model_dir_removed),
     model, 0);

  g_object_set_qdata (G_OBJECT(model_node->folder),
                      hildon_file_system_model_quark, node);

  g_signal_connect_object
    (model_node->folder, "files-added",
     G_CALLBACK(hildon_file_system_model_files_added),
     model, 0);
  g_signal_connect_object
    (model_node->folder, "files-removed",
     G_CALLBACK
     (hildon_file_system_model_files_removed), model, 0);
  g_signal_connect_object
    (model_node->folder, "files-changed",
     G_CALLBACK
     (hildon_file_system_model_files_changed), model, 0);
  g_signal_connect_object
    (model_node->folder, "finished-loading",
     G_CALLBACK (hildon_file_system_model_folder_finished_loading), model,
     0);

  /* The following has to be done last since it might do anything to
     model_node, including loading it again.
  */

  if (gtk_file_folder_is_finished_loading (folder))
    {
      GSList *children = NULL;
      gboolean result;

      g_debug ("LINK FINISHED %s\n", (char *)model_node->path);

      result = gtk_file_folder_list_children
        (folder, &children, &(model_node->error));
      if (result)
        {
	  hildon_file_system_model_files_added (model_node->folder,
						children,
						model);

	  /* XXX - We assume that the root node has less than
   	           MAX_BATCH entries and that has thus been added
   	           completely now.
	  */
	  if (model_node->location
	      && HILDON_IS_FILE_SYSTEM_ROOT (model_node->location))
	    model->priv->first_root_scan_completed = TRUE;

          hildon_file_system_model_folder_finished_loading
            (model_node->folder, model);
          gtk_file_paths_free (children);
        }
      else
	handle_load_error (node);
    }

  free_handle_data (handle_data);
}

static gboolean
link_file_folder (GNode *node, const GtkFilePath *path)
{
  HildonFileSystemModel *model;
  HildonFileSystemModelNode *model_node;
/*  GtkFileFolder *parent_folder; */
  HandleData *handle_data;
  GNode *child_node;

  g_assert(node != NULL && path != NULL);
  model_node = node->data;
  g_assert(model_node != NULL);

  /* Folder already exists or we have already asked for it.
   */
  if (model_node->folder || model_node->get_folder_handle)
    return TRUE;

  g_debug ("LINK %s\n", (char *)model_node->path);

  model = model_node->model;
  g_assert(HILDON_IS_FILE_SYSTEM_MODEL(model));

  model_node->load_time = time(NULL);
  model_node->linking = TRUE;

  g_debug("%s", (char *) path);

  if (!model_node->path)
    model_node->path = gtk_file_path_copy(path);

/*  parent_folder = (node->parent && node->parent->data) ?
      ((HildonFileSystemModelNode *) node->parent->data)->folder : NULL; */

  /* Reset the present_flags.
   */
  child_node = g_node_first_child(node);
  while (child_node)
    {
      HildonFileSystemModelNode *model_node = child_node->data;
      model_node->present_flag = FALSE;
      child_node = g_node_next_sibling(child_node);
    }

  /* hold a reference to the model, it will be released
   * when the get_folder operation has finished
   */
  handle_data = g_slice_new (HandleData);
  handle_data->model = g_object_ref (model);
  handle_data->node = node;

  if (model_node->location)
    {
      model_node->get_folder_handle =
        hildon_file_system_special_location_get_folder
          (model_node->location,
           model->priv->filesystem,
           path, GTK_FILE_INFO_ALL,
           get_folder_callback, handle_data);
    }
  else
    {
      model_node->get_folder_handle =
        gtk_file_system_get_folder (model->priv->filesystem,
                                    path, GTK_FILE_INFO_ALL,
                                    get_folder_callback, handle_data);
    }

  if (model_node->get_folder_handle == NULL)
    {
      model_node->linking = FALSE;
      free_handle_data (handle_data);
      return FALSE;
    }
  else
    {
      g_clear_error (&(model_node->error));
      return TRUE;
    }
}

static gboolean hildon_file_system_model_destroy_model_node(GNode * node,
                                                        gpointer data)
{
    HildonFileSystemModelNode *model_node = node->data;
    g_assert(HILDON_IS_FILE_SYSTEM_MODEL(data));

    if (model_node)
    {
      g_debug("Remove [%s]", (const char *) model_node->path);

      gtk_file_path_free(model_node->path);
      unlink_file_folder(node);

      if (model_node->info)
      {
        gtk_file_info_free (model_node->info);
        model_node->info = NULL;
      }

      g_clear_error(&model_node->error);
      clear_model_node_caches(model_node);

      if (model_node->location) {
          /* We don't want to save the actual ID:s, since that would
             needlessly increase the memory consumption by 2 ints per item.
             Ensure that all expected handlers were disconnected. */
          gint check = g_signal_handlers_disconnect_matched(
            model_node->location, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, node);
          g_assert(check == 3);
          g_object_unref(model_node->location);
      }

      g_free(model_node);
      node->data = NULL;
    }

    return FALSE;
}

/* Kicks off the node and all the children. Both GNodes and ModelNodes.
    returns the next sibling of the deleted node */
static GNode *
hildon_file_system_model_kick_node(GNode *node, gpointer data)
{
  HildonFileSystemModelPrivate *priv;
  GNode *parent_node;
  GNode *destroy_node = node;

  GtkTreePath *tree_path;
  GtkTreeIter iter;

  priv = CAST_GET_PRIVATE(data);

  g_assert(node != NULL && (node->parent != NULL || node == priv->roots));

  iter.stamp = priv->stamp;
  iter.user_data = destroy_node;
  tree_path =
        hildon_file_system_model_get_path(GTK_TREE_MODEL(data), &iter);

  gtk_tree_model_row_deleted(GTK_TREE_MODEL(data), tree_path);
  gtk_tree_path_free(tree_path);

  parent_node = node->parent;
  node = g_node_next_sibling(node);

  g_node_traverse(destroy_node, G_POST_ORDER, G_TRAVERSE_ALL,
      -1, hildon_file_system_model_destroy_model_node, data);

  g_node_destroy(destroy_node);

  if (parent_node && parent_node != priv->roots && parent_node->children ==NULL)
    hildon_file_system_model_send_has_child_toggled( GTK_TREE_MODEL(data),
                                                     parent_node);

  return node;
}

static gboolean notify_volumes_changed(GNode *node, gpointer data)
{
    HildonFileSystemModelNode *model_node = node->data;
    HildonFileSystemVoldev *voldev = NULL;
 
    if (model_node->location){
	hildon_file_system_special_location_volumes_changed(model_node->location);
	/* check if the special location is voldev */
	if (HILDON_IS_FILE_SYSTEM_VOLDEV(model_node->location)){
	  if (model_node->model == NULL){
	     g_warning("hildon tree model is NULL");
	  }
	  else {
	     voldev = HILDON_FILE_SYSTEM_VOLDEV (model_node->location);
	     if ((voldev->vol_type == EXT_CARD) || 
		(voldev->vol_type == USB_STORAGE) ||
		(voldev->vol_type == INT_CARD)){
                voldev->mount = find_mount(model_node->location->basepath);
               if (voldev->mount != NULL){
		 if (hildon_file_system_voldev_is_visible(model_node->location, FALSE) == TRUE){ 
       g_signal_emit(model_node->model, signals[VOLDEV_MOUNTED],
                     0, model_node->location->basepath);
		 }
	       }
	     }
	  }
	}
    }
    return FALSE;
}

static void real_volumes_changed(GtkFileSystem *fs, gpointer data)
{
    HildonFileSystemModel *model;
    HildonFileSystemModelPrivate *priv;

    model = HILDON_FILE_SYSTEM_MODEL (data);
    priv = CAST_GET_PRIVATE(model);

    g_node_traverse(priv->roots,
            G_PRE_ORDER, G_TRAVERSE_ALL, -1,
            notify_volumes_changed, fs);
}

static GNode *
hildon_file_system_model_add_node (GtkTreeModel * model,
				   GNode * parent_node,
				   GtkFileFolder * parent_folder,
				   const GtkFilePath *path,
                                   gboolean with_search)
{
    GNode *node;
    HildonFileSystemModelPrivate *priv;
    HildonFileSystemModelNode *parent_model_node, *model_node;
    GtkFileInfo *file_info = NULL;
    GtkTreePath *tree_path;
    GtkTreeIter iter;
    GtkFilePath *real_path;

    /* Path can be NULL for removable devices that are not present */
    g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), NULL);
    g_return_val_if_fail(parent_node != NULL, NULL);
    g_return_val_if_fail(path != NULL, NULL);

    priv = CAST_GET_PRIVATE(model);

    parent_model_node = parent_node->data ? parent_node->data : NULL;

    if (parent_model_node && parent_model_node->location)
      real_path =
	hildon_file_system_special_location_rewrite_path (parent_model_node->location,
							  priv->filesystem,
							  path);
    else
      real_path = gtk_file_path_copy (path);

    g_debug ("Adding %s (%s)\n", (const char *)path, (const char *) real_path);

    if (parent_folder) {
        GError *error = NULL;
        /* This can cause main loop execution on vfs backend */

	/* We need to use PATH instead of REAL_PATH here since
	   PARENT_FOLDER only knows about the original paths.
	*/

      /* If we have received the path that we are adding by some other
         means than listing the folder (like via some API function),
         then the backend will send us "files-added" signal immediately
         after we first time use it. This is *not good*, because it
         places "garbage list" info processing queue and rest of
         the model believes that model is loading. See bug #14040. */
        g_signal_handlers_block_by_func(parent_folder,
          hildon_file_system_model_files_added, model);
        file_info = gtk_file_folder_get_info(parent_folder, path, &error);
        g_signal_handlers_unblock_by_func(parent_folder,
          hildon_file_system_model_files_added, model);

        /* If file is created and then renamed it can happen that file
         * with this name no longer exists. */
        if (error)
        {
	  g_debug ("ADD ERR %s\n", error->message);
          g_error_free(error);
	  gtk_file_path_free (real_path);
          return NULL;
        }

        g_assert(file_info != NULL);
    }

    /* First check if this item is already part of the model */
    if (with_search)
    {
      node =
        hildon_file_system_model_search_path_internal (parent_node,
                                                       real_path, FALSE);

        if (node) {
            HildonFileSystemModelNode *model_node = node->data;
            g_assert(model_node);
            model_node->present_flag = TRUE;
	    if (model_node->info)
	      gtk_file_info_free (model_node->info);
	    model_node->info = file_info;
	    gtk_file_path_free (real_path);
            return node;
        }
    }

    model_node = g_new0(HildonFileSystemModelNode, 1);
    model_node->info = file_info;
    model_node->model = HILDON_FILE_SYSTEM_MODEL(model);
    model_node->present_flag = TRUE;
    model_node->available = TRUE;
    model_node->path = real_path;

    node = g_node_new(model_node);
    g_node_append(parent_node, node);

    if (!parent_folder
	|| (file_info && gtk_file_info_get_is_folder(file_info))
	|| g_str_has_prefix (gtk_file_path_get_string (path), "obex:///"))
    {
	model_node->location =
	    _hildon_file_system_get_special_location(real_path);
        setup_node_for_location(node);
    }

    /* The following should be replaced by appending the functionality into
       GtkFileInfo, but this requires API changes into gtkfilesystem.h,
       gtkfilesystemunix.h and gtkfilesystemgnomevfs.h (as well as into
       memory backend). Currently we can handle only local files, but
       that's better than nothing... */
    if (!model_node->location)
    {
      gchar *local_path = gtk_file_system_path_to_filename(priv->filesystem,
                                                           real_path);

      if (local_path)
      {
        if (access(local_path, R_OK) != 0)
        {
          GFileError code = g_file_error_from_errno(errno);
          if (code == G_FILE_ERROR_ACCES)
            g_set_error(&model_node->error, G_FILE_ERROR, code, local_path);
        }

        g_free(local_path);
      }
    }

    /* We need to report first that new like has been inserted */

    iter.stamp = priv->stamp;
    iter.user_data = node;
    tree_path = hildon_file_system_model_get_path(model, &iter);
    gtk_tree_model_row_inserted(model, tree_path, &iter);
    gtk_tree_path_free(tree_path);

    if (/* g_node_n_children(parent_node) <= 1 */ g_node_nth_child (parent_node, 1) == NULL /* means - no second child, should work faster */ &&
        parent_node != priv->roots) {
        /* Don't emit signals for fake root */
        hildon_file_system_model_send_has_child_toggled(model,
                                                        parent_node);
    }

    return node;
}

static void
clear_model_node_caches(HildonFileSystemModelNode *model_node)
{
  if (model_node->icon_cache)
  {
    g_object_unref(model_node->icon_cache);
    model_node->icon_cache = NULL;
  }
  if (model_node->icon_cache_expanded)
  {
    g_object_unref(model_node->icon_cache_expanded);
    model_node->icon_cache_expanded = NULL;
  }
  if (model_node->icon_cache_collapsed)
  {
    g_object_unref(model_node->icon_cache_collapsed);
    model_node->icon_cache_collapsed = NULL;
  }
  if (model_node->thumbnail_cache)
  {
    g_object_unref(model_node->thumbnail_cache);
    model_node->thumbnail_cache = NULL;
  }
  if (model_node->thumbnail_request)
  {
    hildon_thumbnail_request_unqueue(model_node->thumbnail_request);
    g_object_unref (model_node->thumbnail_request);
    model_node->thumbnail_request = NULL;
  }

  g_free(model_node->display_text);
  model_node->display_text = NULL;
  pango_attr_list_unref(model_node->display_attrs);
  model_node->display_attrs = NULL;

  g_free(model_node->title_cache);
  g_free(model_node->name_cache);
  g_free(model_node->key_cache);
  model_node->title_cache = NULL;
  model_node->key_cache = NULL;
  model_node->name_cache = NULL;

  if(model_node->thumb_title)
  {
    g_free(model_node->thumb_title);
    model_node->thumb_title = NULL;
  }
  if(model_node->thumb_author)
  {
    g_free(model_node->thumb_author);
    model_node->thumb_author = NULL;
  }
  if(model_node->thumb_album)
  {
    g_free(model_node->thumb_album);
    model_node->thumb_album = NULL;
  }
}

static void hildon_file_system_model_remove_node_list(GtkTreeModel * model,
                                                      GNode * parent_node,
                                                      GSList * children)
{
    GNode *child_node = g_node_first_child(parent_node);

    while (child_node) {
        HildonFileSystemModelNode *model_node = child_node->data;
        if (g_slist_find_custom
            (children, model_node->path, path_compare_helper))
            child_node = hildon_file_system_model_kick_node(child_node, model);
        else
            child_node = g_node_next_sibling(child_node);
    }
}

static void hildon_file_system_model_change_node_list(GtkTreeModel * model,
                                                      GNode * parent_node,
                                                      GtkFileFolder *
                                                      folder,
                                                      GSList * children)
{
    GNode *node;

    g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));
    g_return_if_fail(parent_node != NULL);
    g_return_if_fail(GTK_IS_FILE_FOLDER(folder));
    g_return_if_fail(children != NULL);

    for (node = g_node_first_child(parent_node); node;
         node = g_node_next_sibling(node)) {
        HildonFileSystemModelNode *model_node = node->data;

        if (g_slist_find_custom
            (children, model_node->path, path_compare_helper)) {

            g_debug("Path changed [%s]", (char *) model_node->path);

            /* Ok, current node is updated. We need to refresh it and send
               needed signals. Visible information of special nodes is not going to change */

            clear_model_node_caches(model_node);

            if (model_node->info && !model_node->location)
            {
              GError *error = NULL;

              gtk_file_info_free(model_node->info);

              model_node->info =
                gtk_file_folder_get_info(folder, model_node->path, &error);

              if (error)
              {
                g_assert(model_node->info == NULL);
                g_warning("%s", error->message);
                g_error_free(error);
              }
            }

            emit_node_changed(node);
        }
    }
}

static void wait_node_load(HildonFileSystemModelPrivate * priv,
                           GNode * node)
{
  HildonFileSystemModelNode *model_node = node->data;

  if (model_node->folder
      || model_node->get_folder_handle)   /* Sanity check: node has to
					     be a folder */
  {
    g_debug("Waiting folder [%s] to load", (char *) model_node->path);
    while (!is_node_loaded(node))
    {
        g_usleep(2000); /* microseconds */
        if (gtk_events_pending())       /* Don't do while here, because
                                           there can be enormous amount of
                                           events waiting */
            gtk_main_iteration();
    }
    g_debug("Folder [%s] loaded", (char *) model_node->path);
  }
}

static void hildon_file_system_model_init(HildonFileSystemModel * self)
{
    HildonFileSystemModelPrivate *priv;

    priv =
        G_TYPE_INSTANCE_GET_PRIVATE(self, HILDON_TYPE_FILE_SYSTEM_MODEL,
                                    HildonFileSystemModelPrivate);
    self->priv = (gpointer) priv;

    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_GTK_PATH_INTERNAL] =
        GTK_TYPE_FILE_PATH;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_LOCAL_PATH] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_URI] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_NAME] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_KEY] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_MIME_TYPE] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_SIZE] =
        G_TYPE_INT64;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_TIME] =
        G_TYPE_INT64;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER] =
        G_TYPE_BOOLEAN;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_READONLY] =
        G_TYPE_BOOLEAN;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE] =
        G_TYPE_BOOLEAN;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_HAS_LOCAL_PATH] =
        G_TYPE_BOOLEAN;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_TYPE] = G_TYPE_INT;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON] =
        GDK_TYPE_PIXBUF;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON_EXPANDED] =
        GDK_TYPE_PIXBUF;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON_COLLAPSED] =
        GDK_TYPE_PIXBUF;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_THUMBNAIL] =
        GDK_TYPE_PIXBUF;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_LOAD_READY] =
        G_TYPE_BOOLEAN;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_FREE_SPACE] =
        G_TYPE_INT64;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_TITLE] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_AUTHOR] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_ALBUM] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_HIDDEN] =
        G_TYPE_BOOLEAN;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_UNAVAILABLE_REASON] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_FAILED_ACCESS_MESSAGE] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_SORT_WEIGHT] =
        G_TYPE_INT;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_EXTRA_INFO] =
        G_TYPE_STRING;
    priv->column_types[HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_DRIVE] =
        G_TYPE_BOOLEAN;
    priv->column_types[PRIV_COLUMN_DISPLAY_TEXT] =
	G_TYPE_STRING;
    priv->column_types[PRIV_COLUMN_DISPLAY_ATTRS] =
        PANGO_TYPE_ATTR_LIST;
#ifdef UPSTREAM_DISABLED
    priv->tracker_client = tracker_connect(FALSE);
#endif
    priv->hour24_changed_handler = g_signal_connect_swapped(_hildon_file_system_settings_get_instance(),
							    "notify::hour24",
							    G_CALLBACK(invalidate_display_props),
							    self);
    priv->stamp = g_random_int();
    priv->first_root_scan_completed = FALSE;
}

static void hildon_file_system_model_dispose(GObject *self)
{
  HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(self);

  if (priv->ref_widget)
  {
    g_signal_handler_disconnect(priv->ref_widget, priv->style_changed_handler);
    g_object_unref(priv->ref_widget);
    priv->ref_widget = NULL;
  }
  if (priv->hour24_changed_handler)
    {
      g_signal_handler_disconnect(_hildon_file_system_settings_get_instance(),
				  priv->hour24_changed_handler);
      priv->hour24_changed_handler = 0;
    }
  if (priv->timeout_id)
  {
    g_source_remove(priv->timeout_id);
    priv->timeout_id = 0;
  }
  /* This won't work in finalize (removing nodes sends signals) */
  if (priv->roots)
  {
    hildon_file_system_model_kick_node(priv->roots, self);
    priv->roots = NULL;
  }
#ifdef UPSTREAM_DISABLED
  if (priv->tracker_client)
  {
      tracker_disconnect(priv->tracker_client);
      priv->tracker_client = NULL;
  }
#endif
  G_OBJECT_CLASS(hildon_file_system_model_parent_class)->dispose(self);
}

static void hildon_file_system_model_finalize(GObject * self)
{
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(self);

    g_free(priv->backend_name); /* No need to check NULL */
    g_free(priv->alternative_root_dir);

    /* Disconnecting filesystem volumes-changed signal */
    if (g_signal_handler_is_connected (priv->filesystem,
                                       priv->volumes_changed_handler))
      g_signal_handler_disconnect (priv->filesystem,
                                   priv->volumes_changed_handler);

    g_debug("ref count = %d", G_OBJECT(priv->filesystem)->ref_count);
    g_object_unref(priv->filesystem);

    if (priv->expanded_emblem)
      g_object_unref(priv->expanded_emblem);
    if (priv->collapsed_emblem)
      g_object_unref(priv->collapsed_emblem);

    G_OBJECT_CLASS(hildon_file_system_model_parent_class)->finalize(self);
}

static void hildon_file_system_model_set_property(GObject * object,
                                                  guint property_id,
                                                  const GValue * value,
                                                  GParamSpec * pspec)
{
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(object);

    switch (property_id) {
    case PROP_BACKEND:
        g_assert(priv->backend_name == NULL);   /* We come here exactly
                                                   once */
        priv->backend_name = g_value_dup_string(value);
        break;
    case PROP_BACKEND_OBJECT:
        g_assert(priv->filesystem == NULL);
        priv->filesystem = g_value_get_object(value);
        if (priv->filesystem)
          g_object_ref(priv->filesystem);
        break;
    case PROP_THUMBNAIL_CALLBACK:
        g_warning("Setting thumbnail callback is depricated");
        break;
    case PROP_REF_WIDGET:
        if (priv->ref_widget)
            g_object_unref(priv->ref_widget);
        priv->ref_widget = g_value_get_object(value);
        if (priv->ref_widget) {
            g_object_ref(priv->ref_widget);
	    priv->style_changed_handler =
	      g_signal_connect_swapped(priv->ref_widget, "notify::style",
				       G_CALLBACK(invalidate_display_props), object);
	}
        break;
    case PROP_ROOT_DIR:
        g_assert(priv->alternative_root_dir == NULL);
        priv->alternative_root_dir = g_value_dup_string(value);
        break;
    case PROP_MULTI_ROOT:
        priv->multiroot = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void hildon_file_system_model_get_property(GObject * object,
                                                  guint property_id,
                                                  GValue * value,
                                                  GParamSpec * pspec)
{
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(object);

    switch (property_id) {
    case PROP_BACKEND:
        g_value_set_string(value, priv->backend_name);
        break;
    case PROP_BACKEND_OBJECT:
        g_value_set_object(value, priv->filesystem);
        break;
    case PROP_THUMBNAIL_CALLBACK:
        g_warning("Getting thumbnail callback is depricated");
        g_value_set_pointer(value, NULL);
        break;
    case PROP_REF_WIDGET:
        g_value_set_object(value, priv->ref_widget);
        break;
    case PROP_ROOT_DIR:
        g_value_set_string(value, priv->alternative_root_dir);
        break;
    case PROP_MULTI_ROOT:
        g_value_set_boolean(value, priv->multiroot);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void hildon_file_system_model_class_init(HildonFileSystemModelClass
                                                * klass)
{
    GObjectClass *object;

    g_type_class_add_private(klass, sizeof(HildonFileSystemModelPrivate));

    object = G_OBJECT_CLASS(klass);
    object->constructor = hildon_file_system_model_constructor;
    object->dispose = hildon_file_system_model_dispose;
    object->finalize = hildon_file_system_model_finalize;
    object->set_property = hildon_file_system_model_set_property;
    object->get_property = hildon_file_system_model_get_property;
    klass->device_disconnected =
      hildon_file_system_model_real_device_disconnected;

    g_object_class_install_property(object, PROP_BACKEND,
        g_param_spec_string("backend",
                            "HildonFileChooser backend",
                            "Set GtkFileSystem backend to use",
                            NULL,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_BACKEND_OBJECT,
        g_param_spec_object("backend-object",
                            "Backend object",
                            "GtkFileSystem backend to use. Use this"
                            "if you create backend yourself",
                            GTK_TYPE_FILE_SYSTEM,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object, PROP_THUMBNAIL_CALLBACK,
        g_param_spec_pointer("thumbnail-callback",
                             "Thumbnail creation callback",
                             "This callback property is depricated",
                             G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_REF_WIDGET,
        g_param_spec_object("ref-widget",
                            "Refrence widget",
                            "Any widget on the screen. Needed if "
                            "you want icons.",
                            GTK_TYPE_WIDGET,
                            G_PARAM_READWRITE));

    g_object_class_install_property(object, PROP_ROOT_DIR,
        g_param_spec_string("root-dir",
                            "Root directory",
                            "Specify an alternative root directory. Note that"
                            "gateway and MMCs appear ONLY if you leave"
                            "this to default setting.", NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object, PROP_MULTI_ROOT,
        g_param_spec_boolean("multi-root",
                            "Multiple root directories",
                            "When multiple root directories is enabled, "
                            "each folder under root-dir "
                            "(property) appear as a separate root level folder. "
                            "The directory spesified by root-dir property is not "
                            "displayed itself. This property has effect only when "
                            "root-dir is set.", FALSE,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    hildon_file_system_model_quark =
        g_quark_from_static_string("HildonFileSystemModel Quark");

    signals[FINISHED_LOADING] =
        g_signal_new("finished-loading", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSystemModelClass,
                                     finished_loading), NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE, 1, GTK_TYPE_TREE_ITER);

    signals[DEVICE_DISCONNECTED] =
        g_signal_new("device-disconnected", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(HildonFileSystemModelClass,
                                     device_disconnected), NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED, G_TYPE_NONE, 1,
                     GTK_TYPE_TREE_ITER);

    signals[VOLDEV_MOUNTED] =
        g_signal_new("voldev-mounted", G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__STRING, 
                     G_TYPE_NONE, 1, G_TYPE_STRING);
}

/* We currently assume that all selectable rows can be dragged */
static gboolean
hildon_file_system_model_row_draggable(GtkTreeDragSource *source, GtkTreePath *path)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean draggable;

  model = GTK_TREE_MODEL (source);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get(model, &iter,
		     HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_AVAILABLE,
		     &draggable, -1);

  return draggable;
}

static void hildon_file_system_model_iface_init(GtkTreeModelIface * iface)
{
    iface->get_flags = hildon_file_system_model_get_flags;
    iface->get_n_columns = hildon_file_system_model_get_n_columns;
    iface->get_column_type = hildon_file_system_model_get_column_type;
    iface->get_iter = hildon_file_system_model_get_iter;
    iface->get_path = hildon_file_system_model_get_path;
    iface->get_value = hildon_file_system_model_get_value;
    iface->iter_next = hildon_file_system_model_iter_next;
    iface->iter_children = hildon_file_system_model_iter_children;
    iface->iter_has_child = hildon_file_system_model_iter_has_child;
    iface->iter_n_children = hildon_file_system_model_iter_n_children;
    iface->iter_nth_child = hildon_file_system_model_iter_nth_child;
    iface->iter_parent = hildon_file_system_model_iter_parent;
}

/* All bookkeeping related to DnD is in HildonFileSelection, since
   GtkTreeDnD does not support dragging of multiple items. We only
   use the interface because we want GtkTreeView to limit drag start
   points to real rows (not empty space). */
static void
hildon_file_system_model_drag_source_iface_init(GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = hildon_file_system_model_row_draggable;
}

static void
location_changed(HildonFileSystemSpecialLocation *location, GNode *node)
{
    g_assert(HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION(location));
    g_assert(node != NULL && node->data != NULL);

    g_debug("LOCATION CHANGED: %s", location->basepath);

    clear_model_node_caches(node->data);
    emit_node_changed(node);
}

static void
location_connection_state_changed(HildonFileSystemSpecialLocation *location,
    GNode *node)
{
    HildonFileSystemModelNode *model_node;
    GtkFilePath *path;

    model_node = node->data;

    g_assert(HILDON_IS_FILE_SYSTEM_SPECIAL_LOCATION(location));
    g_assert(model_node != NULL);

    path = _hildon_file_system_path_for_location(
             model_node->model->priv->filesystem, location);

    if (path) {
        if (hildon_file_system_special_location_is_available(location))
        {
            g_debug("Location %s is now available", (char *) model_node->path);

            if (!hildon_file_system_special_location_requires_access(location))
              {
                link_file_folder (node, model_node->path);
              }
        } else {
            g_debug("Location %s is no longer available",
                (char *) model_node->path);

            send_device_disconnected(node);
        }

        /* Ensure that the base path is updated */
        gtk_file_path_free(model_node->path);
        model_node->path = path;

        location_changed(location, node);
    } else {
        g_debug("LOCATION %s FAILED => KICKING AWAY!!", location->basepath);
        hildon_file_system_model_kick_node(node, model_node->model);
        gtk_file_path_free(path);
    }
}

static void
location_rescan(HildonFileSystemSpecialLocation *location, GNode *node)
{
    HildonFileSystemModelNode *model_node;
    HildonFileSystemModel *model;

    g_assert(node != NULL && node->data != NULL);

    model_node = node->data;
    model = model_node->model;

    unlink_file_folder(node);
    link_file_folder(node, model_node->path);
}

static HildonFileSystemModelNode *
create_model_node_for_location(HildonFileSystemModel *self,
    HildonFileSystemSpecialLocation *location)
{
    HildonFileSystemModelNode *model_node = NULL;
    GtkFilePath *path;

    path = _hildon_file_system_path_for_location(
            self->priv->filesystem, location);

    if (path) {
	g_debug ("BASE %s PATH %s\n", location->basepath, (char *)path);

        model_node = g_new0(HildonFileSystemModelNode, 1);
        model_node->model = self;
        model_node->present_flag = TRUE;
        model_node->available = TRUE;
        model_node->path = path;
        model_node->location = g_object_ref(location);

        /* Let the location to initialize it's state */
	hildon_file_system_special_location_volumes_changed(location);

    } else {
        g_debug("BASE LOCATION: %s FAILED => SKIPPING", location->basepath);
    }

    return model_node;
}

static void setup_node_for_location(GNode *node)
{
    HildonFileSystemModelNode *model_node;

    g_assert(node != NULL);

    if ((model_node = node->data) != NULL)
    {
        HildonFileSystemSpecialLocation *location;

        if ((location = model_node->location) != NULL)
        {
            if (!hildon_file_system_special_location_requires_access(location) &&
                hildon_file_system_special_location_is_available(location))
              link_file_folder(node, model_node->path);


            if (location->basepath) {
                if (model_node->path)
                    gtk_file_path_free (model_node->path);

                model_node->path = gtk_file_path_copy ( (GtkFilePath *)location->basepath);
            }

            g_signal_connect(location, "changed",
                G_CALLBACK(location_changed), node);
            g_signal_connect(location, "connection-state",
                G_CALLBACK(location_connection_state_changed), node);
            g_signal_connect(location, "rescan",
                G_CALLBACK(location_rescan), node);
        }
    }
}
/* Similar to g_node_copy_deep, but will also allow nodes to be skipped,
   if they doesn't follow supported URI scheme */
static GNode *my_copy_deep(GNode *node, gpointer data)
{
    GNode *result, *child, *new_child;
    HildonFileSystemModelNode *model_node;

    g_assert(node != NULL);

    /* the fakeroot in device model contains NULL */
    if (node->data == NULL)
        model_node = NULL;
    else {
        model_node = create_model_node_for_location(data, node->data);
        /* If generating the location was unsuccessfull, we skip the location */
        if (model_node == NULL) return NULL;
    }

    result = g_node_new(model_node);

    for (child = g_node_last_child (node); child; child = child->prev) {
        new_child = my_copy_deep(child, data);
        if (new_child)
            g_node_prepend (result, new_child);
    }
    /* Let's setup parent after children, so that adding children do not
       trigger premature "files-added" signals for parents. */

    if (g_ascii_strcasecmp(HILDON_FILE_SYSTEM_SPECIAL_LOCATION(node->data)->basepath, 
			   "file:///") != 0) {
        setup_node_for_location(result);
    }
    else {
        if(HILDON_IS_FILE_SYSTEM_ROOT(node->data)) {
            setup_node_for_location(result);
        }
        else if(HILDON_IS_FILE_SYSTEM_LOCAL_DEVICE(node->data)) {
            setup_node_for_location(result);
        }
	else {
        if (model_node->path)
        {
            gtk_file_path_free(model_node->path);
            model_node->path = NULL;
        }
	    model_node->path = gtk_file_path_copy ( (GtkFilePath *)model_node->location->basepath);
	    g_signal_connect(model_node->location, "changed",
                G_CALLBACK(location_changed), result);
            g_signal_connect(model_node->location, "connection-state",
                G_CALLBACK(location_connection_state_changed), result);
            g_signal_connect(model_node->location, "rescan",
                G_CALLBACK(location_rescan), result);
	}
    }
    return result;
}

static GObject *
hildon_file_system_model_constructor(GType type,
                                     guint n_construct_properties,
                                     GObjectConstructParam *
                                     construct_properties)
{
    GObject *obj;
    HildonFileSystemModelPrivate *priv;
    GtkFilePath *file_path;

    obj = G_OBJECT_CLASS(hildon_file_system_model_parent_class)->
            constructor(type, n_construct_properties, construct_properties);
    priv = CAST_GET_PRIVATE(obj);

    if (!priv->filesystem)
      priv->filesystem = hildon_file_system_create_backend(
                           priv->backend_name, TRUE);

    if (priv->alternative_root_dir == NULL)
    {
        /* Let's use device tree as a base of our tree */
        priv->roots = my_copy_deep(
			_hildon_file_system_get_locations(), obj);

        priv->volumes_changed_handler = g_signal_connect_object(priv->filesystem,
                                     "volumes-changed",
                                     G_CALLBACK(real_volumes_changed),
                                     obj, 0);
    }
    else
    {
      priv->roots = g_node_new(NULL);     /* This is a fake root that
                                             contains real ones */
      g_debug("Alternative root = '%s'", priv->alternative_root_dir);

      file_path = gtk_file_system_filename_to_path(priv->filesystem,
                    priv->alternative_root_dir);
     
      priv->first_root_scan_completed = TRUE;

      if (priv->multiroot)
      {
        HildonFileSystemModelNode *model_node;

        model_node = g_new0(HildonFileSystemModelNode, 1);
        model_node->path = gtk_file_path_copy(file_path);
        model_node->available = TRUE;
        priv->roots->data = model_node;
        model_node->present_flag = TRUE;
        model_node->model = HILDON_FILE_SYSTEM_MODEL(obj);

        if (link_file_folder (priv->roots, file_path))
	  wait_node_load(priv, priv->roots);
      }
      else
      {
        hildon_file_system_model_add_node(GTK_TREE_MODEL(obj), priv->roots, NULL,
                                          file_path, TRUE);
      }

      gtk_file_path_free(file_path);
    }

    return obj;
}

/**
 * hildon_file_system_model_search_local_path:
 * @model: a #HildonFileSystemModel.
 * @path: a #GtkFilePath to load.
 * @iter: a #GtkTreeIter for the result.
 * @start_iter: a #GtkTreeIter for starting point. %NULL for entire model.
 * @recursive: if %FALSE, only immediate children of the parent are
 *   searched. %TRUE searches the entire subtree.
 *
 * a wrapped for hildon_file_system_model_search_path that accepts local
 *   paths.
 *
 * Returns: %TRUE, if the iterator points to desired file.
 *          %FALSE otherwise.
 */
gboolean hildon_file_system_model_search_local_path(HildonFileSystemModel *
                                                    model,
                                                    const gchar * path,
                                                    GtkTreeIter * iter,
                                                    GtkTreeIter *
                                                    start_iter,
                                                    gboolean recursive)
{
    gboolean result;
    GtkFilePath *filepath;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    filepath = gtk_file_system_filename_to_path(priv->filesystem, path);
    result =
        hildon_file_system_model_search_path(model, filepath, iter,
                                             start_iter, recursive);

    gtk_file_path_free(filepath);
    return result;
}

/**
 * hildon_file_system_model_search_uri:
 * @model: a #HildonFileSystemModel.
 * @uri: a #GtkFilePath to load.
 * @iter: a #GtkTreeIter for the result.
 * @start_iter: a #GtkTreeIter for starting point. %NULL for entire model.
 * @recursive: if %FALSE, only immediate children of the parent are
 *   searched. %TRUE searches the entire subtree.
 *
 * a wrapped for hildon_file_system_model_search_path that accepts URIs.
 *
 * Returns: %TRUE, if the iterator points to desired file.
 *          %FALSE otherwise.
 */
gboolean hildon_file_system_model_search_uri(HildonFileSystemModel * model,
                                             const gchar * uri,
                                             GtkTreeIter * iter,
                                             GtkTreeIter * start_iter,
                                             gboolean recursive)
{
    gboolean result;
    GtkFilePath *filepath;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    filepath = gtk_file_system_uri_to_path(priv->filesystem, uri);
    result =
        hildon_file_system_model_search_path(model, filepath, iter,
                                             start_iter, recursive);

    gtk_file_path_free(filepath);
    return result;
}

/**
 * hildon_file_system_model_search_path:
 * @model: a #HildonFileSystemModel.
 * @path: a #GtkFilePath to load.
 * @iter: a #GtkTreeIter for the result.
 * @start_iter: a #GtkTreeIter for starting point. %NULL for entire model.
 * @recursive: if %FALSE, only immediate children of the parent are
 *   searched. %TRUE searches the entire subtree.
 *
 * Searches the model for given path and fills an iterator pointing to it.
 * Note that the path must already exist in model.
 *
 * Returns: %TRUE, if the iterator points to desired file.
 *          %FALSE otherwise.
 */
#ifndef HILDON_DISABLE_DEPRECATED
gboolean hildon_file_system_model_search_path(HildonFileSystemModel *
                                              model,
                                              const GtkFilePath * path,
                                              GtkTreeIter * iter,
                                              GtkTreeIter * start_iter,
                                              gboolean recursive)
{
  HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);
  GNode *start_node;

  g_return_val_if_fail(iter != NULL, FALSE);

  start_node = get_node (priv, start_iter);
  g_return_val_if_fail (start_node != NULL, FALSE);

  iter->stamp = priv->stamp;
  iter->user_data =
    hildon_file_system_model_search_path_internal (start_node,
                                                   path, recursive);
  return iter->user_data != NULL;
}
#endif

/**
 * hildon_file_system_model_load_local_path:
 * @model: a #HildonFileSystemModel.
 * @path: a path to load.
 * @iter: a #GtkTreeIter for the result.
 *
 * Converts the given path to #GtkFilePath and calls
 * hildon_file_system_model_load_path.
 *
 * Returns: %TRUE, if the iterator points to desired file.
 *          %FALSE otherwise.
 */
gboolean hildon_file_system_model_load_local_path(HildonFileSystemModel * model,
                                            const gchar * path,
                                            GtkTreeIter * iter)
{
    gboolean result;
    GtkFilePath *filepath;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    filepath = gtk_file_system_filename_to_path(priv->filesystem, path);
    if (filepath == NULL)
      return FALSE;

    result = hildon_file_system_model_load_path(model, filepath, iter);

    gtk_file_path_free(filepath);
    return result;
}

/**
 * hildon_file_system_model_load_uri:
 * @model: a #HildonFileSystemModel.
 * @uri: an URI to load.
 * @iter: a #GtkTreeIter for the result.
 *
 * Converts the given URI to #GtkFilePath and calls
 * hildon_file_system_model_load_path.
 *
 * Returns: %TRUE, if the iterator points to desired file.
 *          %FALSE otherwise.
 */
gboolean hildon_file_system_model_load_uri(HildonFileSystemModel * model,
                                            const gchar * uri,
                                            GtkTreeIter * iter)
{
    HildonFileSystemSettings *settings;
    gboolean result;
    GtkFilePath *filepath;
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    g_return_val_if_fail (uri != NULL, FALSE);

    filepath = gtk_file_system_uri_to_path(priv->filesystem, uri);
    if (filepath == NULL)
      return FALSE;

    if (!gtk_file_system_path_is_local(priv->filesystem, filepath))
    {
      /* If we're accessing a gateway, its root node doesn't exist until
         settings are read. Wait here until they are */
      settings = _hildon_file_system_settings_get_instance();
      while (!_hildon_file_system_settings_ready(settings))
        gtk_main_iteration();
    }
       
    result = hildon_file_system_model_load_path(model, filepath, iter);

    gtk_file_path_free(filepath);
    return result;
}

/**
 * hildon_file_system_model_load_path:
 * @model: a #HildonFileSystemModel.
 * @path: a #GtkFilePath to load.
 * @iter: a #GtkTreeIter for the result.
 *
 * This method locates the given path from data model. New branches are
 * loaded if the given path doesn't exist in memory. Otherwise similar to
 * hildon_file_system_model_search_path.
 *
 * Returns: %TRUE, if the iterator points to desired file.
 *          %FALSE otherwise.
 */
gboolean hildon_file_system_model_load_path(HildonFileSystemModel * model,
                                            const GtkFilePath * path,
                                            GtkTreeIter * iter)
{
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);
    GtkFilePath *parent_path;
    GtkTreeIter parent_iter;

    g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);

    g_debug ("LOAD %s\n", (char *)path);

    /* Block until the first scanning of the root folder is complete
       so that we know about all memory cards, usb mass storage
       devices, etc.
    */
    while (!priv->first_root_scan_completed)
      {
	g_debug ("+");
	gtk_main_iteration();
      }
    g_debug ("DONE\n");
  
    /* Let's see if given path is already in the tree */
    if (hildon_file_system_model_search_path(model, path, iter,
                                             NULL, TRUE))
    {
      /* In case of gateway, we may need this to allow accessing contents */
      _hildon_file_system_model_mount_device_iter(model, iter);
      g_debug ("FOUND %s\n", (char *)path);
      return TRUE;
    }

    g_debug ("NEED PARENT %s\n", (char *)path);

    /* No, path was not found. Let's try go one level up and loading more
       contents */
    if (!gtk_file_system_get_parent (model->priv->filesystem,
                                     path, &parent_path, NULL)
        || parent_path == NULL)
      {
        /* Let's check a special case: We want remote servers to report
           the used protocol as their parent uri:
               obex://mac/ => obex://
         */
        const gchar *s;
        gint i;

        s = gtk_file_path_get_string(path);
        i = strlen(s) - 1;

	g_debug ("SPECIAL CASE %s\n", s);

        /* Skip tailing slashes */
        while (i >= 0 && s[i] == G_DIR_SEPARATOR) i--;
        /* Skip characters backwards until we encounter next slash */
        while (i >= 0 && s[i] != G_DIR_SEPARATOR) i--;

	g_debug ("SPECIAL CASE I %d\n", i);

        if (i >= 0)
            parent_path = gtk_file_path_new_steal(g_strndup(s, i + 1));
        else {
            g_warning("Attempt to select folder that is not in user visible area");
	    g_debug ("ERR %s\n", (char *)path);
            return FALSE; /* Very BAD. We reached the real root. Given
                             folder was probably not under any of our roots */
        }
      }

    if (hildon_file_system_model_load_path(model, parent_path, &parent_iter))
      {
	gtk_file_path_free(parent_path);

	g_debug ("ADD %s\n", (char *)path);

	/* XXX - We trigger the parent to load its children and then
	 *       wait for it to finish.  This is suboptimal of course
	 *       since it might take a long time.  Instead, we should
	 *       tolerate nodes without a GtkFileInfo and only acquire
	 *       the file info when needed.
	 */
	_hildon_file_system_model_load_children (model, &parent_iter);
      
	/* Since we waited for the parent to load its children, we
	   can now expect it to be there.
	*/

	if (hildon_file_system_model_search_path (model, path, iter,
						  NULL, TRUE))
	  {
	    g_debug ("FOUND %s\n", (char *)path);
	    return TRUE;
	  }
	else
	  {
	    g_debug ("NOT FOUND %s\n", (char *)path);
	    return FALSE;
	  }
      }

    *iter = parent_iter;   /* Return parent iterator if we cannot
                              found asked path */
    gtk_file_path_free(parent_path);
    g_debug ("NO PARENT %s\n", (char *)path);
    return FALSE;
}

static void
hildon_file_system_model_reload_node (HildonFileSystemModel *model,
				      GNode *node,
				      gboolean force)
{
  HildonFileSystemModelNode *model_node = node->data;

  g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));

  if (!node_needs_reload (model, node, force))
    return;

  unlink_file_folder (node);
  link_file_folder (node, model_node->path);
}

void _hildon_file_system_model_queue_reload(HildonFileSystemModel *model,
  GtkTreeIter *parent_iter, gboolean force)
{
  GNode *node;

  g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));
  g_return_if_fail(parent_iter != NULL);
  g_return_if_fail(parent_iter->stamp == model->priv->stamp);

  node = parent_iter->user_data;

  hildon_file_system_model_reload_node (model, node, force);
}

static void
_hildon_file_system_model_load_children(HildonFileSystemModel *model,
                                        GtkTreeIter *parent_iter)
{
  GNode *parent_node;
  HildonFileSystemModelNode *parent_model_node;
  time_t max_time = time (NULL) + 5;

  g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));
  g_return_if_fail(parent_iter != NULL);
  g_return_if_fail(parent_iter->stamp == model->priv->stamp);

  parent_node = parent_iter->user_data;
  parent_model_node = parent_node->data;

  if (!is_node_loaded (parent_node))
    {
      if (parent_model_node->get_folder_handle == NULL)
	link_file_folder (parent_node, parent_model_node->path);
      else
	g_debug ("NOT LINKING %s\n",  (char *)parent_model_node->path);
      while (!is_node_loaded (parent_node)
	     && time (NULL) < max_time)
	{
	  g_debug ("-");
	  gtk_main_iteration ();
	}
      g_debug ("FINISHED %s\n", (char *)parent_model_node->path);
    }
  else
    g_debug ("WAS LOADED %s\n", (char *)parent_model_node->path);
}

GtkFileSystem
    *_hildon_file_system_model_get_file_system(HildonFileSystemModel *
                                               model)
{
    HildonFileSystemModelPrivate *priv = CAST_GET_PRIVATE(model);

    return priv->filesystem;
}

static gint
compare_numbers(gconstpointer a, gconstpointer b)
{
  return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/**
 * hildon_file_system_model_new_item:
 * @model: a #HildonFileSystemModel.
 * @parent: a parent iterator.
 * @stub_name: a boby of the new name.
 * @extension: extension of the new name.
 *
 * Creates a new unique name under #parent. The returned name can be used
 * when creating a new file. If there are no name collisions, stub name
 * will be the final name. If a file with that name already exists, then
 * a number is appended to stub. This function is mainly used by dialog
 * implementations. It's probably not needed in application development.
 *
 * Returns: a New unique name. You have to release this with #g_free. Can be %NULL, if
 * the directory is not yet loaded.
 */
gchar *hildon_file_system_model_new_item(HildonFileSystemModel * model,
                                         GtkTreeIter * parent,
                                         const gchar * stub_name,
                                         const gchar * extension)
{
    GtkTreeModel *treemodel;
    GtkTreeIter iter;
    GList *reserved = NULL;
    GList *list_iter;
    gint final;
    gboolean has_next, full_match = FALSE;
    gchar *result, *allocated = NULL;
    GNode *node;
    HildonFileSystemModelNode *model_node;

    g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), NULL);
    g_return_val_if_fail(stub_name != NULL, NULL);

    treemodel = GTK_TREE_MODEL(model);
    node = parent->user_data;

    if (!is_node_loaded(node))
      return NULL;

    model_node = node->data;

    /* Spacial locations can have sub-locations within themselves.
       those can cause conflicts with autonaming. */
    if (model_node->location && !extension)
    {
      GtkFileSystem *fs;
      GtkFilePath *path;

      fs = model->priv->filesystem;

      /* make_path doesn't work for the fake "files:///" root node */
      path = NULL;
      if (!HILDON_IS_FILE_SYSTEM_ROOT(model_node->location))
	path = gtk_file_system_make_path(fs, model_node->path, stub_name, NULL);

      if (path)
      {
        HildonFileSystemSpecialLocation *location;

	location = _hildon_file_system_get_special_location(path);
        if (location) /* Ok, we are trying to autoname a special location. Let's use user visible name */
        {
          allocated = _hildon_file_system_create_file_name(fs, path, location, NULL);
          stub_name = allocated;
          g_object_unref(location);
        }
        gtk_file_path_free(path);
      }
    }

    for (has_next = gtk_tree_model_iter_children(treemodel, &iter, parent);
         has_next; has_next = gtk_tree_model_iter_next(treemodel, &iter)) {
        gchar *filename;

        gtk_tree_model_get(treemodel, &iter,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_NAME,
                           &filename, -1);

        if (g_str_has_prefix(filename, stub_name)
            && (extension == NULL
                || g_str_has_suffix(filename, extension))) {
            /* Ok, we have a possible candidate. If part after the stub
               prior to extension contains just numbers we have to record
               this information. if this part contains characters then we
               are not concerned about this item */

            char *start = filename + strlen(stub_name);

            if (extension)      /* remove extension */
                filename[strlen(filename) - strlen(extension)] = '\0';

            if (*start == 0)
              full_match = TRUE;
            else
            {
              long value = _hildon_file_system_parse_autonumber(start);

                    if (value >= 0)  /* the string is reserved */
                reserved = g_list_insert_sorted(reserved,
                  GINT_TO_POINTER(value), compare_numbers);
            }
        }

        g_free(filename);
    }

    final = 1;

    for (list_iter = reserved; list_iter; list_iter = list_iter->next)
    {
      gint value = GPOINTER_TO_INT(list_iter->data);
      if (final < value) break;
      final = value + 1;
    }

    g_list_free(reserved);

    if (!full_match)  /* No matches found. Candidate is a good name */
      result = g_strdup(stub_name);
    else
      result = g_strdup_printf("%s (%d)", stub_name, final);

    /* stub_name may point to this, so we have to free
       this AFTER creating the result */
    g_free(allocated);

    return result;
}

/* Devices are not mounted automatically, only in response to user action.
   Additionally, we never try to mount MMC ourselves. So there is now even
   less to do here. */
gboolean _hildon_file_system_model_mount_device_iter(HildonFileSystemModel
                                                     * model,
                                                     GtkTreeIter * iter)
{
    HildonFileSystemModelNode *model_node;
/*    HildonFileSystemModelPrivate *priv; */
    GNode *node;
    static gboolean active_flag = FALSE;

    g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);
    g_return_val_if_fail(model->priv->stamp == iter->stamp, FALSE);

/*    priv = model->priv; */
    node = iter->user_data;
    model_node = node->data;

    if (model_node->location && !active_flag && !model_node->accessed &&
        hildon_file_system_special_location_requires_access(model_node->location))
    {
      HildonFileSystemSettings *settings;

      settings = _hildon_file_system_settings_get_instance();
      active_flag = TRUE;

      /* We really have to know gateway state until we can continue.
         Entering main loop can be dangerous (some functions
         experience re-entrancy issues). Anyway, this is issue only
         if this function is called almost immediately after creating
         the first model in a process. Then settings are not yet ready.
      */
      while (!_hildon_file_system_settings_ready(settings))
        gtk_main_iteration();

      active_flag = FALSE;

      if (hildon_file_system_special_location_is_available(
                                                   model_node->location))
      {
        gboolean success;

        success = link_file_folder(node, model_node->path);

        model_node->accessed = TRUE;

        if (!success)
            return FALSE;

        return TRUE;
      }
    }

    return FALSE;
}

/**
 * hildon_file_system_model_finished_loading:
 * @model: a #HildonFileSystemModel.
 *
 * Checks if model has data in it's processing queue.
 * 
 * Deprecated: This api is broken. It only checks internal processing queue
 * and this information is mostly useless.
 *
 * Returns: %TRUE, data queues are empty.
 */
#ifndef HILDON_DISABLE_DEPRECATED
gboolean hildon_file_system_model_finished_loading(HildonFileSystemModel *
                                                   model)
{
    g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), FALSE);

    /* Cough...
     */
    return TRUE;
}
#endif
/**
 * hildon_file_system_model_autoname_uri:
 * @model: a #HildonFileSystemModel.
 * @uri: an URI to be autonamed.
 * @error: a GError to hold possible error information.
 *
 * This function checks if the given URI already exists in the model.
 * if not, then a copy of it is returned unmodified. If the URI already
 * exists then a number is added in a form file://file(2).html.
 *
 * Returns: either the same uri given as parameter or a modified
 * uri that contains proper index number. In both cases free
 * this value using #g_free. Value can be %NULL, if error was
 * encountered.
 */
gchar *hildon_file_system_model_autoname_uri(HildonFileSystemModel *model,
  const gchar *uri, GError **error)
{
  GtkFileSystem *backend;
  GtkFilePath *folder = NULL;
  GtkFilePath * uri_path = NULL;
  gchar *file = NULL;
  gboolean is_folder = FALSE;
  gchar *result = NULL;
  GtkTreeIter iter;

  g_return_val_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model), NULL);
  g_return_val_if_fail(uri != NULL, NULL);

  backend = model->priv->filesystem;


  if ( ((uri_path = gtk_file_system_uri_to_path(backend, uri)) != NULL) &&
        gtk_file_system_get_parent(backend, uri_path, &folder, NULL)    &&
        (folder != NULL)                                                &&
        hildon_file_system_model_load_path(model, folder, &iter) )
  {

      _hildon_file_system_model_load_children(model, &iter);

      GtkTreeIter ret_iter;

      if ( hildon_file_system_model_search_uri(model, uri, &ret_iter, &iter, FALSE) )
      {
         gtk_tree_model_get
           (GTK_TREE_MODEL(model), &ret_iter,
            HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_NAME, &file,
            HILDON_FILE_SYSTEM_MODEL_COLUMN_IS_FOLDER, &is_folder,
            -1);
      }
  }

  gtk_file_path_free(uri_path);

  if( file == NULL )
  {
      gtk_file_path_free(folder);
      return g_strdup(uri);
  }

  gchar *extension = NULL, *dot, *autonamed;

  dot = _hildon_file_system_search_extension (file, FALSE, is_folder);
  if (dot && dot != file) {
     extension = g_strdup(dot);
     *dot = '\0';
  }

  _hildon_file_system_remove_autonumber(file);

  autonamed = hildon_file_system_model_new_item(model,
                                &iter, file, extension);
  g_free(file);

  if (autonamed)
  {
    GtkFilePath *result_path;

    if (extension) {
       /* Dot is part of the extension */
        gchar *ext_name = g_strconcat(autonamed, extension, NULL);
        g_free(extension);
        g_free(autonamed);
        autonamed = ext_name;
    }

    result_path = gtk_file_system_make_path(backend, folder, autonamed, error);

    if (result_path) {
       result = gtk_file_system_path_to_uri(backend, result_path);
       gtk_file_path_free(result_path);
    }

    g_free(autonamed);
  }

  gtk_file_path_free(folder);

  return result;
}

/**
 * hildon_file_system_model_iter_available:
 * @model: a #HildonFileSystemModel.
 * @iter: a #GtkTreeIter to location to modify.
 * @available: new availability state.
 *
 * This function sets some paths available/not available. Locations
 * that are not available are usually shown dimmed in the gui. This
 * function can be used if program needs for some reason to disable some
 * locations. By default all paths are available.
 */
void hildon_file_system_model_iter_available (HildonFileSystemModel *model,
					      GtkTreeIter *iter,
					      gboolean available)
{
  GNode *node;
  HildonFileSystemModelNode *model_node;
  GtkTreeIter child;

  g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));
  g_return_if_fail(iter != NULL);
  g_return_if_fail(model->priv->stamp == iter->stamp);

  node = iter->user_data;
  model_node = node->data;

  if (model_node->available != available)
    {
      model_node->available = available;
      emit_node_changed(node);
    }

  if (gtk_tree_model_iter_children (GTK_TREE_MODEL (model), &child, iter)) 
    {
      do {
	hildon_file_system_model_iter_available (model, &child, FALSE);
      } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &child));
    }
}

static gboolean
reset_callback(GNode *node, gpointer data)
{
  HildonFileSystemModelNode *model_node;

  g_assert(node != NULL);

  model_node = node->data;
  if (model_node && !model_node->available)
  {
    model_node->available = TRUE;
    emit_node_changed(node);
  }

  return FALSE;
}

/**
 * hildon_file_system_model_reset_available:
 * @model: a #HildonFileSystemModel.
 *
 * Cancels all changes made by #hildon_file_system_model_iter_available.
 * Selection is back to it's default state.
 */
void hildon_file_system_model_reset_available(HildonFileSystemModel *model)
{
  g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));

  g_node_traverse(model->priv->roots, G_POST_ORDER,
      G_TRAVERSE_ALL, -1, reset_callback, NULL);
}


void
_hildon_file_system_model_prioritize_folder(HildonFileSystemModel *model,
                                            GtkTreeIter *folder_iter)
{
  /* We don't have any influence any more over what is loaded first.
   */
}

void rescan_local_device_folders(HildonFileSystemModel *model)
{
    HildonFileSystemModelPrivate *priv;
    HildonFileSystemModelNode *model_node = NULL;
    
    g_return_if_fail(HILDON_IS_FILE_SYSTEM_MODEL(model));
    priv = CAST_GET_PRIVATE(model);

    if(priv->first_root_scan_completed == TRUE) {
      GNode *temp_node = priv->roots;
      if (temp_node){ 
	temp_node = g_node_first_child(temp_node);
	if (temp_node) {
	  model_node = temp_node->data;
	  temp_node = g_node_first_child(temp_node);
	  /* This is supposed to be MyDocs. */
	  g_signal_emit_by_name (model_node->location, "rescan");
	  if (temp_node)
	    temp_node = g_node_first_child(temp_node);
	  while (temp_node) {
	    model_node = temp_node->data;
	    /* There might be ordinary folders also in MyDocs (without
	       ->location), so don't try to emit the ::rescan signal
	       unconditionally. */
	    if (model_node->location)
	      g_signal_emit_by_name (model_node->location, "rescan");
	    temp_node = g_node_next_sibling(temp_node);
	  }
	}
      }
    }
}
