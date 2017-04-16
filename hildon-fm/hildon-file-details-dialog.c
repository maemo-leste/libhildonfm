/*
 * This file is part of hildon-fm
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
 * SECTION: hildon-file-details-dialog
 * @short_description: Hildon file details dialog
 *
 * Provides a dialog box for displaying file and folder details.
 */

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkscrolledwindow.h>
#include <time.h>
#include <libintl.h>
#include <sys/stat.h>

#include <hildon/hildon.h>

#include "hildon-file-details-dialog.h"
#include "hildon-file-system-model.h"

#include <string.h> /* strstr() */

#include "hildon-file-common-private.h"

#define HILDON_FREMANTLE_FM_UI

enum
{
  PROP_SHOW_TABS = 1,
  PROP_ADDITIONAL_TAB,
  PROP_ADDITIONAL_TAB_LABEL,
  PROP_MODEL,
  PROP_ENABLE_READONLY_CHECKBOX,
  PROP_SHOW_TYPE_ICON
};

struct _HildonFileDetailsDialogPrivate {
    GtkWidget *vbox;
    GtkSizeGroup *sizegroup;
    GtkNotebook *notebook;
    GtkWidget *file_location, *file_name;
    GtkWidget *file_type, *file_size;
    GtkWidget *file_date, *file_time;
    GtkWidget *file_readonly, *file_device;
    GtkWidget *file_location_image, *file_device_image;
    GtkWidget *scroll;

    GtkTreeRowReference *active_file;
    gboolean checkbox_original_state;
    gulong delete_handler;

    /* Properties */
    HildonFileSystemModel *model;
    GtkWidget *tab_label;
};

static void
hildon_file_details_dialog_class_init(HildonFileDetailsDialogClass *
                                      klass);
static void hildon_file_details_dialog_init(HildonFileDetailsDialog *
                                            filedetailsdialog);
static void hildon_file_details_dialog_finalize(GObject * object);

static void
hildon_file_details_dialog_set_property( GObject *object, guint param_id,
                                                           const GValue *value,
                                         GParamSpec *pspec );
static void
hildon_file_details_dialog_get_property( GObject *object, guint param_id,
                                                           GValue *value, GParamSpec *pspec );
static void
hildon_file_details_dialog_response(GtkDialog *dialog, gint response_id);

#if 0 //Fremantle
static void do_line_wrapping (GtkWidget *w, gpointer data);
#endif

static GtkDialogClass *file_details_dialog_parent_class = NULL;

GType hildon_file_details_dialog_get_type(void)
{
    static GType file_details_dialog_type = 0;

    if (!file_details_dialog_type) {
        static const GTypeInfo file_details_dialog_info = {
            sizeof(HildonFileDetailsDialogClass),
            NULL,       /* base_init */
            NULL,       /* base_finalize */
            (GClassInitFunc) hildon_file_details_dialog_class_init,
            NULL,       /* class_finalize */
            NULL,       /* class_data */
            sizeof(HildonFileDetailsDialog),
            0,  /* n_preallocs */
            (GInstanceInitFunc) hildon_file_details_dialog_init,
        };

        file_details_dialog_type =
            g_type_register_static(GTK_TYPE_DIALOG,
                                   "HildonFileDetailsDialog",
                                   &file_details_dialog_info,
                                   0);
    }

    return file_details_dialog_type;
}

static gboolean write_access(const gchar *uri)
{
  GFile     *file = g_file_new_for_uri (uri);
  GFileInfo *info;
  gboolean result = FALSE;

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                            G_FILE_QUERY_INFO_NONE, NULL, NULL);
  g_object_unref (file);

  /* Get information about file */
  if (info) {
    /* Detect that the file is writable or not */
    result =
        g_file_info_get_attribute_boolean(info,
                                          G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
    g_object_unref (info);
  }

  return result;
}

/* When model deletes a file, we check if our reference is still valid.
   If not, we emit response, which usually closes the dialog */
static void check_validity(GtkTreeModel *model,
  GtkTreePath *path, gpointer data)
{
  GtkTreeRowReference *ref;

  g_return_if_fail(HILDON_IS_FILE_DETAILS_DIALOG(data));

  ref = HILDON_FILE_DETAILS_DIALOG(data)->priv->active_file;

  if (ref && !gtk_tree_row_reference_valid(ref))
    gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_NONE);
}

