/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2006 Nokia Corporation.  All rights reserved.
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

#include <string.h>
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION (2, 14, 0)
#include "gtkfilesystem/gtkfilesystem.h"
#else
#define GTK_FILE_SYSTEM_ENABLE_UNSUPPORTED
#include <gtk/gtkfilesystem.h>
#endif

#include "hildon-file-common-private.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-root.h"
#include "hildon-file-system-voldev.h"
#include "hildon-file-system-settings.h"

static void
hildon_file_system_root_class_init (HildonFileSystemRootClass *klass);
static void
hildon_file_system_root_init (HildonFileSystemRoot *device);

static void
hildon_file_system_root_volumes_changed (HildonFileSystemSpecialLocation
					*location);

static GCancellable *hildon_file_system_root_get_folder(HildonFileSystemSpecialLocation *location,
                                    GtkFileSystem                *filesystem,
				    GFile                        *file,
				    const char *attributes,
                                    GtkFileSystemGetFolderCallback  callback,
                                    gpointer                        data);

static HildonFileSystemSpecialLocation*
hildon_file_system_root_create_child_location (HildonFileSystemSpecialLocation
					       *location, GFile *file);

G_DEFINE_TYPE (HildonFileSystemRoot,
               hildon_file_system_root,
               HILDON_TYPE_FILE_SYSTEM_SPECIAL_LOCATION);

static void
hildon_file_system_root_class_init (HildonFileSystemRootClass *klass)
{
    HildonFileSystemSpecialLocationClass *location =
            HILDON_FILE_SYSTEM_SPECIAL_LOCATION_CLASS (klass);

    location->volumes_changed = hildon_file_system_root_volumes_changed;
    location->get_folder = hildon_file_system_root_get_folder;
    location->create_child_location =
      hildon_file_system_root_create_child_location;
}

static void
hildon_file_system_root_init (HildonFileSystemRoot *device)
{
    HildonFileSystemSpecialLocation *location;

    location = HILDON_FILE_SYSTEM_SPECIAL_LOCATION (device);
    location->fixed_icon = g_strdup ("general_device_root_folder");
}

static void
hildon_file_system_root_volumes_changed (HildonFileSystemSpecialLocation
					*location)
{
  g_signal_emit_by_name (location, "rescan");
}

HildonFileSystemSpecialLocation*
hildon_file_system_root_create_child_location (HildonFileSystemSpecialLocation
					       *location, GFile *file)
{
  HildonFileSystemSpecialLocation *child = NULL;
  gchar *uri = g_file_get_uri (file);
  /* XXX - Cough, a bit of hardcoding, it is better to ask GnomeVFS
           whether this is a volume or drive.
   */
  /* FIXME */
  if (g_str_has_prefix (uri, "drive://")
      || (g_str_has_prefix (uri, "file:///media/") &&
	  strchr (uri + 14, '/') == NULL)
      || (g_str_has_prefix (uri, "file:///media/usb/") &&
	  strchr (uri + 18, '/') == NULL))
    {
      child = g_object_new (HILDON_TYPE_FILE_SYSTEM_VOLDEV, NULL);
      child->basepath = g_object_ref (file);
      child->permanent = FALSE;
      hildon_file_system_special_location_volumes_changed (child);
    }

  g_free (uri);
  return child;
}

/* Wrapping GtkFileVolumes in a GtkFolder
 */

#define ROOT_TYPE_FILE_FOLDER             (root_file_folder_get_type ())
#define ROOT_FILE_FOLDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ROOT_TYPE_FILE_FOLDER, RootFileFolder))
#define ROOT_IS_FILE_FOLDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ROOT_TYPE_FILE_FOLDER))
#define ROOT_FILE_FOLDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ROOT_TYPE_FILE_FOLDER, RootFileFolderClass))
#define ROOT_IS_FILE_FOLDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ROOT_TYPE_FILE_FOLDER))
#define ROOT_FILE_FOLDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ROOT_TYPE_FILE_FOLDER, RootFileFolderClass))

typedef struct _RootFileFolder      RootFileFolder;
typedef struct _RootFileFolderClass RootFileFolderClass;

struct _RootFileFolderClass
{
  GObjectClass parent_class;
};

struct _RootFileFolder
{
  GObject parent_instance;

  GtkFileSystem *filesystem;
  HildonFileSystemRoot *root;
};

static GType root_file_folder_get_type (void);
static void root_file_folder_iface_init (GtkFolderIface *iface);
static void root_file_folder_init (RootFileFolder *impl);
static void root_file_folder_finalize (GObject *object);

static GFileInfo *root_file_folder_get_info(GtkFolder  *folder,
					    GFile      *file);
