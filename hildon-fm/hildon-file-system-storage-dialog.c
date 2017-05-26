/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2005-2007 Nokia Corporation.  All rights reserved.
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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h> 
#include <sys/statfs.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <hildon/hildon.h>

#include "hildon-file-common-private.h"
#include "hildon-file-system-storage-dialog.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_FILE_SYSTEM_STORAGE_DIALOG, HildonFileSystemStorageDialogPriv))

/*#define ENABLE_DEBUGGING*/

#define KILOBYTE_FACTOR (1024.0)
#define MEGABYTE_FACTOR (1024.0 * 1024.0)
#define GIGABYTE_FACTOR (1024.0 * 1024.0 * 1024.0)

#define HILDON_FREMANTLE_FM_UI

typedef enum {
	URI_TYPE_FILE_SYSTEM,
	URI_TYPE_INTERNAL_MMC,
	URI_TYPE_EXTERNAL_MMC,
	URI_TYPE_UNKNOWN
} URIType;

struct _HildonFileSystemStorageDialogPriv {
        DBusPendingCall  *pending_call;
        guint             get_apps_id;
        GString          *apps_string;

        /* Stats */
        gchar            *uri_str;
        URIType           uri_type;

        guint             file_count;
        guint             folder_count;

        guint64           email_size;
        guint64           image_size;
        guint64           video_size;
        guint64           audio_size;
        guint64           html_size;
        guint64           doc_size;
        guint64           contact_size;
        guint64           installed_app_size;
        guint64           other_size;

        guint64           in_use_size;

        GFileMonitor     *monitor_handle;

        /* Containers */
        GtkWidget        *viewport_common;
        GtkWidget        *viewport_data;

        /* Common widgets */
        /* table moved from local variable to here, the reason is that the file_system_storage_dialog_set_data 
	   needs to append components to the same table since GtkNotebook is removed  */
        GtkWidget        *table;
        GtkWidget        *label_name;
        GtkWidget        *image_type;
        GtkWidget        *label_type;
        GtkWidget        *label_total_size;
        GtkWidget        *label_in_use;
        GtkWidget        *label_available;

        GtkWidget        *label_read_only_stub;
        GtkWidget        *checkbutton_readonly;
};

static void     hildon_file_system_storage_dialog_class_init      (HildonFileSystemStorageDialogClass *klass);
static void     hildon_file_system_storage_dialog_init            (HildonFileSystemStorageDialog      *widget);
static void     file_system_storage_dialog_finalize               (GObject                            *object);
static void     file_system_storage_dialog_clear_data_container   (GtkWidget                          *widget);
static void     file_system_storage_dialog_stats_clear            (GtkWidget                          *widget);
static gboolean file_system_storage_dialog_stats_collect          (GtkWidget                          *widget,
                                                                   GFile                              *uri);
static gboolean file_system_storage_dialog_stats_get_disk         (GFile                              *uri,
                                                                   guint64                            *total,
                                                                   guint64                            *available);
static void     file_system_storage_dialog_stats_get_contacts     (GtkWidget                          *widget);
static gboolean file_system_storage_dialog_stats_get_emails_cb    (GtkWidget                          *widget,
                                                                   GFile                              *uri);
static void     file_system_storage_dialog_stats_get_emails       (GtkWidget                          *widget);
static gboolean file_system_storage_dialog_stats_get_apps_cb      (GIOChannel                         *source,
								   GIOCondition                        condition,
								   GtkWidget                          *widget);
static void     file_system_storage_dialog_stats_get_apps         (GtkWidget                          *widget);
static void     file_system_storage_dialog_request_device_name_cb (DBusPendingCall                    *pending_call,
								   gpointer                            user_data);
static void     file_system_storage_dialog_request_device_name    (HildonFileSystemStorageDialog      *dialog);
static URIType  file_system_storage_dialog_get_type               (const gchar                        *uri_str);
static void     file_system_storage_dialog_set_no_data            (GtkWidget                          *widget);
static void     file_system_storage_dialog_set_data               (GtkWidget                          *widget);
static void     file_system_storage_dialog_update                 (GtkWidget                          *widget);
static void     file_system_storage_dialog_monitor_cb             (GFileMonitor                 *monitor,
                                                                   GFile                        *file,
                                                                   GFile                        *other_file,
                                                                   GFileMonitorEvent             event_type,
                                                                   HildonFileSystemStorageDialog *widget);

G_DEFINE_TYPE (HildonFileSystemStorageDialog, hildon_file_system_storage_dialog, GTK_TYPE_DIALOG)

static void 
hildon_file_system_storage_dialog_class_init (HildonFileSystemStorageDialogClass *klass)
{
	GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = file_system_storage_dialog_finalize;

	g_type_class_add_private (object_class, sizeof (HildonFileSystemStorageDialogPriv));
}

