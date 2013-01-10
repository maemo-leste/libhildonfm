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


#include <glib.h>
#include <string.h>
#include <memory.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <gconf/gconf-client.h>

#include "hildon-file-system-obex.h"
#include "hildon-file-system-settings.h"
#include "hildon-file-system-dynamic-device.h"
#include "hildon-file-system-private.h"
#include "hildon-file-common-private.h"

#undef BT_GCONF_ENABLED

#define BT_GCONF_DEVICE  "/system/bluetooth/device/xx:xx:xx:xx:xx:xx/"
#define BT_GCONF_DEVICE_ADDR_INDEX  25

#define BT_DBUS_ICON_PREFIX "control_bluetooth_"

#define BT_BDA_LENGTH               17
#define BT_URI_BDA_INDEX            8

static void
hildon_file_system_obex_class_init (HildonFileSystemObexClass *klass);
static void
hildon_file_system_obex_finalize (GObject *obj);
static void
hildon_file_system_obex_init (HildonFileSystemObex *device);
GtkFilePath *
hildon_file_system_obex_rewrite_path (HildonFileSystemSpecialLocation *location,
				      GtkFileSystem *filesystem,
				      const GtkFilePath *path);
HildonFileSystemSpecialLocation*
hildon_file_system_obex_create_child_location (HildonFileSystemSpecialLocation *location, gchar *uri);

static gboolean
hildon_file_system_obex_is_visible (HildonFileSystemSpecialLocation *location,
                                    gboolean has_children);

G_DEFINE_TYPE (HildonFileSystemObex,
               hildon_file_system_obex,
               HILDON_TYPE_FILE_SYSTEM_REMOTE_DEVICE);

#if 0
static gchar *_unescape_base_uri (gchar *uri);
#endif

static gchar *_uri_to_display_name (gchar *uri);
static void _uri_filler_function (gpointer data, gpointer user_data);
static gchar *_obex_addr_to_display_name(gchar *obex_addr);

static gchar *_get_icon_from_uri (gchar *uri);


static void
hildon_file_system_obex_class_init (HildonFileSystemObexClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    HildonFileSystemSpecialLocationClass *location =
                HILDON_FILE_SYSTEM_SPECIAL_LOCATION_CLASS (klass);

    gobject_class->finalize = hildon_file_system_obex_finalize;

    location->rewrite_path = hildon_file_system_obex_rewrite_path;
    location->create_child_location =
                hildon_file_system_obex_create_child_location;

    /* The root obex folder doesn't require explicit user access since
       listing it is fast.
    */
    location->requires_access = NULL;

    location->is_visible = hildon_file_system_obex_is_visible;
}

static void
bondings_changed (GObject *settings, GParamSpec *param, gpointer data)
{
  g_signal_emit_by_name (data, "changed");
  g_signal_emit_by_name (data, "rescan");
}

static void
hildon_file_system_obex_init (HildonFileSystemObex *device)
{
    HildonFileSystemSettings *fs_settings;
    HildonFileSystemSpecialLocation *location;

    fs_settings = _hildon_file_system_settings_get_instance ();

    location = HILDON_FILE_SYSTEM_SPECIAL_LOCATION (device);
    location->compatibility_type = HILDON_FILE_SYSTEM_MODEL_GATEWAY;
    location->fixed_icon = g_strdup ("general_bluetooth");
    location->fixed_title = g_strdup (dgettext("osso-connectivity-ui", "conn_ti_bluetooth_cpa"));
    location->failed_access_message = NULL;

    device->bonding_handler_id =
      g_signal_connect (fs_settings,
                        "notify::bondings",
                        G_CALLBACK (bondings_changed),
                        device);
}