static void change_state(HildonFileDetailsDialog *self, gboolean readonly)
{
  GtkTreeIter iter;

  g_return_if_fail(HILDON_IS_FILE_DETAILS_DIALOG(self));

  /* We fail to get the iterator in cases when the reference is
     invalidated, for example when the file is removed. */
  if (hildon_file_details_dialog_get_file_iter(self, &iter))
  {
    gchar *uri;
    GFile *file;
    GFileInfo *info;
    GError *error = NULL;

    /* Get the value of cells referenced by a tree_modle */
    gtk_tree_model_get(GTK_TREE_MODEL(self->priv->model), &iter,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri, -1);

    file = g_file_new_for_uri(uri);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_UNIX_MODE,
                              G_FILE_QUERY_INFO_NONE, NULL, NULL);
    g_object_unref (file);

    /* Change the file information */
    if (info)
    {
      guint32 mode =
          g_file_info_get_attribute_uint32 (info,
                                            G_FILE_ATTRIBUTE_UNIX_MODE);
      if (readonly)
        mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
      else
        mode |= (S_IWUSR | S_IWGRP);

      g_file_info_set_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE, mode);
      g_object_unref (info);
    }

    if (error) {
            GtkWidget *infnote;
            infnote = hildon_note_new_information(GTK_WINDOW(self), error->message);
            if (infnote) {
                    gtk_widget_show(infnote);
                    gtk_dialog_run((GtkDialog *) infnote);
                    gtk_widget_destroy(infnote);
            }
            g_error_free (error);
    }

    g_free(uri);
  }
}

/* Cancel changes if read-only is changed */
static void
hildon_file_details_dialog_response(GtkDialog *dialog, gint response_id)
{
    if (response_id == GTK_RESPONSE_OK)
    {
      HildonFileDetailsDialog *self;
      gboolean state;

      self = HILDON_FILE_DETAILS_DIALOG(dialog);
      state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->priv->file_readonly));

      if (state != self->priv->checkbox_original_state)
        change_state(self, state);
    }
}

static void
hildon_file_details_dialog_map(GtkWidget *widget)
{
  HildonFileDetailsDialogPrivate *priv;

  priv = HILDON_FILE_DETAILS_DIALOG(widget)->priv;

  /* Map the GtkWidget */
  GTK_WIDGET_CLASS(file_details_dialog_parent_class)->map(widget);
}

