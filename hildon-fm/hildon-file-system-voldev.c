/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2007 Nokia Corporation.  All rights reserved.
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

#include <glib.h>
#include <string.h>
#include <ctype.h>

#include "hildon-file-system-voldev.h"
#include "hildon-file-system-settings.h"
#include "hildon-file-system-dynamic-device.h"
#include "hildon-file-common-private.h"
#include "hildon-file-system-private.h"

#define GCONF_PATH "/system/osso/af"
#define GCONF_PATH_MMC "/system/osso/af/mmc"
#define USED_OVER_USB_KEY GCONF_PATH "/mmc-used-over-usb"
#define USED_OVER_USB_INTERNAL_KEY GCONF_PATH "/internal-mmc-used-over-usb"
#define CORRUPTED_MMC_KEY GCONF_PATH_MMC "/mmc-corrupted"
#define CORRUPTED_INTERNAL_MMC_KEY GCONF_PATH_MMC "/internal-mmc-corrupted"
#define OPEN_MMC_COVER_KEY GCONF_PATH "/mmc-cover-open"
#define OPEN_INTERNAL_MMC_COVER_KEY GCONF_PATH "/internal-mmc-cover-open"

static void
hildon_file_system_voldev_class_init (HildonFileSystemVoldevClass *klass);
static void
hildon_file_system_voldev_finalize (GObject *obj);
static void
hildon_file_system_voldev_init (HildonFileSystemVoldev *device);

static void
hildon_file_system_voldev_volumes_changed (HildonFileSystemSpecialLocation
					   *location);

static char *
hildon_file_system_voldev_get_extra_info (HildonFileSystemSpecialLocation
					  *location);
static GCancellable *
hildon_file_system_voldev_get_folder (HildonFileSystemSpecialLocation *location,
				      GtkFileSystem                   *filesystem,
				      GFile                           *file,
				      const char                      *attributes,
				      GtkFileSystemGetFolderCallback   callback,
				      gpointer                         data);
static gboolean
hildon_file_system_voldev_is_available (HildonFileSystemSpecialLocation *location);

static void init_vol_type (GFile *file,
                           HildonFileSystemVoldev *voldev);

G_DEFINE_TYPE (HildonFileSystemVoldev,
               hildon_file_system_voldev,
               HILDON_TYPE_FILE_SYSTEM_SPECIAL_LOCATION);

enum {
    PROP_USED_OVER_USB = 1
};

static void
gconf_value_changed(GConfClient *client, guint cnxn_id,
                    GConfEntry *entry, gpointer data)
{
    HildonFileSystemVoldev *voldev;
    HildonFileSystemSpecialLocation *location;
    gboolean change = FALSE;

    voldev = HILDON_FILE_SYSTEM_VOLDEV (data);
    location = HILDON_FILE_SYSTEM_SPECIAL_LOCATION (voldev);

    if (!voldev->vol_type_valid)
      init_vol_type (location->basepath, voldev);

    if (voldev->vol_type == INT_CARD &&
        g_ascii_strcasecmp (entry->key, USED_OVER_USB_INTERNAL_KEY) == 0)
      change = TRUE;
    else if (voldev->vol_type == EXT_CARD &&
             g_ascii_strcasecmp (entry->key, USED_OVER_USB_KEY) == 0)
      change = TRUE;
    else if (voldev->vol_type == INT_CARD &&
             g_ascii_strcasecmp (entry->key, OPEN_INTERNAL_MMC_COVER_KEY) == 0)
      change = TRUE;
    else if (voldev->vol_type == EXT_CARD &&
             g_ascii_strcasecmp (entry->key, OPEN_MMC_COVER_KEY) == 0)
      change = TRUE;

    if (change)
      {
        voldev->used_over_usb = gconf_value_get_bool (entry->value);
        g_debug("%s = %d", entry->key, voldev->used_over_usb);
        g_signal_emit_by_name (location, "changed");
        g_signal_emit_by_name (data, "rescan");
      }
}

static void
hildon_file_system_voldev_class_init (HildonFileSystemVoldevClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    HildonFileSystemSpecialLocationClass *location =
            HILDON_FILE_SYSTEM_SPECIAL_LOCATION_CLASS (klass);
    GError *error = NULL;

    gobject_class->finalize = hildon_file_system_voldev_finalize;

    location->requires_access = FALSE;
    location->is_visible = hildon_file_system_voldev_is_visible;
    location->volumes_changed = hildon_file_system_voldev_volumes_changed;
    location->get_extra_info = hildon_file_system_voldev_get_extra_info;
    location->get_folder = hildon_file_system_voldev_get_folder;
    location->is_available = hildon_file_system_voldev_is_available;

    klass->gconf = gconf_client_get_default ();
    gconf_client_add_dir (klass->gconf, GCONF_PATH,
                          GCONF_CLIENT_PRELOAD_NONE, &error);
    if (error != NULL)
      {
        g_warning ("gconf_client_add_dir failed: %s", error->message);
        g_error_free (error);
      }
}