static void
hildon_file_system_obex_finalize (GObject *obj)
{
    HildonFileSystemObex *obex;
    HildonFileSystemSettings *fs_settings;

    obex = HILDON_FILE_SYSTEM_OBEX (obj);
    fs_settings = _hildon_file_system_settings_get_instance ();
    if (g_signal_handler_is_connected (fs_settings,
                                       obex->bonding_handler_id))
      {
        g_signal_handler_disconnect (fs_settings,
                                     obex->bonding_handler_id);
      }

    G_OBJECT_CLASS (hildon_file_system_obex_parent_class)->finalize (obj);
}

GtkFilePath *
hildon_file_system_obex_rewrite_path (HildonFileSystemSpecialLocation *location,
				      GtkFileSystem *filesystem,
				      const GtkFilePath *path)
{
  /* XXX - the 'right' thing would be to follow the symlinks below
           "obex:///" explicitly, but we would have to special case
           that as well since in general symlinks should not be
           followed by us but by the GnomeVFS module.  So we just hack
           the URIs...
  */

  GtkFilePath *new_path;
  const char *str = gtk_file_path_get_string (path);

  if (g_str_has_prefix (str, "obex:///"))
    {
      char *new_str = hildon_file_system_unescape_string (str);
      strcpy (new_str + 7, new_str + 8);
      new_path = gtk_file_path_new_steal (new_str);
    }
  else
    new_path = gtk_file_path_copy (path);

  return new_path;
}

HildonFileSystemSpecialLocation*
hildon_file_system_obex_create_child_location (HildonFileSystemSpecialLocation *location, gchar *uri)
{
    HildonFileSystemSpecialLocation *child = NULL;
    gchar *skipped, *found, *name;

    skipped = uri + strlen (location->basepath) + 1;

    found = strchr (skipped, G_DIR_SEPARATOR);

    if(found == NULL || found[1] == 0) {
        child = g_object_new(HILDON_TYPE_FILE_SYSTEM_DYNAMIC_DEVICE, NULL);
        HILDON_FILE_SYSTEM_REMOTE_DEVICE (child)->accessible =
            HILDON_FILE_SYSTEM_REMOTE_DEVICE (location)->accessible;

        name = _get_icon_from_uri (uri);
        if (name) {
            hildon_file_system_special_location_set_icon (child, name);
            g_free (name);
        } else {
            hildon_file_system_special_location_set_icon (child,
                "qgn_list_filesys_divc_gw");
        }

          /* if this fails, NULL is returned and fallback is that
             the obex addr in format [12:34:...] is shown */
        name = _uri_to_display_name (uri);
        if (name) {
            hildon_file_system_special_location_set_display_name (child, name);
            g_free (name);
        }

        child->basepath = g_strdup (uri);
        child->failed_access_message = _("sfil_ib_cannot_connect_device");
        child->permanent = FALSE;
    }

    return child;
}

static gboolean
hildon_file_system_obex_is_visible (HildonFileSystemSpecialLocation *location,
                                    gboolean has_children)
{
  return has_children;
}

#if 0
/* very dependant on how things work now */
static gchar *
_unescape_base_uri (gchar *uri)
{
    gchar *ret = NULL;
    int i;

      /* some checking in the case something changes and thing won't
         go as we expect (we expect that uri is incorrectly escaped) */
    if (memcmp (uri, "obex:///", 8)) {
        if (!memcmp (uri, "obex://[", 8)) {
            return g_strdup (uri);
        } else {
            return NULL;
        }
    }


    ret = g_malloc0 (28);

    g_snprintf (ret, 28, "obex://[");
    g_snprintf (ret + 25, 3, "]/");


    for (i = 0; i < 6; i++) {
      if (i > 0) {
        ret[i*3 + 7] = ':';
      }

      ret[i*3 + 8] = uri[i*5 + 11];
      ret[i*3 + 9] = uri[i*5 + 12];
    }


    return ret;
}
#endif

/* we'll cache addr & name pairs so we don't have to open D-BUS
   connection each time, caching happens per process lifetime */
typedef struct {
    gchar *addr;
    gchar *name;
} CacheData;


