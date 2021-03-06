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

/*
 * HildonFileSystemSettings
 *
 * Shared settings object to be used in HildonFileSystemModel.
 * Setting up dbus/gconf stuff for each model takes time, so creating
 * a single settings object is much more convenient.
 *
 * INTERNAL TO FILE SELECTION STUFF, NOT FOR APPLICATION DEVELOPERS TO USE.
 *
 */

//#include <bt-gconf.h>
#include <gconf/gconf-client.h>
#include <mce/dbus-names.h>
#include <mce/mode-names.h>
#include <sys/types.h>
#include <unistd.h>
#include <libintl.h>
#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "hildon-file-common-private.h"
#include "hildon-file-system-settings.h"

enum {
  PROP_FLIGHT_MODE = 1,
  PROP_BTNAME,
  PROP_GATEWAY,
  PROP_USB,
  PROP_GATEWAY_FTP,
  PROP_MMC_PRESENT,
  PROP_MMC_COVER_OPEN,
  PROP_MMC_USED,
  PROP_MMC_CORRUPTED,
  PROP_INTERNAL_MMC_CORRUPTED,
  PROP_IAP_CONNECTED,
  PROP_BONDINGS
};

#define PRIVATE(obj) HILDON_FILE_SYSTEM_SETTINGS(obj)->priv

#define USB_CABLE_DIR "/system/osso/af"
#define MMC_DIR "/system/osso/af/mmc"

#define USB_CABLE_KEY USB_CABLE_DIR "/usb-cable-attached"
#define MMC_USED_KEY USB_CABLE_DIR "/mmc-used-over-usb"
#define MMC_PRESENT_KEY USB_CABLE_DIR "/mmc-device-present"
#define MMC_COVER_OPEN_KEY USB_CABLE_DIR "/mmc-cover-open"
#define MMC_CORRUPTED_KEY MMC_DIR "/mmc-corrupted"
#define MMC_INTERNAL_CORRUPTED_KEY MMC_DIR "/internal-mmc-corrupted"

#define MCE_MATCH_RULE "type='signal',interface='" MCE_SIGNAL_IF \
                       "',member='" MCE_DEVICE_MODE_SIG "'"

/* XXX - use libconic API to listen to the connectedness status
         instead of talking to ICD directly.
 */

#define ICD_DBUS_INTERFACE          "com.nokia.icd"
#define ICD_STATUS_CHANGED_SIG      "status_changed"

#define ICD_MATCH_RULE "type='signal',interface='" ICD_DBUS_INTERFACE \
                       "',member='" ICD_STATUS_CHANGED_SIG "'"

#define BT_BONDING_CREATED_RULE "type='signal',"                 \
                                "interface='org.bluez.Adapter'," \
                                "member='BondingCreated'"

#define BT_BONDING_REMOVED_RULE "type='signal',"                 \
                                "interface='org.bluez.Adapter'," \
                                "member='BondingRemoved'"

/* For getting and tracking the Bluetooth name
 */
#define BTNAME_SERVICE                  "org.bluez"
#define BTNAME_REQUEST_IF               "org.bluez.Adapter"
#define BTNAME_SIGNAL_IF                "org.bluez.Adapter"

#define BTMANAGER_ROOT_PATH				"/"
#define BTMANAGER_ADDRESS				"org.bluez.Manager"
#define BTADAPTER_ADDRESS				"org.bluez.Adapter"

#define BTDEFAULT_ADAPTER_GET 			"DefaultAdapter"

#define BTPROPERTIES_REQ_GET			"GetProperties"
#define BTNAME_SIG_CHANGED              "NameChanged"
#define BTNAME_LIST_BONDINGS            "ListBondings"

#define BTNAME_MATCH_RULE "type='signal',interface='" BTNAME_SIGNAL_IF \
                          "',member='" BTNAME_SIG_CHANGED "'"

struct _HildonFileSystemSettingsPrivate
{
  DBusConnection *dbus_conn;
  GConfClient *gconf;

  /* Properties */
  gboolean flightmode;
  gboolean usb;
  gchar *btname;
  gchar *gateway;
  gboolean gateway_ftp;
  gboolean iap_connected;
  gchar* connection_name;

  gboolean gconf_ready;
  gboolean flightmode_ready;

  gboolean mmc_is_present;
  gboolean mmc_is_corrupted;
  gboolean mmc_used_over_usb;
  gboolean mmc_cover_open;

  gint bondings;
};

