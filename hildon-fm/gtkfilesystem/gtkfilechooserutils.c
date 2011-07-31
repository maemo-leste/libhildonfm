/* GTK - The GIMP Toolkit
 * gtkfilechooserutils.c: Private utility functions useful for
 *                        implementing a GtkFileChooser interface
 * Copyright (C) 2003, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>
#include "gtkfilechooserutils.h"
#include "gtkfilesystem.h"

#define I_(foo) (foo)

/**
 * _gtk_file_chooser_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 *
 * Installs the necessary properties for a class implementing
 * #GtkFileChooser. A #GtkParamSpecOverride property is installed
 * for each property, using the values from the #GtkFileChooserProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don't collide with some other property values they
 * are using.
 **/
void
_gtk_file_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_ACTION,
				    "action");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_EXTRA_WIDGET,
				    "extra-widget");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND,
				    "file-system-backend");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_FILTER,
				    "filter");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_LOCAL_ONLY,
				    "local-only");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET,
				    "preview-widget");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_PREVIEW_WIDGET_ACTIVE,
				    "preview-widget-active");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_USE_PREVIEW_LABEL,
				    "use-preview-label");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
				    "select-multiple");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_SHOW_HIDDEN,
				    "show-hidden");
  g_object_class_override_property (klass,
				    GTK_FILE_CHOOSER_PROP_DO_OVERWRITE_CONFIRMATION,
				    "do-overwrite-confirmation");
}

#ifdef MAEMO_CHANGES
/**
 * hildon_gtk_file_chooser_install_properties:
 *
 * Exactly the same as the private _gtk_file_chooser_install_properties()
 * but exported for hildon-fm.
 *
 * Since: maemo 2.0
 * Stability: Unstable
 */
void
hildon_gtk_file_chooser_install_properties (GObjectClass *klass)
{
  _gtk_file_chooser_install_properties (klass);
}
#endif /* MAEMO_CHANGES */
