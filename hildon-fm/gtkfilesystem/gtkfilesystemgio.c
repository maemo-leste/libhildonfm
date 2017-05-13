/* GTK - The GIMP Toolkit
 * gtkfilesystem.c: Filesystem abstraction functions.
 * Copyright (C) 2003, Red Hat, Inc.
 * Copyright (C) 2007-2008 Carlos Garnacho
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Carlos Garnacho <carlos@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "gtkfilesystemgio.h"
#include <gtk/gtkicontheme.h>
#include <gtk/gtkprivate.h>

#define DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG(x) g_warning (x);
#else
#define DEBUG(x)
#endif

#define GTK_FILE_SYSTEM_GIO_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_FILE_SYSTEM_GIO, GtkFileSystemGioClass))
#define GTK_IS_FILE_SYSTEM_GIO_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_FILE_SYSTEM_GIO))
#define GTK_FILE_SYSTEM_GIO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_FILE_SYSTEM_GIO, GtkFileSystemGioClass))
#define GTK_FILE_SYSTEM_GIO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_FILE_SYSTEM_GIO, GtkFileSystemGioPrivate))

#define GTK_TYPE_FOLDER_GIO         (_gtk_folder_gio_get_type ())
#define GTK_FOLDER_GIO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FOLDER_GIO, GtkFolderGio))
#define GTK_FOLDER_GIO_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_FOLDER_GIO, GtkFolderGioClass))
#define GTK_IS_FOLDER_GIO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FOLDER_GIO))
#define GTK_IS_FOLDER_GIO_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_FOLDER_GIO))
#define GTK_FOLDER_GIO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_FOLDER_GIO, GtkFolderGioClass))
#define GTK_FOLDER_GIO_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_FOLDER_GIO, GtkFolderGioPrivate))


#define FILES_PER_QUERY 100

/* The pointers we return for a GtkFileSystemVolume are opaque tokens; they are
 * really pointers to GDrive, GVolume or GMount objects.  We need an extra
 * token for the fake "File System" volume.  So, we'll return a pointer to
 * this particular string.
 */
static const gchar *root_volume_token = N_("File System");
#define IS_ROOT_VOLUME(volume) ((gpointer) (volume) == (gpointer) root_volume_token)

enum {
  PROP_0,
  PROP_FILE,
  PROP_ENUMERATOR,
  PROP_ATTRIBUTES
};

typedef struct GtkFileSystemGioClass GtkFileSystemGioClass;
typedef struct GtkFileSystemGioPrivate GtkFileSystemGioPrivate;
typedef struct GtkFolderGioPrivate GtkFolderGioPrivate;
typedef struct AsyncFuncData AsyncFuncData;
typedef struct GtkFolderGioClass GtkFolderGioClass;
typedef struct GtkFolderGio GtkFolderGio;
typedef struct GtkFileSystemBookmark GtkFileSystemBookmark; /* opaque struct */

struct GtkFileSystemGioClass
{
  GObjectClass parent_class;
};

struct GtkFileSystemGio
{
  GObject parent_object;
};

struct GtkFolderGioClass
{
  GObjectClass parent_class;

  void (*files_added)      (GtkFolderGio *folder,
			    GList     *paths);
  void (*files_removed)    (GtkFolderGio *folder,
			    GList     *paths);
  void (*files_changed)    (GtkFolderGio *folder,
			    GList     *paths);
  void (*finished_loading) (GtkFolderGio *folder);
  void (*deleted)          (GtkFolderGio *folder);
};

struct GtkFolderGio
{
  GObject parent_object;
};

struct GtkFileSystemGioPrivate
{
  GVolumeMonitor *volume_monitor;

  /* This list contains elements that can be
   * of type GDrive, GVolume and GMount
   */
  GSList *volumes;

  /* This list contains GtkFileSystemBookmark structs */
  GSList *bookmarks;
  GFile *bookmarks_file;

  GFileMonitor *bookmarks_monitor;
};

struct GtkFolderGioPrivate
{
  GFile *folder_file;
  GHashTable *children;
  GFileMonitor *directory_monitor;
  GFileEnumerator *enumerator;
  GCancellable *cancellable;
  gchar *attributes;

  guint finished_loading : 1;
};

struct AsyncFuncData
{
  GtkFileSystem *file_system;
  GFile *file;
  GCancellable *cancellable;
  gchar *attributes;

  gpointer callback;
  gpointer data;
};

struct GtkFileSystemBookmark
{
  GFile *file;
  gchar *label;
};

/*
 * GtkFolderGio
 */
GType           _gtk_folder_gio_get_type     (void) G_GNUC_CONST;
static void gtk_folder_gio_iface_init (GtkFolderIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFolderGio, _gtk_folder_gio, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FOLDER,
						gtk_folder_gio_iface_init))

static void gtk_folder_gio_set_finished_loading (GtkFolder *folder,
						 gboolean   finished_loading);
static void gtk_folder_gio_add_file             (GtkFolder *folder,
						 GFile     *file,
						 GFileInfo *info);

/*
 * GtkFileSystemGio
 */
static void gtk_file_system_gio_iface_init (GtkFileSystemIface *iface);
static void gtk_file_system_gio_dispose (GObject *object);
static void gtk_file_system_gio_finalize (GObject *object);

G_DEFINE_TYPE_WITH_CODE (GtkFileSystemGio, _gtk_file_system_gio, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_SYSTEM,
						gtk_file_system_gio_iface_init))

/* GtkFileSystemGio methods */

GtkFileSystemGio * _gtk_file_system_gio_new          (void);

static GtkFilePath * _gtk_file_system_gio_uri_to_path (GtkFileSystem *file_system,
						       const gchar    *uri);
static gchar *  _gtk_file_system_gio_path_to_filename (GtkFileSystem     *file_system,
						       const GtkFilePath *path);
static gchar *  _gtk_file_system_gio_path_to_uri    (GtkFileSystem     *file_system,
						     const GtkFilePath *path);

GSList *        _gtk_file_system_gio_list_volumes   (GtkFileSystemGio *file_system);
GSList *        _gtk_file_system_gio_list_bookmarks (GtkFileSystemGio *file_system);

gboolean        _gtk_file_system_gio_parse          (GtkFileSystemGio     *file_system,
						     GFile             *base_file,
						     const gchar       *str,
						     GFile            **folder,
						     gchar            **file_part,
						     GError           **error);