static void
hildon_file_system_voldev_init (HildonFileSystemVoldev *device)
{
    HildonFileSystemSpecialLocation *location;
    GError *error = NULL;
    HildonFileSystemVoldevClass *klass =
            HILDON_FILE_SYSTEM_VOLDEV_GET_CLASS (device);

    location = HILDON_FILE_SYSTEM_SPECIAL_LOCATION (device);
    location->compatibility_type = HILDON_FILE_SYSTEM_MODEL_MMC;
    location->failed_access_message = NULL;

    gconf_client_notify_add (klass->gconf, GCONF_PATH,
                             gconf_value_changed, device, NULL, &error);
    if (error != NULL)
      {
        g_warning ("gconf_client_notify_add failed: %s", error->message);
        g_error_free (error);
      }
}

static void
hildon_file_system_voldev_finalize (GObject *obj)
{
    HildonFileSystemVoldev *voldev;

    voldev = HILDON_FILE_SYSTEM_VOLDEV (obj);

    if (voldev->mount)
      g_object_unref (voldev->mount);
    if (voldev->volume)
      g_object_unref (voldev->volume);

    G_OBJECT_CLASS (hildon_file_system_voldev_parent_class)->finalize (obj);
}

gboolean __attribute__ ((visibility("hidden")))
hildon_file_system_voldev_is_visible (HildonFileSystemSpecialLocation *location,
				      gboolean has_children)
{
  HildonFileSystemVoldev *voldev = HILDON_FILE_SYSTEM_VOLDEV (location);
  HildonFileSystemVoldevClass *klass =
          HILDON_FILE_SYSTEM_VOLDEV_GET_CLASS (voldev);
  gboolean visible = FALSE;
  GError *error = NULL;
  gboolean value, corrupted, cover_open;

  if (!voldev->vol_type_valid)
    init_vol_type (location->basepath, voldev);

  if (voldev->vol_type == INT_CARD)
    {
      value = gconf_client_get_bool (klass->gconf,
				     USED_OVER_USB_INTERNAL_KEY, &error);
      corrupted = gconf_client_get_bool (klass->gconf,
					 CORRUPTED_INTERNAL_MMC_KEY, &error);
      cover_open = gconf_client_get_bool (klass->gconf,
					  OPEN_INTERNAL_MMC_COVER_KEY, &error);
    }
  else if (voldev->vol_type == EXT_CARD)
    {
      value = gconf_client_get_bool (klass->gconf,
				     USED_OVER_USB_KEY, &error);
      corrupted = gconf_client_get_bool (klass->gconf,
					 CORRUPTED_MMC_KEY, &error);
      cover_open = gconf_client_get_bool (klass->gconf,
					  OPEN_MMC_COVER_KEY, &error);
    }
  else
    value = corrupted = cover_open = FALSE; /* USB_STORAGE */

  if (error)
    {
      g_warning ("gconf_client_get_bool failed: %s", error->message);
      g_error_free (error);
    }
  else
    voldev->used_over_usb = value;

  DEBUG_GFILE_URI("%s type: %d, used_over_usb: %d", location->basepath,
		  voldev->vol_type, voldev->used_over_usb);

  if (voldev->mount && !voldev->used_over_usb && !cover_open)
    visible = TRUE;
  else if (voldev->volume &&
	   ((!voldev->used_over_usb && !cover_open) ||
	    voldev->vol_type == USB_STORAGE))
  {
    GMount *mount = g_volume_get_mount (voldev->volume);

    if (voldev->vol_type == USB_STORAGE)
      visible =  !mount && g_volume_can_mount (voldev->volume);
    else
      visible = !mount && g_volume_can_mount (voldev->volume) && corrupted;

    if (mount)
      g_object_unref (mount);
  }

  return visible;
}