static void 
hildon_file_system_storage_dialog_init (HildonFileSystemStorageDialog *widget) 
{
        HildonFileSystemStorageDialogPriv *priv;
        GtkWidget   *notebook;
        #ifndef HILDON_FREMANTLE_FM_UI
           GtkWidget   *page;
	#endif
        GtkWidget   *scrolledwindow;
        #ifndef HILDON_FREMANTLE_FM_UI
	   GtkWidget   *viewport_common, *viewport_data;
	#endif
        GtkWidget   *label_name_stub;
        GtkWidget   *label_name;
        GtkWidget   *label_total_size_stub;
        GtkWidget   *hbox;
        GtkWidget   *image_type;
        GtkWidget   *label_type;
        GtkWidget   *label_total_size;
        GtkWidget   *label_in_use_stub;
        GtkWidget   *label_in_use;
        GtkWidget   *label_available_stub;
        GtkWidget   *label_available;
        GtkWidget   *label_read_only_stub;
        GtkWidget   *checkbutton_readonly;
        GtkWidget   *label_type_stub;
        #ifndef HILDON_FREMANTLE_FM_UI
           GtkWidget   *label_common;
           GtkWidget   *label_data;
	#endif
	GdkGeometry  geometry;

        priv = GET_PRIV (widget);

	/* Window properties */
	gtk_window_set_title (GTK_WINDOW (widget), _("sfil_ti_storage_details"));
	gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (widget), FALSE);
	gtk_dialog_add_button (GTK_DIALOG (widget), 
			       _("sfil_bd_storage_details_dialog_ok"), 
			       GTK_RESPONSE_OK);

	/* Create a note book with 2 pages */
	notebook = gtk_notebook_new ();
        #ifndef HILDON_FREMANTLE_FM_UI
	   gtk_widget_show (notebook);
	   gtk_box_pack_start (GTK_BOX (GTK_DIALOG (widget)->vbox), notebook, 
			       TRUE, TRUE, 
			       HILDON_MARGIN_DEFAULT);
	#endif

	/* Setup a good size, copied from the old storage details dialog. */
	geometry.min_width = 133;
	geometry.max_width = 602;

	/* Scrolled windows do not ask space for whole contents in size_request.
	 * So, we must force the dialog to have larger than minimum size 
	 */
	geometry.min_height = 240 + (2 * HILDON_MARGIN_DEFAULT);
	geometry.max_height = 240 + (2 * HILDON_MARGIN_DEFAULT);
	
	gtk_window_set_geometry_hints (GTK_WINDOW (widget),
				       GTK_WIDGET (notebook), &geometry,
				       GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
        
        /* Create first page for "Common" properties */
        #ifndef HILDON_FREMANTLE_FM_UI
            scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	    gtk_container_add (GTK_CONTAINER (notebook), scrolledwindow);
	    gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), HILDON_MARGIN_DEFAULT);
	    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
					    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	    viewport_common = gtk_viewport_new (NULL, NULL);
	    gtk_widget_show (viewport_common);
	    gtk_container_add (GTK_CONTAINER (scrolledwindow), viewport_common);
	#else 
	    scrolledwindow = hildon_pannable_area_new();
	#endif

        gtk_widget_show (scrolledwindow);

        priv->table = gtk_table_new (1, 2, FALSE);
        gtk_widget_show (priv->table);
	#ifndef HILDON_FREMANTLE_FM_UI
           gtk_container_add (GTK_CONTAINER (viewport_common), table);
	#else
	   hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (scrolledwindow), 
						   GTK_WIDGET (GTK_TABLE(priv->table)));
	   gtk_box_pack_start (GTK_BOX (GTK_DIALOG (widget)->vbox), scrolledwindow, 
			       TRUE, TRUE, 
			       HILDON_MARGIN_DEFAULT);
	#endif
        gtk_table_set_col_spacings (GTK_TABLE (priv->table), HILDON_MARGIN_DOUBLE);

        /* Name : Nokia X */
        label_name_stub = gtk_label_new (_("sfil_fi_storage_details_name"));
        gtk_widget_show (label_name_stub);
        gtk_table_attach (GTK_TABLE (priv->table), label_name_stub, 0, 1, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_justify (GTK_LABEL (label_name_stub), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (label_name_stub), 1.0, 0.5);

        label_name = gtk_label_new ("");
        gtk_widget_show (label_name);
        gtk_table_attach (GTK_TABLE (priv->table), label_name, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_name), 0.0, 0.5);

        /* Type : Device X */
        label_type_stub = gtk_label_new (_("sfil_fi_storage_details_type"));
        gtk_widget_show (label_type_stub);
        gtk_table_attach (GTK_TABLE (priv->table), label_type_stub, 0, 1, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_justify (GTK_LABEL (label_type_stub), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (label_type_stub), 1.0, 0.5);

        hbox = gtk_hbox_new (FALSE, HILDON_MARGIN_DEFAULT);
        gtk_widget_show (hbox);
        gtk_table_attach (GTK_TABLE (priv->table), hbox, 1, 2, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (GTK_FILL), 0, 0);

	image_type = gtk_image_new_from_icon_name ("filemanager_removable_storage",
                                                   HILDON_ICON_SIZE_SMALL);
        gtk_widget_show (image_type);
        gtk_box_pack_start (GTK_BOX (hbox), image_type, FALSE, FALSE, 0);

        label_type = gtk_label_new ("Storage device");
        gtk_widget_show (label_type);
        gtk_box_pack_start (GTK_BOX (hbox), label_type, TRUE, TRUE, 0);
        gtk_misc_set_alignment (GTK_MISC (label_type), 0.0, 0.5);

        /* Total size : X MB       */
        label_total_size_stub = gtk_label_new (_("sfil_fi_storage_details_size"));
        gtk_widget_show (label_total_size_stub);
        gtk_table_attach (GTK_TABLE (priv->table), label_total_size_stub, 0, 1, 2, 3,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_justify (GTK_LABEL (label_total_size_stub), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (label_total_size_stub), 1.0, 0.5);

        label_total_size = gtk_label_new ("");
        gtk_widget_show (label_total_size);
        gtk_table_attach (GTK_TABLE (priv->table), label_total_size, 1, 2, 2, 3,
                          (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_total_size), 0.0, 0.5);

        /* In use: X MB*/
        label_in_use_stub = gtk_label_new (_("sfil_fi_storage_details_in_use"));
        gtk_widget_show (label_in_use_stub);
        gtk_table_attach (GTK_TABLE (priv->table), label_in_use_stub, 0, 1, 3, 4,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_justify (GTK_LABEL (label_in_use_stub), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (label_in_use_stub), 1.0, 0.5);

        label_in_use = gtk_label_new ("");
        gtk_widget_show (label_in_use);
        gtk_table_attach (GTK_TABLE (priv->table), label_in_use, 1, 2, 3, 4,
                          (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_in_use), 0.0, 0.5);
        
        /* Available: XMB */
        label_available_stub = gtk_label_new (_("sfil_fi_storage_details_available"));
        gtk_widget_show (label_available_stub);
        gtk_table_attach (GTK_TABLE (priv->table), label_available_stub, 0, 1, 4, 5,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_justify (GTK_LABEL (label_available_stub), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (label_available_stub), 1.0, 0.5);

        label_available = gtk_label_new ("");
        gtk_widget_show (label_available);
        gtk_table_attach (GTK_TABLE (priv->table), label_available, 1, 2, 4, 5,
                          (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_available), 0.0, 0.5);

        /* Read-only: to be defined, check the spec if it is available */
	label_read_only_stub = gtk_label_new (_("sfil_fi_storage_details_readonly"));
        gtk_widget_show (label_read_only_stub);
        gtk_table_attach (GTK_TABLE (priv->table), label_read_only_stub, 0, 1, 5, 6,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_justify (GTK_LABEL (label_read_only_stub), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (label_read_only_stub), 1.0, 0.5);

        checkbutton_readonly = gtk_check_button_new ();
        gtk_widget_show (checkbutton_readonly);
        gtk_widget_set_sensitive (checkbutton_readonly, FALSE);
        gtk_table_attach (GTK_TABLE (priv->table), checkbutton_readonly, 1, 2, 5, 6,
                          (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                          (GtkAttachOptions) (0), 0, 0);

        #ifndef HILDON_FREMANTLE_FM_UI
            page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0);
	    label_common = gtk_label_new (_("sfil_ti_storage_details_common"));
	    gtk_widget_show (label_common);
            gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), page, label_common);
	    /* Create second page for "Data" properties */
	    scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	    gtk_widget_show (scrolledwindow);
	    gtk_container_add (GTK_CONTAINER (notebook), scrolledwindow);
	    gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), HILDON_MARGIN_DEFAULT);
	    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), 
					    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	    viewport_data = gtk_viewport_new (NULL, NULL);
	    gtk_widget_show (viewport_data);
	    gtk_container_add (GTK_CONTAINER (scrolledwindow), viewport_data);

	    /* There is no table or widget set here, we use the _set_no_data()
	     * or the _set_data() functions here to do that.
	     */

	    page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1);
	    label_data = gtk_label_new (_("sfil_ti_storage_details_data"));
	    gtk_widget_show (label_data);
	    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), page, label_data);

	    /* Set private widgets */
	    priv->viewport_common = viewport_common;
	    priv->viewport_data = viewport_data;
	#endif

        priv->label_name = label_name;
        priv->image_type = image_type;
        priv->label_type = label_type;
        priv->label_total_size = label_total_size;
        priv->label_in_use = label_in_use;
        priv->label_available = label_available;

        priv->label_read_only_stub = label_read_only_stub;
        priv->checkbutton_readonly = checkbutton_readonly;
}