G_DEFINE_TYPE(HildonFileSystemSettings, \
  hildon_file_system_settings, G_TYPE_OBJECT)

static void
hildon_file_system_settings_get_property(GObject *object,
                        guint        prop_id,
                        GValue      *value,
                        GParamSpec  *pspec)
{
  HildonFileSystemSettingsPrivate *priv = PRIVATE(object);

  switch (prop_id)
  {
    case PROP_FLIGHT_MODE:
      g_value_set_boolean(value, priv->flightmode);
      break;
    case PROP_BTNAME:
      g_value_set_string(value, priv->btname);
      break;
    case PROP_GATEWAY:
      g_value_set_string(value, priv->gateway);
      break;
    case PROP_USB:
      g_value_set_boolean(value, priv->usb);
      break;
    case PROP_GATEWAY_FTP:
      g_value_set_boolean(value, priv->gateway_ftp);
      break;
    case PROP_MMC_PRESENT:
      g_value_set_boolean(value, priv->mmc_is_present);
      break;
    case PROP_MMC_USED:
      g_value_set_boolean(value, priv->mmc_used_over_usb);
      break;
    case PROP_MMC_COVER_OPEN:
      g_value_set_boolean(value, priv->mmc_cover_open);
      break;
    case PROP_MMC_CORRUPTED:
      g_value_set_boolean(value, priv->mmc_is_corrupted);
      break;
    case PROP_IAP_CONNECTED:
      g_value_set_boolean(value, priv->iap_connected);
      break;
    case PROP_BONDINGS:
      g_value_set_int(value, priv->bondings);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
hildon_file_system_settings_set_property(GObject *object,
                        guint        prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  /* We do not have any writable properties */
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

#if 0 //fremantle
static void
set_gateway_from_gconf_value(HildonFileSystemSettings *self,
                             GConfValue *value)
{
  g_free(self->priv->gateway);
  self->priv->gateway = NULL;
  self->priv->gateway_ftp = FALSE;

  if (value && value->type == GCONF_VALUE_STRING)
  {
    const gchar *address;
    address = gconf_value_get_string(value);

    if (address == NULL) {
      g_warning("gconf_value_get_string failed");
    } else {
      gchar key[256];
      GSList *list;

      g_debug("got address '%s'", address);
      self->priv->gateway = g_strdup(address);

      g_snprintf(key, sizeof(key), "%s/%s/services",
                 GNOME_BT_DEVICE, address);
      list = gconf_client_get_list(self->priv->gconf, key,
                                   GCONF_VALUE_STRING, NULL);

      if (list == NULL) {
        g_warning("gconf_client_get_list failed");
      } else {
        const GSList *p;
        g_debug("got a list, first string '%s'",
                     list ? (char*)(list->data): "none");
        for (p = list; p != NULL; p = g_slist_next(p)) {
          if (strstr((const char*)(p->data), "FTP") != NULL) {
            self->priv->gateway_ftp = TRUE;
            break;
          }
        }
        g_slist_free(list);
      }
    }
  }

  g_object_notify(G_OBJECT(self), "gateway");
  g_object_notify(G_OBJECT(self), "gateway-ftp");
}
#endif

static void
set_mmc_cover_open_from_gconf_value(HildonFileSystemSettings *self,
                                    GConfValue *value)
{
  if (value && value->type == GCONF_VALUE_BOOL)
  {
    self->priv->mmc_cover_open = gconf_value_get_bool(value);
    g_object_notify(G_OBJECT(self), "mmc-cover-open");
  }
}

static void
set_mmc_corrupted_from_gconf_value(HildonFileSystemSettings *self,
                                   GConfValue *value)
{
  if (value && value->type == GCONF_VALUE_BOOL)
  {
    self->priv->mmc_is_corrupted = gconf_value_get_bool(value);
    g_object_notify(G_OBJECT(self), "mmc-is-corrupted");
  }
}

static void
set_mmc_present_from_gconf_value(HildonFileSystemSettings *self,
                                 GConfValue *value)
{
  if (value && value->type == GCONF_VALUE_BOOL)
  {
    self->priv->mmc_is_present = gconf_value_get_bool(value);
    g_object_notify(G_OBJECT(self), "mmc-is-present");
  }
}

static void
set_mmc_used_from_gconf_value(HildonFileSystemSettings *self,
                              GConfValue *value)
{
  if (value && value->type == GCONF_VALUE_BOOL)
  {
    self->priv->mmc_used_over_usb = gconf_value_get_bool(value);
    g_object_notify(G_OBJECT(self), "mmc-used");
  }
}

static void
set_usb_from_gconf_value(HildonFileSystemSettings *self,
                         GConfValue *value)
{
  if (value && value->type == GCONF_VALUE_BOOL)
  {
    self->priv->usb = gconf_value_get_bool(value);
    g_object_notify(G_OBJECT(self), "usb-cable");
  }
}

static void
set_bt_name_from_message(HildonFileSystemSettings *self,
                         DBusMessage *message)
{
  DBusMessageIter iter;
  const char *name = NULL;

  g_assert(message != NULL);
  if (!dbus_message_iter_init(message, &iter))
  {
    g_warning ("%s: message did not have argument", G_STRFUNC);
    return;
  }
  dbus_message_iter_get_basic(&iter, &name);

  g_debug ("BT name changed into \"%s\"", name);

  g_free(self->priv->btname);
  self->priv->btname = g_strdup(name);
  g_object_notify(G_OBJECT(self), "btname");
}

static void
set_flight_mode_from_message(HildonFileSystemSettings *self,
                             DBusMessage *message)
{
  DBusMessageIter iter;
  gboolean new_mode;
  const char *mode_name = NULL;

  g_assert(message != NULL);
  if (!dbus_message_iter_init(message, &iter))
  {
    g_warning ("%s: message did not have argument", G_STRFUNC);
    return;
  }
  dbus_message_iter_get_basic(&iter, &mode_name);

  if (g_ascii_strcasecmp(mode_name, MCE_FLIGHT_MODE) == 0)
    new_mode = TRUE;
  else if (g_ascii_strcasecmp(mode_name, MCE_NORMAL_MODE) == 0)
    new_mode = FALSE;
  else
    new_mode = self->priv->flightmode;

  if (new_mode != self->priv->flightmode)
  {
    self->priv->flightmode = new_mode;
    g_object_notify(G_OBJECT(self), "flight-mode");
  }
}

static void
set_icd_status_from_message (HildonFileSystemSettings *self,
                             DBusMessage *message)
{
  gchar *name, *type, *status, *uierr;
  gboolean new_value = self->priv->iap_connected;
  gboolean active_connection_changed = FALSE;
  
  if (dbus_message_get_args (message, NULL,
                             DBUS_TYPE_STRING, &name,
                             DBUS_TYPE_STRING, &type,
                             DBUS_TYPE_STRING, &status,
                             DBUS_TYPE_STRING, &uierr,
                             DBUS_TYPE_INVALID))
    {
	  if ( self->priv->connection_name == NULL )
		  self->priv->connection_name = g_strdup(name);
  
      if (strcmp (status, "IDLE") == 0)
      {
		new_value = FALSE;
		// if lost connection is the active connection
		if ( strcmp( name, self->priv->connection_name ) == 0 )
		{
  		  active_connection_changed = TRUE;
  		  g_free(self->priv->connection_name);
  		  self->priv->connection_name = NULL;
		}
      }
      else if (strcmp (status, "CONNECTED") == 0)
      {
        new_value = TRUE;
        g_free(self->priv->connection_name);
        self->priv->connection_name = g_strdup(name);
        
      //  if ( strcmp( name, self->priv->connection_name ) == 0 )
  		  active_connection_changed = TRUE;
      }
    }
  
  if (new_value != self->priv->iap_connected)
    {
      if (active_connection_changed)
	    self->priv->iap_connected = new_value;
      g_object_notify (G_OBJECT(self), "iap-connected");
    }
}

#if 0 //fremantle
static void
bonding_list_received (DBusPendingCall          *call,
		       HildonFileSystemSettings *self)
{
  DBusMessage *reply;
  DBusMessageIter iter, list_iter;
  gint n_elements = 0;

  reply = dbus_pending_call_steal_reply(call);

  if (reply)
    {
      dbus_message_iter_init (reply, &iter);
      dbus_message_iter_recurse (&iter, &list_iter);

      while (dbus_message_iter_get_arg_type (&list_iter) == DBUS_TYPE_STRING)
        {
	  n_elements++;
	  dbus_message_iter_next (&list_iter);
	}

      dbus_message_unref (reply);
    }

  self->priv->bondings = n_elements;
  g_object_notify (G_OBJECT (self), "bondings");
}

static void
request_bonding_list (HildonFileSystemSettings *self)
{
  DBusMessage *request;
  DBusPendingCall *pending_call;

  request = dbus_message_new_method_call(BTNAME_SERVICE,
					 BTNAME_REQUEST_PATH,
					 BTNAME_REQUEST_IF,
					 BTNAME_LIST_BONDINGS);

  if (dbus_connection_send_with_reply (self->priv->dbus_conn,
				       request, &pending_call, -1))
    {
      dbus_pending_call_set_notify (pending_call,
				    (DBusPendingCallNotifyFunction) bonding_list_received,
				    self, NULL);
      dbus_pending_call_unref (pending_call);
    }

  dbus_message_unref (request);
}
#endif

static DBusHandlerResult
hildon_file_system_settings_handle_dbus_signal(DBusConnection *conn,
                                               DBusMessage *msg,
                                               gpointer data)
{
  g_assert(HILDON_IS_FILE_SYSTEM_SETTINGS(data));

  if (dbus_message_is_signal(msg, MCE_SIGNAL_IF, MCE_DEVICE_MODE_SIG))
    {
      set_flight_mode_from_message(HILDON_FILE_SYSTEM_SETTINGS(data), msg);
    }
  else if (dbus_message_is_signal(msg, BTNAME_SIGNAL_IF, BTNAME_SIG_CHANGED))
    {
      set_bt_name_from_message(HILDON_FILE_SYSTEM_SETTINGS(data), msg);
    }
#if 0 //fremantle
  else if (dbus_message_is_signal(msg, "org.bluez.Adapter", "BondingCreated")
           || dbus_message_is_signal(msg, "org.bluez.Adapter",
                                     "BondingRemoved"))
    {
      request_bonding_list (HILDON_FILE_SYSTEM_SETTINGS (data));
    }
#endif
  else if (dbus_message_is_signal (msg, ICD_DBUS_INTERFACE,
                                   ICD_STATUS_CHANGED_SIG))
    {
      set_icd_status_from_message (HILDON_FILE_SYSTEM_SETTINGS (data), msg);
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void mode_received(DBusPendingCall *call, void *user_data)
{
  HildonFileSystemSettings *self;
  DBusMessage *message;
  DBusError error;

  self = HILDON_FILE_SYSTEM_SETTINGS(user_data);

  g_assert(dbus_pending_call_get_completed(call));
  message = dbus_pending_call_steal_reply(call);
  if (message == NULL)
  {
    g_warning ("%s: no reply", G_STRFUNC);
    return;
  }

  dbus_error_init(&error);

  if (dbus_set_error_from_message(&error, message))
  {
    //g_warning ("%s: %s: %s", G_STRFUNC, error.name, error.message);
    dbus_error_free(&error);
  }
  else
    set_flight_mode_from_message(self, message);

  dbus_message_unref(message);

  self->priv->flightmode_ready = TRUE;
}

/*
Go thrue GetProperties container, look for "Name" and update name.
*/

static void set_bt_name_from_properties(HildonFileSystemSettings *self, DBusMessage *msg)
{
	DBusMessageIter iter;
	DBusMessageIter dict_iter;
	DBusMessageIter dict_entry_iter;
	DBusMessageIter dict_entry_content_iter;

	int key_type;
	int array_type;

	if (!dbus_message_iter_init (msg, &iter))
	{
		return ;	// no arguments
	}
	dbus_message_iter_recurse (&iter, &dict_iter);

	while ((array_type = dbus_message_iter_get_arg_type (&dict_iter)) == DBUS_TYPE_DICT_ENTRY)
	{
		dbus_message_iter_recurse (&dict_iter, &dict_entry_iter);

		while ((key_type = dbus_message_iter_get_arg_type (&dict_entry_iter)) == DBUS_TYPE_STRING)
		{
			char *key;
			
			dbus_message_iter_get_basic (&dict_entry_iter, &key);
			dbus_message_iter_next (&dict_entry_iter);

			if (strcmp(key, "Name") == 0)
			{
				int dict_entry_type;

				dict_entry_type = dbus_message_iter_get_arg_type (&dict_entry_iter);
				if (dict_entry_type == DBUS_TYPE_VARIANT)
				{			
					int dict_entry_content_type;

					dbus_message_iter_recurse (&dict_entry_iter, &dict_entry_content_iter);
					dict_entry_content_type = dbus_message_iter_get_arg_type (&dict_entry_content_iter);	

					if (dict_entry_content_type == DBUS_TYPE_STRING)
					{ 
						gchar *value = NULL;
						dbus_message_iter_get_basic (&dict_entry_content_iter, &value);
						g_free(self->priv->btname);
						self->priv->btname = g_strdup(value);
						g_object_notify(G_OBJECT(self), "btname");

						g_debug("BT name changed into \"%s\"", value);
					}
				}
			}
			dbus_message_iter_next (&dict_entry_iter);
        }

		if (key_type != DBUS_TYPE_INVALID)
        	break;

		dbus_message_iter_next (&dict_iter);
	}	
}

/*
Examine getProperties dbus message retval validity
*/

static void btadapter_properties_received(DBusPendingCall *call, void *user_data)
{
	HildonFileSystemSettings *self;
	DBusMessage *message;
	DBusError error;

	self = HILDON_FILE_SYSTEM_SETTINGS(user_data);
	message = dbus_pending_call_steal_reply(call);

	if(message == NULL)
	{
		g_warning ("%s: no reply", G_STRFUNC);
		return;
	}

	dbus_error_init(&error);
	if(dbus_set_error_from_message(&error, message))
	{
		g_warning ("%s: %s: %s", G_STRFUNC, error.name, error.message);
		dbus_error_free(&error);
	}

	set_bt_name_from_properties(self, message);
}

/*
DBus message contains path for default adapter.
New DBus call is made with this path to ask adapter properties.
*/

static void btdefault_adapter_received(DBusPendingCall *call, void *user_data)
{
	HildonFileSystemSettings *self;
	DBusConnection* conn;
	DBusMessage *message;	// received message
	DBusMessage *msg_det_default_adapter;	// message to be sent
	DBusMessageIter iter;
	gchar* path = NULL;
	DBusError error;

	self = HILDON_FILE_SYSTEM_SETTINGS(user_data);
	conn = self->priv->dbus_conn;

	message = dbus_pending_call_steal_reply(call);

	if (message == NULL)
	{
		g_warning ("%s: no reply", G_STRFUNC);
		return;
	}

	dbus_error_init(&error);

	if (dbus_set_error_from_message(&error, message))
	{
		g_debug ("%s: %s: %s", G_STRFUNC, error.name, error.message);
		dbus_error_free(&error);
		goto end;
	}

	// Get path from message.
	if (!dbus_message_iter_init(message, &iter))
	{
		g_warning ("%s: message did not have argument", G_STRFUNC);
		goto end;
	}

	dbus_message_iter_get_basic (&iter, &path);

	if ( path == NULL )
	{
		g_warning ("%s: no default bt adapter", G_STRFUNC);
		goto end;
	}

	// Ask for properties
	msg_det_default_adapter = dbus_message_new_method_call(
			BTNAME_SERVICE,path,BTADAPTER_ADDRESS,BTPROPERTIES_REQ_GET);

	if (message == NULL)
	{
		g_warning ("%s: no reply", G_STRFUNC);
	}
	else
	{
		DBusPendingCall *call = NULL;

		if (dbus_connection_send_with_reply(conn, msg_det_default_adapter, &call, -1))
		{
			dbus_pending_call_set_notify(call, btadapter_properties_received, user_data, NULL);
			dbus_pending_call_block(call);
			dbus_pending_call_unref(call);
		}
	}
	
	end:
	dbus_message_unref(message);
}


static void
hildon_file_system_settings_setup_dbus(HildonFileSystemSettings *self)
{
  DBusConnection *conn;
  DBusMessage *request;
  DBusError error;
  DBusPendingCall *call = NULL;

  dbus_error_init(&error);
  self->priv->dbus_conn = conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  if (conn == NULL)
  {
    g_warning ("%s: %s", error.name, error.message);
    g_warning ("This causes that device state changes are not refreshed");
    dbus_error_free(&error);
    return;
  }
  dbus_connection_set_exit_on_disconnect (self->priv->dbus_conn, FALSE);

  /* Let's query initial state. These calls are async, so they do not
     consume too much startup time */
  request = dbus_message_new_method_call(MCE_SERVICE,
        MCE_REQUEST_PATH, MCE_REQUEST_IF, MCE_DEVICE_MODE_GET);
  if (request == NULL)
  {
    g_warning ("%s: dbus_message_new_method_call failed", G_STRFUNC);
    return;
  }
  dbus_message_set_auto_start(request, TRUE);

  if (dbus_connection_send_with_reply(conn, request, &call, -1))
  {
    dbus_pending_call_set_notify(call, mode_received, self, NULL);
    dbus_pending_call_unref(call);
  }

  dbus_message_unref(request);

  request = dbus_message_new_method_call(BTNAME_SERVICE,
        BTMANAGER_ROOT_PATH, BTMANAGER_ADDRESS, BTDEFAULT_ADAPTER_GET);
  if (request == NULL)
  {
    g_warning ("%s: dbus_message_new_method_call failed", G_STRFUNC);
    return;
  }
  dbus_message_set_auto_start(request, TRUE);

  if (dbus_connection_send_with_reply(conn, request, &call, -1))
  {
    dbus_pending_call_set_notify(call, btdefault_adapter_received, self, NULL);
    dbus_pending_call_block(call);
    dbus_pending_call_unref(call);
  }

  dbus_message_unref(request);

  dbus_connection_setup_with_g_main(conn, NULL);
  dbus_error_init (&error);
  dbus_bus_add_match(conn, MCE_MATCH_RULE, &error);
  if (dbus_error_is_set(&error))
  {
    g_warning ("%s: dbus_bus_add_match failed", G_STRFUNC);
    dbus_error_free(&error);
  }

  dbus_error_init (&error);
  dbus_bus_add_match(conn, BTNAME_MATCH_RULE, &error);
  if (dbus_error_is_set(&error))
  {
    g_warning ("%s: dbus_bus_add_match failed: %s", G_STRFUNC , error.message);
    dbus_error_free(&error);
  }

  dbus_error_init (&error);
  dbus_bus_add_match (conn, ICD_MATCH_RULE, &error);
  if (dbus_error_is_set(&error))
    {
      g_warning ("%s: dbus_bus_add_match failed: %s", G_STRFUNC , error.message);
      dbus_error_free (&error);
    }

#if 0 //fremantle
  dbus_error_init (&error);
  dbus_bus_add_match (conn, BT_BONDING_CREATED_RULE, &error);
  if (dbus_error_is_set(&error))
    {
      g_warning("dbus_bus_add_match failed: %s\n", error.message);
      dbus_error_free (&error);
    }

  dbus_error_init (&error);
  dbus_bus_add_match (conn, BT_BONDING_REMOVED_RULE, &error);
  if (dbus_error_is_set(&error))
    {
      g_warning("dbus_bus_add_match failed: %s\n", error.message);
      dbus_error_free (&error);
    }
#endif

  if (!dbus_connection_add_filter(conn,
      hildon_file_system_settings_handle_dbus_signal, self, NULL))
  {
    g_warning ("%s: dbus_connection_add_filter failed", G_STRFUNC);
  }
}

#if 0 //fremantle
static void
gconf_gateway_changed(GConfClient *client, guint cnxn_id,
                      GConfEntry *entry, gpointer data)
{
  g_assert(entry != NULL);

  if (g_ascii_strcasecmp(entry->key, BTCOND_GCONF_PREFERRED) == 0)
    set_gateway_from_gconf_value(
      HILDON_FILE_SYSTEM_SETTINGS(data), entry->value);
}
#endif

static void
gconf_mmc_value_changed(GConfClient *client, guint cnxn_id,
                        GConfEntry *entry, gpointer data)
{
  g_assert(entry != NULL);

  if (g_ascii_strcasecmp(entry->key, MMC_CORRUPTED_KEY) == 0)
    set_mmc_corrupted_from_gconf_value(
      HILDON_FILE_SYSTEM_SETTINGS(data), entry->value);
}

static void
gconf_value_changed(GConfClient *client, guint cnxn_id,
                    GConfEntry *entry, gpointer data)
{
  g_assert(entry != NULL);

  if (g_ascii_strcasecmp(entry->key, USB_CABLE_KEY) == 0)
    set_usb_from_gconf_value(
      HILDON_FILE_SYSTEM_SETTINGS(data), entry->value);
  else if (g_ascii_strcasecmp(entry->key, MMC_USED_KEY) == 0)
         set_mmc_used_from_gconf_value(
           HILDON_FILE_SYSTEM_SETTINGS(data), entry->value);
  else if (g_ascii_strcasecmp(entry->key, MMC_PRESENT_KEY) == 0)
         set_mmc_present_from_gconf_value(
           HILDON_FILE_SYSTEM_SETTINGS(data), entry->value);
  else if (g_ascii_strcasecmp(entry->key, MMC_COVER_OPEN_KEY) == 0)
         set_mmc_cover_open_from_gconf_value(
           HILDON_FILE_SYSTEM_SETTINGS(data), entry->value);
}

static void
hildon_file_system_settings_finalize(GObject *obj)
{
  HildonFileSystemSettingsPrivate *priv = PRIVATE(obj);

  g_free(priv->btname);
  g_free(priv->gateway);
  g_free(priv->connection_name);
  
  if (priv->gconf)
  {
#if 0 //fremantle
    gconf_client_remove_dir(priv->gconf, BTCOND_GCONF_PATH, NULL);
#endif
    g_object_unref(priv->gconf);
  }

  if (priv->dbus_conn)
  {
    dbus_bus_remove_match(priv->dbus_conn, MCE_MATCH_RULE, NULL);
    dbus_bus_remove_match(priv->dbus_conn, BTNAME_MATCH_RULE, NULL);
    dbus_bus_remove_match(priv->dbus_conn, ICD_MATCH_RULE, NULL);
    dbus_connection_remove_filter(priv->dbus_conn,
      hildon_file_system_settings_handle_dbus_signal, obj);
    dbus_connection_close(priv->dbus_conn);
    dbus_connection_unref(priv->dbus_conn);
  }

  G_OBJECT_CLASS(hildon_file_system_settings_parent_class)->finalize(obj);
}

static void
hildon_file_system_settings_class_init(HildonFileSystemSettingsClass *klass)
{
  GObjectClass *object_class;

  g_type_class_add_private(klass, sizeof(HildonFileSystemSettingsPrivate));

  object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = hildon_file_system_settings_finalize;
  object_class->get_property = hildon_file_system_settings_get_property;
  object_class->set_property = hildon_file_system_settings_set_property;

  g_object_class_install_property(object_class, PROP_FLIGHT_MODE,
    g_param_spec_boolean("flight-mode", "Flight mode",
                         "Whether or not the device is in flight mode",
                         TRUE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_BTNAME,
    g_param_spec_string("btname", "BT name",
                        "Bluetooth name of the device",
                        NULL, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_GATEWAY,
    g_param_spec_string("gateway", "Gateway",
                        "Currently selected gateway device",
                        NULL, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_USB,
    g_param_spec_boolean("usb-cable", "USB cable",
                         "Whether or not the USB cable is connected",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_GATEWAY_FTP,
    g_param_spec_boolean("gateway-ftp", "Gateway ftp",
                         "Whether current gateway device supports file transfer",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_MMC_USED,
    g_param_spec_boolean("mmc-used", "MMC used",
                         "Whether or not the MMC is being used",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_MMC_PRESENT,
    g_param_spec_boolean("mmc-is-present", "MMC present",
                         "Whether or not the MMC is present",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_MMC_CORRUPTED,
    g_param_spec_boolean("mmc-is-corrupted", "MMC corrupted",
                         "Whether or not the MMC is corrupted",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_MMC_COVER_OPEN,
    g_param_spec_boolean("mmc-cover-open", "MMC cover open",
                         "Whether or not the MMC cover is open",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_IAP_CONNECTED,
    g_param_spec_boolean("iap-connected", "IAP Connected",
                         "Whether or not we have a internet connection",
                         FALSE, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_BONDINGS,
    g_param_spec_int("bondings", "Bluetooth bondings",
		     "Number of bluetooth bondings",
		     0, G_MAXINT, 0, G_PARAM_READABLE));
}

static gboolean delayed_init(gpointer data)
{
  HildonFileSystemSettings *self;
  GConfValue *value;
  GError *error = NULL;

  self = HILDON_FILE_SYSTEM_SETTINGS(data);

  self->priv->gconf = gconf_client_get_default();
#if 0 //fremantle
  gconf_client_add_dir(self->priv->gconf, BTCOND_GCONF_PATH,
                       GCONF_CLIENT_PRELOAD_NONE, &error);
  if (error != NULL)
  {
    g_warning("gconf_client_add_dir failed: %s", error->message);
    g_error_free(error);
    error = NULL;
  }
#endif

  gconf_client_add_dir(self->priv->gconf, USB_CABLE_DIR,
                       GCONF_CLIENT_PRELOAD_NONE, &error);
  if (error != NULL)
  {
    g_warning ("%s: gconf_client_add_dir failed: %s", G_STRFUNC, error->message);
    g_error_free(error);
    error = NULL;
  }

  gconf_client_add_dir(self->priv->gconf, MMC_DIR,
                       GCONF_CLIENT_PRELOAD_NONE, &error);
  if (error != NULL)
  {
    g_warning ("%s: gconf_client_add_dir failed: %s", G_STRFUNC, error->message);
    g_error_free(error);
    error = NULL;
  }

#if 0 //fremantle
  gconf_client_notify_add(self->priv->gconf, BTCOND_GCONF_PATH,
                          gconf_gateway_changed, self, NULL, &error);
  if (error != NULL)
  {
    g_warning("gconf_client_notify_add failed: %s", error->message);
    g_error_free(error);
    error = NULL;
  }
#endif

  gconf_client_notify_add(self->priv->gconf, USB_CABLE_DIR,
                          gconf_value_changed, self, NULL, &error);
  if (error != NULL)
  {
    g_warning ("%s: gconf_client_notify_add failed: %s", G_STRFUNC, error->message);
    g_error_free(error);
    error = NULL;
  }

  gconf_client_notify_add(self->priv->gconf, MMC_DIR,
                          gconf_mmc_value_changed, self, NULL, &error);

  if (error != NULL)
  {
    g_warning ("%s: gconf_client_notify_add failed: %s", G_STRFUNC, error->message);
    g_error_free(error);
    error = NULL;
  }

#if 0 //fremantle
  value = gconf_client_get_without_default(self->priv->gconf,
                                           BTCOND_GCONF_PREFERRED, &error);
  if (error != NULL)
  {
    g_warning("gconf_client_get_without_default failed: %s",
               error->message);
    g_error_free(error);
    error = NULL;
  }
  else if (value != NULL)
  {
    set_gateway_from_gconf_value(self, value);
    gconf_value_free(value);
  }
#endif

  value = gconf_client_get_without_default(self->priv->gconf,
                                           USB_CABLE_KEY, &error);
  if (error != NULL)
  {
    g_warning ("%s: gconf_client_get_without_default failed: %s", G_STRFUNC,
               error->message);
    g_error_free(error);
    error = NULL;
  }
  else if (value != NULL)
  {
    set_usb_from_gconf_value(self, value);
    gconf_value_free(value);
  }

  value = gconf_client_get_without_default(self->priv->gconf,
                                           MMC_USED_KEY, &error);
  if (error != NULL)
  {
    g_warning ("%s: gconf_client_get_without_default failed: %s", G_STRFUNC,
               error->message);
    g_error_free(error);
    error = NULL;
  }
  else if (value != NULL)
  {
    set_mmc_used_from_gconf_value(self, value);
    gconf_value_free(value);
  }

  self->priv->gconf_ready = TRUE;

  return FALSE; /* We need this only once */
}

static void
hildon_file_system_settings_init(HildonFileSystemSettings *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, \
    HILDON_TYPE_FILE_SYSTEM_SETTINGS, HildonFileSystemSettingsPrivate);

  self->priv->flightmode = TRUE;
  self->priv->iap_connected = TRUE;
  self->priv->connection_name = NULL;
  
  /* This ugly stuff blocks the execution, so let's do it
     only after we are in idle */
  g_idle_add(delayed_init, self);

  hildon_file_system_settings_setup_dbus(self);
}

HildonFileSystemSettings *_hildon_file_system_settings_get_instance(void)
{
  static HildonFileSystemSettings *singleton = NULL;

  if (G_UNLIKELY(singleton == NULL))
    singleton = g_object_new(HILDON_TYPE_FILE_SYSTEM_SETTINGS, NULL);

  return singleton;
}

gboolean _hildon_file_system_settings_ready(HildonFileSystemSettings *self)
{
  return self->priv->gconf_ready &&
         self->priv->flightmode_ready;
}

GKeyFile *
hildon_file_system_open_user_settings (void)
{
  GError *error = NULL;
  gchar *file = g_strdup_printf ("%s/.osso/hildon-fm", g_get_home_dir ());
  GKeyFile *keys = g_key_file_new ();

  if (!g_key_file_load_from_file (keys, file, 0, &error))
    {
      if (!g_error_matches (error, G_FILE_ERROR, 
			    G_FILE_ERROR_NOENT))
	g_debug ("%s: %s\n", file, error->message);
      g_error_free (error);
    }
  g_free (file);
  return keys;
}

void
hildon_file_system_write_user_settings (GKeyFile *keys)
{
  GError *error = NULL;
  gsize len;
  gchar *data = g_key_file_to_data (keys, &len, &error);

  if (error)
    {
      g_warning ("%s\n", error->message);
      g_error_free (error);
    }
  else
    {
      gchar *file = g_strdup_printf ("%s/.osso/hildon-fm", g_get_home_dir ());
      g_file_set_contents (file, data, len, &error);
      if (error)
	{
	  g_debug ("%s: %s\n", file, error->message);
	  g_error_free (error);
	}
      g_free (file);
    }
  g_free (data);
}