static GList *cache_list = NULL;


static gchar *_uri_to_display_name (gchar *uri)
{
    CacheData *local = g_malloc0 (sizeof (CacheData));
    gchar *ret = NULL;


    local->addr = g_malloc0(BT_BDA_LENGTH + 1);
    memcpy (local->addr, uri + BT_URI_BDA_INDEX, BT_BDA_LENGTH);
    local->name = NULL;


    g_list_foreach (cache_list, _uri_filler_function, local);


    if(local->name) {
        ret = g_strdup (local->name);
        g_free (local);
    } else {
        local->name = _obex_addr_to_display_name (local->addr);
        ret = g_strdup (local->name);

        cache_list = g_list_append (cache_list, local);
    }


    return ret;
}


static void _uri_filler_function (gpointer data, gpointer user_data)
{
    CacheData *src, *dest;

    src  = (CacheData *) data;
    dest = (CacheData *) user_data;

    if (dest->name)
        return;


    if (!memcmp (src->addr, dest->addr, 18)) {
        g_free(dest->addr);

        dest->addr = src->addr;
        dest->name = src->name;
    }
}


static gchar *_dbus_bluez_get_property(const gchar *obex_addr,
                                       const gchar *property)
{
    DBusConnection *conn;
    DBusMessage *msg, *ret_adapt, *ret_dev;
    DBusMessageIter iter_adapt, adapts, iter_dev;
    DBusMessageIter dicts, dict, dict_value;
    DBusError error;
    char *tmp, *name = NULL;

    if (!obex_addr || !*obex_addr)
        return NULL;

    dbus_error_init (&error);
    conn = dbus_bus_get_private (DBUS_BUS_SYSTEM, &error);
    
    if (!conn) {
        dbus_error_free(&error);
        return NULL;
    }

    /* Find first adapter with the obex_addr */
    dbus_connection_set_exit_on_disconnect (conn, FALSE);

    msg = dbus_message_new_method_call ("org.bluez", "/",
                                        "org.bluez.Manager", "ListAdapters");

    if (!msg)
        goto escape;

    dbus_error_init (&error);
    ret_adapt = dbus_connection_send_with_reply_and_block (conn, msg, -1,
                                                           &error);
    dbus_message_unref (msg);

    if (dbus_error_is_set (&error)) {
        dbus_error_free (&error);
        goto escape_adapt;
    }

    if (!dbus_message_iter_init (ret_adapt, &iter_adapt)) {
        dbus_message_unref (ret_adapt);
        goto escape_adapt;
    }

    dbus_message_iter_recurse (&iter_adapt, &adapts);

    do {
        if (dbus_message_iter_get_arg_type (&adapts) != DBUS_TYPE_OBJECT_PATH)
            continue;

        dbus_message_iter_get_basic (&adapts, &tmp);

        if (!tmp || !*tmp)
            continue;

        /* Get device path to call GetProperties */
        msg = dbus_message_new_method_call ("org.bluez", tmp,
                                            "org.bluez.Adapter", "FindDevice");

        if (!msg)
            continue;

        dbus_message_iter_init_append (msg, &iter_dev);
        dbus_message_iter_append_basic (&iter_dev, DBUS_TYPE_STRING,
                                        &obex_addr);

        dbus_error_init (&error);
        ret_dev = dbus_connection_send_with_reply_and_block (conn, msg, -1,
                                                             &error);
        dbus_message_unref (msg);

        if (dbus_error_is_set (&error)) {
            dbus_error_free (&error);
            continue;
        }

        if (!dbus_message_iter_init (ret_dev, &iter_dev) ||
            dbus_message_iter_get_arg_type (&iter_dev) != DBUS_TYPE_OBJECT_PATH) {
            dbus_message_unref (ret_dev);
            continue;
        }

        dbus_message_iter_get_basic (&iter_dev, &tmp);
        dbus_message_unref (ret_dev);

        /* We have the device, let's get the name */
        msg = dbus_message_new_method_call ("org.bluez", tmp,
                                        "org.bluez.Device", "GetProperties");

        if (!msg)
            continue;

        dbus_error_init (&error);
        ret_dev = dbus_connection_send_with_reply_and_block (conn, msg, -1,
                                                             &error);
        dbus_message_unref (msg);

        if (dbus_error_is_set (&error)) {
            dbus_error_free (&error);
            continue;
        }

        if (!dbus_message_iter_init (ret_dev, &iter_dev)) {
            dbus_message_unref (ret_dev);
            continue;
        }

        dbus_message_iter_recurse (&iter_dev, &dicts);

        do {

            if (dbus_message_iter_get_arg_type (&dicts) != DBUS_TYPE_DICT_ENTRY)
                continue;

            dbus_message_iter_recurse (&dicts, &dict);

            /* Try to get the Key */
            dbus_message_iter_get_basic (&dict, &tmp);
            /* We only care about one property */
            if (g_strcmp0 (tmp, property) ||
                /* Go to the value */
                !dbus_message_iter_next (&dict) ||
                dbus_message_iter_get_arg_type (&dict) != DBUS_TYPE_VARIANT)
                continue;

            dbus_message_iter_recurse(&dict, &dict_value);
            dbus_message_iter_get_basic (&dict_value, &tmp);
            name = g_strdup(tmp);

            dbus_message_unref (ret_dev);

            goto escape_adapt;
        } while (dbus_message_iter_next (&dicts));

        dbus_message_unref (ret_dev);

    } while (dbus_message_iter_next (&adapts));

escape_adapt:
    dbus_message_unref (ret_adapt);

escape:
    dbus_connection_close (conn);
    dbus_connection_unref (conn);

    return name;
}


