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
 * SECTION:hildon-file-chooser-dialog
 * @short_description: Hildon file chooser dialog
 *
 * #HildonFileChooserDialog provides a dialog box for Hildon applications,
 * with features similar to #GtkFileChooserDialog.  It allows to pick a file
 * to open, to specify file name and location for a file to be saved, and to
 * select or create folders.
 *
 * It exposes the #GtkFileChooser interface, but also has additional
 * functionality.  For example see
 * hildon_file_chooser_dialog_set_safe_folder(),
 * the #HildonFileChooserDialog:autonaming and other properties.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE  /* To get the GNU version of basename. */
#define HILDON_FREMANTLE_FM_UI

#include <string.h>
#include <hildon/hildon.h>

#include "gtkfilesystem/gtkfilechooserutils.h"
#include "gtkfilesystem/gtkfilesystem.h"

#include "hildon-file-selection.h"
#include "hildon-file-chooser-dialog.h"
#include "hildon-file-system-private.h"
#include "hildon-file-system-settings.h"
#include <libintl.h>
#include <gdk/gdkx.h>
#include <stdlib.h>

#include "hildon-file-common-private.h"

#define HILDON_RESPONSE_FOLDER_BUTTON 12345
#define HILDON_RESPONSE_FOLDER_CREATED 54321

#define HILDON_RESPONSE_UP_BUTTON 12346
#define HILDON_RESPONSE_PATH_BUTTON 64321

/* Common height for filetrees. About 8 lines. Filetree sets default margins,
    so we need to take them into account. See #9962. */
#define FILE_SELECTION_HEIGHT (8 * 30 + 2 * HILDON_MARGIN_DEFAULT)
#define FILE_SELECTION_WIDTH_LIST 240   /* Width used in select folder
                                           mode */
#define FILE_SELECTION_UPBUTTON_WIDTH 85 /* the width of the up level button */
#define FILE_SELECTION_WIDTH_TOTAL 590  /* Width for full filetree (both
                                           content and navigation pane) */
/* adopted from GTK MAXPATHLEN */
#define MAXPATHLEN 1024

static void sync_extensions_combo (HildonFileChooserDialogPrivate *priv);
static void hildon_file_chooser_dialog_reset_files_visibility(HildonFileChooserDialogPrivate *priv,
							      gint dialog_type);

#define HILDON_FILE_CHOOSER_DIALOG_TYPE_SELECTION_MODE (hildon_file_chooser_dialog_selection_mode_get_type())
static GType
hildon_file_chooser_dialog_selection_mode_get_type(void)
{
  static GType selection_mode_type = 0;
  static GEnumValue selection_mode[] = {
    {HILDON_FILE_SELECTION_MODE_LIST, "1", "list"},
    {HILDON_FILE_SELECTION_MODE_THUMBNAILS, "2", "thumbnails"}
  };

  if (!selection_mode_type) {
    selection_mode_type =
        g_enum_register_static ("HildonFileChooserDialogSelectionMode", selection_mode);
  }
  return selection_mode_type;
}


void hildon_gtk_file_chooser_install_properties(GObjectClass * klass);

static void hildon_file_chooser_update_path_button(HildonFileChooserDialog *self);

enum {
    PROP_EMPTY_TEXT = 0x2000,
    PROP_FILE_SYSTEM_MODEL,
    PROP_FOLDER_BUTTON,
    PROP_LOCATION,
    PROP_AUTONAMING,
    PROP_OPEN_BUTTON_TEXT,
    PROP_MULTIPLE_TEXT,
    PROP_MAX_NAME_LENGTH,
    PROP_MAX_FULL_PATH_LENGTH,
    PROP_SELECTION_MODE,
    PROP_SHOW_FILES,
    PROP_SYNC_MODE
};

struct _HildonFileChooserDialogPrivate {
    GtkWidget *up_button;
    GtkWidget *path_button;
    GtkWidget *path_label;
    GtkWidget *location_button;

    GtkWidget *action_button;
    GtkWidget *folder_button;
    HildonFileSelection *filetree;
    HildonFileSystemModel *model;
    GtkSizeGroup *caption_size_group;
    GtkSizeGroup *value_size_group;

    GtkWidget *entry_name;
    GtkWidget *eventbox_location;
    GtkWidget *hbox_location, *image_location, *title_location;
    /* horizontal address box containing the up level button 
       and the path button */
    GtkWidget *hbox_address;
    GtkWidget *extensions_combo;
    GtkFileChooserAction action;
    GtkWidget *popup;
    GtkWidget *multiple_label, *hbox_items;
    gulong changed_handler;
    gint max_full_path_length;
    gint max_filename_length;
    gboolean popup_protect;
    GCancellable *cancellable;

    /* Popup menu contents */
    GtkWidget *sort_type, *sort_name, *sort_date, *sort_size;

    GtkWidget *mode_list, *mode_thumbnails;
    GSList *filters;
    GtkWidget *filters_separator;
    GSList *filter_menu_items;
    GSList * filter_item_menu_toggle_handlers;

    gchar *stub_name;   /* Set by call to set_current_name */
    gchar *ext_name;
    gboolean autonaming_enabled;
    gboolean edited;
    gboolean should_show_folder_button;
    gboolean should_show_location;
    gboolean show_upnp;
    gboolean show_files;

    GtkWidget *confirmation_note;
    gboolean do_overwrite_confirmation;

    GList *extensions_list;
    GList *ext_names_list;

    gboolean sync_mode;
    GtkFilePath *dg_file_path;
    gchar *dg_uri;
 };

static void hildon_response_up_button_clicked(GtkWidget *widget,
					      gpointer   data);

static void hildon_response_path_button_clicked(GtkWidget *widget,
					        gpointer   data);

static void hildon_file_chooser_dialog_iface_init(GtkFileChooserIface *
                                                  iface);
static GObject *
hildon_file_chooser_dialog_constructor(GType type,
                                       guint n_construct_properties,
                                       GObjectConstructParam *
                                       construct_properties);

G_DEFINE_TYPE_EXTENDED
    (HildonFileChooserDialog, hildon_file_chooser_dialog,
     GTK_TYPE_DIALOG, 0,
     G_IMPLEMENT_INTERFACE(GTK_TYPE_FILE_CHOOSER,
                           hildon_file_chooser_dialog_iface_init))

/* <glib>

    Borrowed from glib, because it's static there.
    I do not know if there is a better way to do
    this. conversion. */
static int
unescape_character (const char *scanner)
{
  int first_digit;
  int second_digit;

  first_digit = g_ascii_xdigit_value (scanner[0]);
  if (first_digit < 0)
    return -1;

  second_digit = g_ascii_xdigit_value (scanner[1]);
  if (second_digit < 0)
    return -1;

  return (first_digit << 4) | second_digit;
}

static void chooser_entry_invalid_input_cb (GtkEntry *entry,
                                            GtkInvalidInputType inv_type,
                                            gpointer user_data)
{
  if (inv_type == GTK_INVALID_INPUT_MAX_CHARS_REACHED) {
    /* show the infoprint "ckdg_ib_maximum_characters_reached" */
    hildon_banner_show_information (GTK_WIDGET (entry), NULL,
                                    HCS("ckdg_ib_maximum_characters_reached"));
  }
}

static gchar *
g_unescape_uri_string (const char *escaped,
                       int         len,
                       const char *illegal_escaped_characters,
                       gboolean    ascii_must_not_be_escaped)
{
  const gchar *in, *in_end;
  gchar *out, *result;
  int c;

  if (escaped == NULL)
    return NULL;

  if (len < 0)
    len = strlen (escaped);

  result = g_malloc (len + 1);

  out = result;
  for (in = escaped, in_end = escaped + len; in < in_end; in++)
    {
      c = *in;

      if (c == '%')
        {
          /* catch partial escape sequences past the end of the substring */
          if (in + 3 > in_end)
            break;

          c = unescape_character (in + 1);

          /* catch bad escape sequences and NUL characters */
          if (c <= 0)
            break;

          /* catch escaped ASCII */
          if (ascii_must_not_be_escaped && c <= 0x7F)
            break;

          /* catch other illegal escaped characters */
          if (strchr (illegal_escaped_characters, c) != NULL)
            break;

          in += 2;
        }

      *out++ = c;
    }

  g_assert (out - result <= len);
  *out = '\0';

  if (in != in_end)
    {
      g_free (result);
      return NULL;
    }

  return result;
}

/* </glib> */

static gint
get_path_length_from_uri(const char *uri)
{
  const char *delim;
  char *unescaped;
  gint len;

  if (!uri) return 0;

  /* Skip protocol and hostname */
  delim = strstr(uri, "://");
  if (!delim) return 0;
  delim = strchr(delim + 3, '/');
  if (!delim) return 0;

  unescaped = g_unescape_uri_string (delim, -1, "/", FALSE);
  if (!unescaped) return 0;

  g_debug("Original uri = %s", uri);
  len = strlen (unescaped);
  g_debug("Unescaped path = %s, length = %d", unescaped, len);
  g_free(unescaped);

  return len;
}

static gint
get_global_pane_position ()
{
  GError *error = NULL;
  GKeyFile *keys;
  gint pos;

  keys = hildon_file_system_open_user_settings ();
  pos = g_key_file_get_integer (keys, "default", "pane_position", &error);
  if (error)
    {
      pos = 250;
      if (!g_error_matches (error, G_KEY_FILE_ERROR,
			    G_KEY_FILE_ERROR_KEY_NOT_FOUND)
	  && !g_error_matches (error, G_KEY_FILE_ERROR,
			       G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
	g_debug ("%s\n", error->message);
      g_error_free (error);
    }

  g_key_file_free (keys);
  return pos;
}

static void
set_global_pane_position (gint pos)
{
  GKeyFile *keys = hildon_file_system_open_user_settings ();
  g_key_file_set_integer (keys, "default", "pane_position", pos);
  hildon_file_system_write_user_settings (keys);
  g_key_file_free (keys);
}

static void
hildon_file_chooser_dialog_set_limit(HildonFileChooserDialog *self)
{
  /* The full pathname is limited to MAX_FULL_PATH_LENGTH characters,
     thus we could try to be smart here and limit the length of the
     input field to MAX_FULL_PATH_LENGTH minus the length of the
     current folder pathname.  That, however, leads to quite wierd
     behavior when we get close to the limit, or even beyond it,
     especially when the user changes the current folder.  Instead, we
     allow arbitrary entry lengths and validate the full pathname
     length when the user hits "Ok".
  */

  gtk_entry_set_max_length (GTK_ENTRY (self->priv->entry_name),
                            self->priv->max_filename_length);
}

static void file_activated_handler(GtkWidget * widget, gpointer data)
{
    g_assert(HILDON_IS_FILE_CHOOSER_DIALOG(data));
    HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(data)->priv;

    if(priv->action == GTK_FILE_CHOOSER_ACTION_OPEN) {
          gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_OK);
    }
 
}

static void folder_activated_handler(GtkWidget * widget, gpointer data)
{
    g_assert(HILDON_IS_FILE_CHOOSER_DIALOG(data));
    HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(data)->priv;

    switch (priv->action) {
       case GTK_FILE_CHOOSER_ACTION_OPEN:
       case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	 hildon_file_chooser_update_path_button(HILDON_FILE_CHOOSER_DIALOG(data));
	 break;
       default:
	 g_debug("wring dialog for the folder activated handler\n");
	 break;
    }
}

static void
hildon_file_chooser_dialog_select_text(HildonFileChooserDialogPrivate *priv)
{
  if (GTK_WIDGET_DRAWABLE(priv->entry_name)) {
    gtk_widget_grab_focus(priv->entry_name);
    gtk_editable_select_region(GTK_EDITABLE(priv->entry_name), 0, -1);
  }
}

static gboolean
hildon_file_chooser_dialog_save_multiple_set(HildonFileChooserDialogPrivate *priv)
{
  const char *text = gtk_label_get_text(GTK_LABEL(priv->multiple_label));
  return text && strlen(text) > 0;
}

/* 
   Sets content to name entry. 
   Returns TRUE if content has changed, FALSE if not 
*/ 
static gboolean
set_entry (GtkWidget *entry, const char *name, const char *ext)
{
  if (name == NULL)
    return FALSE;

  gchar *orig_name = g_strdup (hildon_entry_get_text (HILDON_ENTRY (entry)));
  const gchar *new_name;
  gboolean retval;

  g_debug ("SET ENTRY '%s' '%s'\n", name, ext);

  hildon_entry_set_text (HILDON_ENTRY(entry), name);

  /* Include the extension in the name when it is not recognized.  EXT
     always includes the starting '.'.
   */

  if (ext && !_hildon_file_system_is_known_extension (ext))
    {
      gint position = strlen (name);
      gtk_editable_insert_text (GTK_EDITABLE (entry),
                                ext, strlen (ext),
                                &position);
    }

  new_name = gtk_entry_get_text (GTK_ENTRY (entry));

  if (strcmp (new_name, orig_name) == 0)
	retval = FALSE;
  else
    retval = TRUE;

  g_free (orig_name);
  return retval;

}

static gchar *
get_entry (GtkWidget *entry, const char *ext)
{
  gchar *name;
  g_object_get (entry, "text", &name, NULL);
  g_strstrip(name);

  /* If the original extension was not recognized, then it was offered
     to the user for editing, and whatever we have in the entry now is
     the full filename.  It the extension was recognized, we add it
     back now.
  */

  if (ext && _hildon_file_system_is_known_extension (ext))
    {
       gchar *ext_name = g_strconcat (name, ext, NULL);
       g_free (name);
       name = ext_name;
    }

  return name;
}


static void
hildon_file_chooser_dialog_do_autonaming(HildonFileChooserDialogPrivate *
                                         priv)
{
    gboolean changed;

    g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));

    if (GTK_WIDGET_VISIBLE(priv->entry_name) &&
        priv->stub_name && priv->stub_name[0] && !priv->edited)
    {
        gchar *name = NULL;

        g_signal_handler_block(priv->entry_name, priv->changed_handler);
        if (priv->autonaming_enabled) {
            GtkTreeIter iter;

            g_debug ("Trying [%s] [%s]\n", priv->stub_name, priv->ext_name);
            if (hildon_file_selection_get_current_folder_iter
                (priv->filetree, &iter)) {
                name =
                    hildon_file_system_model_new_item(priv->model, &iter,
                                                      priv->stub_name,
                                                      priv->ext_name);
                g_debug ("Got [%s]\n", name);
            }
        }

        if (name)
        {
          changed = set_entry (priv->entry_name, name, priv->ext_name);
          g_free(name);
		}
        else
          changed = set_entry (priv->entry_name, priv->stub_name, priv->ext_name);

        g_signal_handler_unblock(priv->entry_name, priv->changed_handler);
    }
    else
      changed = set_entry (priv->entry_name, priv->stub_name, priv->ext_name);

	if (changed) {
    	gtk_editable_select_region(GTK_EDITABLE(priv->entry_name), 0, -1);
		if (!gtk_widget_is_focus(GTK_WIDGET (priv->entry_name))) {
			gtk_widget_grab_focus (GTK_WIDGET (priv->entry_name));
		}
	}

    return;
}

