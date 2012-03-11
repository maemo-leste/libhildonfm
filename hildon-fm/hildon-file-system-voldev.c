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
                                           *location, GtkFileSystem *fs);

static char *
hildon_file_system_voldev_get_extra_info (HildonFileSystemSpecialLocation
					  *location);
static void init_vol_type (const char *path,
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

    if (voldev->volume)
      gnome_vfs_volume_unref (voldev->volume);
    if (voldev->drive)
      gnome_vfs_drive_unref (voldev->drive);

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

  if (voldev->vol_type == INT_CARD) {
    value = gconf_client_get_bool (klass->gconf,
                                   USED_OVER_USB_INTERNAL_KEY, &error);
    corrupted = gconf_client_get_bool (klass->gconf,
				       CORRUPTED_INTERNAL_MMC_KEY, &error);
    cover_open = gconf_client_get_bool (klass->gconf,
				        OPEN_INTERNAL_MMC_COVER_KEY, &error);
  } else if (voldev->vol_type == EXT_CARD) {
    value = gconf_client_get_bool (klass->gconf,
                                   USED_OVER_USB_KEY, &error);
    corrupted = gconf_client_get_bool (klass->gconf,
				       CORRUPTED_MMC_KEY, &error);
    cover_open = gconf_client_get_bool (klass->gconf,
				        OPEN_MMC_COVER_KEY, &error);
  } else
    value = corrupted = cover_open = FALSE; /* USB_STORAGE */

  if (error)
    {
      g_warning ("gconf_client_get_bool failed: %s", error->message);
      g_error_free (error);
    }
  else
    voldev->used_over_usb = value;

  g_debug("%s type: %d, used_over_usb: %d", location->basepath,
               voldev->vol_type, voldev->used_over_usb);

  if (voldev->volume && !voldev->used_over_usb && !cover_open)
    visible = gnome_vfs_volume_is_mounted (voldev->volume);
  else if (voldev->drive && voldev->vol_type == USB_STORAGE)
    visible = FALSE; /* USB drives are never visible */
  else if (voldev->drive && !voldev->used_over_usb && !cover_open)
    visible = (gnome_vfs_drive_is_connected (voldev->drive)
	       && !gnome_vfs_drive_is_mounted (voldev->drive)
	       && corrupted);

  return visible;
}

static GnomeVFSDrive *
find_drive (const char *device)
{
  GnomeVFSVolumeMonitor *monitor;
  GList *drives, *d;
  GnomeVFSDrive *drive = NULL;

  monitor = gnome_vfs_get_volume_monitor ();

  drives = gnome_vfs_volume_monitor_get_connected_drives (monitor);
  for (d = drives; d; d = d->next)
    {
      GnomeVFSDrive *dr = d->data;

      if (!strcmp (device, gnome_vfs_drive_get_device_path (dr)))
        {
          drive = dr;
          break;
        }
    }
  g_list_free (drives);

  if (drive)
    gnome_vfs_drive_ref (drive);

  return drive;
}

GnomeVFSVolume *
find_volume (const char *uri)
{
  GnomeVFSVolumeMonitor *monitor;
  GList *volumes, *v;
  GnomeVFSVolume *volume = NULL;

  monitor = gnome_vfs_get_volume_monitor ();

  volumes = gnome_vfs_volume_monitor_get_mounted_volumes (monitor);
  for (v = volumes; v; v = v->next)
    {
      GnomeVFSVolume *vo = v->data;
      gchar *activation_uri = gnome_vfs_volume_get_activation_uri (vo);

      if (!strcmp (uri, activation_uri))
        {
          g_free (activation_uri);
          volume = vo;
          break;
        }
      g_free (activation_uri);
    }
  g_list_free (volumes);

  if (volume)
    gnome_vfs_volume_ref (volume);

  return volume;
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

static void init_vol_type (const char *path,
                           HildonFileSystemVoldev *voldev)
{
  HildonFileSystemVoldevClass *klass;
  gchar *value;
  gboolean drive;

  if (voldev->vol_type_valid)
    /* already initialised */
    return;

  if (path == NULL)
    {
      g_warning ("path == NULL");
      return;
    }

  klass = HILDON_FILE_SYSTEM_VOLDEV_GET_CLASS (voldev);

  if (g_str_has_prefix (path, "drive:///dev/sd") ||
      g_str_has_prefix (path, "file:///media/usb/"))
    {
      voldev->vol_type = USB_STORAGE;
      voldev->vol_type_valid = TRUE;
      return;
    }
  else if (g_str_has_prefix (path, "drive://"))
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
          if (g_str_has_prefix (path, buf))
            voldev->vol_type = EXT_CARD;
          else
          {
            if(g_str_has_prefix (path, "drive:///media/mmc"))
              voldev->vol_type = EXT_CARD;
            else
              voldev->vol_type = INT_CARD;
          }
        }
      else
        {
          snprintf (buf, 100, "file://%s", value);
          if (strncmp (buf, path, 100) == 0)
            voldev->vol_type = EXT_CARD;
          else
          {
            if( g_str_has_prefix (path, "file:///media/mmc"))
              voldev->vol_type = EXT_CARD;
            else
              voldev->vol_type = INT_CARD;
          }
        }

      voldev->vol_type_valid = TRUE;
      g_free (value);
    }
}

static void
hildon_file_system_voldev_volumes_changed (HildonFileSystemSpecialLocation
                                           *location, GtkFileSystem *fs)
{
  HildonFileSystemVoldev *voldev = HILDON_FILE_SYSTEM_VOLDEV (location);

  if (voldev->volume)
    {
      gnome_vfs_volume_unref (voldev->volume);
      voldev->volume = NULL;
    }
  if (voldev->drive)
    {
      gnome_vfs_drive_unref (voldev->drive);
      voldev->drive = NULL;
    }

  if (g_str_has_prefix (location->basepath, "drive://"))
    voldev->drive = find_drive (location->basepath + 8);
  else
    voldev->volume = find_volume (location->basepath);

  if (!voldev->vol_type_valid)
    init_vol_type (location->basepath, voldev);

  if (voldev->volume)
    {
      g_free (location->fixed_title);
      g_free (location->fixed_icon);
      location->fixed_title =
        gnome_vfs_volume_get_display_name (voldev->volume);
      location->fixed_icon = gnome_vfs_volume_get_icon (voldev->volume);
    }
  else if (voldev->drive)
    {
      g_free (location->fixed_title);
      g_free (location->fixed_icon);
      location->fixed_title = gnome_vfs_drive_get_display_name (voldev->drive);
      location->fixed_icon = gnome_vfs_drive_get_icon (voldev->drive);
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

  if (voldev->volume)
    return g_strdup (gnome_vfs_volume_get_device_path (voldev->volume));
  else if (voldev->drive)
    return g_strdup (gnome_vfs_drive_get_device_path (voldev->drive));
  else
    return NULL;
}