static void
file_system_storage_dialog_finalize (GObject *object)
{
  HildonFileSystemStorageDialogPriv *priv;

  priv = GET_PRIV (object);

  if (priv->monitor_handle) {
    g_file_monitor_cancel (priv->monitor_handle);
    g_object_unref (priv->monitor_handle);
    priv->monitor_handle = NULL;
  }

  if (priv->pending_call) {
    dbus_pending_call_cancel (priv->pending_call);
    dbus_pending_call_unref (priv->pending_call);
    priv->pending_call = NULL;
  }

  if (priv->get_apps_id) {
    g_source_remove (priv->get_apps_id);
    priv->get_apps_id = 0;
  }

  if (priv->apps_string) {
    g_string_free (priv->apps_string, TRUE);
  }

  g_free (priv->uri_str);

  G_OBJECT_CLASS (hildon_file_system_storage_dialog_parent_class)->finalize (object);
}

static void
file_system_storage_dialog_clear_data_container (GtkWidget *widget)
{
        HildonFileSystemStorageDialog     *storage;
        HildonFileSystemStorageDialogPriv *priv;
        GList                             *children;
        
        storage = HILDON_FILE_SYSTEM_STORAGE_DIALOG (widget);
        priv = GET_PRIV (storage);

        children = gtk_container_get_children (GTK_CONTAINER (priv->viewport_data));
        g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);
}

static void
file_system_storage_dialog_stats_clear (GtkWidget *widget)
{
        HildonFileSystemStorageDialogPriv *priv;
 
        priv = GET_PRIV (widget);

        priv->file_count = 0;
        priv->folder_count = 0;

        priv->email_size = 0;
        priv->image_size = 0;
        priv->video_size = 0;
        priv->audio_size = 0;
        priv->html_size = 0;
        priv->doc_size = 0;
        priv->contact_size = 0;
        priv->installed_app_size = 0;
        priv->other_size = 0;
        priv->in_use_size = 0;
}