/* Set PRIV->stub_name and PRIV->ext_name from NAME so that stub_name
   contains everything before any potential autonaming token and
   ext_name everything after it.

   Concretely, this means that ext_name gets the extension of NAME,
   including unrecognized ones.  SET_ENTRY and GET_ENTRY make sure
   that the user can edit unrecognized extensions.
*/
static void
set_stub_and_ext (HildonFileChooserDialogPrivate *priv,
                  const char *name)
{
  char *dot;
  gboolean is_folder;

  g_debug ("SET STUB AND EXT %s\n", name);

  g_free (priv->stub_name);
  g_free (priv->ext_name);
  priv->stub_name = g_strdup (name);
  priv->ext_name = NULL;

  /* XXX - Determine whether we are talking about a folder here.  If
           action is CREATE_FOLDER, the dialog might actually be used
           for the "Rename <object>" dialog.  We distinguish between
           these two cases by looking at the "show-location" property,
           which is false for Rename dialogs.  But a Rename dialog
           might still be used for a folder, of course, so we really
           have to ask the filesystem.

           The following is about the right amount of code one should
           have to write for figuring out whether a file is a
           directory, I'd say.
  */
  if (priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
    {
      if (priv->should_show_location)
        {
          /* A "Create Folder" dialog.
           */
          is_folder = TRUE;
        }
      else
        {
          /* A "Rename" dialog.  Let's see whether we are renaming a
             folder.
          */

          is_folder = FALSE;
        }
    }
  else
    {
      /* Other actions are never about folders.
       */
      is_folder = FALSE;
    }

  // TODO this should change, we should know:
  // * what the user typed in the entry box
  // * what the system proposed for the entry box (autonamed files parts)
  // * what the system extension is
  // Mixing all that together in the text field loses information, leading to corner cases

  dot = _hildon_file_system_search_extension (priv->stub_name, FALSE, is_folder);
  if (dot) {
      /* if there is a dot and the extension is not the whole name, or the extension is known, separate them */
      if (_hildon_file_system_is_known_extension (dot) || dot != priv->stub_name) {
            g_free(priv->ext_name);
            priv->ext_name = g_strdup(dot);
            *dot = '\0';
	    sync_extensions_combo (priv);
      }
  }

  hildon_file_chooser_dialog_do_autonaming (priv);
}

static void
hildon_file_chooser_dialog_finished_loading(GObject *sender, GtkTreeIter *iter, gpointer data)
{
  GtkTreeIter current_iter;
  HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(data)->priv;

  if (hildon_file_selection_get_current_folder_iter(priv->filetree, &current_iter) &&
      iter->user_data == current_iter.user_data)
  {
    hildon_file_chooser_dialog_do_autonaming(priv);
  }
}

static void
hildon_file_chooser_dialog_update_location_info
(HildonFileChooserDialogPrivate * priv)
{
    GtkTreeIter iter;

    if (hildon_file_selection_get_current_folder_iter
        (priv->filetree, &iter)) {
        GdkPixbuf *icon;
	gchar *location_value;

        gtk_tree_model_get(GTK_TREE_MODEL(priv->model), &iter,
			   HILDON_FILE_SYSTEM_MODEL_COLUMN_THUMBNAIL, &icon,
                           HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME,
                           &location_value, -1);
	gtk_label_set_text(GTK_LABEL(priv->title_location), location_value); //non-changeable location_value
	hildon_button_set_value(HILDON_BUTTON(priv->location_button), location_value);
 
        if (icon) /* It's possible that we don't get an icon */
        {
          gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_location), icon);
          g_object_unref(icon);
        }

	g_free(location_value);
    } else
        g_warning("Failed to get current folder iter");
}

static void
hildon_file_chooser_dialog_selection_changed(HildonFileChooserDialogPrivate
    *priv)
{
    if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    {
        GSList *files = _hildon_file_selection_get_selected_files(
                                      HILDON_FILE_SELECTION(priv->filetree));

        gtk_file_paths_free(files);
    }
}

static void hildon_file_chooser_dialog_current_folder_changed(GtkWidget *
  widget, gpointer data)
{
  HildonFileChooserDialogPrivate *priv = data;

  hildon_file_chooser_dialog_selection_changed(priv);

  if (GTK_WIDGET_VISIBLE(priv->eventbox_location))
     hildon_file_chooser_dialog_update_location_info(priv);
  if (GTK_WIDGET_VISIBLE(priv->entry_name))
     hildon_file_chooser_dialog_do_autonaming(priv);
}

/* If a row changes in the model, we check if the location label
   should be updated */

static void
check_for_location_update (GtkTreeModel *model,
			   GtkTreePath *path,
			   GtkTreeIter *iter,
			   gpointer data)
{
  HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(data)->priv;
  GtkTreeIter current_iter;

  if (!priv || !path || !model)
    return;
  
  if (priv->filetree && HILDON_IS_FILE_SELECTION (priv->filetree)) 
    {
      GtkTreePath *current_path;

      hildon_file_selection_get_current_folder_iter (priv->filetree,
						     &current_iter);
      current_path = gtk_tree_model_get_path (model, &current_iter);
      if (current_path)
        {
          if (gtk_tree_path_compare (path, current_path) == 0)
              hildon_file_chooser_dialog_update_location_info(priv);
          gtk_tree_path_free (current_path);
        }
    }
}

#if GTK_CHECK_VERSION (2, 14, 0)
static gboolean
hildon_file_chooser_dialog_set_current_folder(GtkFileChooser  *chooser,
                                              GFile           *file,
                                              GError         **error)
{
    char *name;
    HildonFileChooserDialog *self;
    gchar *uri;
    gboolean result;

    self = HILDON_FILE_CHOOSER_DIALOG(chooser);
    uri = g_file_get_uri(file);
    result = hildon_file_selection_set_current_folder_uri
      (self->priv->filetree, uri, error);
    g_free(uri);
    hildon_file_chooser_dialog_set_limit(self);

    /* Now resplit the name into stub and ext parts since now the
       situation might have changed as to whether it is a folder or
       not.  Only do this with a non-empty stub, tho.
    */
    if (self->priv->stub_name && self->priv->stub_name[0])
      {
	if (self->priv->ext_name)
	  name = g_strconcat (self->priv->stub_name,
			      self->priv->ext_name, NULL);
	else
	  name = g_strdup (self->priv->stub_name);
	
	set_stub_and_ext (self->priv, name);
	g_free (name);
      }

    return result;
}
#else
static gboolean
hildon_file_chooser_dialog_set_current_folder(GtkFileChooser * chooser,
                                              const GtkFilePath * path,
                                              GError ** error)
{
    char *name;
    HildonFileChooserDialog *self;
    gboolean result;

    self = HILDON_FILE_CHOOSER_DIALOG(chooser);

    result = _hildon_file_selection_set_current_folder_path
      (self->priv->filetree, path, error);
    hildon_file_chooser_dialog_set_limit(self);

    /* Now resplit the name into stub and ext parts since now the
       situation might have changed as to whether it is a folder or
       not.  Only do this with a non-empty stub, tho.
    */
    if (self->priv->stub_name && self->priv->stub_name[0])
      {
	if (self->priv->ext_name)
	  name = g_strconcat (self->priv->stub_name,
			      self->priv->ext_name, NULL);
	else
	  name = g_strdup (self->priv->stub_name);
	
	set_stub_and_ext (self->priv, name);
	g_free (name);
      }

    return result;
}
#endif

#if GTK_CHECK_VERSION (2, 14, 0)
static GFile *
hildon_file_chooser_dialog_get_current_folder(GtkFileChooser * chooser)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    gchar *uri = hildon_file_selection_get_current_folder_uri (priv->filetree);
    GFile *file = g_file_new_for_uri(uri);
    g_free(uri);
    return file;
}
#else
static GtkFilePath
    *hildon_file_chooser_dialog_get_current_folder(GtkFileChooser *
                                                   chooser)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    return _hildon_file_selection_get_current_folder_path (priv->filetree);
}
#endif

/* Sets current name as if entered by user */
static void hildon_file_chooser_dialog_set_current_name(GtkFileChooser *
                                                        chooser,
                                                        const gchar * name)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;

    set_stub_and_ext (priv, name);

    /* If we have autonaming enabled, we try to remove possible
       autonumber from stub part. We do not want to do this
       always (saving existing file would be difficult otherwise) */
    if (priv->autonaming_enabled)
      _hildon_file_system_remove_autonumber(priv->stub_name);

    g_debug("Current name set: body = %s, ext = %s", priv->stub_name, priv->ext_name);
    hildon_file_chooser_dialog_set_limit(HILDON_FILE_CHOOSER_DIALOG(chooser));
    hildon_file_chooser_dialog_do_autonaming(priv);
}

#if GTK_CHECK_VERSION (2, 14, 0)
static gboolean hildon_file_chooser_dialog_select_file(GtkFileChooser *
                                                       chooser,
                                                       GFile * file,
                                                       GError ** error)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    gchar *uri = g_file_get_uri(file);

    if (hildon_file_selection_select_uri(priv->filetree, uri, error))
      {
        g_free(uri);
        if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
            || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
        {
          gchar* file_basename = g_file_get_basename (file);
          set_stub_and_ext (priv, file_basename);
          g_free(file_basename);
        }
        return TRUE;
      }

    g_free(uri);
    return FALSE;
}
#else
static gboolean hildon_file_chooser_dialog_select_path(GtkFileChooser *
                                                       chooser,
                                                       const GtkFilePath *
                                                       path,
                                                       GError ** error)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;

    if (_hildon_file_selection_select_path(priv->filetree, path, error))
      {
        if (priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
            || priv->action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
          set_stub_and_ext (priv,
                            basename (gtk_file_path_get_string (path)));
        return TRUE;
      }

    return FALSE;
}
#endif

#if GTK_CHECK_VERSION (2, 14, 0)
static void hildon_file_chooser_dialog_unselect_file(GtkFileChooser *chooser,
                                                     GFile          *file)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    gchar *uri = g_file_get_uri(file);
    hildon_file_selection_unselect_uri(priv->filetree, uri);
    g_free(uri);
}
#else
static void hildon_file_chooser_dialog_unselect_path(GtkFileChooser *
                                                     chooser,
                                                     const GtkFilePath *
                                                     path)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    _hildon_file_selection_unselect_path(priv->filetree, path);
}
#endif

static void hildon_file_chooser_dialog_select_all(GtkFileChooser * chooser)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    hildon_file_selection_select_all(priv->filetree);
}

static void hildon_file_chooser_dialog_unselect_all(GtkFileChooser *
                                                    chooser)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    hildon_file_selection_unselect_all(priv->filetree);
}