GCancellable *  _gtk_file_system_gio_get_folder             (GtkFileSystem                     *file_system,
							     GFile                             *file,
							     const char                        *attributes,
							     GtkFileSystemGetFolderCallback     callback,
							     gpointer                           data);
GCancellable *  _gtk_file_system_gio_get_info               (GtkFileSystem                     *file_system,
							     GFile                             *file,
							     const gchar                       *attributes,
							     GtkFileSystemGetInfoCallback       callback,
							     gpointer                           data);
GCancellable *  _gtk_file_system_gio_mount_volume           (GtkFileSystem                     *file_system,
							     GtkFileSystemVolume               *volume,
							     GMountOperation                   *mount_operation,
							     GtkFileSystemVolumeMountCallback   callback,
							     gpointer                           data);
GCancellable *  _gtk_file_system_gio_mount_enclosing_volume (GtkFileSystem                     *file_system,
							     GFile                             *file,
							     GMountOperation                   *mount_operation,
							     GtkFileSystemVolumeMountCallback   callback,
							     gpointer                           data);

gboolean        _gtk_file_system_gio_insert_bookmark    (GtkFileSystem      *file_system,
							 GFile              *file,
							 gint                position,
							 GError            **error);
gboolean        _gtk_file_system_gio_remove_bookmark    (GtkFileSystem      *file_system,
							 GFile              *file,
							 GError            **error);

gchar *         _gtk_file_system_gio_get_bookmark_label (GtkFileSystem *file_system,
							 GFile         *file);
void            _gtk_file_system_gio_set_bookmark_label (GtkFileSystem *file_system,
							 GFile         *file,
							 const gchar   *label);

static GtkFileSystemVolume * _gtk_file_system_gio_get_volume_for_file (GtkFileSystem       *file_system,
								       GFile               *file);

/* GtkFolderGio functions */
static GSList *     _gtk_folder_gio_list_children (GtkFolder  *folder);
static gboolean     gtk_folder_gio_list_children (GtkFolder    *folder,
						  GSList          **children,
						  GError          **error);
static GFileInfo *  _gtk_folder_gio_get_info      (GtkFolder  *folder,
						   GFile      *file);

static gboolean     _gtk_folder_gio_is_finished_loading (GtkFolder *folder);


/* GtkFileSystemVolume methods */
gchar *               _gtk_file_system_gio_volume_get_display_name (GtkFileSystemVolume *volume);
gboolean              _gtk_file_system_gio_volume_is_mounted       (GtkFileSystemVolume *volume);
GFile *               _gtk_file_system_gio_volume_get_root         (GtkFileSystemVolume *volume);
GdkPixbuf *           _gtk_file_system_gio_volume_render_icon      (GtkFileSystemVolume  *volume,
								    GtkWidget            *widget,
								    gint                  icon_size,
								    GError              **error);

GtkFileSystemVolume  *_gtk_file_system_gio_volume_ref              (GtkFileSystemVolume *volume);
void                  _gtk_file_system_gio_volume_unref            (GtkFileSystemVolume *volume);

/* GtkFileSystemBookmark methods */
void                   _gtk_file_system_gio_bookmark_free          (GtkFileSystemBookmark *bookmark);

/* GFileInfo helper functions */
GdkPixbuf *     _gtk_file_info_render_icon (GFileInfo *info,
					    GtkWidget *widget,
					    gint       icon_size);

gboolean	_gtk_file_info_consider_as_directory (GFileInfo *info);

/* GtkFileSystemBookmark methods */
void
_gtk_file_system_gio_bookmark_free (GtkFileSystemBookmark *bookmark)
{
  g_object_unref (bookmark->file);
  g_free (bookmark->label);
  g_slice_free (GtkFileSystemBookmark, bookmark);
}

/* GtkFileSystemGio methods */

static void
gtk_file_system_gio_iface_init (GtkFileSystemIface *iface)
{
  iface->list_volumes = _gtk_file_system_gio_list_volumes;
//  iface->get_volume_for_path = _gtk_file_system_gio_get_volume_for_path;
  iface->get_folder = _gtk_file_system_gio_get_folder;
  iface->get_info = _gtk_file_system_gio_get_info;
//  iface->create_folder = _gtk_file_system_gio_create_folder;
//  iface->cancel_operation = _gtk_file_system_gio_cancel_operation;
//  iface->volume_free = _gtk_file_system_gio_volume_free;
//  iface->volume_get_base_path = _gtk_file_system_gio_volume_get_base_path;
//  iface->volume_get_is_mounted = _gtk_file_system_gio_volume_get_is_mounted;
//  iface->volume_mount = _gtk_file_system_gio_volume_mount;
  iface->volume_get_display_name = _gtk_file_system_gio_volume_get_display_name;
//  iface->volume_get_icon_name = _gtk_file_system_gio_volume_get_icon_name;
//  iface->get_parent = _gtk_file_system_gio_get_parent;
//  iface->make_path = _gtk_file_system_gio_make_path;
  iface->parse = _gtk_file_system_gio_parse;
  iface->path_to_uri = _gtk_file_system_gio_path_to_uri;
  iface->path_to_filename = _gtk_file_system_gio_path_to_filename;
  iface->uri_to_path = _gtk_file_system_gio_uri_to_path;
//  iface->filename_to_path = _gtk_file_system_gio_filename_to_path;
  iface->insert_bookmark = _gtk_file_system_gio_insert_bookmark;
  iface->remove_bookmark = _gtk_file_system_gio_remove_bookmark;
  iface->list_bookmarks = _gtk_file_system_gio_list_bookmarks;
  iface->get_bookmark_label = _gtk_file_system_gio_get_bookmark_label;
  iface->set_bookmark_label = _gtk_file_system_gio_set_bookmark_label;


  iface->get_volume_for_path = 1;
  iface->create_folder = 1;
  iface->cancel_operation = 1;
  iface->volume_free = 1;
  iface->volume_get_base_path = 1;
  iface->volume_get_is_mounted = 1;
  iface->volume_mount = 1;
  iface->volume_get_icon_name = 1;
  iface->get_parent = 1;
  iface->make_path = 1;
  iface->filename_to_path = 1;
}

