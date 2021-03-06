/*
 * This file is part of hildon-fm
 *
 * Copyright (C) 2005 Nokia Corporation.  All rights reserved.
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

#ifndef __HILDON_FILE_DETAILS_DIALOG_H__
#define __HILDON_FILE_DETAILS_DIALOG_H__

#include <gtk/gtk.h>
#include "hildon-file-system-model.h"

G_BEGIN_DECLS

#define HILDON_TYPE_FILE_DETAILS_DIALOG \
  (hildon_file_details_dialog_get_type ())
#define HILDON_FILE_DETAILS_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), HILDON_TYPE_FILE_DETAILS_DIALOG,\
   HildonFileDetailsDialog))
#define HILDON_FILE_DETAILS_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), HILDON_TYPE_FILE_DETAILS_DIALOG,\
   HildonFileDetailsDialogClass))
#define HILDON_IS_FILE_DETAILS_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HILDON_TYPE_FILE_DETAILS_DIALOG))
#define HILDON_IS_FILE_DETAILS_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), HILDON_TYPE_FILE_DETAILS_DIALOG))
#define HILDON_FILE_DETAILS_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), HILDON_TYPE_FILE_DETAILS_DIALOG,\
   HildonFileDetailsDialogClass))

typedef struct _HildonFileDetailsDialog HildonFileDetailsDialog;
typedef struct _HildonFileDetailsDialogPrivate HildonFileDetailsDialogPrivate;
typedef struct _HildonFileDetailsDialogClass HildonFileDetailsDialogClass;

struct _HildonFileDetailsDialog {
  GtkDialog parent;
  HildonFileDetailsDialogPrivate *priv;
};

struct _HildonFileDetailsDialogClass {
  GtkDialogClass parent_class;
};

GType hildon_file_details_dialog_get_type(void) G_GNUC_CONST;

/* Depricated constructor... */
#ifndef HILDON_DISABLE_DEPRECATED
GtkWidget *hildon_file_details_dialog_new(GtkWindow * parent,
  const gchar * filename);
#endif

/* New API inspired by GtkComboBox */
GtkWidget *hildon_file_details_dialog_new_with_model(GtkWindow *parent,
  HildonFileSystemModel *model);

void hildon_file_details_dialog_set_file_iter(HildonFileDetailsDialog *self, GtkTreeIter *iter);
gboolean hildon_file_details_dialog_get_file_iter(HildonFileDetailsDialog *self, GtkTreeIter *iter);

GtkWidget *
hildon_file_details_dialog_add_label_with_value (HildonFileDetailsDialog *dialog,
                                                 const gchar             *label,
                                                 const gchar             *value);

// Helper function
gchar *
hildon_format_file_size_for_display (gint64 size);


G_END_DECLS

#endif /* __HILDON_FILEDETAILS_H__ */