static gboolean root_file_folder_list_children (GtkFolder  *folder,
                                              GSList        **children,
                                              GError        **error);
static gboolean root_file_folder_is_finished_loading (GtkFolder *folder);

G_DEFINE_TYPE_WITH_CODE (RootFileFolder, root_file_folder, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FOLDER,
                                                root_file_folder_iface_init))

static void
root_file_folder_class_init (RootFileFolderClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = root_file_folder_finalize;
}

static void
root_file_folder_iface_init (GtkFolderIface *iface)
{
  iface->get_info = root_file_folder_get_info;
  iface->list_children = root_file_folder_list_children;
  iface->is_finished_loading = root_file_folder_is_finished_loading;
}

static void
root_file_folder_init (RootFileFolder *folder)
{
  folder->filesystem = NULL;
  folder->root = NULL;
}

static void
root_file_folder_finalize (GObject *object)
{
  RootFileFolder *folder = ROOT_FILE_FOLDER (object);

  if (folder->root)
    g_object_unref (folder->root);

  G_OBJECT_CLASS (root_file_folder_parent_class)->finalize (object);
}

static GFileInfo *
root_file_folder_get_info (GtkFolder *folder,
			   GFile     *file)
{
  GFileInfo *info = g_file_info_new ();
  gchar *basename = g_file_get_basename (file);

  /* XXX - maybe provide more detail...
   */
  DEBUG_GFILE_URI ("path %s basename %s", file, basename);
  g_file_info_set_display_name (info, basename);
  g_free (basename);
  g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);

  return info;
}

static gboolean
root_file_folder_list_children (GtkFolder  *folder,
                                GSList        **children,
                                GError        **error)
{
  RootFileFolder *root_folder = ROOT_FILE_FOLDER (folder);
  GSList *volumes = gtk_file_system_list_volumes (root_folder->filesystem);
  GSList *l;

  *error = NULL;

  for (l = volumes; l; l = l->next)
    {
      if (G_IS_MOUNT (l->data))
	*children = g_slist_append (*children, g_mount_get_root (l->data));
      else
	{
	  char *id, *uri;

	  if (G_IS_VOLUME (l->data))
	    {
	      GMount *mount = g_volume_get_mount (l->data);

	      /* Do not add mounted volumes, we already have them as GMount */
	      if (mount)
		{
		  g_object_unref (mount);
		  continue;
		}

	      id = g_volume_get_identifier (
		     l->data, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	    }
	  else
	    {
	      g_assert (G_IS_DRIVE (l->data));

	      id = g_drive_get_identifier (
		     l->data, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	    }

	  uri = g_strdup_printf ("drive://%s", id);
	  g_free (id);
	  *children = g_slist_append (*children, g_file_new_for_uri (uri));
	  g_free (uri);
	}

      DEBUG_GFILE_URI ("!!!!!!!!!!!!!!!!!!!!!!ADD %s", g_slist_last(*children)->data);
      g_object_unref (l->data);
    }

  g_slist_free (volumes);

  return TRUE;
}

static gboolean
root_file_folder_is_finished_loading (GtkFolder *folder)
{
  return TRUE;
}

struct get_folder_clos {
  GCancellable *cancellable;
  RootFileFolder *root_folder;
  GtkFileSystemGetFolderCallback callback;
  gpointer data;
};

static gboolean
deliver_get_folder_callback (gpointer data)
{
  struct get_folder_clos *clos = (struct get_folder_clos *)data;
  GDK_THREADS_ENTER ();
  clos->callback (clos->cancellable, GTK_FOLDER (clos->root_folder),
                  NULL, clos->data);
  GDK_THREADS_LEAVE ();
  g_object_unref (clos->cancellable);
  g_object_unref (clos->root_folder);
  g_free (clos);

  return FALSE;
}

static GCancellable *
hildon_file_system_root_get_folder (HildonFileSystemSpecialLocation *location,
				    GtkFileSystem                   *filesystem,
				    GFile                           *file,
				    const char                      *attributes,
				    GtkFileSystemGetFolderCallback   callback,
				    gpointer                         data)
{
  GCancellable *cancellable = g_cancellable_new ();
  RootFileFolder *root_folder = g_object_new (ROOT_TYPE_FILE_FOLDER, NULL);
  struct get_folder_clos *clos = g_new (struct get_folder_clos, 1);

  root_folder->filesystem = filesystem;
  root_folder->root = HILDON_FILE_SYSTEM_ROOT (location);
  g_object_ref (location);

  clos->cancellable = g_object_ref (cancellable);
  clos->root_folder = root_folder;
  clos->callback = callback;
  clos->data = data;

  g_idle_add (deliver_get_folder_callback, clos);

  return cancellable;
}