static GtkFilePath *
_gtk_file_system_gio_uri_to_path (GtkFileSystem *file_system,
				  const gchar    *uri)
{
  GFile *file = g_file_new_for_commandline_arg (uri);
  gchar *path = g_file_get_uri(file);
  g_object_unref (file);

  g_warning ("%s uri %s -> %s", __FUNCTION__,  uri, path);

  return gtk_file_path_new_steal(path);
}

static gchar *
_gtk_file_system_gio_path_to_filename (GtkFileSystem     *file_system,
				       const GtkFilePath *path)
{
  /* FIXME - is this what we need to do? */
  GFile *file = g_file_new_for_commandline_arg (gtk_file_path_get_string(path));
  gchar *filename = g_file_get_path(file);
  g_object_unref (file);

  g_warning ("%s path %s -> %s", __FUNCTION__,  path, filename);

  return filename;
}

static gchar *
_gtk_file_system_gio_path_to_uri (GtkFileSystem     *file_system,
				  const GtkFilePath *path)
{
  gchar *rv = g_strdup (gtk_file_path_get_string (path));
  g_warning ("%s path %s -> %s", __FUNCTION__,  path, rv);

  return rv;
}

static void
volumes_changed (GVolumeMonitor *volume_monitor,
		 gpointer        volume,
		 gpointer        user_data)
{
  GtkFileSystemGio *file_system;

  gdk_threads_enter ();

  file_system = GTK_FILE_SYSTEM_GIO (user_data);
  g_signal_emit_by_name (file_system, "volumes-changed", volume);
  gdk_threads_leave ();
}

static void
gtk_file_system_gio_dispose (GObject *object)
{
  GtkFileSystemGioPrivate *priv;

  DEBUG ("dispose");

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (object);

  if (priv->volumes)
    {
      g_slist_foreach (priv->volumes, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->volumes);
      priv->volumes = NULL;
    }

  if (priv->volume_monitor)
    {
      g_signal_handlers_disconnect_by_func (priv->volume_monitor, volumes_changed, object);
      g_object_unref (priv->volume_monitor);
      priv->volume_monitor = NULL;
    }

  G_OBJECT_CLASS (_gtk_file_system_gio_parent_class)->dispose (object);
}

static void
gtk_file_system_gio_finalize (GObject *object)
{
  GtkFileSystemGioPrivate *priv;

  DEBUG ("finalize");

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (object);

  if (priv->bookmarks_monitor)
    g_object_unref (priv->bookmarks_monitor);

  if (priv->bookmarks)
    {
      g_slist_foreach (priv->bookmarks, (GFunc) _gtk_file_system_gio_bookmark_free, NULL);
      g_slist_free (priv->bookmarks);
    }

  if (priv->bookmarks_file)
    g_object_unref (priv->bookmarks_file);

  G_OBJECT_CLASS (_gtk_file_system_gio_parent_class)->finalize (object);
}

static void
_gtk_file_system_gio_class_init (GtkFileSystemGioClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  DEBUG("class_init");

  object_class->dispose = gtk_file_system_gio_dispose;
  object_class->finalize = gtk_file_system_gio_finalize;

  g_type_class_add_private (object_class, sizeof (GtkFileSystemGioPrivate));
}

static GFile *
get_legacy_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GFile *
get_bookmarks_file (void)
{
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_user_config_dir (), "gtk-3.0", "bookmarks", NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  return file;
}

static GSList *
read_bookmarks (GFile *file)
{
  gchar *contents;
  gchar **lines, *space;
  GSList *bookmarks = NULL;
  gint i;

  if (!g_file_load_contents (file, NULL, &contents,
			     NULL, NULL, NULL))
    return NULL;

  lines = g_strsplit (contents, "\n", -1);

  for (i = 0; lines[i]; i++)
    {
      GtkFileSystemBookmark *bookmark;

      if (!*lines[i])
	continue;

      if (!g_utf8_validate (lines[i], -1, NULL))
	continue;

      bookmark = g_slice_new0 (GtkFileSystemBookmark);

      if ((space = strchr (lines[i], ' ')) != NULL)
	{
	  space[0] = '\0';
	  bookmark->label = g_strdup (space + 1);
	}

      bookmark->file = g_file_new_for_uri (lines[i]);
      bookmarks = g_slist_prepend (bookmarks, bookmark);
    }

  bookmarks = g_slist_reverse (bookmarks);
  g_strfreev (lines);
  g_free (contents);

  return bookmarks;
}

static void
save_bookmarks (GFile  *bookmarks_file,
		GSList *bookmarks)
{
  GError *error = NULL;
  GString *contents;
  GSList *l;
  GFile *parent_file;
  gchar *path;

  contents = g_string_new ("");

  for (l = bookmarks; l; l = l->next)
    {
      GtkFileSystemBookmark *bookmark = l->data;
      gchar *uri;

      uri = g_file_get_uri (bookmark->file);
      if (!uri)
	continue;

      g_string_append (contents, uri);

      if (bookmark->label)
	g_string_append_printf (contents, " %s", bookmark->label);

      g_string_append_c (contents, '\n');
      g_free (uri);
    }

  parent_file = g_file_get_parent (bookmarks_file);
  path = g_file_get_path (parent_file);
  if (g_mkdir_with_parents (path, 0700) == 0)
    {
      if (!g_file_replace_contents (bookmarks_file,
				    contents->str,
				    strlen (contents->str),
				    NULL, FALSE, 0, NULL,
				    NULL, &error))
	{
	  g_critical ("%s", error->message);
	  g_error_free (error);
	}
    }
  g_free (path);
  g_object_unref (parent_file);
  g_string_free (contents, TRUE);
}

static void
bookmarks_file_changed (GFileMonitor      *monitor,
			GFile             *file,
			GFile             *other_file,
			GFileMonitorEvent  event,
			gpointer           data)
{
  GtkFileSystemGioPrivate *priv;

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (data);

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
      g_slist_foreach (priv->bookmarks, (GFunc) _gtk_file_system_gio_bookmark_free, NULL);
      g_slist_free (priv->bookmarks);

      priv->bookmarks = read_bookmarks (file);

      gdk_threads_enter ();
      g_signal_emit_by_name (data, "bookmarks-changed");
      gdk_threads_leave ();
      break;
    default:
      /* ignore at the moment */
      break;
    }
}