static void
hildon_file_details_dialog_class_init(HildonFileDetailsDialogClass * klass)
{
    GObjectClass *gobject_class;

    file_details_dialog_parent_class = g_type_class_peek_parent(klass);
    gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = hildon_file_details_dialog_finalize;
    gobject_class->get_property = hildon_file_details_dialog_get_property;
    gobject_class->set_property = hildon_file_details_dialog_set_property;
    GTK_WIDGET_CLASS(klass)->map = hildon_file_details_dialog_map;
    GTK_DIALOG_CLASS(klass)->response = hildon_file_details_dialog_response;

    g_type_class_add_private(klass, sizeof(HildonFileDetailsDialogPrivate));

  /**
   * HildonFileDetailsDialog:additional-tab:
   *
   * @Deprecated: since 2.22: There is no support for tabs in details dialog.
   */
  g_object_class_install_property( gobject_class, PROP_ADDITIONAL_TAB,
                                   g_param_spec_object("additional-tab",
                                   "Additional tab",
                                   "Tab to show additinal information",
                                   GTK_TYPE_WIDGET,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  /**
   * HildonFileDetailsDialog:show-tabs:
   *
   * @Deprecated: since 2.22: There is no support for tabs in details dialog.
   */
  g_object_class_install_property( gobject_class, PROP_SHOW_TABS,
                                   g_param_spec_boolean("show-tabs",
                                   "Show tab labels",
                                   "Do we want to show the tab label.",
                                   FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  /**
   * HildonFileDetailsDialog:additional-tab-label:
   *
   * @Deprecated: since 2.22: There is no support for tabs in details dialog.
   */
  g_object_class_install_property( gobject_class, PROP_ADDITIONAL_TAB_LABEL,
                                   g_param_spec_string("additional-tab-label",
                                   "Additional tab label",
                                   "Label to the additional tab",
                                   NULL, G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class, PROP_MODEL,
                 g_param_spec_object("model", "Model",
                 "HildonFileSystemModel to use when fetching information",
                 HILDON_TYPE_FILE_SYSTEM_MODEL, G_PARAM_READWRITE));

  /**
   * HildonFileDetailsDialog:enable-read-only-checkbox:
   *
   * Whether or not to enable the read-only checkbox.
   */
  g_object_class_install_property(gobject_class,
                 PROP_ENABLE_READONLY_CHECKBOX,
                 g_param_spec_boolean("enable-read-only-checkbox",
                 "Enable read-only checkbox",
                 "Whether or not to enable the read-only checkbox.",
                 TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * HildonFileDetailsDialog:show-type-icon
   *
   * Whether or not to show the file icon next to the file type
   */
  g_object_class_install_property(gobject_class,
                 PROP_SHOW_TYPE_ICON,
                 g_param_spec_boolean("show-type-icon",
                 "Show file type icon",
                 "Whether or not to show the file icon next to the file type.",
                 FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
hildon_file_details_dialog_init(HildonFileDetailsDialog *self)
{
    GtkWidget *vbox;
    GtkSizeGroup *group;
    GdkGeometry geometry;

    HildonFileDetailsDialogPrivate *priv;

    /* Initialize the private property */
    self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
                                HILDON_TYPE_FILE_DETAILS_DIALOG,
                                HildonFileDetailsDialogPrivate);
    priv->notebook = GTK_NOTEBOOK(gtk_notebook_new());
    priv->scroll = hildon_pannable_area_new();
    priv->tab_label = gtk_label_new(_("sfil_ti_notebook_file"));
    g_object_ref(priv->tab_label);
    g_object_ref_sink (GTK_OBJECT(priv->tab_label));
    gtk_widget_show(priv->tab_label);

    priv->vbox = vbox = gtk_vbox_new(FALSE, 0);
    priv->sizegroup = group = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

    /* Add labels to the dialog */
    self->priv->file_name = hildon_file_details_dialog_add_label_with_value (self,
      _("ckdg_fi_properties_name_prompt"), "...");
    self->priv->file_type = hildon_file_details_dialog_add_label_with_value (self,
      _("ckdg_fi_properties_type_prompt"), "...");
    self->priv->file_location = hildon_file_details_dialog_add_label_with_value (self,
      _("sfil_fi_properties_location_prompt"), "...");
    self->priv->file_device = hildon_file_details_dialog_add_label_with_value (self,
      _("sfil_fi_properties_device_prompt"), "...");
    self->priv->file_date = hildon_file_details_dialog_add_label_with_value (self,
        _("ckdg_fi_properties_date_prompt"), "...");
    self->priv->file_time = hildon_file_details_dialog_add_label_with_value (self,
        _("ckdg_fi_properties_time_prompt"), "...");
    self->priv->file_size = hildon_file_details_dialog_add_label_with_value (self,
        _("ckdg_fi_properties_size_prompt"), "...");
    self->priv->file_readonly = hildon_file_details_dialog_add_label_with_value (self,
        _("ckdg_fi_properties_read_only"), "");

    //gtk_box_pack_start (GTK_BOX (self->priv->vbox), self->priv->file_readonly, FALSE, TRUE, 0);

       hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (priv->scroll), 
					       GTK_WIDGET (GTK_BOX(vbox)));
       g_object_set (G_OBJECT (priv->scroll), "mov-mode", HILDON_MOVEMENT_MODE_BOTH, NULL);

       gtk_box_pack_start(GTK_BOX(GTK_DIALOG(self)->vbox),
			  GTK_WIDGET(priv->scroll), TRUE, TRUE, 0);

    gtk_widget_show_all(GTK_WIDGET(GTK_BOX(GTK_DIALOG(self)->vbox)));
    gtk_widget_hide (self->priv->file_readonly);

    gtk_window_set_title (GTK_WINDOW (self), _("sfil_ti_file_details"));

    /* From widget specs, generic dialog size */
    geometry.min_width = 133;
    geometry.max_width = 602;
    /* Scrolled windows do not ask space for whole contents in size_request.
       So, we must force the dialog to have larger than minimum size */
    geometry.min_height = 240 + (2 * HILDON_MARGIN_DEFAULT);
    geometry.max_height = 240 + (2 * HILDON_MARGIN_DEFAULT);

    gtk_window_set_geometry_hints(GTK_WINDOW(self),
                                  GTK_WIDGET(priv->notebook), &geometry,
                                  GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
    gtk_widget_show_all(GTK_WIDGET(priv->notebook));
    gtk_widget_set_size_request (GTK_WIDGET (self), 400, -1);
}

static void
hildon_file_details_dialog_set_property(GObject *object, guint param_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  HildonFileDetailsDialogPrivate *priv;
  GtkNotebook *notebook;
  GtkLabel *label;

  priv = HILDON_FILE_DETAILS_DIALOG(object)->priv;
  notebook = priv->notebook;
  label = GTK_LABEL(priv->tab_label);

  switch( param_id )
  {
    case PROP_SHOW_TABS:
    {
      gtk_notebook_set_show_tabs(notebook, g_value_get_boolean(value));
      gtk_notebook_set_show_border(notebook, g_value_get_boolean(value));
      break;
    }
    case PROP_ADDITIONAL_TAB:
    {
      GtkWidget *widget = g_value_get_object(value);
      GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

      if (gtk_notebook_get_n_pages(notebook) == 2)
        gtk_notebook_remove_page(notebook, 1);

      if (widget == NULL)
      {
        widget = g_object_new(GTK_TYPE_LABEL,
            "label", _("sfil_ia_filetype_no_details"), "yalign", 0.0f, NULL);
        gtk_widget_show(widget);
      }

      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), widget);
      gtk_viewport_set_shadow_type(GTK_VIEWPORT(GTK_BIN(sw)->child),
                                   GTK_SHADOW_NONE);
      gtk_widget_show_all(sw);
      gtk_notebook_append_page(notebook, sw, priv->tab_label);
      gtk_notebook_set_current_page(notebook, 0);
      break;
    }
    case PROP_ADDITIONAL_TAB_LABEL:
      gtk_label_set_text(label, g_value_get_string(value));
      break;
    case PROP_MODEL:
    {
      HildonFileSystemModel *new_model = g_value_get_object(value);
      if (new_model != priv->model)
      {
        if (priv->model)
        {
          g_signal_handler_disconnect(priv->model, priv->delete_handler);
          g_object_unref(priv->model);
          priv->delete_handler = 0;
        }
        priv->model = new_model;
        if (new_model)
        {
          g_object_ref(new_model);
          priv->delete_handler = g_signal_connect(priv->model, "row-deleted",
            G_CALLBACK(check_validity), object);
        }
      }
      break;
    }
    case PROP_ENABLE_READONLY_CHECKBOX:
    {
      gtk_widget_set_sensitive(priv->file_readonly,
                               g_value_get_boolean(value));
      break;
    }
    case PROP_SHOW_TYPE_ICON:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
  }
}

static void
hildon_file_details_dialog_get_property( GObject *object, guint param_id,
                                                           GValue *value, GParamSpec *pspec )
{
  HildonFileDetailsDialogPrivate *priv;

  priv = HILDON_FILE_DETAILS_DIALOG(object)->priv;

  switch (param_id)
  {
    case PROP_SHOW_TABS:
      g_value_set_boolean(value, gtk_notebook_get_show_tabs(priv->notebook));
      break;
    case PROP_ADDITIONAL_TAB:
      g_assert(gtk_notebook_get_n_pages(priv->notebook) == 2);
      g_value_set_object(value, gtk_notebook_get_nth_page(priv->notebook, 1));
      break;
    case PROP_ADDITIONAL_TAB_LABEL:
      g_value_set_string(value, gtk_label_get_text(GTK_LABEL(priv->tab_label)));
      break;
    case PROP_MODEL:
      g_value_set_object(value, priv->model);
      break;
    case PROP_SHOW_TYPE_ICON:
      g_value_set_boolean (value, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
  }
}

static void hildon_file_details_dialog_finalize(GObject * object)
{
  HildonFileDetailsDialogPrivate *priv;

  g_return_if_fail(HILDON_IS_FILE_DETAILS_DIALOG(object));

  priv = HILDON_FILE_DETAILS_DIALOG(object)->priv;
  if (priv->model)
  {
    g_signal_handler_disconnect(priv->model, priv->delete_handler);
    g_object_unref(priv->model);
  }
  if (priv->tab_label)
    g_object_unref(priv->tab_label);
  if (priv->active_file)
    gtk_tree_row_reference_free(priv->active_file);
  if (priv->sizegroup)
    g_object_unref (priv->sizegroup);

  G_OBJECT_CLASS(file_details_dialog_parent_class)->finalize(object);
}

/*******************/
/* Public functions */
/*******************/

/**
 * hildon_file_details_dialog_new:
 * @parent: the parent window.
 * @filename: the filename.
 *
 * Creates a new #hildon_file_details_dialog AND new underlying
 * HildonFileSystemModel. Be carefull with #filename
 * parameter: You don't get any notification if something fails.
 *
 * Deprecated: use hildon_file_details_dialog_new_with_model() instead.
 *
 * Returns: a new #HildonFileDetailsDialog.
 */
#ifndef HILDON_DISABLE_DEPRECATED
GtkWidget *hildon_file_details_dialog_new(GtkWindow * parent,
                                          const gchar * filename)
{
  HildonFileDetailsDialog *dialog;
  HildonFileSystemModel *model;
  GtkTreeIter iter;

  model = g_object_new(HILDON_TYPE_FILE_SYSTEM_MODEL, NULL);

  dialog =
        g_object_new(HILDON_TYPE_FILE_DETAILS_DIALOG,
          "has-separator", FALSE, "model", model, NULL);

  if (filename && filename[0] &&
    hildon_file_system_model_load_local_path(dialog->priv->model, filename, &iter))
      hildon_file_details_dialog_set_file_iter(dialog, &iter);

  if (parent)
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

  return GTK_WIDGET(dialog);
}
#endif
/**
 * hildon_file_details_dialog_new_with_model:
 * @parent: the parent window.
 * @model: a #HildonFileSystemModel object used to fetch data.
 *
 * This is the preferred way to create #HildonFileDetailsDialog.
 * You can use a shared model structure to save loading times
 * (because you probably already have one at your disposal).
 *
 * Returns: a new #HildonFileDetailsDialog.
 */
GtkWidget *hildon_file_details_dialog_new_with_model(GtkWindow *parent,
  HildonFileSystemModel *model)
{
  GtkWidget *dialog;

  dialog = g_object_new(HILDON_TYPE_FILE_DETAILS_DIALOG,
    "has-separator", FALSE, "model", model, NULL);

  if (parent)
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

  return dialog;
}

static void
hildon_caption_set_value (HildonCaption *caption,
                          const gchar   *value)
{
    GtkWidget *label = gtk_bin_get_child (GTK_BIN (caption));
    gtk_label_set_label (GTK_LABEL (label), value);
}

/**
 * hildon_file_details_dialog_set_file_iter:
 * @self: a #HildonFileDetailsDialog.
 * @iter: a #GtkTreeIter pointing to desired file.
 *
 * Sets the dialog to display information about a file defined by
 * given iterator.
 */
void hildon_file_details_dialog_set_file_iter(HildonFileDetailsDialog *self, GtkTreeIter *iter)
{
  GtkTreeModel *model;
  GtkTreePath *path, *path_old;
  GtkTreeIter temp_iter, parent_iter;
  gchar *name, *mime, *uri, *size_string, *desc;
  const gchar *fmt;
  gint64 time_stamp, size;
  struct tm *time_struct;
  time_t time_val;
  gint type;
  gboolean location_readonly = TRUE;

  g_return_if_fail(HILDON_IS_FILE_DETAILS_DIALOG(self));

  model = GTK_TREE_MODEL(self->priv->model);

  /* Save iterator to priv struct as row reference */
  if (self->priv->active_file)
          path_old = gtk_tree_row_reference_get_path(self->priv->active_file);
  else
          path_old = NULL;
  path = gtk_tree_model_get_path(model, iter);
  g_return_if_fail(path); // add some safety with logging here to clear up bug NB#51729, NB#52272, NB#52271

  /* check if we are called for the same file again */
  if (path_old && gtk_tree_path_compare(path, path_old) == 0) {
          gtk_tree_path_free(path);
          gtk_tree_path_free(path_old);
          return;
  }

  gtk_tree_row_reference_free(self->priv->active_file);
  self->priv->active_file = gtk_tree_row_reference_new(model, path);
  gtk_tree_path_free(path);
  gtk_tree_path_free(path_old);

  /* Setup the view */
  gtk_tree_model_get(model, iter,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME, &name,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_MIME_TYPE, &mime,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &uri,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_SIZE, &size,
    HILDON_FILE_SYSTEM_MODEL_COLUMN_FILE_TIME, &time_stamp,
    -1);

  hildon_caption_set_value (HILDON_CAPTION (self->priv->file_name), name);
  desc = g_content_type_get_description (mime);
  hildon_caption_set_value (HILDON_CAPTION (self->priv->file_type), desc);
  g_free (desc);

  if (!(mime && *mime))
      g_warning("COLUMN_MIME_TYPE contains empty mime type for file: %s", name);

  size_string = hildon_format_file_size_for_display (size);
  hildon_caption_set_value (HILDON_CAPTION (self->priv->file_size), size_string);
  g_free (size_string);

  if (time_stamp != 0)
    {
      gchar buffer[256];
      /* Too bad. We cannot use GDate function, because it doesn't handle
	 time, just dates */
      time_val = (time_t) time_stamp;
      time_struct = localtime(&time_val);
      
      /* There are no more logical names for these. We are allowed
	 to hardcode */
      strftime(buffer, sizeof(buffer), "%X", time_struct);
      hildon_caption_set_value (HILDON_CAPTION (self->priv->file_time), buffer);
      
      /* If format is passed directly to strftime, gcc complains about
	 that some locales use only 2 digit year numbers. Using
	 a temporary disable this warning (from strftime man page) */
      fmt = "%x";
      strftime(buffer, sizeof(buffer), dgettext("hildon-libs", "wdgt_va_date"),
    		  time_struct);
      hildon_caption_set_value (HILDON_CAPTION (self->priv->file_date), buffer);
    }
  else
    {
      hildon_caption_set_value (HILDON_CAPTION (self->priv->file_time), "-");
      hildon_caption_set_value (HILDON_CAPTION (self->priv->file_date), "-");
    }

  {
    GdkPixbuf *icon;

    gtk_tree_model_get
      (model, iter,
       HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON, &icon,
       -1);
    if (icon)
      {
        hildon_caption_set_icon_image (HILDON_CAPTION (self->priv->file_type),
                                       gtk_image_new_from_pixbuf (icon));
        g_object_unref (icon);
      }
  }

  /* Parent information */
  if (gtk_tree_model_iter_parent(model, &parent_iter, iter))
  {
    gchar *location_name, *parent_path;
    GdkPixbuf *location_icon;

    gtk_tree_model_get(model, &parent_iter,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME, &location_name,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_URI, &parent_path,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON, &location_icon, -1);

    if (parent_path)
      location_readonly = !write_access(parent_path);

    hildon_caption_set_value (HILDON_CAPTION (self->priv->file_location), location_name);
    hildon_caption_set_icon_image (HILDON_CAPTION (self->priv->file_location),
                                       gtk_image_new_from_pixbuf (location_icon));

    if (location_icon)
      g_object_unref(location_icon);
    g_free(location_name);

    /* Go upwards in model until we find a device node */
    while (TRUE)
    {
      gtk_tree_model_get(model, &parent_iter,
        HILDON_FILE_SYSTEM_MODEL_COLUMN_TYPE, &type, -1);

      if (type >= HILDON_FILE_SYSTEM_MODEL_MMC)
        break;

      if (gtk_tree_model_iter_parent(model, &temp_iter, &parent_iter))
        parent_iter = temp_iter;
      else
        break;
    }

    gtk_tree_model_get(model, &parent_iter,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_DISPLAY_NAME, &location_name,
      HILDON_FILE_SYSTEM_MODEL_COLUMN_ICON, &location_icon,
      -1);

    hildon_caption_set_value (HILDON_CAPTION (self->priv->file_device),
                              location_name);
    hildon_caption_set_icon_image (HILDON_CAPTION (self->priv->file_device),
                              gtk_image_new_from_pixbuf (location_icon));

    if (location_icon)
      g_object_unref(location_icon);
    g_free(location_name);
    g_free(parent_path);
  }
  else
  {   /* We really should not come here */
      g_warn_if_reached ();
      hildon_caption_set_value (HILDON_CAPTION (self->priv->file_location), "...");
      hildon_caption_set_icon_image (HILDON_CAPTION (self->priv->file_location), NULL);
      hildon_caption_set_value (HILDON_CAPTION (self->priv->file_device), "...");
      hildon_caption_set_icon_image (HILDON_CAPTION (self->priv->file_device), NULL);
  }

  if (location_readonly || !write_access(uri)) {
    gtk_widget_show (self->priv->file_readonly);
  }

  g_free(uri);
  g_free(name);
  g_free(mime);
}

/**
 * hildon_file_details_dialog_get_file_iter:
 * @self: a #HildonFileDetailsDialog.
 * @iter: a #GtkTreeIter to be filled.
 *
 * Gets an iterator pointing to displayed file.
 *
 * Returns: %TRUE, if dialog is displaying some information.
 */
gboolean
hildon_file_details_dialog_get_file_iter(HildonFileDetailsDialog *self, GtkTreeIter *iter)
{
  GtkTreePath *path;
  gboolean result;

  g_return_val_if_fail(HILDON_IS_FILE_DETAILS_DIALOG(self), FALSE);

  if (!self->priv->active_file)
    return FALSE;
  path = gtk_tree_row_reference_get_path(self->priv->active_file);
  if (!path)
    return FALSE;

  result = gtk_tree_model_get_iter(GTK_TREE_MODEL(self->priv->model), iter, path);
  gtk_tree_path_free(path);

  return result;
}

/**
 * hildon_file_details_dialog_add_label_with_value:
 * @dialog: the dialog
 * @label: a label string
 * @value: a value string
 *
 * Adds an additional row with a label and a string to the dialog.
 *
 * Returns: a #HildonCaption
 *
 * Since: 2.13
 */
GtkWidget*
hildon_file_details_dialog_add_label_with_value (HildonFileDetailsDialog *dialog,
                                                 const gchar             *label,
                                                 const gchar             *value)
{
  GtkWidget* value_widget;
  GtkWidget* caption;

  g_return_val_if_fail (HILDON_IS_FILE_DETAILS_DIALOG (dialog), NULL);
  g_return_val_if_fail (label != NULL, NULL);
  g_return_val_if_fail (value != NULL, NULL);

  value_widget = g_object_new (GTK_TYPE_LABEL,
                               "xalign", 0.0f,
                               "ellipsize", PANGO_ELLIPSIZE_NONE,
                               "label", value,
                               "visible", TRUE,
                               NULL);
  caption = hildon_caption_new (dialog->priv->sizegroup,
      label, value_widget, NULL, HILDON_CAPTION_OPTIONAL);
  hildon_caption_set_separator (HILDON_CAPTION (caption), "");
  gtk_widget_show (caption);
  gtk_box_pack_start (GTK_BOX (dialog->priv->vbox), caption, FALSE, TRUE, 0);
  return caption;
}


/**
 * hildon_format_file_size_for_display:
 * @size: a size in bytes
 *
 * Formats a file size in bytes for display in applications.
 *
 * This function is similar to g_format_file_size_for_display but the
 * translations are from Maemo so might differ slightly.
 *
 * Returns: a newly allocated string with the formatted size.
 * Since: 2.1.7
 */
gchar *
hildon_format_file_size_for_display(gint64 file_size)
{
	if (file_size < 1024)
	    return g_strdup_printf(_("ckdg_va_properties_size_kb"), 1);
	else if (file_size < 100 * 1024)
	    return g_strdup_printf(_("ckdg_va_properties_size_1kb_99kb"),
               (int)(file_size / (gint64)1024));
	else if (file_size < 1024 * 1024)
		return g_strdup_printf(_("ckdg_va_properties_size_100kb_1mb"),
               (int)(file_size / (gint64)1024));
	else if (file_size < 10 * 1024 * 1024)
		return g_strdup_printf(_("ckdg_va_properties_size_1mb_10mb"),
               file_size / (1024.0f * 1024.0f));
	else if (file_size < 1024 * 1024 * 1024)
		return g_strdup_printf(_("ckdg_va_properties_size_10mb_1gb"),
               file_size / (1024.0f * 1024.0f));
	else
		return g_strdup_printf(_("ckdg_va_properties_size_1gb_or_greater"),
				       file_size / (1024.0 * 1024.0 * 1024.0));
}