static gboolean
file_system_storage_dialog_stats_collect (GtkWidget   *widget, 
                                          GFile       *uri)
{
  HildonFileSystemStorageDialogPriv *priv;
  GFileInfo                         *info;
  GFileEnumerator *enumerator;
  GError                            *error = NULL;

  priv = GET_PRIV (widget);
  priv->folder_count++;

  enumerator =
      g_file_enumerate_children (uri,
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                 G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE","
                                 G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
                                 &error);

  if (!enumerator) {
    gchar *uri_str = g_file_get_uri (uri);

    if (error)
    {
      g_warning ("Could not open directory:'%s', error:'%s'\n", uri_str,
                 error->message);
      g_error_free (error);
    }
    else
      g_warning ("Could not open directory:'%s',no error message\n", uri_str);

    g_free (uri_str);

    return FALSE;
  }

  info = g_file_enumerator_next_file (enumerator, NULL, &error);

  while (info && !error) {
    const char *name = g_file_info_get_name (info);
    GFile *child;

    if (!strcmp (name, ".") || !strcmp (name, ".."))
      continue;

    child = g_file_get_child (uri, name);

    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
      file_system_storage_dialog_stats_collect (widget, child);
    } else {
      gchar *mime_type;

      priv->file_count++;
      priv->in_use_size += g_file_info_get_size (info);

      if (!g_file_info_get_content_type (info)) {
        priv->other_size += g_file_info_get_size (info);
        g_object_unref (child);
	info = g_file_enumerator_next_file (enumerator, NULL, &error);
        continue;
      }

      mime_type = g_ascii_strdown (g_file_info_get_content_type (info), -1);

      if (g_str_has_prefix (mime_type, "image") ||
          g_str_has_prefix (mime_type, "sketch/png") ||
          g_str_has_prefix (mime_type, "application/x-sketch-png")) {
        priv->image_size += g_file_info_get_size (info);
      } else if (g_str_has_prefix (mime_type, "audio")) {
        priv->audio_size += g_file_info_get_size (info);
      } else if (g_str_has_prefix (mime_type, "video")) {
        priv->video_size += g_file_info_get_size (info);
      } else if (g_str_has_prefix (mime_type, "text/xml") ||
                 g_str_has_prefix (mime_type, "text/html")) {
        priv->html_size += g_file_info_get_size (info);
      } else if (g_str_has_prefix (mime_type, "text/plain") ||
                 g_str_has_prefix (mime_type, "text/x-notes") ||
                 g_str_has_prefix (mime_type, "text/note") ||
                 g_str_has_prefix (mime_type, "text/richtext") ||
                 g_str_has_prefix (mime_type, "application/pdf") ||
                 g_str_has_prefix (mime_type, "application/rss+xml")) {
        priv->doc_size += g_file_info_get_size (info);
      } else
        priv->other_size += g_file_info_get_size (info);

      g_free (mime_type);
    }

    g_object_unref (child);
    g_object_unref (info);
    info = g_file_enumerator_next_file (enumerator, NULL, &error);
  }

  g_object_unref (enumerator);

#if 0
  g_debug ("Collecting stats for path: '%s' (%d files), "
           "total size:%" GNOME_VFS_SIZE_FORMAT_STR ", "
           "I:%" GNOME_VFS_SIZE_FORMAT_STR ", "
           "A:%" GNOME_VFS_SIZE_FORMAT_STR ", "
           "V:%" GNOME_VFS_SIZE_FORMAT_STR ", "
           "D:%" GNOME_VFS_SIZE_FORMAT_STR ", "
           "H:%" GNOME_VFS_SIZE_FORMAT_STR ", "
           "O:%" GNOME_VFS_SIZE_FORMAT_STR "\n",
           gnome_vfs_uri_get_path (uri),
           g_list_length (files),
           priv->in_use_size,
           priv->image_size,
           priv->audio_size,
           priv->video_size,
           priv->doc_size,
           priv->html_size,
           priv->other_size);
#endif

  return TRUE;
} 

static gboolean
file_system_storage_dialog_stats_get_disk (GFile   *uri,
                                           guint64 *total,
                                           guint64 *available)
{

  GFileInfo *info;

  info =  g_file_query_filesystem_info (uri,
                                        G_FILE_ATTRIBUTE_FILESYSTEM_SIZE","
                                        G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
                                        NULL, NULL);

  if (total) {
    *total =
        g_file_info_get_attribute_uint64 (info,
                                          G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
  }

  if (available) {
    *available =
        g_file_info_get_attribute_uint64 (info,
                                          G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
  }

  return TRUE;
}

static void 
file_system_storage_dialog_stats_get_contacts (GtkWidget *widget)
{
  HildonFileSystemStorageDialogPriv *priv;
  GFile                             *file;
  GFileInfo                         *info;
  gchar                             *uri_str;

  priv = GET_PRIV (widget);

  if (priv->uri_type != URI_TYPE_FILE_SYSTEM &&
      priv->uri_type != URI_TYPE_UNKNOWN) {
    return;
  }

  uri_str = g_build_filename (g_get_home_dir (), ".osso-email",
                              "AddressBook.xml", NULL);
#if 0
	g_debug ("Collecting stats for contacts by URI type:%d using path: '%s'\n", type, uri_str);
#endif

  file = g_file_new_for_uri (uri_str);
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE, NULL, NULL);

  if (info) {
    priv->contact_size = g_file_info_get_size (info);
    priv->in_use_size += g_file_info_get_size (info);
    g_object_unref (info);
  }

  g_object_unref (file);
  g_free (uri_str);
}
/*
static gboolean
file_system_storage_dialog_stats_get_emails_cb (const gchar      *path,
						GnomeVFSFileInfo *info,
						gboolean          recursing_loop, 
						GtkWidget        *widget,
						gboolean         *recurse)
{
	HildonFileSystemStorageDialogPriv *priv;

	if (!path || !widget || !info) {
		return FALSE;
	}

	priv = GET_PRIV (widget);
	
	if (strcmp (info->name, ".") == 0 ||
	    strcmp (info->name, "..") == 0) {
		if (recursing_loop) {
			return FALSE;
		}
	      
		*recurse = TRUE;
	} else {
		*recurse = FALSE;
	}

	priv->email_size += info->size;
	priv->in_use_size += info->size;

	return TRUE;
}
*/
static gboolean
file_system_storage_dialog_stats_get_emails_cb (GtkWidget   *widget,
                                                GFile       *uri)
{
  HildonFileSystemStorageDialogPriv *priv;
  GFileInfo                         *info;
  GFileEnumerator *enumerator;
  GError                            *error = NULL;

  priv = GET_PRIV (widget);
  priv->folder_count++;

  enumerator =
      g_file_enumerate_children (uri,
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                 G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
                                 &error);

  if (!enumerator) {
    gchar *uri_str = g_file_get_uri (uri);

    if (error)
    {
      g_warning ("Could not open directory:'%s', error:'%s'\n", uri_str,
                 error->message);
      g_error_free (error);
    }
    else
      g_warning ("Could not open directory:'%s',no error message\n", uri_str);

    g_free (uri_str);

    return FALSE;
  }

  info = g_file_enumerator_next_file (enumerator, NULL, &error);

  while (info && !error) {
    const char *name = g_file_info_get_name (info);
    GFile *child;

    if (!strcmp (name, ".") || !strcmp (name, ".."))
      continue;

    child = g_file_get_child (uri, name);

    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
      file_system_storage_dialog_stats_get_emails_cb (widget, child);
    } else {
      priv->email_size += g_file_info_get_size (info);
      priv->in_use_size += g_file_info_get_size (info);
    }

    g_object_unref (child);
    g_object_unref (info);
    info = g_file_enumerator_next_file (enumerator, NULL, &error);
  }

  g_object_unref (enumerator);

  return TRUE;
}