static gboolean
mount_referenced_by_volume_activation_root (GList *volumes, GMount *mount)
{
  GList *l;
  GFile *mount_root;
  gboolean ret;

  ret = FALSE;

  mount_root = g_mount_get_root (mount);

  for (l = volumes; l != NULL; l = l->next)
    {
      GVolume *volume = G_VOLUME (l->data);
      GFile *volume_activation_root;

      volume_activation_root = g_volume_get_activation_root (volume);
      if (volume_activation_root != NULL)
	{
	  if (g_file_has_prefix (volume_activation_root, mount_root))
	    {
	      ret = TRUE;
	      g_object_unref (volume_activation_root);
	      break;
	    }
	  g_object_unref (volume_activation_root);
	}
    }

  g_object_unref (mount_root);
  return ret;
}

static void
get_volumes_list (GtkFileSystemGio *file_system)
{
  GtkFileSystemGioPrivate *priv;
  GList *l, *ll;
  GList *drives;
  GList *volumes;
  GList *mounts;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);

  if (priv->volumes)
    {
      g_slist_foreach (priv->volumes, (GFunc) g_object_unref, NULL);
      g_slist_free (priv->volumes);
      priv->volumes = NULL;
    }

  /* first go through all connected drives */
  drives = g_volume_monitor_get_connected_drives (priv->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    {
      drive = l->data;
      volumes = g_drive_get_volumes (drive);

      if (volumes)
	{
	  for (ll = volumes; ll != NULL; ll = ll->next)
	    {
	      volume = ll->data;
	      mount = g_volume_get_mount (volume);

	      if (mount)
		{
		  /* Show mounted volume */
		  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
		  g_object_unref (mount);
		}
	      else
		{
		  /* Do show the unmounted volumes in the sidebar;
		   * this is so the user can mount it (in case automounting
		   * is off).
		   *
		   * Also, even if automounting is enabled, this gives a visual
		   * cue that the user should remember to yank out the media if
		   * he just unmounted it.
		   */
		  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (volume));
		}

	      g_object_unref (volume);
	    }

	  g_list_free (volumes);
	}
      else if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive))
	{
	  /* If the drive has no mountable volumes and we cannot detect media change.. we
	   * display the drive in the sidebar so the user can manually poll the drive by
	   * right clicking and selecting "Rescan..."
	   *
	   * This is mainly for drives like floppies where media detection doesn't
	   * work.. but it's also for human beings who like to turn off media detection
	   * in the OS to save battery juice.
	   */

	  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (drive));
	}

      g_object_unref (drive);
    }

  g_list_free (drives);

  /* add all volumes that is not associated with a drive */
  volumes = g_volume_monitor_get_volumes (priv->volume_monitor);

  for (l = volumes; l != NULL; l = l->next)
    {
      volume = l->data;
      drive = g_volume_get_drive (volume);

      if (drive)
	{
	  g_object_unref (drive);
	  continue;
	}

      mount = g_volume_get_mount (volume);

      if (mount)
	{
	  /* show this mount */
	  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
	  g_object_unref (mount);
	}
      else
	{
	  /* see comment above in why we add an icon for a volume */
	  priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (volume));
	}

      g_object_unref (volume);
    }

  /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
  mounts = g_volume_monitor_get_mounts (priv->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      mount = l->data;
      volume = g_mount_get_volume (mount);

      if (volume)
	{
	  g_object_unref (volume);
	  continue;
	}

      /* if there's exists one or more volumes with an activation root inside the mount,
       * don't display the mount
       */
      if (mount_referenced_by_volume_activation_root (volumes, mount))
	{
	  g_object_unref (mount);
	  continue;
	}

      /* show this mount */
      priv->volumes = g_slist_prepend (priv->volumes, g_object_ref (mount));
      g_object_unref (mount);
    }

  g_list_free (volumes);

  g_list_free (mounts);
}

static void
_gtk_file_system_gio_init (GtkFileSystemGio *file_system)
{
  GtkFileSystemGioPrivate *priv;
  GFile *bookmarks_file;
  GError *error = NULL;

  DEBUG ("init");

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);

  /* Volumes */
  priv->volume_monitor = g_volume_monitor_get ();

  g_signal_connect (priv->volume_monitor, "mount-added",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "mount-removed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "mount-changed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-added",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-removed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "volume-changed",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-connected",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-disconnected",
		    G_CALLBACK (volumes_changed), file_system);
  g_signal_connect (priv->volume_monitor, "drive-changed",
		    G_CALLBACK (volumes_changed), file_system);

  /* Bookmarks */
  bookmarks_file = get_bookmarks_file ();
  priv->bookmarks = read_bookmarks (bookmarks_file);
  if (!priv->bookmarks)
    {
      /* Use the legacy file instead */
      g_object_unref (bookmarks_file);
      bookmarks_file = get_legacy_bookmarks_file ();
      priv->bookmarks = read_bookmarks (bookmarks_file);
    }

  priv->bookmarks_monitor = g_file_monitor_file (bookmarks_file,
						 G_FILE_MONITOR_NONE,
						 NULL, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    g_signal_connect (priv->bookmarks_monitor, "changed",
		      G_CALLBACK (bookmarks_file_changed), file_system);

  priv->bookmarks_file = g_object_ref (bookmarks_file);
}

/* GtkFileSystemGio public methods */
GtkFileSystem *
gtk_file_system_gio_new (void)
{
  return g_object_new (GTK_TYPE_FILE_SYSTEM_GIO, NULL);
}

GSList *
_gtk_file_system_gio_list_volumes (GtkFileSystemGio *file_system)
{
  GtkFileSystemGioPrivate *priv;
  GSList *list;

  DEBUG ("list_volumes");

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_GIO (file_system), NULL);

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);
  get_volumes_list (GTK_FILE_SYSTEM_GIO (file_system));

  list = g_slist_copy (priv->volumes);

#ifndef G_OS_WIN32
  /* Prepend root volume */
  list = g_slist_prepend (list, (gpointer) root_volume_token);
#endif

  return list;
}

GSList *
_gtk_file_system_gio_list_bookmarks (GtkFileSystemGio *file_system)
{
  GtkFileSystemGioPrivate *priv;
  GSList *bookmarks, *files = NULL;

  DEBUG ("list_bookmarks");

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      GtkFileSystemBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      files = g_slist_prepend (files, g_object_ref (bookmark->file));
    }

  return g_slist_reverse (files);
}

static gboolean
is_valid_scheme_character (char c)
{
  return g_ascii_isalnum (c) || c == '+' || c == '-' || c == '.';
}