#if GTK_CHECK_VERSION (2, 14, 0)
static GSList *hildon_file_chooser_dialog_get_files(GtkFileChooser *
                                                    chooser)
{
    GtkFilePath *file_path, *base_path;
    GtkFileSystem *backend;
    gchar *name, *name_without_dot_prefix;
    GFile *file;

    /* If we are asking a name from user, return it. Otherwise return
       selection */
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;

    if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
        hildon_file_chooser_dialog_save_multiple_set(priv))
    {
        gchar *uri = hildon_file_selection_get_current_folder_uri(priv->filetree);
        file = g_file_new_for_uri (uri);
        g_free(uri);
        return g_slist_append(NULL, file);
    }

    if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    {
        GSList *files = _hildon_file_selection_get_selected_files(priv->filetree);
	GFile *file = g_object_ref(g_slist_nth_data(files, 0));

	g_slist_foreach (files, (GFunc) g_object_unref, NULL);
        g_slist_free(files);
        return g_slist_append(NULL, file);
    }

    name = get_entry (priv->entry_name, priv->ext_name);

    name_without_dot_prefix = name;
    while (*name_without_dot_prefix == '.')
      name_without_dot_prefix++;

    g_debug("Inputted name: [%s]", name_without_dot_prefix);

    backend = _hildon_file_system_model_get_file_system(priv->model);
    base_path =
            _hildon_file_selection_get_current_folder_path (priv->filetree);
    file_path =
            gtk_file_system_make_path(backend, base_path,
            name_without_dot_prefix, NULL);

    gtk_file_path_free(base_path);
    g_free(name);

    file = g_file_new_for_commandline_arg (gtk_file_path_get_string(file_path));
    g_free(file_path);
    return g_slist_append(NULL, file);
}
#else
static GSList *hildon_file_chooser_dialog_get_paths(GtkFileChooser *
                                                    chooser)
{
    GtkFilePath *file_path, *base_path;
    GtkFileSystem *backend;
    gchar *name, *name_without_dot_prefix;

    /* If we are asking a name from user, return it. Otherwise return
       selection */
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;

    if (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
        hildon_file_chooser_dialog_save_multiple_set(priv))
        return g_slist_append(NULL,
                              _hildon_file_selection_get_current_folder_path
                              (priv->filetree));

    if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
        return _hildon_file_selection_get_selected_files(priv->filetree);

    name = get_entry (priv->entry_name, priv->ext_name);

    name_without_dot_prefix = name;
    while (*name_without_dot_prefix == '.')
      name_without_dot_prefix++;

    g_debug("Inputted name: [%s]", name_without_dot_prefix);

    backend = _hildon_file_system_model_get_file_system(priv->model);
    base_path =
            _hildon_file_selection_get_current_folder_path (priv->filetree);
    file_path =
            gtk_file_system_make_path(backend, base_path,
            name_without_dot_prefix, NULL);

    gtk_file_path_free(base_path);
    g_free(name);

    return g_slist_append(NULL, file_path);
}
#endif

#if GTK_CHECK_VERSION (2, 14, 0)
static GFile*
hildon_file_chooser_dialog_get_preview_file(GtkFileChooser * chooser)
#else
static GtkFilePath
    *hildon_file_chooser_dialog_get_preview_path(GtkFileChooser * chooser)
#endif
{
    g_warning("HildonFileChooserDialog doesn\'t implement preview");
    return NULL;
}

static GtkFileSystem
    *hildon_file_chooser_dialog_get_file_system(GtkFileChooser * chooser)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    return _hildon_file_system_model_get_file_system(priv->model);
}

static void hildon_file_chooser_toggle_filter (GtkCheckMenuItem * item,
                                               gpointer data)
{
  HildonFileChooserDialog *chooser = HILDON_FILE_CHOOSER_DIALOG(data);
  HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
  gint filter_index = 0;

  filter_index = g_slist_index(priv->filter_menu_items, item);
  hildon_file_selection_set_filter(priv->filetree, g_slist_nth_data(priv->filters, filter_index));

}

static void hildon_file_chooser_dialog_add_filter(GtkFileChooser * chooser,
                                                  GtkFileFilter * filter)
{
    GtkWidget * menu_item = NULL;
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    gulong * signal_handler = NULL;

    if (g_slist_find(priv->filters, filter)) {
        g_warning("gtk_file_chooser_add_filter() called on filter "
                  "already in list");
        return;
    }

    g_object_ref(filter);
    g_object_ref_sink (GTK_OBJECT(filter));
    priv->filters = g_slist_append(priv->filters, filter);
    if (priv->filters_separator == NULL) {
      priv->filters_separator = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup), priv->filters_separator);
    }
    if (gtk_file_filter_get_name(filter) != NULL) {
      GSList * node = NULL;
      node = priv->filter_menu_items;
      while (node) {
        if (node->data) {
          break;
        } else {
          node = g_slist_next(node);
        }
      }
      if (node) {
        menu_item = gtk_radio_menu_item_new_with_label_from_widget (node->data, gtk_file_filter_get_name(filter));
      } else {
        menu_item = gtk_radio_menu_item_new_with_label (NULL, gtk_file_filter_get_name(filter));
      }
      gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup), menu_item);
      if (hildon_file_selection_get_filter(priv->filetree) == filter) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
      }
      signal_handler = g_malloc0(sizeof(gulong));
      *signal_handler =
        g_signal_connect(menu_item, "toggled", G_CALLBACK(hildon_file_chooser_toggle_filter), chooser);
      gtk_widget_show_all(priv->popup);
    }
    priv->filter_menu_items = g_slist_append(priv->filter_menu_items, menu_item);
    priv->filter_item_menu_toggle_handlers = g_slist_append(priv->filter_item_menu_toggle_handlers, signal_handler);
}

static void hildon_file_chooser_dialog_remove_filter(GtkFileChooser *
                                                     chooser,
                                                     GtkFileFilter *
                                                     filter)
{
    gint filter_index;
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    GSList * menu_item_node = NULL;
    GSList * signal_handlers_node = NULL;
    filter_index = g_slist_index(priv->filters, filter);

    menu_item_node = g_slist_nth(priv->filter_menu_items, filter_index);
    if (menu_item_node->data != NULL)
      gtk_container_remove(GTK_CONTAINER(priv->popup), menu_item_node->data);
    priv->filter_menu_items = g_slist_delete_link(priv->filter_menu_items, menu_item_node);

    signal_handlers_node = g_slist_nth(priv->filter_item_menu_toggle_handlers, filter_index);
    g_free(signal_handlers_node->data);
    priv->filter_item_menu_toggle_handlers = g_slist_delete_link(priv->filter_item_menu_toggle_handlers, signal_handlers_node);

    if (filter_index < 0) {
        g_warning("gtk_file_chooser_remove_filter() called on filter "
                  "not in list");
        return;
    }

    priv->filters = g_slist_remove(priv->filters, filter);

    if (filter == hildon_file_selection_get_filter(priv->filetree))
        hildon_file_selection_set_filter(priv->filetree, NULL);

    if (priv->filters == NULL) {
      gtk_container_remove(GTK_CONTAINER(priv->popup), priv->filters_separator);
      priv->filters_separator = NULL;
    }

    g_object_unref(filter);
    gtk_widget_show_all(priv->popup);
}

static GSList *hildon_file_chooser_dialog_list_filters(GtkFileChooser *
                                                       chooser)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(chooser)->priv;
    return g_slist_copy(priv->filters);
}

static gboolean
hildon_file_chooser_dialog_add_remove_shortcut_folder(GtkFileChooser *chooser,
#if GTK_CHECK_VERSION (2, 14, 0)
                                                      GFile          *file,
#else
                                                      const GtkFilePath *path,
#endif
                                                      GError         **error)
{
    g_warning("HildonFileChooserDialog doesn\'t implement shortcuts");
    return FALSE;
}

static GSList
    *hildon_file_chooser_dialog_list_shortcut_folders(GtkFileChooser *
                                                      chooser)
{
    g_warning("HildonFileChooserDialog doesn\'t implement shortcuts");
    return NULL;
}

static void update_folder_button_visibility(HildonFileChooserDialogPrivate
                                            * priv)
{
    if (priv->should_show_folder_button
        && (priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER/*GTK_FILE_CHOOSER_ACTION_SAVE*/
            || priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER))
      {
        gtk_widget_show(priv->folder_button);
	g_object_set (priv->eventbox_location, "can-focus", TRUE, NULL);
      }
    else
      {
        gtk_widget_hide(priv->folder_button);
	g_object_set (priv->eventbox_location, "can-focus", FALSE, NULL);
      }
    switch (priv->action){
        case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	  gtk_widget_hide(priv->action_button);
	  break;
    case GTK_FILE_CHOOSER_ACTION_OPEN:
          gtk_widget_hide(priv->action_button);
	  gtk_widget_hide(priv->action_button);
          break;
	  /*currently, it has no changes to the UI
	    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
	    
	    break;*/
        default:
	  break;
    }
}

static void update_location_visibility(HildonFileChooserDialogPrivate *
                                       priv)
{
  if (priv->should_show_location){
      hildon_file_chooser_dialog_update_location_info(priv);
      gtk_widget_show_all(priv->eventbox_location);
      switch (priv->action) {
         case GTK_FILE_CHOOSER_ACTION_SAVE:
	   gtk_widget_hide(priv->title_location);
	   gtk_widget_hide(priv->image_location);
	   break;
         case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
	   gtk_widget_hide(priv->location_button);
	   break;
         default:
	   gtk_widget_hide(priv->eventbox_location);
	   break;
      }
  }
  else
      gtk_widget_hide(priv->eventbox_location);
}

/* We build ui for current action */
static void build_ui(HildonFileChooserDialog * self)
{
    HildonFileChooserDialogPrivate *priv = self->priv;
    const gchar *title = gtk_window_get_title(GTK_WINDOW(self));

    switch (priv->action) {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
	gtk_widget_hide(priv->entry_name);
        gtk_widget_hide(priv->hbox_items);
        gtk_widget_set_size_request(GTK_WIDGET(priv->filetree),
                                    FILE_SELECTION_WIDTH_TOTAL,
                                    FILE_SELECTION_HEIGHT);
	hildon_file_chooser_dialog_reset_files_visibility(priv,
							  GTK_FILE_CHOOSER_ACTION_OPEN);
        gtk_widget_show(GTK_WIDGET(priv->filetree));
	hildon_file_selection_hide_navigation_pane(priv->filetree);
        hildon_file_selection_show_content_pane(priv->filetree);
	gtk_widget_show_all(priv->hbox_address);
	gtk_widget_hide(priv->path_button);
	gtk_widget_show(priv->path_label);
        if (!(title && *title))
          gtk_window_set_title(GTK_WINDOW(self), _("ckdg_ti_open_file"));
        gtk_button_set_label(GTK_BUTTON(priv->action_button),
                             _("ckdg_bd_select_object_ok_open"));
        break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
        if (hildon_file_chooser_dialog_save_multiple_set(priv))
        {
	  gtk_widget_hide(priv->entry_name);
          gtk_widget_show_all(priv->hbox_items);
        }
        else
        {
	  gtk_widget_show_all(priv->entry_name);
	  gtk_widget_hide(priv->hbox_items);
        }

	hildon_button_set_title (HILDON_BUTTON (priv->location_button),
 				_("sfil_fi_save_objects_location"));

        gtk_widget_hide(GTK_WIDGET(priv->filetree));

        /* Content pane of the filetree widget needs to be realized.
           Otherwise automatic location change etc don't work correctly.
           This is because "rows-changed" handler in GtkTreeView exits
           immediately if treeview is not realized. */
        _hildon_file_selection_realize_help(priv->filetree);
        if (!(title && *title))
          gtk_window_set_title(GTK_WINDOW(self), _("sfil_ti_save_file"));
        gtk_button_set_label(GTK_BUTTON(priv->action_button),
                             _("ckdg_bd_save_object_dialog_ok"));
        gtk_button_set_label(GTK_BUTTON(priv->folder_button),
	                     _("sfil_bd_save_object_dialog_change_folder"));
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(self),
                                          _("ckdg_va_save_object_name_stub_default"));
        break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
        gtk_widget_hide(priv->entry_name);

        gtk_widget_hide(priv->hbox_items);

	gtk_widget_set_size_request(GTK_WIDGET(priv->filetree),
                                    FILE_SELECTION_WIDTH_TOTAL,
				    FILE_SELECTION_HEIGHT);
	gtk_widget_show(GTK_WIDGET(priv->filetree));

	gtk_widget_show_all(priv->hbox_address);
	gtk_widget_hide(priv->path_label);
	hildon_file_chooser_dialog_reset_files_visibility(priv,
							  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	hildon_file_selection_show_content_pane(priv->filetree);
	hildon_file_selection_hide_navigation_pane(priv->filetree);

        if (!(title && *title))
          gtk_window_set_title(GTK_WINDOW(self), _("ckdg_ti_change_folder"));
        gtk_button_set_label(GTK_BUTTON(priv->action_button),
                             _("ckdg_bd_change_folder_ok"));
        gtk_button_set_label(GTK_BUTTON(priv->folder_button),
                             _("ckdg_bd_change_folder_new_folder"));
        hildon_helper_set_insensitive_message (priv->action_button,
                                               _("sfil_ib_select_file"));
        break;
    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
        hildon_button_set_title (HILDON_BUTTON (priv->location_button),
 	   	                       	       _("ckdg_fi_new_folder_location"));
	gtk_widget_show_all(priv->entry_name);

        gtk_widget_hide(GTK_WIDGET(priv->filetree));
        _hildon_file_selection_realize_help(priv->filetree);
        gtk_widget_hide(priv->hbox_items);
        if (!(title && *title))
          gtk_window_set_title(GTK_WINDOW(self), dgettext("hildon-libs",
                             "ckdg_ti_new_folder"));
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(self),
                                          _("ckdg_va_new_folder_name_stub"));
        gtk_button_set_label(GTK_BUTTON(priv->action_button),
                             _("ckdg_bd_new_folder_dialog_ok"));
        break;
    default:
        g_assert_not_reached();
    };
    /* according the spec of Fremantle File management, 
       this function is changed */
    update_folder_button_visibility(priv);
    update_location_visibility(priv);
}