static gchar *_get_icon_from_uri (gchar *uri)
{
#ifdef BT_GCONF_ENABLED
    GConfClient *gconf_client;
    gchar key[] = BT_GCONF_DEVICE "icon";
#endif
    gchar *icon, *ret = NULL;

    g_assert (uri != NULL);

#ifdef BT_GCONF_ENABLED
    gconf_client = gconf_client_get_default ();

    if (!gconf_client) {
        return NULL;
    }

    /* copy BDA from URI to the GCONF key */
    memcpy (key + BT_GCONF_DEVICE_ADDR_INDEX, uri + BT_URI_BDA_INDEX,
            BT_BDA_LENGTH);

    ret = gconf_client_get_string (gconf_client, key, NULL);

    g_object_unref (gconf_client);

    if (ret && *ret) {
        return ret;
    }
    g_free (ret);
#endif

    ret = g_strndup (uri + BT_URI_BDA_INDEX, BT_BDA_LENGTH);
    icon = _dbus_bluez_get_property (ret, "Icon");
    g_free (ret);
    ret = NULL;

    if (icon && *icon)
        ret = g_strconcat (BT_DBUS_ICON_PREFIX, icon, NULL);

    g_free (icon);

    return ret;
}


static gchar *_obex_addr_to_display_name(gchar *obex_addr)
{
#ifdef BT_GCONF_ENABLED
    GConfClient *gconf_client;
    gchar *ret = NULL;
    gchar key[] = BT_GCONF_DEVICE "name";
#endif

    g_assert (obex_addr != NULL);

#ifdef BT_GCONF_ENABLED
    gconf_client = gconf_client_get_default ();

    if (!gconf_client) {
        return NULL;
    }

    /* copy BDA to the GCONF key */
    memcpy (key + BT_GCONF_DEVICE_ADDR_INDEX, obex_addr, BT_BDA_LENGTH);

    ret = gconf_client_get_string (gconf_client, key, NULL);

    g_object_unref (gconf_client);

    if (ret && *ret) {
        return ret;
    }
    g_free (ret);
#endif

    return _dbus_bluez_get_property (obex_addr, "Alias");
}