static gboolean
has_uri_scheme (const char *str)
{
  const char *p;

  p = str;

  if (!is_valid_scheme_character (*p))
    return FALSE;

  do
    p++;
  while (is_valid_scheme_character (*p));

  return (strncmp (p, "://", 3) == 0);
}

gboolean
_gtk_file_system_gio_parse (GtkFileSystemGio     *file_system,
			    GFile             *base_file,
			    const gchar       *str,
			    GFile            **folder,
			    gchar            **file_part,
			    GError           **error)
{
  GFile *file;
  gboolean result = FALSE;
  gboolean is_dir = FALSE;
  gchar *last_slash = NULL;
  gboolean is_uri;

  DEBUG ("parse");

  if (str && *str)
    is_dir = (str [strlen (str) - 1] == G_DIR_SEPARATOR);

  last_slash = strrchr (str, G_DIR_SEPARATOR);

  is_uri = has_uri_scheme (str);

  if (is_uri)
    {
      const char *colon;
      const char *slash_after_hostname;

      colon = strchr (str, ':');
      g_assert (colon != NULL);
      g_assert (strncmp (colon, "://", 3) == 0);

      slash_after_hostname = strchr (colon + 3, '/');

      if (slash_after_hostname == NULL)
	{
	  /* We don't have a full hostname yet.  So, don't switch the folder
	   * until we have seen a full hostname.  Otherwise, completion will
	   * happen for every character the user types for the hostname.
	   */

	  *folder = NULL;
	  *file_part = NULL;
	  g_set_error (error,
		       GTK_FILE_CHOOSER_ERROR,
		       GTK_FILE_CHOOSER_ERROR_INCOMPLETE_HOSTNAME,
		       "Incomplete hostname");
	  return FALSE;
	}
    }

  if (str[0] == '~' || g_path_is_absolute (str) || is_uri)
    file = g_file_parse_name (str);
  else
    {
      if (base_file)
	file = g_file_resolve_relative_path (base_file, str);
      else
	{
	  *folder = NULL;
	  *file_part = NULL;
	  g_set_error (error,
		       GTK_FILE_CHOOSER_ERROR,
		       GTK_FILE_CHOOSER_ERROR_BAD_FILENAME,
		       _("Invalid path"));
	  return FALSE;
	}
    }

  if (base_file && g_file_equal (base_file, file))
    {
      /* this is when user types '.', could be the
       * beginning of a hidden file, ./ or ../
       */
      *folder = g_object_ref (file);
      *file_part = g_strdup (str);
      result = TRUE;
    }
  else if (is_dir)
    {
      /* it's a dir, or at least it ends with the dir separator */
      *folder = g_object_ref (file);
      *file_part = g_strdup ("");
      result = TRUE;
    }
  else
    {
      GFile *parent_file;

      parent_file = g_file_get_parent (file);

      if (!parent_file)
	{
	  g_set_error (error,
		       GTK_FILE_CHOOSER_ERROR,
		       GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
		       "Could not get parent file");
	  *folder = NULL;
	  *file_part = NULL;
	}
      else
	{
	  *folder = parent_file;
	  result = TRUE;

	  if (last_slash)
	    *file_part = g_strdup (last_slash + 1);
	  else
	    *file_part = g_strdup (str);
	}
    }

  g_object_unref (file);

  return result;
}

static void
free_async_data (AsyncFuncData *async_data)
{
  g_object_unref (async_data->file_system);
  g_object_unref (async_data->file);
  g_object_unref (async_data->cancellable);

  g_free (async_data->attributes);
  g_free (async_data);
}

static void
enumerate_children_callback (GObject      *source_object,
			     GAsyncResult *result,
			     gpointer      user_data)
{
  GFileEnumerator *enumerator;
  AsyncFuncData *async_data;
  GtkFolder *folder = NULL;
  GFile *file;
  GError *error = NULL;

  file = G_FILE (source_object);
  async_data = (AsyncFuncData *) user_data;
  enumerator = g_file_enumerate_children_finish (file, result, &error);

  if (enumerator)
    {
      folder = g_object_new (GTK_TYPE_FOLDER_GIO,
			     "file", source_object,
			     "enumerator", enumerator,
			     "attributes", async_data->attributes,
			     NULL);
      g_object_unref (enumerator);
    }

  gdk_threads_enter ();
  ((GtkFileSystemGetFolderCallback) async_data->callback) (async_data->cancellable,
							   folder, error, async_data->data);
  gdk_threads_leave ();

  free_async_data (async_data);

  if (folder)
    g_object_unref (folder);

  if (error)
    g_error_free (error);
}

GCancellable *
_gtk_file_system_gio_get_folder (GtkFileSystem                  *file_system,
				 GFile                          *file,
				 const char                     *attributes,
				 GtkFileSystemGetFolderCallback  callback,
				 gpointer                        data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_GIO (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);
  async_data->attributes = g_strdup (attributes);

  async_data->callback = callback;
  async_data->data = data;

  g_file_enumerate_children_async (file,
				   attributes,
				   G_FILE_QUERY_INFO_NONE,
				   G_PRIORITY_DEFAULT,
				   cancellable,
				   enumerate_children_callback,
				   async_data);
  return cancellable;
}

static void
query_info_callback (GObject      *source_object,
		     GAsyncResult *result,
		     gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;
  GFileInfo *file_info;
  GFile *file;

  DEBUG ("query_info_callback");

  file = G_FILE (source_object);
  async_data = (AsyncFuncData *) user_data;
  file_info = g_file_query_info_finish (file, result, &error);

  if (async_data->callback)
    {
      gdk_threads_enter ();
      ((GtkFileSystemGetInfoCallback) async_data->callback) (async_data->cancellable,
							     file_info, error, async_data->data);
      gdk_threads_leave ();
    }

  if (file_info)
    g_object_unref (file_info);

  if (error)
    g_error_free (error);

  free_async_data (async_data);
}

GCancellable *
_gtk_file_system_gio_get_info (GtkFileSystem *file_system,
			       GFile                        *file,
			       const gchar                  *attributes,
			       GtkFileSystemGetInfoCallback  callback,
			       gpointer                      data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_GIO (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  g_file_query_info_async (file,
			   attributes,
			   G_FILE_QUERY_INFO_NONE,
			   G_PRIORITY_DEFAULT,
			   cancellable,
			   query_info_callback,
			   async_data);

  return cancellable;
}

static void
drive_poll_for_media_cb (GObject      *source_object,
			 GAsyncResult *result,
			 gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;

  g_drive_poll_for_media_finish (G_DRIVE (source_object), result, &error);
  async_data = (AsyncFuncData *) user_data;

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable,
							     (GtkFileSystemVolume *) source_object,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);
}

