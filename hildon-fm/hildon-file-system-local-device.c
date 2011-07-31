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

#include <gtk/gtk.h>
#if GTK_CHECK_VERSION (2, 14, 0)
#include "gtkfilesystem/gtkfilesystem.h"
#else
#define GTK_FILE_SYSTEM_ENABLE_UNSUPPORTED
#include <gtk/gtkfilesystem.h>
#endif

#include <libgnomevfs/gnome-vfs.h>
#include "hildon-file-common-private.h"
#include "hildon-file-system-local-device.h"
#include "hildon-file-system-settings.h"

static void
hildon_file_system_local_device_class_init (HildonFileSystemLocalDeviceClass
                                            *klass);
static void
hildon_file_system_local_device_init (HildonFileSystemLocalDevice *device);
static void
hildon_file_system_local_device_finalize (GObject *obj);
static void
btname_changed(GObject *settings, GParamSpec *param, gpointer data);
static gchar*
hildon_file_system_local_device_get_display_name (HildonFileSystemSpecialLocation
                                                  *location, GtkFileSystem *fs);
static GtkFileSystemHandle*
hildon_file_system_local_device_get_folder (HildonFileSystemSpecialLocation *location,
                                            GtkFileSystem *fs,
                                            const GtkFilePath *path,
                                            GtkFileInfoType types,
                                            GtkFileSystemGetFolderCallback callack,
                                            gpointer data);
static void
hildon_file_system_local_device_volumes_changed (HildonFileSystemSpecialLocation
						 *location, GtkFileSystem *fs);

static char *
hildon_file_system_local_device_get_extra_info (HildonFileSystemSpecialLocation
						*location);

static gpointer parent_class = NULL;

G_DEFINE_TYPE (HildonFileSystemLocalDevice,
               hildon_file_system_local_device,
               HILDON_TYPE_FILE_SYSTEM_SPECIAL_LOCATION);

static void
hildon_file_system_local_device_class_init (HildonFileSystemLocalDeviceClass
                                            *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    HildonFileSystemSpecialLocationClass *location =
            HILDON_FILE_SYSTEM_SPECIAL_LOCATION_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);
    gobject_class->finalize = hildon_file_system_local_device_finalize;
    location->get_display_name = hildon_file_system_local_device_get_display_name;
    location->get_folder = hildon_file_system_local_device_get_folder;
    location->volumes_changed = hildon_file_system_local_device_volumes_changed;
    location->get_extra_info = hildon_file_system_local_device_get_extra_info;
}

static void
hildon_file_system_local_device_init (HildonFileSystemLocalDevice *device)
{
    HildonFileSystemSettings *fs_settings;
    HildonFileSystemSpecialLocation *location;

    fs_settings = _hildon_file_system_settings_get_instance ();
    device->signal_handler_id = g_signal_connect (fs_settings,
                                                  "notify::btname",
                                                  G_CALLBACK (btname_changed),
                                                  device);

    location = HILDON_FILE_SYSTEM_SPECIAL_LOCATION (device);
    location->fixed_icon = g_strdup ("general_device_root_folder");
    location->compatibility_type = HILDON_FILE_SYSTEM_MODEL_LOCAL_DEVICE;
    location->sort_weight = SORT_WEIGHT_DEVICE;
}

static void
hildon_file_system_local_device_finalize (GObject *obj)
{
    HildonFileSystemSpecialLocation *location;
    HildonFileSystemLocalDevice *device;
    HildonFileSystemSettings *fs_settings;

    location = HILDON_FILE_SYSTEM_SPECIAL_LOCATION (obj);
    device = HILDON_FILE_SYSTEM_LOCAL_DEVICE (obj);
    fs_settings = _hildon_file_system_settings_get_instance ();

    if (g_signal_handler_is_connected (fs_settings, device->signal_handler_id))
    {
        g_signal_handler_disconnect (fs_settings, device->signal_handler_id);
    }

    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
btname_changed(GObject *settings, GParamSpec *param, gpointer data)
{
    HildonFileSystemLocalDevice *device;
    device = HILDON_FILE_SYSTEM_LOCAL_DEVICE (data);
    g_signal_emit_by_name (device, "changed");
}

/* Title that should be used for the location. If the virtual function is not
 * defined, then NULL is returned (which in turn can be intepreted as fallback
 * to GtkFileInfo)
 */
static gchar*
hildon_file_system_local_device_get_display_name (HildonFileSystemSpecialLocation
                                                  *location, GtkFileSystem *fs)
{
    HildonFileSystemSettings *fs_settings;
    gchar *name = NULL;

    fs_settings = _hildon_file_system_settings_get_instance ();
    g_object_get (fs_settings, "btname", &name, NULL);

    if (!name)
      name = g_strdup (g_getenv ("OSSO_PRODUCT_NAME"));

    if (!name)
      name = g_strdup ("");

    return name;
}

static GtkFileSystemHandle*
hildon_file_system_local_device_get_folder (HildonFileSystemSpecialLocation *location,
                                            GtkFileSystem *fs,
                                            const GtkFilePath *path,
                                            GtkFileInfoType types,
                                            GtkFileSystemGetFolderCallback callback,
                                            gpointer data)
{
  GtkFilePath *real_path;
  GtkFileSystemHandle *retval;

  /* We just want to call gtk_file_system_get_folder(), but we should
   * ensure that path is a local path.
   */

  if (g_str_has_prefix (path, "file:///"))
    {
      real_path = gtk_file_system_uri_to_path (fs, path);
      retval = gtk_file_system_get_folder (fs, real_path, types,
                                           callback, data);
      gtk_file_path_free (real_path);
    }
  else
    {
      retval = gtk_file_system_get_folder (fs, path, types,
                                           callback, data);
    }

  return retval;
}

static void
hildon_file_system_local_device_volumes_changed (HildonFileSystemSpecialLocation
						 *location, GtkFileSystem *fs)
{
  g_signal_emit_by_name (location, "rescan");
}

static char *
hildon_file_system_local_device_get_extra_info (HildonFileSystemSpecialLocation
						*location)
{
  const gchar* path;
  const gchar* device;
  GnomeVFSVolumeMonitor *monitor;
  GnomeVFSVolume* volume;

  path = g_getenv ("MYDOCSDIR");
  monitor = gnome_vfs_get_volume_monitor ();
  volume = gnome_vfs_volume_monitor_get_volume_for_path (monitor, path);

  if (volume) {
    device = gnome_vfs_volume_get_device_path (volume);
    g_object_unref (volume);
    return device;
  }
  else
    return NULL;
}