/*
  Small help utilities for response_handler
*/
static GString *check_illegal_characters(const gchar * name)
{
    GString *illegals = NULL;

    while (*name) {
        gunichar unichar;

        unichar = g_utf8_get_char(name);

        if (g_utf8_strchr ("\\/:*?\"<>|", -1, unichar))
          {
            if (!illegals)
                illegals = g_string_new(NULL);
            if (g_utf8_strchr (illegals->str, -1, unichar) == NULL)
              g_string_append_unichar (illegals, unichar);
          }

        name = g_utf8_next_char(name);
    }

    return illegals;
}

/* Sets current directory of the target to be the same as current
   directory of source */
static void sync_current_folders(HildonFileChooserDialog * source,
                                 HildonFileChooserDialog * target)
{
    HildonFileSelection *fs;
    gchar *uri;

    fs = HILDON_FILE_SELECTION(source->priv->filetree);
    /*if (hildon_file_selection_get_active_pane(fs) ==
        HILDON_FILE_SELECTION_PANE_CONTENT)
      uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(source));
    else
    uri = NULL;*/
    /* no need to check the pane in FREMANTLE */
    uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER(source));
    if (!uri || g_ascii_strcasecmp(uri, "files:///") == 0) {
      /* re-locate the uri to an existing node because of the 
	 hacked root in FREMANTLE
       */
       /* uri = gtk_file_chooser_get_current_folder_uri(GTK_FILE_CHOOSER
	 (source)); */
      if(!uri)
	g_free(uri);
      uri = g_strconcat ("file://", g_getenv("MYDOCSDIR"), NULL);
    }
    gtk_file_chooser_set_current_folder_uri(GTK_FILE_CHOOSER(target), uri);
    g_free(uri);
}

/* Used to pop dialogs when folder button is pressed */
static GtkWidget
    *hildon_file_chooser_dialog_create_sub_dialog(HildonFileChooserDialog *
                                                  self,
                                                  GtkFileChooserAction
                                                  action)
{
    GtkWidget *dialog;
    HildonFileChooserDialogPrivate *priv;

    gboolean local_only = FALSE;
    gboolean show_hidden = FALSE;
    gboolean show_upnp = FALSE;
  
    g_object_get (self->priv->filetree, "local-only", &local_only, NULL);
    g_object_get (self->priv->filetree, "show-hidden", &show_hidden, NULL);
    g_object_get (self->priv->filetree, "show-upnp", &show_upnp, NULL);
        
    dialog = hildon_file_chooser_dialog_new_with_properties
      (GTK_WINDOW(self),
       "action", action,
       "file-system-model",
       self->priv->model, 
       "local-only", local_only,
       "show-hidden", show_hidden,
       "sync-mode", self->priv->sync_mode,
       NULL);
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
    g_assert(HILDON_IS_FILE_CHOOSER_DIALOG(dialog));
    priv = HILDON_FILE_CHOOSER_DIALOG(dialog)->priv;

    g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));
    hildon_file_chooser_dialog_reset_files_visibility(priv,
			  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER + 4);
    sync_current_folders(self, HILDON_FILE_CHOOSER_DIALOG(dialog));

    return dialog;
}

static void create_folder_callback(GCancellable *cancellable,
    const GtkFilePath *path, const GError *error, gpointer data)
{
    HildonFileChooserDialog *self;
    GtkDialog *dialog;
    const gchar *message;

    g_assert(HILDON_IS_FILE_CHOOSER_DIALOG(data));
    self = HILDON_FILE_CHOOSER_DIALOG(data);

    /* There can be still pending cancelled handles
     * from previous operations, just ignore them
     */
    if (self->priv->cancellable != cancellable)
      {
	g_object_unref (cancellable);
	return;
      }

    g_object_unref (cancellable);
    g_object_unref (self->priv->cancellable);
    self->priv->cancellable = NULL;

    dialog = GTK_DIALOG(self);

    if (error) {
	if (g_error_matches(error, GTK_FILE_CHOOSER_ERROR,
			    GTK_FILE_CHOOSER_ERROR_ALREADY_EXISTS))
            message = HCS("ckdg_ib_folder_already_exists");
        else
            message = HCS("sfil_ni_operation_failed");

        hildon_banner_show_information (GTK_WIDGET(self), NULL, message);
        hildon_file_chooser_dialog_select_text(self->priv);
        gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_OK, TRUE);
    } else {
        /* Fake response to close the dialog after folder is created */
        gtk_dialog_response(dialog, HILDON_RESPONSE_FOLDER_CREATED);
    }

    /* This reference was added while setting up the callback */
    g_object_unref(self);
}

static void dialog_response_cb(GtkDialog *dialog,
    gint response_id, gpointer data)
{
    HildonFileChooserDialog *self = HILDON_FILE_CHOOSER_DIALOG(data);
    gboolean edit_entry = FALSE;

    if (self->priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
        if (response_id == GTK_RESPONSE_OK) {
            GtkFileSystem *backend = _hildon_file_system_model_get_file_system(self->priv->model);;
            g_free(self->priv->dg_uri);
            self->priv->dg_uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
            g_debug("About to create folder %s", self->priv->dg_uri);
    
            if (self->priv->dg_file_path)
                gtk_file_path_free(self->priv->dg_file_path);
    
            self->priv->dg_file_path = gtk_file_system_uri_to_path(backend, self->priv->dg_uri);
    
            /* There shouldn't be a way to invoke two simultaneous folder
            creating actions */
    
            HildonFileChooserDialogPrivate *sub_priv =
                    HILDON_FILE_CHOOSER_DIALOG (dialog)->priv;
    
	    if (sub_priv->cancellable)
            {
		g_cancellable_cancel (sub_priv->cancellable);
		g_object_unref (sub_priv->cancellable);
		sub_priv->cancellable = NULL;
            }
    
            /* Callback is quaranteed to be called, it unrefs the object data */
	    sub_priv->cancellable =
            gtk_file_system_create_folder (backend, self->priv->dg_file_path,
                                            create_folder_callback,
                                            g_object_ref(dialog));
    
            /* Make OK button insensitive while folder operation is going */
            gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
    
            gtk_widget_show (GTK_WIDGET (dialog));
            return;
        }
    
        /* If user cancelled the operation, we still can have handle
        */
	if (self->priv->cancellable)
	  g_cancellable_cancel (self->priv->cancellable);
    
        /* If we created a folder, change into it */
        if (response_id == HILDON_RESPONSE_FOLDER_CREATED)
        {
            g_assert(self->priv->dg_file_path != NULL);
            #if GTK_CHECK_VERSION (2, 14, 0)
            GFile *file = g_file_new_for_commandline_arg((gtk_file_path_get_string(self->priv->dg_file_path)));
            if (!hildon_file_chooser_dialog_set_current_folder
            (GTK_FILE_CHOOSER(self), file, NULL) && self->priv->dg_uri) {
                            hildon_file_selection_move_cursor_to_uri (self->priv->filetree, 
                                                                    self->priv->dg_uri);
                    }
            g_object_unref(file);
            #else
            if (!hildon_file_chooser_dialog_set_current_folder
            (GTK_FILE_CHOOSER(self), self->priv->dg_file_path, NULL) && self->priv->dg_uri) {
                            hildon_file_selection_move_cursor_to_uri (self->priv->filetree, 
                                                                    self->priv->dg_uri);
                    }
            #endif
            edit_entry = TRUE;
        }
        gtk_file_path_free(self->priv->dg_file_path);
        self->priv->dg_file_path = NULL;
    } else {
        if (response_id == GTK_RESPONSE_OK) {
            sync_current_folders(HILDON_FILE_CHOOSER_DIALOG(dialog), self);
            edit_entry = TRUE;
        }
    }

    if (edit_entry && self->priv->edited)
    {
        self->priv->edited = FALSE;
        hildon_file_chooser_dialog_set_current_name 
            (GTK_FILE_CHOOSER(self),
            get_entry (self->priv->entry_name, self->priv->ext_name));
    }

    g_free (self->priv->dg_uri);
    self->priv->dg_uri = NULL;
    gtk_widget_destroy (GTK_WIDGET (dialog));
    self->priv->popup_protect = FALSE;
    gtk_window_present(GTK_WINDOW(self));
}