static void
volume_mount_cb (GObject      *source_object,
		 GAsyncResult *result,
		 gpointer      user_data)
{
  AsyncFuncData *async_data;
  GError *error = NULL;

  g_volume_mount_finish (G_VOLUME (source_object), result, &error);
  async_data = (AsyncFuncData *) user_data;

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable,
							     (GtkFileSystemVolume *) source_object,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);
}

GCancellable *
_gtk_file_system_gio_mount_volume (GtkFileSystem *file_system,
				   GtkFileSystemVolume              *volume,
				   GMountOperation                  *mount_operation,
				   GtkFileSystemVolumeMountCallback  callback,
				   gpointer                          data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;
  gboolean handled = FALSE;

  DEBUG ("volume_mount");

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  if (G_IS_DRIVE (volume))
    {
      /* this path happens for drives that are not polled by the OS and where the last media
       * check indicated that no media was available. So the thing to do here is to
       * invoke poll_for_media() on the drive
       */
      g_drive_poll_for_media (G_DRIVE (volume), cancellable, drive_poll_for_media_cb, async_data);
      handled = TRUE;
    }
  else if (G_IS_VOLUME (volume))
    {
      g_volume_mount (G_VOLUME (volume), G_MOUNT_MOUNT_NONE, mount_operation, cancellable, volume_mount_cb, async_data);
      handled = TRUE;
    }

  if (!handled)
    free_async_data (async_data);

  return cancellable;
}

static void
enclosing_volume_mount_cb (GObject      *source_object,
			   GAsyncResult *result,
			   gpointer      user_data)
{
  GtkFileSystemVolume *volume;
  AsyncFuncData *async_data;
  GError *error = NULL;

  async_data = (AsyncFuncData *) user_data;
  g_file_mount_enclosing_volume_finish (G_FILE (source_object), result, &error);
  volume = _gtk_file_system_gio_get_volume_for_file (async_data->file_system, G_FILE (source_object));

  /* Silently drop G_IO_ERROR_ALREADY_MOUNTED error for gvfs backends without visible mounts. */
  /* Better than doing query_info with additional I/O every time. */
  if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED))
    g_clear_error (&error);

  gdk_threads_enter ();
  ((GtkFileSystemVolumeMountCallback) async_data->callback) (async_data->cancellable, volume,
							     error, async_data->data);
  gdk_threads_leave ();

  if (error)
    g_error_free (error);

  _gtk_file_system_gio_volume_unref (volume);
}

GCancellable *
_gtk_file_system_gio_mount_enclosing_volume (GtkFileSystem *file_system,
					     GFile                             *file,
					     GMountOperation                   *mount_operation,
					     GtkFileSystemVolumeMountCallback   callback,
					     gpointer                           data)
{
  GCancellable *cancellable;
  AsyncFuncData *async_data;

  g_return_val_if_fail (GTK_IS_FILE_SYSTEM_GIO (file_system), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  DEBUG ("mount_enclosing_volume");

  cancellable = g_cancellable_new ();

  async_data = g_new0 (AsyncFuncData, 1);
  async_data->file_system = g_object_ref (file_system);
  async_data->file = g_object_ref (file);
  async_data->cancellable = g_object_ref (cancellable);

  async_data->callback = callback;
  async_data->data = data;

  g_file_mount_enclosing_volume (file,
				 G_MOUNT_MOUNT_NONE,
				 mount_operation,
				 cancellable,
				 enclosing_volume_mount_cb,
				 async_data);
  return cancellable;
}

gboolean
_gtk_file_system_gio_insert_bookmark (GtkFileSystem *file_system,
				      GFile          *file,
				      gint            position,
				      GError        **error)
{
  GtkFileSystemGioPrivate *priv;
  GSList *bookmarks;
  GtkFileSystemBookmark *bookmark;
  gboolean result = TRUE;

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (bookmark->file, file))
	{
	  /* File is already in bookmarks */
	  result = FALSE;
	  break;
	}
    }

  if (!result)
    {
      gchar *uri = g_file_get_uri (file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS,
		   "%s already exists in the bookmarks list",
		   uri);

      g_free (uri);

      return FALSE;
    }

  bookmark = g_slice_new0 (GtkFileSystemBookmark);
  bookmark->file = g_object_ref (file);

  priv->bookmarks = g_slist_insert (priv->bookmarks, bookmark, position);
  save_bookmarks (priv->bookmarks_file, priv->bookmarks);

  g_signal_emit_by_name (file_system, "bookmarks-changed");

  return TRUE;
}

gboolean
_gtk_file_system_gio_remove_bookmark (GtkFileSystem  *file_system,
				      GFile          *file,
				      GError        **error)
{
  GtkFileSystemGioPrivate *priv;
  GtkFileSystemBookmark *bookmark;
  GSList *bookmarks;
  gboolean result = FALSE;

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);

  if (!priv->bookmarks)
    return FALSE;

  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      bookmark = bookmarks->data;

      if (g_file_equal (bookmark->file, file))
	{
	  result = TRUE;
	  priv->bookmarks = g_slist_remove_link (priv->bookmarks, bookmarks);
	  _gtk_file_system_gio_bookmark_free (bookmark);
	  g_slist_free_1 (bookmarks);
	  break;
	}

      bookmarks = bookmarks->next;
    }

  if (!result)
    {
      gchar *uri = g_file_get_uri (file);

      g_set_error (error,
		   GTK_FILE_CHOOSER_ERROR,
		   GTK_FILE_CHOOSER_ERROR_NONEXISTENT,
		   "%s does not exist in the bookmarks list",
		   uri);

      g_free (uri);

      return FALSE;
    }

  save_bookmarks (priv->bookmarks_file, priv->bookmarks);

  g_signal_emit_by_name (file_system, "bookmarks-changed");

  return TRUE;
}

gchar *
_gtk_file_system_gio_get_bookmark_label (GtkFileSystem *file_system,
					 GFile         *file)
{
  GtkFileSystemGioPrivate *priv;
  GSList *bookmarks;
  gchar *label = NULL;

  DEBUG ("get_bookmark_label");

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      GtkFileSystemBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
	  label = g_strdup (bookmark->label);
	  break;
	}
    }

  return label;
}