static void 
file_system_storage_dialog_stats_get_emails (GtkWidget *widget)
{
        HildonFileSystemStorageDialogPriv *priv;
        gchar                             *uri_str;
        GFile                             *uri;

        priv = GET_PRIV (widget);

	switch (priv->uri_type) {
	case URI_TYPE_INTERNAL_MMC:
		uri_str = g_build_path ("/",
					g_getenv ("INTERNAL_MMC_MOUNTPOINT"),
					".archive",
					NULL);
		break;

	case URI_TYPE_EXTERNAL_MMC:
		uri_str = g_build_path ("/",
					g_getenv ("MMC_MOUNTPOINT"),
					".archive",
					NULL);
		break;

	case URI_TYPE_FILE_SYSTEM:
	case URI_TYPE_UNKNOWN:
	default:
		uri_str = g_build_path ("/",
					g_get_home_dir (),
					"apps",
					"email",
					"Mail",
					NULL);
		break;
	}

#ifdef ENABLE_DEBUGGING
	g_debug ("Collecting stats for emails by URI type:%d using path: '%s'\n", type, uri_str);
#endif

        uri = g_file_new_for_path (uri_str);
        file_system_storage_dialog_stats_get_emails_cb (widget, uri);
        g_object_unref (uri);
        g_free (uri_str);
}

static gboolean 
file_system_storage_dialog_stats_get_apps_cb (GIOChannel   *source, 
					      GIOCondition  condition, 
					      GtkWidget    *widget)
{
	HildonFileSystemStorageDialogPriv *priv;

        priv = GET_PRIV (widget);

	if (condition & G_IO_IN) {
		GIOStatus status;

		
		status = G_IO_STATUS_AGAIN;
		while (status == G_IO_STATUS_AGAIN) {
			gchar *input;
			gsize  len;

			status = g_io_channel_read_to_end (source, &input, &len, NULL);
			if (len < 1) {
				break;
			}

			g_string_append (priv->apps_string, input);
			g_free (input);
		}
	}

	if (condition & G_IO_HUP) {
		HildonFileSystemStorageDialogPriv  *priv;
		gchar                             **rows, **row;

		priv = GET_PRIV (widget);

		g_io_channel_shutdown (source, FALSE, NULL);
		g_io_channel_unref (source);

                priv->get_apps_id = 0;
                
		/* Get install application stats */
		rows = g_strsplit (priv->apps_string->str, "\n", -1);
		for (row = rows; *row != NULL; row++) {
                        guint64            bytes;
			gchar            **cols;
			gint               len;

			cols = g_strsplit (*row, "\t", -1);
			len = g_strv_length (cols) - 1;

			if (!cols || len < 0) {
				continue;
			}
		
			bytes = 1024 * atoi (cols[len]);
			priv->installed_app_size += bytes;
			priv->in_use_size += bytes;
			g_strfreev (cols);
		}
		
		g_strfreev (rows);
		file_system_storage_dialog_set_data (widget);

		return FALSE;
	}

	return TRUE;
}

static void     
file_system_storage_dialog_stats_get_apps (GtkWidget *widget)
{
        HildonFileSystemStorageDialogPriv *priv;
	GIOChannel                        *channel;
	GPid                               pid;
	gchar                             *argv[] = { "/usr/bin/maemo-list-user-packages", NULL };
	gint                               out;
	gboolean                           success;

        priv = GET_PRIV (widget);

        if (priv->apps_string) {
                g_string_truncate (priv->apps_string, 0);
        } else {
                priv->apps_string = g_string_new (NULL);
        }

        if (priv->get_apps_id) {
                g_source_remove (priv->get_apps_id);
                priv->get_apps_id = 0;
        }
        
	if (priv->uri_type != URI_TYPE_FILE_SYSTEM) {
		return;
	}

	success = g_spawn_async_with_pipes (NULL, argv, NULL, 
					    G_SPAWN_SEARCH_PATH, 
					    NULL, NULL, 
					    &pid, 
					    NULL, &out, NULL, 
					    NULL);
	if (!success) {
		g_warning ("Could not spawn command: '%s' to get list of applications", argv[0]);
		return;
	}

	channel = g_io_channel_unix_new (out);
	g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);
	priv->get_apps_id = g_io_add_watch_full (
                channel, 
                G_PRIORITY_DEFAULT_IDLE, 
                G_IO_IN | G_IO_HUP, 
                (GIOFunc) 
                file_system_storage_dialog_stats_get_apps_cb, 
                widget, 
                NULL);
}