static void handle_folder_popup_sync (HildonFileChooserDialog *self)
{
   GtkFileSystem *backend;
   GtkWidget *dialog;
   gint response;
   gchar *uri = NULL;
   gboolean edit_entry = FALSE;
 
   g_return_if_fail (HILDON_IS_FILE_CHOOSER_DIALOG (self));
 
   /* Prevent race condition that can cause multiple
      subdialogs to be popped up (in a case mainloop
      is run before subdialog blocks additional clicks. */
   if (self->priv->popup_protect)
   {
     g_debug("Blocked multiple subdialogs");
     return;
   }
 
   self->priv->popup_protect = TRUE;
 
   backend = _hildon_file_system_model_get_file_system(self->priv->model);

   if (self->priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
   {
     GtkFilePath *file_path = NULL;
 
     dialog = hildon_file_chooser_dialog_create_sub_dialog(self,
                GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

     while ((response = gtk_dialog_run(GTK_DIALOG(dialog))) == GTK_RESPONSE_OK)
      {
	uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
	g_debug("About to create folder %s", uri);

	if (file_path)
	  gtk_file_path_free(file_path);

	file_path = gtk_file_system_uri_to_path(backend, uri);

	/* There shouldn't be a way to invoke two simultaneous folder
	   creating actions */

	HildonFileChooserDialogPrivate *sub_priv =
		HILDON_FILE_CHOOSER_DIALOG (dialog)->priv;

	if (sub_priv->cancellable)
          {
	    g_cancellable_cancel (sub_priv->cancellable);
	    g_object_unref (sub_priv->cancellable);
	    sub_priv->cancellable = NULL;
	  }

	/* Callback is quaranteed to be called, it unrefs the object data */
	sub_priv->cancellable =
	  gtk_file_system_create_folder (backend, file_path,
					 create_folder_callback,
					 g_object_ref(dialog));

	/* Make OK button insensitive while folder operation is going */
	gtk_dialog_set_response_sensitive (GTK_DIALOG(dialog), GTK_RESPONSE_OK, FALSE);
      }

    /* If user cancelled the operation, we still can have handle
     */
    if (self->priv->cancellable)
      {
	g_cancellable_cancel (self->priv->cancellable);
	g_object_unref (self->priv->cancellable);
	self->priv->cancellable = NULL;
      }

    /* If we created a folder, change into it */
    if (response == HILDON_RESPONSE_FOLDER_CREATED)
      {
        g_assert(file_path != NULL);
        #if GTK_CHECK_VERSION (2, 14, 0)
        GFile *file = g_file_new_for_commandline_arg((gtk_file_path_get_string(file_path)));
        if (!hildon_file_chooser_dialog_set_current_folder
          (GTK_FILE_CHOOSER(self), file, NULL) && uri) {
			hildon_file_selection_move_cursor_to_uri (self->priv->filetree, 
								  uri);
		}
        g_object_unref(file);
        #else
        if (!hildon_file_chooser_dialog_set_current_folder
          (GTK_FILE_CHOOSER(self), file_path, NULL) && uri) {
			hildon_file_selection_move_cursor_to_uri (self->priv->filetree, 
								  uri);
		}
        #endif
	edit_entry = TRUE;
      }
    gtk_file_path_free(file_path);
   }
   else
   {
     dialog = hildon_file_chooser_dialog_create_sub_dialog(self,
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
      {
        sync_current_folders(HILDON_FILE_CHOOSER_DIALOG(dialog), self);
	edit_entry = TRUE;
      }
  }

  if (edit_entry && self->priv->edited)
  {
    self->priv->edited = FALSE;
    hildon_file_chooser_dialog_set_current_name 
      (GTK_FILE_CHOOSER(self),
       get_entry (self->priv->entry_name, self->priv->ext_name));
   }
 
  gtk_widget_destroy(dialog);
  gtk_window_present(GTK_WINDOW(self));
  self->priv->popup_protect = FALSE;
  g_free(uri);
}

static void handle_folder_popup_async (HildonFileChooserDialog *self)
{
  GtkWidget *dialog;

  g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(self));

  /* Prevent race condition that can cause multiple
     subdialogs to be popped up (in a case mainloop
     is run before subdialog blocks additional clicks. */
  if (self->priv->popup_protect)
  {
    g_debug("Blocked multiple subdialogs");
    return;
  }

  self->priv->popup_protect = TRUE;

  if (self->priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
  {
    self->priv->dg_uri = NULL;
    self->priv->dg_file_path = NULL;

    dialog = hildon_file_chooser_dialog_create_sub_dialog(self,
               GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);
  }
  else
  {
    dialog = hildon_file_chooser_dialog_create_sub_dialog(self,
               GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  }

  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), self);
  gtk_widget_show (dialog);
}

static void handle_folder_popup(HildonFileChooserDialog *self)
{
  if (self->priv->sync_mode)
    handle_folder_popup_sync (self);
  else
    handle_folder_popup_async (self);
}

static gboolean
on_location_hw_enter_pressed (GtkWidget * widget, GdkEventKey * event,
			      gpointer data)
{
  HildonFileChooserDialog *self = HILDON_FILE_CHOOSER_DIALOG(data);
  switch (event->keyval) {
  case GDK_Return:
    if (self->priv->should_show_folder_button
	&& (self->priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
            || self->priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER))
      handle_folder_popup(self);
    return TRUE;
  default:
    return FALSE;
  }
}

static void
on_confirmation_note_response (GtkWidget * widget, gint arg1, gpointer data)
{
    HildonFileChooserDialog *dialog = HILDON_FILE_CHOOSER_DIALOG (data);
    if (arg1 == GTK_RESPONSE_OK)
    {
        gtk_dialog_response (GTK_DIALOG (dialog), arg1);
    }
    else
    {
        gtk_widget_destroy (dialog->priv->confirmation_note);
        dialog->priv->confirmation_note = NULL;
    }
}

static void response_handler(GtkWidget * widget, gint arg1, gpointer data)
{
    HildonFileChooserDialog *self;
    HildonFileChooserDialogPrivate *priv;

    self = HILDON_FILE_CHOOSER_DIALOG(widget);
    priv = self->priv;

    switch (arg1) {
    case GTK_RESPONSE_OK:
        if (GTK_WIDGET_VISIBLE(priv->entry_name)) {

            gchar *entry_text = g_strdup(hildon_entry_get_text(HILDON_ENTRY(priv->entry_name)));

            g_strstrip(entry_text);

            if (entry_text[0] == '\0') {
                /* We don't accept empty field */
                g_signal_stop_emission_by_name(widget, "response");
                priv->edited = FALSE;
                hildon_file_chooser_dialog_do_autonaming(priv);
                hildon_file_chooser_dialog_select_text(priv);
                hildon_banner_show_information
                  (widget, NULL, HCS("ckdg_ib_enter_name"));
            } else if (entry_text[0] == '.') {
                /* We don't allow files with a dot as the first character.
                 */
                hildon_file_chooser_dialog_select_text(priv);
                g_signal_stop_emission_by_name(widget, "response");
                hildon_banner_show_information
                  (widget, NULL, _("sfil_ib_invalid_name_dot"));
            } else {
                GString *illegals = check_illegal_characters(entry_text);

                if (illegals) {
                    gchar *msg;

                    hildon_file_chooser_dialog_select_text(priv);
                    g_signal_stop_emission_by_name(widget, "response");

                    msg = g_strdup_printf(
                              HCS("ckdg_ib_illegal_characters_entered"),
                              illegals->str);
                    hildon_banner_show_information (widget, NULL, msg);
                    g_free(msg);
                    g_string_free(illegals, TRUE);
                }
                else if (self->priv->max_full_path_length >= 0)
                     {  /* Let's check that filename is not too long.
                         */
                    gchar *uri;
                    gint path_length;

                    uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(widget));
                    path_length = get_path_length_from_uri(uri);

                    if (path_length > self->priv->max_full_path_length)
                    {
                        g_signal_stop_emission_by_name(widget, "response");
                        hildon_file_chooser_dialog_select_text(priv);
                    	hildon_banner_show_information (widget, NULL,
							dgettext("hildon-common-strings",
								"file_ib_name_too_long"));

                    }
                    if (priv->do_overwrite_confirmation)
                    {
                        GtkFileChooserConfirmation conf;
                        gboolean overwrite = TRUE;

                        conf = GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;

                        g_signal_emit_by_name (widget, "confirm-overwrite", &conf);

                        switch (conf)
                        {
                        case GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM:
                            if (priv->confirmation_note == NULL)
                            {
                                GFile *file = g_file_new_for_uri (uri);

                                if (g_file_query_exists (file, NULL))
                                {
                                    gchar *basename, *label;

                                    basename = g_file_get_basename (file);
                                    label = g_strconcat (_("docm_nc_replace_file"), "\n", basename, NULL);

                                    priv->confirmation_note =
                                        hildon_note_new_confirmation (GTK_WINDOW (widget), label);

                                    g_signal_connect (priv->confirmation_note, "response",
                                                      G_CALLBACK (on_confirmation_note_response), widget);
                                    gtk_widget_show_all (priv->confirmation_note);

                                    overwrite = FALSE;

                                    g_free (label);
                                    g_free (basename);
                                }

                                g_object_unref (file);
                            }
                            else
                            {
                                gtk_widget_destroy (priv->confirmation_note);
                                priv->confirmation_note = NULL;
                            }
                            break;

                        case GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME:
                            break;

                        case GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN:
                            overwrite = FALSE;
                            break;

                        default:
                            g_assert_not_reached ();
                            overwrite = FALSE;
                            break;
                        }

                        if (!overwrite)
                            g_signal_stop_emission_by_name(widget, "response");
                    }
                    g_free(uri);
                }
            }
            g_free(entry_text);
        }
        break;
    case HILDON_RESPONSE_FOLDER_BUTTON:
      g_signal_stop_emission_by_name(widget, "response");
      handle_folder_popup(self);
      break;
    default:
        break;
    }
}

static void hildon_file_chooser_dialog_set_property(GObject * object,
                                                    guint prop_id,
                                                    const GValue * value,
                                                    GParamSpec * pspec)
{
    HildonFileChooserDialog *self;
    HildonFileChooserDialogPrivate *priv;

    self = HILDON_FILE_CHOOSER_DIALOG(object);
    priv = self->priv;

    switch (prop_id) {
    case GTK_FILE_CHOOSER_PROP_ACTION:
        priv->action = g_value_get_enum(value);
        build_ui(self);
        break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
        g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));
        hildon_file_selection_set_select_multiple(priv->filetree,
                                                  g_value_get_boolean
                                                  (value));
        break;
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN: /* Same handler */
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
        g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));
        g_object_set_property(G_OBJECT(priv->filetree), pspec->name, value);
        break;
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
    {
        const gchar *s = g_value_get_string(value);

        /* We should come here once if at all */
        g_assert(priv->model == NULL || s == NULL);

        if (!priv->model)
            priv->model =
                g_object_new(HILDON_TYPE_FILE_SYSTEM_MODEL, "backend", s,
                             "ref-widget", object, NULL);
        break;
    }
    case GTK_FILE_CHOOSER_PROP_FILTER:
        g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));
        hildon_file_selection_set_filter(priv->filetree,
                                         g_value_get_object(value));
        break;
    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
        priv->do_overwrite_confirmation = g_value_get_boolean (value);
        break;
    case PROP_EMPTY_TEXT:
        g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));
        g_object_set_property(G_OBJECT(priv->filetree), pspec->name, value);
        break;
    case PROP_FILE_SYSTEM_MODEL:
        g_assert(priv->model == NULL || g_value_get_object(value) == NULL);
        if (!priv->model) {
            if ((priv->model = g_value_get_object(value)) != NULL)
                g_object_ref(priv->model);
        }
        break;
    case PROP_FOLDER_BUTTON:
        priv->should_show_folder_button = g_value_get_boolean(value);
        update_folder_button_visibility(priv);
        break;
    case PROP_LOCATION:
        priv->should_show_location = g_value_get_boolean(value);
        update_location_visibility(priv);
        break;
    case PROP_AUTONAMING:
        priv->autonaming_enabled = g_value_get_boolean(value);
        break;
    case PROP_OPEN_BUTTON_TEXT:
        g_object_set_property(G_OBJECT(priv->action_button), "label", value);
        break;
    case PROP_MULTIPLE_TEXT:
        g_object_set_property(G_OBJECT(priv->multiple_label), "label", value);
        build_ui(HILDON_FILE_CHOOSER_DIALOG(object));
        break;
    case PROP_MAX_NAME_LENGTH:
    {
        gint new_value = g_value_get_int(value);
        if (new_value != priv->max_filename_length)
        {
          g_debug("Maximum name length is %d characters",
            new_value);
          priv->max_filename_length = new_value;
          hildon_file_chooser_dialog_set_limit(self);
        }
        break;
    }
    case PROP_MAX_FULL_PATH_LENGTH:
    {
      gint new_value;
      const char *filename_len;

      new_value = g_value_get_int(value);

      if (new_value == 0)
      {
        /* Figuring out maximum allowed path length */
        filename_len = g_getenv("MAX_FILENAME_LENGTH");
        if (filename_len)
          new_value = atoi(filename_len);
        if (new_value <= 0)
          new_value = MAX_FILENAME_LENGTH_DEFAULT;
      }

      if (new_value != priv->max_full_path_length)
      {
        g_debug("Maximum full path length is %d characters",
            new_value);
        priv->max_full_path_length = new_value;
        hildon_file_chooser_dialog_set_limit(self);
      }

      break;
    }
    case PROP_SELECTION_MODE:
      g_critical("The \"selection-mode\" property is deprecated, only "
		 "HILDON_FILE_SELECTION_MODE_THUMBNAILS is supported");
      break;
    case PROP_SHOW_FILES:
        priv->show_files = g_value_get_boolean(value);
	g_object_set(priv->filetree, "show-files", priv->show_files, NULL);
	hildon_file_selection_set_filter(priv->filetree, NULL);
        break;
    case PROP_SYNC_MODE:
        priv->sync_mode = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void hildon_file_chooser_dialog_get_property(GObject * object,
                                                    guint prop_id,
                                                    GValue * value,
                                                    GParamSpec * pspec)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(object)->priv;

    switch (prop_id) {
    case GTK_FILE_CHOOSER_PROP_ACTION:
        g_value_set_enum(value, priv->action);
        break;
    case GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE:
        g_value_set_boolean(value,
                            hildon_file_selection_get_select_multiple
                            (priv->filetree));
        break;
    case GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN: /* Same handler */
    case GTK_FILE_CHOOSER_PROP_LOCAL_ONLY:
        g_object_get_property(G_OBJECT(priv->filetree), pspec->name, value);
        break;
    case GTK_FILE_CHOOSER_PROP_FILTER:
        g_value_set_object(value,
                           hildon_file_selection_get_filter(priv->
                                                            filetree));
        break;
    case GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION:
        g_value_set_boolean (value, priv->do_overwrite_confirmation);
        break;
    case PROP_EMPTY_TEXT:
        g_object_get_property(G_OBJECT(priv->filetree), pspec->name, value);
        break;
    case PROP_FILE_SYSTEM_MODEL:
        g_value_set_object(value, priv->model);
        break;
    case PROP_FOLDER_BUTTON:
        g_value_set_boolean(value, priv->should_show_folder_button);
        break;
    case PROP_LOCATION:
        g_value_set_boolean(value, priv->should_show_location);
        break;
    case PROP_AUTONAMING:
        g_value_set_boolean(value, priv->autonaming_enabled);
        break;
    case PROP_OPEN_BUTTON_TEXT:
        g_object_get_property(G_OBJECT(priv->action_button), "label", value);
        break;
    case PROP_MULTIPLE_TEXT:
        g_object_get_property(G_OBJECT(priv->multiple_label), "label", value);
        break;
    case PROP_MAX_NAME_LENGTH:
        g_value_set_int(value, priv->max_filename_length);
        break;
    case PROP_MAX_FULL_PATH_LENGTH:
        g_value_set_int(value, priv->max_full_path_length);
        break;
    case PROP_SELECTION_MODE:
        g_critical("The \"selection-mode\" property is deprecated.");
        g_value_set_enum(value, hildon_file_selection_get_mode(priv->filetree));
        break;
    case PROP_SHOW_FILES:
        g_value_set_boolean(value, priv->show_files);
        break;
    case PROP_SYNC_MODE:
        g_value_set_boolean(value, priv->sync_mode);
        break;
    default:   /* Backend is not readable */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    };
}

static void hildon_file_chooser_dialog_iface_init(GtkFileChooserIface *
                                                  iface)
{
    iface->set_current_folder =
        hildon_file_chooser_dialog_set_current_folder;
    iface->get_current_folder =
        hildon_file_chooser_dialog_get_current_folder;
    iface->set_current_name = hildon_file_chooser_dialog_set_current_name;
    #if GTK_CHECK_VERSION (2, 14, 0)
    iface->select_file = hildon_file_chooser_dialog_select_file;
    iface->unselect_file = hildon_file_chooser_dialog_unselect_file;
    #else
    iface->select_path = hildon_file_chooser_dialog_select_path;
    iface->unselect_path = hildon_file_chooser_dialog_unselect_path;
    #endif
    iface->select_all = hildon_file_chooser_dialog_select_all;
    iface->unselect_all = hildon_file_chooser_dialog_unselect_all;
    #if GTK_CHECK_VERSION (2, 14, 0)
    iface->get_files = hildon_file_chooser_dialog_get_files;
    iface->get_preview_file = hildon_file_chooser_dialog_get_preview_file;
    #else
    iface->get_paths = hildon_file_chooser_dialog_get_paths;
    iface->get_preview_path = hildon_file_chooser_dialog_get_preview_path;
    #endif
    iface->get_file_system = hildon_file_chooser_dialog_get_file_system;
    iface->add_filter = hildon_file_chooser_dialog_add_filter;
    iface->remove_filter = hildon_file_chooser_dialog_remove_filter;
    iface->list_filters = hildon_file_chooser_dialog_list_filters;
    iface->add_shortcut_folder =
        hildon_file_chooser_dialog_add_remove_shortcut_folder;
    iface->remove_shortcut_folder =
        hildon_file_chooser_dialog_add_remove_shortcut_folder;
    iface->list_shortcut_folders =
        hildon_file_chooser_dialog_list_shortcut_folders;
}