void
_gtk_file_system_gio_set_bookmark_label (GtkFileSystem *file_system,
					 GFile         *file,
					 const gchar   *label)
{
  GtkFileSystemGioPrivate *priv;
  gboolean changed = FALSE;
  GSList *bookmarks;

  DEBUG ("set_bookmark_label");

  priv = GTK_FILE_SYSTEM_GIO_GET_PRIVATE (file_system);
  bookmarks = priv->bookmarks;

  while (bookmarks)
    {
      GtkFileSystemBookmark *bookmark;

      bookmark = bookmarks->data;
      bookmarks = bookmarks->next;

      if (g_file_equal (file, bookmark->file))
	{
	  g_free (bookmark->label);
	  bookmark->label = g_strdup (label);
	  changed = TRUE;
	  break;
	}
    }

  save_bookmarks (priv->bookmarks_file, priv->bookmarks);

  if (changed)
    g_signal_emit_by_name (file_system, "bookmarks-changed");
}

static GtkFileSystemVolume *
_gtk_file_system_gio_get_volume_for_file (GtkFileSystem *file_system,
					  GFile         *file)
{
  GMount *mount;

  DEBUG ("get_volume_for_file");

  mount = g_file_find_enclosing_mount (file, NULL, NULL);

  if (!mount && g_file_is_native (file))
    return (GtkFileSystemVolume *) root_volume_token;

  return (GtkFileSystemVolume *) mount;
}

/* GtkFolder methods */

static void
gtk_folder_gio_iface_init (GtkFolderIface *iface)
{
  iface->get_info = _gtk_folder_gio_get_info;
  iface->list_children = gtk_folder_gio_list_children;
  iface->is_finished_loading = _gtk_folder_gio_is_finished_loading;
}

static void
gtk_folder_gio_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      priv->folder_file = g_value_dup_object (value);
      break;
    case PROP_ENUMERATOR:
      priv->enumerator = g_value_dup_object (value);
      break;
    case PROP_ATTRIBUTES:
      priv->attributes = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_folder_gio_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, priv->folder_file);
      break;
    case PROP_ENUMERATOR:
      g_value_set_object (value, priv->enumerator);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_string (value, priv->attributes);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
query_created_file_info_callback (GObject      *source_object,
				  GAsyncResult *result,
				  gpointer      user_data)
{
  GFile *file = G_FILE (source_object);
  GError *error = NULL;
  GFileInfo *info;
  GtkFolder *folder;
  GSList *files;

  info = g_file_query_info_finish (file, result, &error);

  if (error)
    {
      g_error_free (error);
      return;
    }

  gdk_threads_enter ();

  folder = GTK_FOLDER (user_data);
  gtk_folder_gio_add_file (folder, file, info);

  files = g_slist_prepend (NULL, file);
  g_signal_emit_by_name (folder, "files-added", files);
  g_slist_free (files);

  g_object_unref (info);
  gdk_threads_leave ();
}

static void
directory_monitor_changed (GFileMonitor      *monitor,
			   GFile             *file,
			   GFile             *other_file,
			   GFileMonitorEvent  event,
			   gpointer           data)
{
  GtkFolderGioPrivate *priv;
  GtkFolderGio *folder;
  GSList *files;

  folder = GTK_FOLDER_GIO (data);
  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);
  files = g_slist_prepend (NULL, file);

  gdk_threads_enter ();

  switch (event)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
      g_file_query_info_async (file,
			       priv->attributes,
			       G_FILE_QUERY_INFO_NONE,
			       G_PRIORITY_DEFAULT,
			       priv->cancellable,
			       query_created_file_info_callback,
			       folder);
      break;
    case G_FILE_MONITOR_EVENT_DELETED:
      if (g_file_equal (file, priv->folder_file))
	g_signal_emit_by_name (folder, "deleted");
      else
	g_signal_emit_by_name (folder, "files-removed", files);
      break;
    default:
      break;
    }

  gdk_threads_leave ();

  g_slist_free (files);
}

static void
enumerator_files_callback (GObject      *source_object,
			   GAsyncResult *result,
			   gpointer      user_data)
{
  GFileEnumerator *enumerator;
  GtkFolderGioPrivate *priv;
  GtkFolder *folder;
  GError *error = NULL;
  GSList *files = NULL;
  GList *file_infos, *f;

  enumerator = G_FILE_ENUMERATOR (source_object);
  file_infos = g_file_enumerator_next_files_finish (enumerator, result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  folder = GTK_FOLDER (user_data);
  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);

  if (!file_infos)
    {
      g_file_enumerator_close_async (enumerator,
				     G_PRIORITY_DEFAULT,
				     NULL, NULL, NULL);

      gtk_folder_gio_set_finished_loading (folder, TRUE);
      return;
    }

  g_file_enumerator_next_files_async (enumerator, FILES_PER_QUERY,
				      G_PRIORITY_DEFAULT,
				      priv->cancellable,
				      enumerator_files_callback,
				      folder);

  for (f = file_infos; f; f = f->next)
    {
      GFileInfo *info;
      GFile *child_file;

      info = f->data;
      child_file = g_file_get_child (priv->folder_file, g_file_info_get_name (info));
      gtk_folder_gio_add_file (folder, child_file, info);
      files = g_slist_prepend (files, child_file);
    }

  gdk_threads_enter ();
  g_signal_emit_by_name (folder, "files-added", files);
  gdk_threads_leave ();

  g_list_foreach (file_infos, (GFunc) g_object_unref, NULL);
  g_list_free (file_infos);

  g_slist_foreach (files, (GFunc) g_object_unref, NULL);
  g_slist_free (files);
}

static void
gtk_folder_gio_constructed (GObject *object)
{
  GtkFolderGioPrivate *priv;
  GError *error = NULL;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (object);
  priv->directory_monitor = g_file_monitor_directory (priv->folder_file, G_FILE_MONITOR_NONE, NULL, &error);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  else
    g_signal_connect (priv->directory_monitor, "changed",
		      G_CALLBACK (directory_monitor_changed), object);

  g_file_enumerator_next_files_async (priv->enumerator,
				      FILES_PER_QUERY,
				      G_PRIORITY_DEFAULT,
				      priv->cancellable,
				      enumerator_files_callback,
				      object);
  /* This isn't needed anymore */
  g_object_unref (priv->enumerator);
  priv->enumerator = NULL;

  G_OBJECT_CLASS (_gtk_folder_gio_parent_class)->constructed (object);
}