static void
file_system_storage_dialog_request_device_name_cb (DBusPendingCall *pending_call, 
						   gpointer         user_data)
{
        HildonFileSystemStorageDialog     *dialog = user_data;
        HildonFileSystemStorageDialogPriv *priv;
        DBusMessage                       *reply;
        DBusError                          error;
        gchar                             *name;

        priv = GET_PRIV (dialog);
        
        name = NULL;

        reply = dbus_pending_call_steal_reply (pending_call);
        if (reply) {
                dbus_error_init (&error);

                if (!dbus_set_error_from_message (&error, reply)) {
                        gchar *tmp = NULL;
                
                        if (dbus_message_get_args (reply, NULL,
                                                   DBUS_TYPE_STRING, &tmp,
                                                   DBUS_TYPE_INVALID)) {
                                name = g_strdup (tmp);
                        }
                } else {
                        g_warning ("Did not get the name: %s %s\n",
                                    error.name, error.message);
                }
                
                dbus_message_unref (reply);
        }
        
        if (name) {
                gtk_label_set_text (GTK_LABEL (priv->label_name), name);
        } else {
                /* This should never happen but just in case, use the same
                 * fallback as the file manager.
                 */
                gtk_label_set_text (GTK_LABEL (priv->label_name), "Internet Tablet");
        }

        dbus_pending_call_unref (priv->pending_call);
        priv->pending_call = NULL;
}

static void
file_system_storage_dialog_request_device_name (HildonFileSystemStorageDialog *dialog)
{
        HildonFileSystemStorageDialogPriv *priv;
	static DBusConnection             *conn;
	DBusMessage                       *message;

        priv = GET_PRIV (dialog);

        if (!conn) {
                conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);
                if (!conn) {
                        return;
                }
                dbus_connection_setup_with_g_main (conn, NULL);
        }
        
        if (priv->pending_call) {
                dbus_pending_call_cancel (priv->pending_call);
                dbus_pending_call_unref (priv->pending_call);
                priv->pending_call = NULL;
        }

	message = dbus_message_new_method_call ("org.bluez",
						"/org/bluez/hci0",
						"org.bluez.Adapter",
						"GetName");

        if (dbus_connection_send_with_reply (conn, message, &priv->pending_call, -1)) {
                dbus_pending_call_set_notify (priv->pending_call,
                                              file_system_storage_dialog_request_device_name_cb,
                                              dialog, NULL);
        }

        dbus_message_unref (message);
}

static URIType
file_system_storage_dialog_get_type (const gchar *uri_str)
{
  const gchar *env;
  GFile       *uri;
  URIType retval = URI_TYPE_UNKNOWN;

  if (!uri_str)
    goto out;

  uri = g_file_new_for_uri (uri_str);

  env = g_getenv ("MYDOCSDIR");

  if (env) {
    GFile *uri_env = g_file_new_for_path (env);
    gboolean found = g_file_equal (uri, uri_env);

    g_object_unref (uri_env);

    if (found) {
      retval =  URI_TYPE_FILE_SYSTEM;
      goto out;
    }
  }

  env = g_getenv ("INTERNAL_MMC_MOUNTPOINT");
  if (env) {
    GFile *uri_env = g_file_new_for_path (env);
    gboolean found = g_file_equal (uri, uri_env);

    g_object_unref (uri_env);

    if (found) {
      retval = URI_TYPE_INTERNAL_MMC;
      goto out;
    }
  }

  env = g_getenv ("MMC_MOUNTPOINT");
  if (env) {
    GFile *uri_env = g_file_new_for_path (env);
    gboolean found = g_file_equal (uri, uri_env);

    g_object_unref (uri_env);

    if (found) {
      retval = URI_TYPE_EXTERNAL_MMC;
      goto out;
    }
  }

out:
  g_object_unref (uri);

  return retval;
}

static void
file_system_storage_dialog_set_no_data (GtkWidget *widget) 
{
        HildonFileSystemStorageDialog     *storage;
        HildonFileSystemStorageDialogPriv *priv;
        GtkWidget                         *label_no_data;

        g_return_if_fail (HILDON_IS_FILE_SYSTEM_STORAGE_DIALOG (widget));

        storage = HILDON_FILE_SYSTEM_STORAGE_DIALOG (widget);
        priv = GET_PRIV (storage);

        /* Clear current child */
        file_system_storage_dialog_clear_data_container (widget);

        /* Create new widget */
        label_no_data = gtk_label_new (_("sfil_li_storage_details_no_data"));
        gtk_widget_show (label_no_data);
        gtk_misc_set_alignment (GTK_MISC (label_no_data), 0.0, 0.0);
        gtk_container_add (GTK_CONTAINER (priv->viewport_data), label_no_data);
}