static void hildon_file_chooser_dialog_show(GtkWidget * widget)
{
    HildonFileChooserDialogPrivate *priv;

    g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(widget));

    GTK_WIDGET_CLASS(hildon_file_chooser_dialog_parent_class)->
        show(widget);

    priv = HILDON_FILE_CHOOSER_DIALOG(widget)->priv;
    hildon_file_chooser_dialog_select_text(priv);
    hildon_file_chooser_dialog_selection_changed(priv);
}

static void hildon_file_chooser_dialog_destroy(GtkObject * obj)
{
    HildonFileChooserDialog *dialog;
    gint pos;

    dialog = HILDON_FILE_CHOOSER_DIALOG(obj);
    
    if (dialog->priv->filetree)
      {
	pos = -1;
	g_object_get (dialog->priv->filetree,
		      "pane-position", &pos,
		      NULL);
	if (pos >= 0)
	  set_global_pane_position (pos);
      }

    /* We need sometimes to break cyclic references */

    if (dialog->priv->model) {
        g_signal_handlers_disconnect_by_func
        (dialog->priv->model,
         (gpointer) hildon_file_chooser_dialog_finished_loading,
         dialog);

        g_signal_handlers_disconnect_by_func
        (dialog->priv->model,
         (gpointer) check_for_location_update,
         dialog);

        g_object_unref(dialog->priv->model);
        dialog->priv->model = NULL;
    }

    dialog->priv->filetree = NULL;

    GTK_OBJECT_CLASS(hildon_file_chooser_dialog_parent_class)->
        destroy(obj);
}

static void hildon_file_chooser_dialog_finalize(GObject * obj)
{
    HildonFileChooserDialogPrivate *priv =
        HILDON_FILE_CHOOSER_DIALOG(obj)->priv;

    g_object_unref(priv->caption_size_group);
    g_object_unref(priv->value_size_group);

    if (priv->confirmation_note)
        gtk_widget_destroy (priv->confirmation_note);

    g_free(priv->stub_name);
    g_free(priv->ext_name);

    g_slist_foreach(priv->filters, (GFunc) g_object_unref, NULL);
    g_slist_free(priv->filters);
    g_slist_free(priv->filter_menu_items);
    g_slist_foreach(priv->filter_item_menu_toggle_handlers, (GFunc) g_free, NULL);
    g_slist_free(priv->filter_item_menu_toggle_handlers);
    g_object_unref(priv->popup);

    G_OBJECT_CLASS(hildon_file_chooser_dialog_parent_class)->finalize(obj);
}

static void
hildon_file_chooser_dialog_class_init(HildonFileChooserDialogClass * klass)
{
    GParamSpec *pspec;
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass,
                             sizeof(HildonFileChooserDialogPrivate));

    gobject_class->set_property = hildon_file_chooser_dialog_set_property;
    gobject_class->get_property = hildon_file_chooser_dialog_get_property;
    gobject_class->constructor = hildon_file_chooser_dialog_constructor;
    gobject_class->finalize = hildon_file_chooser_dialog_finalize;
    GTK_WIDGET_CLASS(klass)->show = hildon_file_chooser_dialog_show;
    GTK_WIDGET_CLASS(klass)->show_all = gtk_widget_show;
    GTK_OBJECT_CLASS(klass)->destroy = hildon_file_chooser_dialog_destroy;

    pspec = g_param_spec_string("empty-text", "Empty text",
                                "String to use when selected "
                                "folder is empty",
                                NULL, G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_EMPTY_TEXT, pspec);

    g_object_class_install_property(gobject_class, PROP_FILE_SYSTEM_MODEL,
                                    g_param_spec_object
                                    ("file-system-model",
                                     "File system model",
                                     "Tell the file chooser to use "
                                     "existing model instead of creating "
                                     "a new one",
                                     HILDON_TYPE_FILE_SYSTEM_MODEL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY));

    pspec =
        g_param_spec_boolean("show-folder-button", "Show folder button",
                             "Whether the folder button should be visible "
                             "(if it's possible)",
                             TRUE, G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_FOLDER_BUTTON,
                                    pspec);

    pspec = g_param_spec_boolean("show-location", "Show location",
                                 "Whether the location information should "
                                 "be visible (if it's possible)",
                                 TRUE, G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_LOCATION, pspec);

    pspec = g_param_spec_boolean("autonaming", "Autonaming",
                                 "Whether the text set to name entry "
                                 "should be automatically appended by a "
                                 "counter when the given name already "
                                 "exists",
                                 TRUE, G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_AUTONAMING, pspec);

    g_object_class_install_property(gobject_class, PROP_OPEN_BUTTON_TEXT,
            g_param_spec_string("open-button-text", "Open button text",
                                "String to use in leftmost (=open) button",
                                NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MULTIPLE_TEXT,
            g_param_spec_string("save-multiple", "Save multiple files",
                                "Text to be displayed in items field when saving multiple files",
                                NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MAX_NAME_LENGTH,
            g_param_spec_int("max-name-length", "Maximum name length",
                             "Maximum length of an individual file/folder "
                             "name when entered by user. Note that the actual "
                             "limit can be smaller, if the maximum full "
                             "path length kicks in. Use -1 for no limit.",
                             -1, G_MAXINT, -1, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property(gobject_class, PROP_MAX_FULL_PATH_LENGTH,
            g_param_spec_int("max-full-path-length", "Maximum full path length",
                             "Maximum length of the whole path of an individual "
                             "file/folder name when entered by user. "
                             "Use -1 for no limit or 0 to look the value "
                             "from MAX_FILENAME_LENGTH environment variable",
                             -1, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    /**
     * HildonFileChooserDialog:selection-mode:
     *
     * @Deprecated: since 2.22: this property is ignored and will do nothing.
     *  The underlying #HildonFileSelection supports only the %HILDON_FILE_SELECTION_MODE_THUMBNAILS mode. 
     */
    pspec = g_param_spec_enum("selection-mode", "Selection mode",
                              "View mode used for hildon file selection widget",
                              HILDON_FILE_CHOOSER_DIALOG_TYPE_SELECTION_MODE,
                              HILDON_FILE_SELECTION_MODE_LIST,
                              G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_SELECTION_MODE, pspec);

    pspec = g_param_spec_boolean("show-files", "Show files",
                                 "show files in the change folder dialog ",
                                 FALSE, G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_SHOW_FILES, pspec);

    pspec = g_param_spec_boolean("sync-mode", "Sync mode",
                                 "Sync mode uses gtk_dialog_run to show sub-dialogs, async mode uses gtk_widget_show",
                                 TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    g_object_class_install_property(gobject_class, PROP_SYNC_MODE, pspec);

    hildon_gtk_file_chooser_install_properties(gobject_class);
}

static void hildon_file_chooser_dialog_sort_changed(GtkWidget * item,
                                                    gpointer data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
        HildonFileSelectionSortKey key;
        HildonFileChooserDialogPrivate *priv = data;

        if (item == priv->sort_type)
            key = HILDON_FILE_SELECTION_SORT_TYPE;
        else if (item == priv->sort_name)
            key = HILDON_FILE_SELECTION_SORT_NAME;
        else if (item == priv->sort_date)
            key = HILDON_FILE_SELECTION_SORT_MODIFIED;
        else
            key = HILDON_FILE_SELECTION_SORT_SIZE;

        hildon_file_selection_set_sort_key(priv->filetree, key,
                                           GTK_SORT_ASCENDING);
    }
}

static void hildon_file_chooser_dialog_mode_changed(GtkWidget * item,
                                                    gpointer data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
        HildonFileSelectionMode mode;
        HildonFileChooserDialogPrivate *priv = data;

        mode =
            (item ==
             priv->
             mode_list ? HILDON_FILE_SELECTION_MODE_LIST :
             HILDON_FILE_SELECTION_MODE_THUMBNAILS);
        hildon_file_selection_set_mode(priv->filetree, mode);
    }
}

static void
hildon_file_chooser_entry_changed( GtkWidget * widget,
                                    gpointer data)
{
    HildonFileChooserDialogPrivate *priv;

    g_return_if_fail( GTK_IS_WIDGET(widget));
    g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(data));

    priv = HILDON_FILE_CHOOSER_DIALOG( data )->priv;
    priv->edited = TRUE;

    g_free (priv->stub_name);
    priv->stub_name = g_strdup (hildon_entry_get_text (HILDON_ENTRY (widget)));

    gtk_dialog_set_response_sensitive(GTK_DIALOG(data),
                                      GTK_RESPONSE_OK,
				      strlen(hildon_entry_get_text(HILDON_ENTRY(widget))) > 0);
}

static void hildon_file_chooser_update_path_button(HildonFileChooserDialog *self)
{
    g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(self));
    HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(self)->priv;
    GtkTreeIter iter;
    GtkTreeIter cur_iter;
    GdkPixbuf *icon;
    gchar *current_folder;
    GPtrArray *path_components;
    gint i;
    GString *path_str;

    hildon_file_selection_get_current_folder_iter(priv->filetree, &cur_iter);
    gtk_tree_model_get(GTK_TREE_MODEL(priv->model), &cur_iter,
		       HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON, &icon,
		       HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME, &current_folder,
		       -1);

    path_components = g_ptr_array_sized_new(8);
    /* current_folder will be free'd when the whole ptrarray is free'd */
    g_ptr_array_add(path_components, current_folder);
    while (gtk_tree_model_iter_parent(GTK_TREE_MODEL(priv->model), &iter, &cur_iter))
      {
	gchar *title;

	gtk_tree_model_get(GTK_TREE_MODEL(priv->model), &iter,
			   HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME, &title,
			   -1);
	g_ptr_array_add(path_components, title);
	cur_iter = iter;
      }

    /* activate the buttons if we are not at the top */
    if (path_components->len > 1)
      {
	gtk_widget_set_sensitive(priv->up_button, TRUE);
	gtk_widget_set_sensitive(priv->path_button, TRUE);
	gtk_widget_set_sensitive(priv->folder_button, TRUE);
      }
    else {
	gtk_widget_set_sensitive(priv->up_button, FALSE);
	gtk_widget_set_sensitive(priv->path_button, FALSE);
	gtk_widget_set_sensitive(priv->folder_button, FALSE);
      }

    path_str = g_string_sized_new(MAXPATHLEN / 8);
    /* ignore the display name of the root node ('/') */
    for (i = path_components->len - 2; i >= 0; --i)
      {
	g_string_append(path_str, g_ptr_array_index(path_components, i));
	if (i != 0)
	  g_string_append_c(path_str, G_DIR_SEPARATOR);
      }

    hildon_button_set_title(HILDON_BUTTON(priv->path_button), current_folder);
    hildon_button_set_value(HILDON_BUTTON(priv->path_button), path_str->str);
    hildon_button_set_image(HILDON_BUTTON(priv->path_button),
			    gtk_image_new_from_pixbuf(icon));
    hildon_button_set_image_position(HILDON_BUTTON(priv->path_button),
				     GTK_POS_LEFT);
    if (icon)
      g_object_unref(icon);

    if (priv->action == GTK_FILE_CHOOSER_ACTION_OPEN)
      {
	gtk_label_set_text(GTK_LABEL(priv->path_label), path_str->str);
      }

    g_ptr_array_foreach(path_components, (GFunc)g_free, NULL);
    g_ptr_array_free(path_components, TRUE);
    g_string_free(path_str, TRUE);
}

static void hildon_response_up_button_clicked(GtkWidget *widget,
	  				      gpointer   data )
{
    GtkTreeIter iter;
    GtkTreeIter cur_iter;
    gboolean multiroot = FALSE;

    g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(data));
    HildonFileChooserDialogPrivate *priv = HILDON_FILE_CHOOSER_DIALOG(data)->priv;
    hildon_file_selection_get_current_folder_iter(priv->filetree, &cur_iter);

    if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(priv->model), &iter, &cur_iter)) {
      hildon_file_selection_set_current_folder_iter(priv->filetree, &iter);
      cur_iter = iter;
      if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(priv->model), &iter, &cur_iter)){
          gtk_widget_set_sensitive(priv->up_button, TRUE);
          gtk_widget_set_sensitive(priv->path_button, TRUE);
          gtk_widget_set_sensitive(priv->folder_button, TRUE);
      }
      else {
	gtk_widget_set_sensitive(priv->up_button, FALSE);
	g_object_get(priv->model, "multi-root", &multiroot, NULL);
	if (!multiroot) {//bookmarks use self-made model, leave button enabled       
	  gtk_widget_set_sensitive(priv->path_button, FALSE);
	  gtk_widget_set_sensitive(priv->folder_button, FALSE);
	}
	else {
	  gtk_widget_set_sensitive(priv->path_button, TRUE);
	  gtk_widget_set_sensitive(priv->folder_button, TRUE);
	}
      }
    }
}

static void hildon_response_path_button_clicked(GtkWidget *widget,
					        gpointer   data )

{
    gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_OK);
}

static void hildon_file_chooser_dialog_location_button_clicked(GtkWidget *widget, 
							       gpointer data)
{
  HildonFileChooserDialog *self = HILDON_FILE_CHOOSER_DIALOG(data);

  g_debug ("LOCATION_PRESSED %d\n",
	   self->priv->should_show_folder_button);

  if (self->priv->should_show_folder_button
      && (self->priv->action == GTK_FILE_CHOOSER_ACTION_SAVE
	  || self->priv->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER))
    handle_folder_popup(self);
}