static GVolume *
find_volume (GFile *file)
{
  GVolumeMonitor *monitor;
  GList *volumes, *v;
  GVolume *volume = NULL;
  gchar *path = g_file_get_uri (file);

  monitor = g_volume_monitor_get ();
  volumes = g_volume_monitor_get_volumes (monitor);

  for (v = volumes; v; v = v->next)
    {
      GVolume *vol = v->data;
      gchar *id =
          g_volume_get_identifier (vol, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
      gchar *uri = g_strdup_printf ("drive://%s", id);

      g_free (id);

      if (!strcmp (path, uri))
        {
          volume = vol;
          g_object_ref (volume);
	  g_free (uri);
          break;
        }

      g_free (uri);
    }

  g_free (path);
  g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
  g_list_free (volumes);
  g_object_unref (monitor);

  return volume;
}

GMount *
find_mount (GFile *file)
{
  return g_file_find_enclosing_mount (file, NULL, NULL);
}

static void
capitalize_and_remove_trailing_spaces (gchar *str)
{
  /* STR must not consist of only whitespace.
   */

  gchar *last_non_space;

  if (*str)
    {
      last_non_space = str;
      *str = g_ascii_toupper (*str);
      str++;
      while (*str)
        {
          *str = g_ascii_tolower (*str);
	  if (!isspace (*str))
	    last_non_space = str;
          str++;
        }
      *(last_non_space+1) = '\0';
    }
}

static char *
beautify_mmc_name (char *name, gboolean internal)
{
  if (name && strncmp (name, "mmc-undefined-name", 18) == 0)
    {
      g_free (name);
      name = NULL;
    }

  if (!name)
    {
      if (internal)
	/* string hardcoded as agreed on Bug #140752 Comment 11-12 */
	name = "Nokia N900";
      else
	name = _("sfil_li_memorycard_removable");
      name = g_strdup (name);
    }
  else
    capitalize_and_remove_trailing_spaces (name);

  return name;
}

static void init_vol_type (GFile *file,
                           HildonFileSystemVoldev *voldev)
{
  HildonFileSystemVoldevClass *klass;
  gchar *value;
  gboolean drive;
  gchar *uri;

  if (voldev->vol_type_valid)
    /* already initialised */
    return;

  if (file == NULL)
    {
      g_warning ("file == NULL");
      return;
    }

  uri = g_file_get_uri (file);
  klass = HILDON_FILE_SYSTEM_VOLDEV_GET_CLASS (voldev);

  if (g_str_has_prefix (uri, "drive:///dev/sd") ||
      g_str_has_prefix (uri, "drive:///dev/sr") ||
      g_str_has_prefix (uri, "drive:///dev/fd") ||
      g_str_has_prefix (uri, "file:///media/usb/"))
    {
      voldev->vol_type = USB_STORAGE;
      voldev->vol_type_valid = TRUE;
      return;
    }
  else if (g_str_has_prefix (uri, "drive://"))
    {
      drive = TRUE;
      value = gconf_client_get_string (klass->gconf,
                    "/system/osso/af/mmc-device-name", NULL);
    }
  else
    {
      drive = FALSE;
      value = gconf_client_get_string (klass->gconf,
                    "/system/osso/af/mmc-mount-point", NULL);
    }

  if (value)
    {
      char buf[100];

      if (drive)
        {
          snprintf (buf, 100, "drive://%s", value);
	  if (g_str_has_prefix (uri, buf))
            voldev->vol_type = EXT_CARD;
          else
          {
	    if(g_str_has_prefix (uri, "drive:///media/mmc"))
              voldev->vol_type = EXT_CARD;
            else
              voldev->vol_type = INT_CARD;
          }
        }
      else
        {
          snprintf (buf, 100, "file://%s", value);
	  if (strncmp (buf, uri, 100) == 0)
            voldev->vol_type = EXT_CARD;
          else
          {
	    if( g_str_has_prefix (uri, "file:///media/mmc"))
              voldev->vol_type = EXT_CARD;
            else
              voldev->vol_type = INT_CARD;
          }
        }

      voldev->vol_type_valid = TRUE;
      g_free (value);
    }

  g_free (uri);
}

static void
hildon_file_system_voldev_volumes_changed (HildonFileSystemSpecialLocation
					   *location)
{
  HildonFileSystemVoldev *voldev = HILDON_FILE_SYSTEM_VOLDEV (location);
  gchar *uri = g_file_get_uri (location->basepath);

  location->permanent=FALSE;

  if (voldev->mount)
    {
      g_object_unref (voldev->mount);
      voldev->mount = NULL;
    }
  if (voldev->volume)
    {
      g_object_unref (voldev->volume);
      voldev->volume = NULL;
    }

  if (g_str_has_prefix (uri, "drive://"))
    voldev->volume = find_volume (location->basepath);
  else
    voldev->mount = find_mount (location->basepath);

  g_free (uri);

  if (!voldev->vol_type_valid)
    init_vol_type (location->basepath, voldev);

  if (voldev->mount)
    {
      GIcon *icon = g_mount_get_icon (voldev->mount);

      g_free (location->fixed_title);
      g_free (location->fixed_icon);
      location->fixed_title = g_mount_get_name (voldev->mount);
      location->fixed_icon = NULL;

      if (icon)
	{
	  if (G_IS_THEMED_ICON (icon))
	    {
	      location->fixed_icon =
		  g_strdup(g_themed_icon_get_names (G_THEMED_ICON (icon))[0]);
	    }
	  else
	    location->fixed_icon = g_icon_to_string (icon);
	  g_object_unref (icon);
	}
    }
  else if (voldev->volume)
    {
      GIcon *icon = g_volume_get_icon (voldev->volume);

      g_free (location->fixed_title);
      g_free (location->fixed_icon);
      location->fixed_icon = NULL;
      location->fixed_title = g_volume_get_name (voldev->volume);

      if (icon)
	{
	  if (G_IS_THEMED_ICON (icon))
	    {
	      location->fixed_icon =
		  g_strdup(g_themed_icon_get_names (G_THEMED_ICON (icon))[0]);
	    }
	  else
	    location->fixed_icon = g_icon_to_string (icon);
	  g_object_unref (icon);
	}
    }

  /* XXX - GnomeVFS should provide the right icons and display names.
   */

  location->sort_weight = SORT_WEIGHT_USB;
  if (location->fixed_icon)
    {
      if (strcmp (location->fixed_icon, "gnome-dev-removable-usb") == 0
	  || strcmp (location->fixed_icon, "gnome-dev-harddisk-usb") == 0)
        location->fixed_icon = g_strdup ("filemanager_removable_storage");
      else if (strcmp (location->fixed_icon, "gnome-dev-removable") == 0
	       || strcmp (location->fixed_icon, "gnome-dev-media-sdmmc") == 0)
	{
	  if (voldev->vol_type == INT_CARD)
	    {
	      location->sort_weight = SORT_WEIGHT_INTERNAL_MMC;
	      location->fixed_icon =
		g_strdup ("general_device_root_folder");
	    }
	  else
	    {
	      location->sort_weight = SORT_WEIGHT_EXTERNAL_MMC;
	      location->fixed_icon = 
		g_strdup ("general_removable_memory_card");
	    }
	  
	  location->fixed_title = beautify_mmc_name (location->fixed_title,
                                      voldev->vol_type == INT_CARD);
	}
    }

  g_signal_emit_by_name (location, "changed");
  g_signal_emit_by_name (location, "rescan");
}

static char *
hildon_file_system_voldev_get_extra_info (HildonFileSystemSpecialLocation
					  *location)
{
  HildonFileSystemVoldev *voldev = HILDON_FILE_SYSTEM_VOLDEV (location);
  gchar *rv = NULL;

  if (voldev->mount) {
    GVolume *v = g_mount_get_volume (voldev->mount);

    if (v) {
      rv = g_volume_get_identifier (v, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
      g_object_unref (v);
    }
  }
  else if (voldev->volume) {
    rv = g_volume_get_identifier (voldev->volume,
                                  G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
  }

  return rv;
}

#define VOLDEV_TYPE_FILE_FOLDER             (voldev_file_folder_get_type ())
#define VOLDEV_FILE_FOLDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), VOLDEV_TYPE_FILE_FOLDER, VoldevFileFolder))
#define VOLDEV_IS_FILE_FOLDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VOLDEV_TYPE_FILE_FOLDER))
#define VOLDEV_FILE_FOLDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), VOLDEV_TYPE_FILE_FOLDER, VoldevFileFolderClass))
#define VOLDEV_IS_FILE_FOLDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), VOLDEV_TYPE_FILE_FOLDER))
#define VOLDEV_FILE_FOLDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), VOLDEV_TYPE_FILE_FOLDER, VoldevFileFolderClass))

typedef struct _VoldevFileFolder      VoldevFileFolder;
typedef struct _VoldevFileFolderClass VoldevFileFolderClass;

struct _VoldevFileFolderClass
{
  GObjectClass parent_class;
};

struct _VoldevFileFolder
{
  GObject parent_instance;

  GtkFileSystem *filesystem;
  HildonFileSystemVoldev *voldev;
};

static GType voldev_file_folder_get_type (void);
static void voldev_file_folder_iface_init (GtkFolderIface *iface);
static void voldev_file_folder_init (VoldevFileFolder *impl);
static void voldev_file_folder_finalize (GObject *object);

static GFileInfo *voldev_file_folder_get_info(GtkFolder  *folder,
					    GFile      *file);
static gboolean voldev_file_folder_list_children (GtkFolder  *folder,
					      GSList        **children,
					      GError        **error);
static gboolean voldev_file_folder_is_finished_loading (GtkFolder *folder);

G_DEFINE_TYPE_WITH_CODE (VoldevFileFolder, voldev_file_folder, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FOLDER,
						voldev_file_folder_iface_init))

static void
voldev_file_folder_class_init (VoldevFileFolderClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = voldev_file_folder_finalize;
}

static void
voldev_file_folder_iface_init (GtkFolderIface *iface)
{
  iface->get_info = voldev_file_folder_get_info;
  iface->list_children = voldev_file_folder_list_children;
  iface->is_finished_loading = voldev_file_folder_is_finished_loading;
}

static void
voldev_file_folder_init (VoldevFileFolder *folder)
{
  folder->filesystem = NULL;
  folder->voldev = NULL;
}

static void
voldev_file_folder_finalize (GObject *object)
{
  VoldevFileFolder *folder = VOLDEV_FILE_FOLDER (object);

  if (folder->voldev)
    g_object_unref (folder->voldev);

  G_OBJECT_CLASS (voldev_file_folder_parent_class)->finalize (object);
}

static GFileInfo *
voldev_file_folder_get_info (GtkFolder *folder,
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
voldev_file_folder_list_children (GtkFolder  *folder,
				  GSList        **children,
				  GError        **error)
{
  *children = NULL;
  *error = NULL;

  return TRUE;
}

static gboolean
voldev_file_folder_is_finished_loading (GtkFolder *folder)
{
  VoldevFileFolder *voldev_folder = VOLDEV_FILE_FOLDER(folder);
  HildonFileSystemSpecialLocation *location =
      HILDON_FILE_SYSTEM_SPECIAL_LOCATION (voldev_folder->voldev);

  if (!g_file_has_uri_scheme(location->basepath, "drive"))
    return gtk_file_folder_is_finished_loading(folder);
  else
    return TRUE;
}

struct get_folder_clos {
  GCancellable *cancellable;
  VoldevFileFolder *voldev_folder;
  GtkFileSystemGetFolderCallback callback;
  gpointer data;
};

static gboolean
deliver_get_folder_callback (gpointer data)
{
  struct get_folder_clos *clos = (struct get_folder_clos *)data;
  GDK_THREADS_ENTER ();
  clos->callback (clos->cancellable, GTK_FOLDER (clos->voldev_folder),
		  NULL, clos->data);
  GDK_THREADS_LEAVE ();
  g_object_unref (clos->cancellable);
  g_object_unref (clos->voldev_folder);
  g_free (clos);
  return FALSE;
}

static GCancellable *
hildon_file_system_voldev_get_folder (HildonFileSystemSpecialLocation *location,
				      GtkFileSystem                   *filesystem,
				      GFile                           *file,
				      const char                      *attributes,
				      GtkFileSystemGetFolderCallback   callback,
				      gpointer                         data)
{
  if (g_file_has_uri_scheme (file, "drive"))
    {
      HildonFileSystemVoldev *voldev = HILDON_FILE_SYSTEM_VOLDEV (location);

      if (voldev->volume)
	{
	  GCancellable *cancellable = g_cancellable_new ();
	  VoldevFileFolder *voldev_folder =
	      g_object_new (VOLDEV_TYPE_FILE_FOLDER, NULL);
	  struct get_folder_clos *clos = g_new (struct get_folder_clos, 1);

	  voldev_folder->filesystem = filesystem;
	  voldev_folder->voldev = HILDON_FILE_SYSTEM_VOLDEV (location);
	  g_object_ref (location);

	  clos->cancellable = g_object_ref (cancellable);
	  clos->voldev_folder = voldev_folder;
	  clos->callback = callback;
	  clos->data = data;

	  g_idle_add (deliver_get_folder_callback, clos);

	  return cancellable;
	}

      return NULL;
    }
  else
    {
      return gtk_file_system_get_folder (filesystem,
					 file,
					 attributes,
					 callback,
					 data);
    }
}

static gboolean
hildon_file_system_voldev_is_available (HildonFileSystemSpecialLocation *location)
{
  HildonFileSystemVoldev *voldev = HILDON_FILE_SYSTEM_VOLDEV (location);

  if (voldev->volume)
    {
      GMount *mount = g_volume_get_mount (voldev->volume);

      if (mount)
	{
	  g_object_unref (mount);
	  return TRUE;
	}
    }
  else if (voldev->mount)
    return TRUE;

  return FALSE;
}