static void
file_system_storage_dialog_set_data (GtkWidget *widget) 
{
        HildonFileSystemStorageDialogPriv *priv;
        guint64                            size;
       	#ifndef HILDON_FREMANTLE_FM_UI
	   GtkWidget                      *table;
	#endif
        GtkWidget                         *label_size;
        GtkWidget                         *label_category;
        const gchar                       *category_str;
        gchar                             *str;
        gint                               category;
        gint                               col;
        gboolean                           have_data;

        g_return_if_fail (HILDON_IS_FILE_SYSTEM_STORAGE_DIALOG (widget));
        priv = GET_PRIV (widget);

        /* Clear current child */
        file_system_storage_dialog_clear_data_container (widget);

       	#ifndef HILDON_FREMANTLE_FM_UI
           /* Create new table for categories */
           table = gtk_table_new (1, 1, FALSE);
           gtk_widget_show (table);
           gtk_container_add (GTK_CONTAINER (priv->viewport_data), table);
           gtk_table_set_col_spacings (GTK_TABLE (table), HILDON_MARGIN_DOUBLE);
	#endif

        category = 0;
        col = 6;
        have_data = FALSE;

        while (category < 9) {
                switch (category) {
                case 0: 
                        category_str = _("sfil_li_emails");
                        size = priv->email_size;
                        break;
                case 1: 
                        category_str = _("sfil_li_images");
                        size = priv->image_size;
                        break;
                case 2: 
                        category_str = _("sfil_li_video_clips");
                        size = priv->video_size;
                        break;
                case 3: 
                        category_str = _("sfil_li_sound_clips");
                        size = priv->audio_size;
                        break;
                case 4: 
                        category_str = _("sfil_li_web_pages");
                        size = priv->html_size;
                        break;
                case 5: 
                        category_str = _("sfil_li_documents");
                        size = priv->doc_size;
                        break;
                case 6: 
                        category_str = _("sfil_li_contacts");
                        size = priv->contact_size;
                        break;
                case 7: 
                        category_str = _("sfil_li_installed_applications");
                        size = priv->installed_app_size;
                        break;
                case 8: 
		default:
                        category_str = _("sfil_li_other_files");
                        size = priv->other_size;
                        break;
                }

                category++;

                if (size < 1) {
                        continue;
                }

				str = hildon_format_file_size_for_display (size);

                label_size = gtk_label_new (str);
				g_free (str);
                gtk_widget_show (label_size);
                gtk_table_attach (GTK_TABLE (priv->table), label_size, 0, 1, col, col + 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_label_set_justify (GTK_LABEL (label_size), GTK_JUSTIFY_RIGHT);
                gtk_misc_set_alignment (GTK_MISC (label_size), 1.0, 0.5);
                
                label_category = gtk_label_new (category_str);
                gtk_widget_show (label_category);
                gtk_table_attach (GTK_TABLE (priv->table), label_category, 1, 2, col, col + 1,
                                  (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_label_set_justify (GTK_LABEL (label_size), GTK_JUSTIFY_LEFT);
                gtk_misc_set_alignment (GTK_MISC (label_category), 0.0, 0.5);
                
                col++;
                have_data = TRUE;
        }

        if (!have_data) {
                file_system_storage_dialog_set_no_data (widget);
        }
}

static void
file_system_storage_dialog_update (GtkWidget *widget)
{
        HildonFileSystemStorageDialogPriv *priv;
        GList                             *mounts, *l;
        GFile                             *uri;
        guint64                            total_size, available_size;
	const gchar                       *type_icon_name;
	const gchar                       *type_name;
        gchar                             *display_name;
        gchar                             *total;
        gchar                             *available;
        gchar                             *in_use;
        
        priv = GET_PRIV (widget);
        uri = g_file_new_for_uri (priv->uri_str);

        /* Clean up any old values in case the URI has changed. */
        file_system_storage_dialog_stats_clear (widget);
        gtk_label_set_text (GTK_LABEL (priv->label_total_size), "");
        gtk_label_set_text (GTK_LABEL (priv->label_available), "");
        gtk_label_set_text (GTK_LABEL (priv->label_in_use), "");

        /* Find out what storage we have. */
        if (priv->uri_type == URI_TYPE_INTERNAL_MMC ||
            priv->uri_type == URI_TYPE_EXTERNAL_MMC) {
                GVolumeMonitor *monitor = g_volume_monitor_get ();
                GMount         *mount = NULL;

		/* Get URI and Volume to obtain details */
                mounts = g_volume_monitor_get_mounts (monitor);

                for (l = mounts; l && !mount; l = l->next) {
                    GFile *mount_root = g_mount_get_root (l->data);

                    if (g_file_equal (mount_root, uri))
                        mount = l->data;

                    g_object_unref (mount_root);
                }

                g_list_foreach (mounts, (GFunc) g_object_unref, NULL);
                g_list_free (mounts);
                g_object_unref (monitor);

                if (mount) {
			/* Read only */
                        GFile     *mount_root = g_mount_get_root (mount);
                        GFileInfo *info =
                            g_file_query_filesystem_info (
                              mount_root, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY,
                              NULL, NULL);
                        gboolean readonly =
                            g_file_info_get_attribute_boolean (
                              info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY);

                        g_object_unref (info);
                        g_object_unref (mount_root);
                        gtk_toggle_button_set_active (
                              GTK_TOGGLE_BUTTON (priv->checkbutton_readonly),
                              readonly);
			
			/* Display name */
                        display_name = g_mount_get_name (mount);

                        if (display_name) {
                                gchar *translated;
                                
                                if (strcmp (display_name, "mmc-undefined-name") == 0) {
                                        translated = g_strdup (_("sfil_li_memorycard_removable"));
                                }
                                else if (strcmp (display_name, "mmc-undefined-name-internal") == 0) {
                                        translated = g_strdup (_("sfil_li_memorycard_internal"));
                                } else {
                                        translated = g_strdup (display_name);
                                }
                                g_free (display_name);
                                display_name = translated;
                        }

                        gtk_label_set_text (GTK_LABEL (priv->label_name),
                                            display_name ? display_name : "");

                        g_free (display_name);
		} else {
                        /* We didn't find any matching volume, apparently we
                         * were called with an invalid or unmounted volume. Just
                         * leave the dialog empty.
                         */
                        g_object_unref (uri);
			return;
		}
	} else {
                gtk_label_set_text (GTK_LABEL (priv->label_name), "");
                file_system_storage_dialog_request_device_name (
                        HILDON_FILE_SYSTEM_STORAGE_DIALOG (widget));
	}

	/* Type label and icon */
	switch (priv->uri_type) {
	case URI_TYPE_FILE_SYSTEM:
		type_icon_name = "general_device_root_folder";
		type_name = _("sfil_va_type_internal_memory");
		break;
	case URI_TYPE_INTERNAL_MMC:
		type_icon_name = "qgn_list_gene_internal_memory_card";
		type_name = _("sfil_va_type_internal_memorycard");
		break;
	case URI_TYPE_EXTERNAL_MMC:
		type_icon_name = "general_removable_memory_card";
		type_name = _("sfil_va_type_removable_memorycard");
		break;
	case URI_TYPE_UNKNOWN:
	default:
		type_icon_name = "filemanager_removable_storage";
		type_name = _("sfil_va_type_storage_other");
		break;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image_type), 
                                      type_icon_name,
                                      HILDON_ICON_SIZE_SMALL);
	gtk_label_set_text (GTK_LABEL (priv->label_type), type_name);

        /* Set volume stats */
        if (file_system_storage_dialog_stats_get_disk (uri,
						       &total_size,
						       &available_size)) {
		total = hildon_format_file_size_for_display (total_size);
		available = hildon_format_file_size_for_display (available_size);
		in_use = hildon_format_file_size_for_display (total_size - available_size);
	} else {
                total = g_strdup(_("sfil_va_total_size_removable_storage"));
                available = g_strdup(_("sfil_va_total_size_removable_storage"));
                in_use = g_strdup(_("sfil_va_total_size_removable_storage"));
        }

        gtk_label_set_text (GTK_LABEL (priv->label_total_size), total ? total : "");
        gtk_label_set_text (GTK_LABEL (priv->label_available), available ? available : "");
        gtk_label_set_text (GTK_LABEL (priv->label_in_use), in_use ? in_use : "");

        g_free(total);
        g_free(available);
        g_free(in_use);

        /* Sort out file categories */
        file_system_storage_dialog_stats_collect (widget, uri);
        file_system_storage_dialog_stats_get_contacts (widget);
        file_system_storage_dialog_stats_get_emails (widget);
        file_system_storage_dialog_stats_get_apps (widget);
#ifndef HILDON_FREMANTLE_FM_UI
           file_system_storage_dialog_set_data (widget);
#endif

        /* Clean up */
        g_object_unref (uri);
}

static void
file_system_storage_dialog_monitor_cb (GFileMonitor                 *monitor,
                                       GFile                        *file,
                                       GFile                        *other_file,
                                       GFileMonitorEvent             event_type,
                                       HildonFileSystemStorageDialog *widget)
{
      HildonFileSystemStorageDialogPriv *priv;
      GFile *uri;

      if (event_type != G_FILE_MONITOR_EVENT_DELETED)
        return;

      priv = GET_PRIV (widget);
      uri = g_file_new_for_uri (priv->uri_str);

      if (g_file_equal (file, uri))
        gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);

      g_object_unref (uri);
}

/**
 * hildon_file_system_storage_dialog_new:
 * @parent: A parent window or %NULL
 * @uri_str: A URI on string form pointing to a storage root
 *
 * Creates a storage details dialog. An example of @uri_str is
 * "file:///media/mmc1" or "file:///home/user/MyDocs". %NULL can be used if you
 * want to set the URI later with hildon_file_system_storage_dialog_set_uri().
 *
 * Return value: The created dialog
 **/
GtkWidget *
hildon_file_system_storage_dialog_new (GtkWindow   *parent,
				       const gchar *uri_str)
{
        GtkWidget *widget;

	widget = g_object_new (HILDON_TYPE_FILE_SYSTEM_STORAGE_DIALOG, NULL);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (widget), parent);
	}

	if (uri_str) {
		hildon_file_system_storage_dialog_set_uri (widget, uri_str);
	}

        return GTK_WIDGET (widget);
}