static void hildon_file_chooser_dialog_init(HildonFileChooserDialog * self)
{
    GtkMenuShell *shell;
    GtkRadioMenuItem *item;
    HildonFileChooserDialogPrivate *priv;
    GtkBox *box;
    GtkWidget *label_items;

    GtkWidget *image = NULL;

    self->priv = priv =
        G_TYPE_INSTANCE_GET_PRIVATE(self, HILDON_TYPE_FILE_CHOOSER_DIALOG,
                                    HildonFileChooserDialogPrivate);

    priv->caption_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    priv->value_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    priv->filters = NULL;
    priv->autonaming_enabled = TRUE;
    priv->should_show_folder_button = TRUE;
    priv->should_show_location = TRUE;
    priv->stub_name = priv->ext_name = NULL;
    priv->action = GTK_FILE_CHOOSER_ACTION_OPEN;

    image = gtk_image_new_from_icon_name ("filemanager_folder_up",
					  HILDON_ICON_SIZE_FINGER);

    priv->up_button = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, 
					HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
    hildon_button_set_image(HILDON_BUTTON(priv->up_button), image);
    gtk_button_set_alignment(GTK_BUTTON(priv->up_button), 0.0, 0.5);

    g_signal_connect (priv->up_button, "clicked",
		      G_CALLBACK (hildon_response_up_button_clicked), self);
    gtk_widget_show(priv->up_button);

    priv->path_button = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT |
					  HILDON_SIZE_AUTO_WIDTH, 
					  //HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
					  HILDON_BUTTON_ARRANGEMENT_VERTICAL);

    gtk_button_set_alignment(GTK_BUTTON(priv->path_button), 0.0, 0.5);
    g_signal_connect (priv->path_button, "clicked",
		      G_CALLBACK (hildon_response_path_button_clicked), self);
    gtk_widget_show(priv->path_button);

    priv->path_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    // Set path to be truncated from the left
    gtk_label_set_ellipsize (GTK_LABEL(priv->path_label), PANGO_ELLIPSIZE_START);	

    gtk_widget_show(priv->path_label);

    priv->action_button =
        gtk_dialog_add_button(GTK_DIALOG(self),
                              _("ckdg_bd_select_object_ok_open"),
                              GTK_RESPONSE_OK);
    priv->folder_button =
        gtk_dialog_add_button(GTK_DIALOG(self),
                              _("ckdg_bd_change_folder_new_folder"),
                              HILDON_RESPONSE_FOLDER_BUTTON);
    priv->entry_name = hildon_entry_new(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);

    priv->changed_handler =
          g_signal_connect( priv->entry_name, "changed",
                          G_CALLBACK( hildon_file_chooser_entry_changed ),
                          self );

    g_signal_connect(priv->entry_name, "invalid-input",
                     G_CALLBACK(chooser_entry_invalid_input_cb), self);

    priv->hbox_location = gtk_hbox_new(FALSE, HILDON_MARGIN_DEFAULT);
    priv->hbox_items = gtk_hbox_new(FALSE, HILDON_MARGIN_DEFAULT);
    priv->image_location = gtk_image_new();
    priv->title_location = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(priv->title_location), 0.0f, 0.5f);

    priv->location_button = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, 
					      HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
    hildon_button_set_title(HILDON_BUTTON(priv->location_button),  _("sfil_fi_save_objects_items")); 
    gtk_button_set_alignment(GTK_BUTTON(priv->location_button), 0.0, 0.5);
    hildon_button_add_size_groups(HILDON_BUTTON(priv->location_button),
				  priv->caption_size_group,
				  priv->value_size_group,
				  NULL);
    hildon_button_set_style(HILDON_BUTTON(priv->location_button), HILDON_BUTTON_STYLE_PICKER);

    gtk_widget_show(priv->location_button);

    gtk_box_pack_start(GTK_BOX(priv->hbox_location), priv->image_location,
                       FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(priv->hbox_location), priv->title_location,
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(priv->hbox_location), priv->location_button,
                       TRUE, TRUE, 0);

    priv->eventbox_location = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(priv->eventbox_location), FALSE);
    gtk_container_add(GTK_CONTAINER(priv->eventbox_location), priv->hbox_location);
    gtk_widget_add_events(priv->eventbox_location, GDK_BUTTON_PRESS_MASK);
    g_object_set (priv->eventbox_location, "can-focus", FALSE, NULL);
  
    /* organize address area */
    priv->hbox_address = gtk_hbox_new(FALSE, HILDON_MARGIN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(priv->hbox_address), priv->up_button,
                       FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(priv->hbox_address), priv->path_button,
		       TRUE, TRUE, HILDON_MARGIN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(priv->hbox_address), priv->path_label,
		       TRUE, TRUE, HILDON_MARGIN_DEFAULT);

    label_items = g_object_new(GTK_TYPE_LABEL, "label",  _("sfil_fi_save_objects_items"), "xalign", 1.0f, NULL);
    priv->multiple_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(priv->hbox_items), label_items, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(priv->hbox_items), priv->multiple_label, FALSE, TRUE, 0);
    gtk_size_group_add_widget(priv->caption_size_group, label_items);

    priv->popup = gtk_menu_new();
    shell = GTK_MENU_SHELL(priv->popup);
    g_object_ref(priv->popup);
    g_object_ref_sink (GTK_OBJECT(priv->popup));

    priv->sort_type =
        gtk_radio_menu_item_new_with_label(NULL, _("sfil_me_sort_type"));
    item = GTK_RADIO_MENU_ITEM(priv->sort_type);
    priv->sort_name =
        gtk_radio_menu_item_new_with_label_from_widget(item,
                                                       _("sfil_me_sort_name"));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(priv->sort_name),
                                   TRUE);
    priv->sort_date =
        gtk_radio_menu_item_new_with_label_from_widget(item,
                                                       _("sfil_me_sort_date"));
    priv->sort_size =
        gtk_radio_menu_item_new_with_label_from_widget(item,
                                                       _("sfil_me_sort_size"));

    priv->mode_list = gtk_radio_menu_item_new_with_label(NULL,
                                                         _("sfil_me_view_list"));
    priv->mode_thumbnails =
        gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM
                                                       (priv->mode_list),
                                                       _("sfil_me_view_thumbnails"));
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(priv->mode_thumbnails), TRUE);

    priv->filters_separator = NULL;
    priv->filter_menu_items = NULL;
    priv->filter_item_menu_toggle_handlers = NULL;

    priv->confirmation_note = NULL;
    priv->do_overwrite_confirmation = FALSE;

    priv->sync_mode = TRUE;
    priv->dg_file_path = NULL;
    priv->dg_uri = NULL;

    gtk_menu_shell_append(shell, priv->sort_type);
    gtk_menu_shell_append(shell, priv->sort_name);
    gtk_menu_shell_append(shell, priv->sort_date);
    gtk_menu_shell_append(shell, priv->sort_size);
    gtk_menu_shell_append(shell, gtk_separator_menu_item_new());
    gtk_menu_shell_append(shell, priv->mode_list);
    gtk_menu_shell_append(shell, priv->mode_thumbnails);
    gtk_widget_show_all(priv->popup);

    box = GTK_BOX(GTK_DIALOG(self)->vbox);
    gtk_box_pack_start(box, priv->entry_name, FALSE, TRUE, 
 		       HILDON_MARGIN_DEFAULT);
    gtk_box_pack_start(box, priv->hbox_items, FALSE, TRUE, 
		       HILDON_MARGIN_DEFAULT);
    gtk_box_pack_start(box, priv->eventbox_location, FALSE, TRUE,
                       HILDON_MARGIN_DEFAULT);
 


    g_signal_connect(self, "response", G_CALLBACK(response_handler), NULL);
    g_signal_connect(priv->mode_list, "toggled",
                     G_CALLBACK(hildon_file_chooser_dialog_mode_changed),
                     priv);
    g_signal_connect(priv->mode_thumbnails, "toggled",
                     G_CALLBACK(hildon_file_chooser_dialog_mode_changed),
                     priv);
    g_signal_connect(priv->sort_type, "toggled",
                     G_CALLBACK(hildon_file_chooser_dialog_sort_changed),
                     priv);
    g_signal_connect(priv->sort_name, "toggled",
                     G_CALLBACK(hildon_file_chooser_dialog_sort_changed),
                     priv);
    g_signal_connect(priv->sort_date, "toggled",
                     G_CALLBACK(hildon_file_chooser_dialog_sort_changed),
                     priv);
    g_signal_connect(priv->sort_size, "toggled",
                     G_CALLBACK(hildon_file_chooser_dialog_sort_changed),
                     priv);
    g_signal_connect(priv->location_button, "clicked",
		     G_CALLBACK(hildon_file_chooser_dialog_location_button_clicked), 
		     self);

    g_signal_connect (priv->eventbox_location, "key-press-event", 
 		      G_CALLBACK (on_location_hw_enter_pressed), self);	
    gtk_dialog_set_default_response(GTK_DIALOG(self), GTK_RESPONSE_OK);
}

static GObject *
hildon_file_chooser_dialog_constructor(GType type,
                                       guint n_construct_properties,
                                       GObjectConstructParam *
                                       construct_properties)
{
    GObject *obj;
    HildonFileChooserDialogPrivate *priv;
    GtkTreeIter iter;

    obj =
        G_OBJECT_CLASS(hildon_file_chooser_dialog_parent_class)->
        constructor(type, n_construct_properties, construct_properties);
    /* Now we know if specific backend is requested */
    priv = HILDON_FILE_CHOOSER_DIALOG(obj)->priv;

    g_assert(priv->model);
    priv->filetree = g_object_new
      (HILDON_TYPE_FILE_SELECTION,
       "model", priv->model,
       "visible-columns", (HILDON_FILE_SELECTION_SHOW_NAME
			   | HILDON_FILE_SELECTION_SHOW_MODIFIED),
       "pane-position", get_global_pane_position (),
       NULL);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(obj)->vbox), 
			       priv->hbox_address, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(obj)->vbox),
      GTK_WIDGET(priv->filetree), TRUE, TRUE, 0);

    g_signal_connect_swapped(priv->filetree, "selection-changed",
                     G_CALLBACK
                     (hildon_file_chooser_dialog_selection_changed), priv);
    g_signal_connect_swapped(priv->filetree, "notify::active-pane",
                     G_CALLBACK
                     (hildon_file_chooser_dialog_selection_changed), priv);
    g_signal_connect(priv->filetree, "current-folder-changed",
                     G_CALLBACK
                     (hildon_file_chooser_dialog_current_folder_changed), priv);
    g_signal_connect(priv->filetree, "file-activated",
                     G_CALLBACK(file_activated_handler), obj);
    g_signal_connect(priv->filetree, "folder-activated",
                     G_CALLBACK(folder_activated_handler), obj);
    g_signal_connect_object(priv->model, "finished-loading",
                     G_CALLBACK(hildon_file_chooser_dialog_finished_loading),
                     obj, 0);
    g_signal_connect_object (priv->model, "row-changed", 
                     G_CALLBACK (check_for_location_update), obj, 0);

    gtk_dialog_set_has_separator(GTK_DIALOG(obj), FALSE);
    hildon_file_chooser_dialog_set_limit(HILDON_FILE_CHOOSER_DIALOG(obj));
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->model), &iter);
    hildon_file_selection_set_current_folder_iter(priv->filetree, &iter);
    gtk_widget_set_sensitive(priv->up_button, FALSE);
    gboolean multiroot = FALSE;
    g_object_get(priv->model, "multi-root", &multiroot, NULL);
    if (multiroot){ //bookmarks use self-made model, leave button enabled
        gtk_widget_set_sensitive(priv->path_button, TRUE);
        gtk_widget_set_sensitive(priv->folder_button, TRUE);
    }
    else {
      gtk_widget_set_sensitive(priv->path_button, FALSE);
      gtk_widget_set_sensitive(priv->folder_button, FALSE);
    }
    return obj;
}

/** Public API ****************************************************/

/**
 * hildon_file_chooser_dialog_new:
 * @parent: Transient parent window for dialog.
 * @action: Action to perform
 *          (open file/save file/select folder/new folder).
 *
 * Creates a new #HildonFileChooserDialog using the given action.
 *
 * Return value: a new #HildonFileChooserDialog.
 */
GtkWidget *hildon_file_chooser_dialog_new(GtkWindow * parent,
                                          GtkFileChooserAction action)
{
    GtkWidget *dialog;
    dialog =
        g_object_new(HILDON_TYPE_FILE_CHOOSER_DIALOG, "action", action,
                     NULL);

    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

    return dialog;
}

/**
 * hildon_file_chooser_dialog_new_with_properties:
 * @parent: Transient parent window for dialog.
 * @first_property: First option to pass to dialog.
 * @Varargs: arguments
 *
 * Creates new HildonFileChooserDialog. This constructor is handy if you
 * need to pass several options.
 *
 * Return value: New HildonFileChooserDialog object.
 */
GtkWidget *hildon_file_chooser_dialog_new_with_properties(GtkWindow *
                                                          parent,
                                                          const gchar *
                                                          first_property,
                                                          ...)
{
    GtkWidget *dialog;
    va_list args;
 
    va_start(args, first_property);
    dialog =
        GTK_WIDGET(g_object_new_valist
                   (HILDON_TYPE_FILE_CHOOSER_DIALOG, first_property,
                    args));
    va_end(args);

    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

    return dialog;
}