static void
gtk_folder_gio_finalize (GObject *object)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (object);

  g_hash_table_unref (priv->children);

  if (priv->folder_file)
    g_object_unref (priv->folder_file);

  if (priv->directory_monitor)
    g_object_unref (priv->directory_monitor);

  g_cancellable_cancel (priv->cancellable);
  g_object_unref (priv->cancellable);
  g_free (priv->attributes);

  G_OBJECT_CLASS (_gtk_folder_gio_parent_class)->finalize (object);
}

static void
_gtk_folder_gio_class_init (GtkFolderGioClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = gtk_folder_gio_set_property;
  object_class->get_property = gtk_folder_gio_get_property;
  object_class->constructed = gtk_folder_gio_constructed;
  object_class->finalize = gtk_folder_gio_finalize;

  g_object_class_install_property (object_class,
				   PROP_FILE,
				   g_param_spec_object ("file",
							"File",
							"GFile for the folder",
							G_TYPE_FILE,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_ENUMERATOR,
				   g_param_spec_object ("enumerator",
							"Enumerator",
							"GFileEnumerator to list files",
							G_TYPE_FILE_ENUMERATOR,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_string ("attributes",
							"Attributes",
							"Attributes to query for",
							NULL,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GtkFolderGioPrivate));
}

static void
_gtk_folder_gio_init (GtkFolderGio *folder)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);

  priv->children = g_hash_table_new_full (g_file_hash,
					  (GEqualFunc) g_file_equal,
					  (GDestroyNotify) g_object_unref,
					  (GDestroyNotify) g_object_unref);
  priv->cancellable = g_cancellable_new ();
}

static void
gtk_folder_gio_set_finished_loading (GtkFolder *folder,
				     gboolean   finished_loading)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);
  priv->finished_loading = (finished_loading == TRUE);

  gdk_threads_enter ();
  g_signal_emit_by_name (folder, "finished-loading");
  gdk_threads_leave ();
}

static void
gtk_folder_gio_add_file (GtkFolder *folder,
			 GFile     *file,
			 GFileInfo *info)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);

  g_hash_table_insert (priv->children,
		       g_object_ref (file),
		       g_object_ref (info));
}

static GSList *
_gtk_folder_gio_list_children (GtkFolder *folder)
{
  GtkFolderGioPrivate *priv;
  GList *files, *elem;
  GSList *children = NULL;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);
  files = g_hash_table_get_keys (priv->children);
  children = NULL;

  for (elem = files; elem; elem = elem->next)
    children = g_slist_prepend (children, g_object_ref (elem->data));

  g_list_free (files);

  return children;
}

static gboolean
gtk_folder_gio_list_children (GtkFolder    *folder,
			      GSList          **children,
			      GError          **error)
{
  *error = NULL;
  *children = _gtk_folder_gio_list_children(folder);
  return TRUE;
}

static GFileInfo *
_gtk_folder_gio_get_info (GtkFolder  *folder,
			  GFile      *file)
{
  GtkFolderGioPrivate *priv;
  GFileInfo *info;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);
  info = g_hash_table_lookup (priv->children, file);

  if (!info)
    return NULL;

  return g_object_ref (info);
}

static gboolean
_gtk_folder_gio_is_finished_loading (GtkFolder *folder)
{
  GtkFolderGioPrivate *priv;

  priv = GTK_FOLDER_GIO_GET_PRIVATE (folder);

  return priv->finished_loading;
}

/* GtkFileSystemVolume public methods */
gchar *
_gtk_file_system_gio_volume_get_display_name (GtkFileSystemVolume *volume)
{
  DEBUG ("volume_get_display_name");

  if (IS_ROOT_VOLUME (volume))
    return g_strdup (_(root_volume_token));
  if (G_IS_DRIVE (volume))
    return g_drive_get_name (G_DRIVE (volume));
  else if (G_IS_MOUNT (volume))
    return g_mount_get_name (G_MOUNT (volume));
  else if (G_IS_VOLUME (volume))
    return g_volume_get_name (G_VOLUME (volume));

  return NULL;
}

gboolean
_gtk_file_system_gio_volume_is_mounted (GtkFileSystemVolume *volume)
{
  gboolean mounted;

  DEBUG ("volume_is_mounted");

  if (IS_ROOT_VOLUME (volume))
    return TRUE;

  mounted = FALSE;

  if (G_IS_MOUNT (volume))
    mounted = TRUE;
  else if (G_IS_VOLUME (volume))
    {
      GMount *mount;

      mount = g_volume_get_mount (G_VOLUME (volume));

      if (mount)
	{
	  mounted = TRUE;
	  g_object_unref (mount);
	}
    }

  return mounted;
}

GFile *
_gtk_file_system_gio_volume_get_root (GtkFileSystemVolume *volume)
{
  GFile *file = NULL;

  DEBUG ("volume_get_base");

  if (IS_ROOT_VOLUME (volume))
    return g_file_new_for_uri ("file:///");

  if (G_IS_MOUNT (volume))
    file = g_mount_get_root (G_MOUNT (volume));
  else if (G_IS_VOLUME (volume))
    {
      GMount *mount;

      mount = g_volume_get_mount (G_VOLUME (volume));

      if (mount)
	{
	  file = g_mount_get_root (mount);
	  g_object_unref (mount);
	}
    }

  return file;
}

GtkFileSystemVolume *
_gtk_file_system_gio_volume_ref (GtkFileSystemVolume *volume)
{
  if (IS_ROOT_VOLUME (volume))
    return volume;

  if (G_IS_MOUNT (volume)  ||
      G_IS_VOLUME (volume) ||
      G_IS_DRIVE (volume))
    g_object_ref (volume);

  return volume;
}

void
_gtk_file_system_gio_volume_unref (GtkFileSystemVolume *volume)
{
  /* Root volume doesn't need to be freed */
  if (IS_ROOT_VOLUME (volume))
    return;

  if (G_IS_MOUNT (volume)  ||
      G_IS_VOLUME (volume) ||
      G_IS_DRIVE (volume))
    g_object_unref (volume);
}