/**
 * hildon_file_system_storage_dialog_set_uri:
 * @widget: The HildonFileSystemStorageDialog dialog
 * @uri_str: A URI on string form pointing to a storage root
 * 
 * Sets the storage URI for the dialog, and updates its contents. Note that it
 * should be the root of the storage, for example file:///home/user/MyDocs, if
 * you want the "device memory". If you pass in "file:///" for example, it will
 * traverse the whole file system to collect information about used memory,
 * which most likely isn't what you want.
 **/
void
hildon_file_system_storage_dialog_set_uri (GtkWidget   *widget,
                                           const gchar *uri_str)
{
  HildonFileSystemStorageDialogPriv *priv;
  GFile *file;
  GError *error = NULL;

  g_return_if_fail (HILDON_IS_FILE_SYSTEM_STORAGE_DIALOG (widget));
  g_return_if_fail (uri_str != NULL && uri_str[0] != '\0');

  file = g_file_new_for_uri (uri_str);

  priv = GET_PRIV (widget);

  g_free (priv->uri_str);

  priv->uri_type = file_system_storage_dialog_get_type (uri_str);
  priv->uri_str = g_strdup (uri_str);

  if (priv->monitor_handle) {
    g_file_monitor_cancel (priv->monitor_handle);
    g_object_unref (priv->monitor_handle);
    priv->monitor_handle = NULL;
  }

  priv->monitor_handle =
      g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);

  g_object_unref (file);

  if (priv->monitor_handle) {
    g_signal_connect (priv->monitor_handle, "changed",
                      (GCallback)file_system_storage_dialog_monitor_cb, widget);
  }
  else {
    g_warning ("Could not add monitor for uri:'%s', %s",
               uri_str, error->message);
    g_error_free (error);
  }

  file_system_storage_dialog_update (widget);
}