/**
 * hildon_file_chooser_dialog_add_extra:
 * @self: dialog widget
 * @widget: widget to add
 *
 * Add @widget to the dialog, below the "Name" and "Location" fields.
 * When @widget is a #HildonCaption, care is taken that the labels line
 * up with existing HildonCaptions.
 */
void
hildon_file_chooser_dialog_add_extra (HildonFileChooserDialog *self,
				      GtkWidget *widget)
{
  HildonFileChooserDialogPrivate *priv = self->priv;

  /* XXX - HildonCaption widgets seem to need special treatment when
           setting their size groups...
  */
  if (HILDON_IS_CAPTION (widget)) {
    hildon_caption_set_size_group (HILDON_CAPTION (widget), 
				   priv->caption_size_group);
   } else if (HILDON_IS_BUTTON (widget)) {
     hildon_button_add_size_groups(HILDON_BUTTON(widget), 
 				  priv->caption_size_group, 
 				  priv->value_size_group,
 				  NULL);
   } else {
   }


  gtk_box_pack_start (GTK_BOX(GTK_DIALOG(self)->vbox), widget,
		      FALSE, TRUE, 0);
  gtk_widget_show (widget);
}

static void
sync_extensions_combo (HildonFileChooserDialogPrivate *priv)
{
    if (priv->ext_name && priv->extensions_combo)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      gboolean valid;
      HildonTouchSelector *selector;

      selector = hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(priv->extensions_combo));
      model = hildon_touch_selector_get_model(selector, 0);

      /* if there is a valid extension selected, just leave it */
      if (hildon_touch_selector_get_selected (selector, 0, &iter))
        {
	  gboolean matches;
	  gchar *ext;

	  gtk_tree_model_get (model, &iter, 0, &ext, -1);
	  matches = (strcmp (ext, priv->ext_name + 1) == 0);
	  g_free (ext);

	  if (matches)
	    return;
	}

      valid = gtk_tree_model_get_iter_first (model, &iter);
      while (valid)
	{
	  gchar *ext;
	  gtk_tree_model_get (model, &iter, 0, &ext, -1);
	  if (strcmp (ext, priv->ext_name + 1) == 0){
              hildon_touch_selector_select_iter (selector, 0, &iter, TRUE);
              hildon_button_set_value(HILDON_BUTTON(priv->extensions_combo), ext);

	      g_free (ext);
	      break;
	  }
	  g_free (ext);
	  valid = gtk_tree_model_iter_next (model, &iter); 
	}
    }
}

static void
extension_changed (GtkComboBox *widget, gpointer data)
{
  HildonFileChooserDialogPrivate *priv =  HILDON_FILE_CHOOSER_DIALOG (data)->priv;
  gchar *selected_extension;
  GList* name_list;
  GList* extensions_list;

  g_free (priv->ext_name);
  selected_extension = g_strdup(hildon_button_get_value (HILDON_BUTTON (widget)));
  name_list = g_list_first (priv->ext_names_list);
  extensions_list = g_list_first (priv->extensions_list);
  while(name_list != NULL) {
    if(g_ascii_strcasecmp(selected_extension, (char *)name_list->data) == 0){
       priv->ext_name = g_strconcat (".", (char *)extensions_list->data, NULL);
       break;
    }
    name_list = g_list_next (name_list);
    extensions_list = g_list_next (extensions_list);
  }
  hildon_file_chooser_dialog_do_autonaming (priv);
  g_free(selected_extension);
}


static void hildon_file_chooser_dialog_reset_files_visibility(HildonFileChooserDialogPrivate *priv,
							      gint dialog_type)
{
  switch (dialog_type){
     case GTK_FILE_CHOOSER_ACTION_OPEN:
       g_object_set(priv->filetree, "show-files", TRUE, NULL);
       hildon_file_selection_set_filter(priv->filetree, NULL);
       break;
     case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
       g_object_set(priv->filetree, "show-files", FALSE, NULL);
       hildon_file_selection_set_filter(priv->filetree, NULL);
       break;
     case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER + 4:
       g_object_set(priv->filetree, "show-files", FALSE, NULL);
       hildon_file_selection_set_filter(priv->filetree, NULL);
       break;
     default:
       break;
  }
}

/**
 * hildon_file_chooser_dialog_add_extensions_combo:
 * @self: dialog widget
 * @extensions: extensions to offer
 * @ext_names: names of the extensions to show in the UI
 *
 * Create and add a combo box widget with a list of file extensions.
 * This combobox will track and modify the extension of the current
 * filename; it is not a filter.
 *
 * @extensions should be a vector of strings, terminated by #NULL.  The
 * strings in it are the extensions, without a leading '.'.
 *
 * @ext_names, when non-#NULL, is a vector parallel to @extensions
 * that determines the names of the extensions to use in the UI.  When
 * @ext_names is NULL, the extensions themselves are used as the
 * names.
 *
 * Returns: the created #HildonPickerButton widget
 */

GtkWidget *
hildon_file_chooser_dialog_add_extensions_combo (HildonFileChooserDialog *self,
						 char **extensions,
						 char **ext_names)
{
  GtkWidget       *selector;
  GtkWidget       *button;
  gboolean        need_release = FALSE;
  gint            i = 0;

  g_return_val_if_fail (HILDON_IS_FILE_CHOOSER_DIALOG (self), NULL);
  g_return_val_if_fail (self->priv->extensions_combo == NULL, NULL);
  g_return_val_if_fail (extensions != NULL, NULL);
  g_return_val_if_fail (extensions[0] != NULL, NULL);

  /* duplicate the extensions for ext_names if ext_names is NULL */
  if(ext_names == NULL) {
    ext_names = g_strdupv(extensions);
    need_release = TRUE;
  }
    
  button = hildon_picker_button_new(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH, 
				    HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_button_set_text(HILDON_BUTTON(button), NULL, ext_names[0]);


  selector = hildon_touch_selector_new_text();

  self->priv->extensions_combo = button;
  g_list_free(self->priv->extensions_list);
  g_list_free(self->priv->ext_names_list);
  self->priv->extensions_list = NULL;
  self->priv->ext_names_list = NULL;
  for (i = 0; extensions[i]; i++) {
    g_return_val_if_fail(ext_names[i] != NULL, NULL);
    hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), g_strdup(ext_names[i]));
    self->priv->extensions_list = g_list_append(self->priv->extensions_list, g_strdup(extensions[i]));
    self->priv->ext_names_list = g_list_append(self->priv->ext_names_list, g_strdup(ext_names[i]));
  }

  hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (button), 
				     HILDON_TOUCH_SELECTOR (selector));
  hildon_touch_selector_set_active (HILDON_TOUCH_SELECTOR (selector), 0, 0);

  gtk_button_set_alignment(GTK_BUTTON(button), 0, 0.5);
  hildon_button_set_title(HILDON_BUTTON(button), _("sfil_fi_save_object_dialog_type"));
  hildon_button_add_size_groups(HILDON_BUTTON(button), 
				self->priv->caption_size_group,
				self->priv->value_size_group,
				NULL);
  g_signal_connect (button, "value-changed",
 		    G_CALLBACK (extension_changed), self);

  hildon_file_chooser_dialog_add_extra (self, button);
  
  gtk_widget_show (button);
  sync_extensions_combo (self->priv);
  if(need_release){
    g_strfreev (ext_names);
  }
  return button;
}

/**
 * hildon_file_chooser_dialog_get_extension
 * @self: dialog widget
 *
 * Return the extension of the current filename.
 * 
 * Return value: the current extension as a newly allocated string
 */
gchar *
hildon_file_chooser_dialog_get_extension (HildonFileChooserDialog *self)
{
  HildonFileChooserDialogPrivate *priv = self->priv;

  if (priv->ext_name)
    return g_strdup (priv->ext_name + 1);
  else
    return NULL;
}

/**
 * hildon_file_chooser_dialog_set_extension
 * @self: dialog widget
 * @extension: the new extension
 *
 * Set the extension of the current filename.
 */
void
hildon_file_chooser_dialog_set_extension (HildonFileChooserDialog *self,
					  const gchar *extension)
{
  HildonFileChooserDialogPrivate *priv = self->priv;

  g_free (priv->ext_name);
  priv->ext_name = g_strconcat (".", extension, NULL);
  hildon_file_chooser_dialog_do_autonaming (priv);
  sync_extensions_combo (priv);
}

/**
 * hildon_file_chooser_dialog_set_safe_folder:
 * @self: a #HildonFileChooserDialog widget.
 * @local_path: a path to safe folder.
 *
 * Sets a safe folder that is used as a fallback in a case
 * that automatic location change fails.
 */
void hildon_file_chooser_dialog_set_safe_folder(
  HildonFileChooserDialog *self, const gchar *local_path)
{
  GtkFileSystem *fs;
  GtkFilePath *path;

  g_return_if_fail (HILDON_IS_FILE_CHOOSER_DIALOG(self));
  fs = _hildon_file_system_model_get_file_system(self->priv->model);

  path = gtk_file_system_filename_to_path (fs, local_path);
  g_object_set(self->priv->model, "safe-folder", path, NULL);
  gtk_file_path_free(path);
}

/**
 * hildon_file_chooser_dialog_set_safe_folder_uri:
 * @self: a #HildonFileChooserDialog widget.
 * @uri: an uri to safe folder.
 *
 * See #hildon_file_chooser_dialog_set_safe_folder.
 */
void hildon_file_chooser_dialog_set_safe_folder_uri(
  HildonFileChooserDialog *self, const gchar *uri)
{
  GtkFileSystem *fs;
  GtkFilePath *path;

  g_return_if_fail (HILDON_IS_FILE_CHOOSER_DIALOG(self));

  fs = _hildon_file_system_model_get_file_system(self->priv->model);
  path = gtk_file_system_uri_to_path (fs, uri);
  g_object_set(self->priv->model, "safe-folder", path, NULL);
  gtk_file_path_free(path);
}

/**
 * hildon_file_chooser_dialog_get_safe_folder:
 * @self: a #HildonFileChooserDialog widget.
 *
 * Gets safe folder location as local path.
 *
 * Returns: a local path. Free this with #g_free.
 */
gchar *hildon_file_chooser_dialog_get_safe_folder(
  HildonFileChooserDialog *self)
{
  GtkFileSystem *fs;
  GtkFilePath *path;
  gchar *result = NULL;

  g_return_val_if_fail (HILDON_IS_FILE_CHOOSER_DIALOG(self), NULL);

  fs = _hildon_file_system_model_get_file_system(self->priv->model);
  g_object_get(self->priv->model, "safe-folder", &path, NULL);

  if (path) {
    result = gtk_file_system_path_to_filename(fs, path);
    gtk_file_path_free(path);
  }

  return result;
}

/**
 * hildon_file_chooser_dialog_get_safe_folder_uri:
 * @self: a #HildonFileChooserDialog widget.
 *
 * Gets safe folder location as uri.
 *
 * Returns: an uri. Free this with #g_free.
 */
gchar *hildon_file_chooser_dialog_get_safe_folder_uri(
  HildonFileChooserDialog *self)
{
  GtkFileSystem *fs;
  GtkFilePath *path;
  gchar *result = NULL;

  g_return_val_if_fail (HILDON_IS_FILE_CHOOSER_DIALOG(self), NULL);

  fs = _hildon_file_system_model_get_file_system(self->priv->model);
  g_object_get(self->priv->model, "safe-folder", &path, NULL);

  if (path) {
    result = gtk_file_system_path_to_uri(fs, path);
    gtk_file_path_free(path);
  }

  return result;
}

/**
 * hildon_file_chooser_dialog_focus_to_input:
 * @d: the dialog.
 *
 * Selects the text in input box and transfers focus there.
 */
void hildon_file_chooser_dialog_focus_to_input(HildonFileChooserDialog *d)
{
  g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(d));
  hildon_file_chooser_dialog_select_text(d->priv);
}



/**
 * hildon_file_chooser_dialog_set_show_upnp:
 * @self: a #HildonFileChooserDialog widget.
 * @value: a gboolean value to be set.
 *
 * Set whether the dialog shows UPNP locations.
 *
 */
void hildon_file_chooser_dialog_set_show_upnp(
  HildonFileChooserDialog *self, gboolean value)
{
  HildonFileChooserDialogPrivate *priv;
  g_return_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(self));

  priv = self->priv;
  g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));

  g_object_set (priv->filetree, "show-upnp", value, NULL);
}


/**
 * hildon_file_chooser_dialog_get_show_upnp:
 * @self: a #HildonFileChooserDialog widget.
 *
 * Gets whether the dialog shows UPNP locations.
 * Returns: gboolean value..
 *
 */
gboolean hildon_file_chooser_dialog_get_show_upnp (HildonFileChooserDialog *self)
{
  HildonFileChooserDialogPrivate *priv;
  gboolean show_upnp;

  g_return_val_if_fail(HILDON_IS_FILE_CHOOSER_DIALOG(self), FALSE);

  priv = self->priv;
  g_assert(HILDON_IS_FILE_SELECTION(priv->filetree));

  g_object_get (priv->filetree, "show-upnp", &show_upnp, NULL);
  return show_upnp;
}
