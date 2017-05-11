/* GTK - The GIMP Toolkit
 * gtkfilechooserprivate.h: Interface definition for file selector GUIs
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

#ifndef __GTK_FILE_CHOOSER_PRIVATE_H__
#define __GTK_FILE_CHOOSER_PRIVATE_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "gtkfilesystem.h"
#include "gtkfilesystemmodel.h"
typedef struct _GtkQuery GtkQuery;
typedef struct _GtkSearchEngine GtkSearchEngine;

G_BEGIN_DECLS

#define GTK_FILE_CHOOSER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FILE_CHOOSER, GtkFileChooserIface))

typedef struct _GtkFileChooserIface GtkFileChooserIface;

struct _GtkFileChooserIface
{
  GTypeInterface base_iface;

  /* Methods
   */
  gboolean       (*set_current_folder) 	   (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  GFile *        (*get_current_folder) 	   (GtkFileChooser    *chooser);
  void           (*set_current_name)   	   (GtkFileChooser    *chooser,
					    const gchar       *name);
  gboolean       (*select_file)        	   (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  void           (*unselect_file)      	   (GtkFileChooser    *chooser,
					    GFile             *file);
  void           (*select_all)         	   (GtkFileChooser    *chooser);
  void           (*unselect_all)       	   (GtkFileChooser    *chooser);
  GSList *       (*get_files)          	   (GtkFileChooser    *chooser);
  GFile *        (*get_preview_file)   	   (GtkFileChooser    *chooser);
  GtkFileSystem *(*get_file_system)    	   (GtkFileChooser    *chooser);
  void           (*add_filter)         	   (GtkFileChooser    *chooser,
					    GtkFileFilter     *filter);
  void           (*remove_filter)      	   (GtkFileChooser    *chooser,
					    GtkFileFilter     *filter);
  GSList *       (*list_filters)       	   (GtkFileChooser    *chooser);
  gboolean       (*add_shortcut_folder)    (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  gboolean       (*remove_shortcut_folder) (GtkFileChooser    *chooser,
					    GFile             *file,
					    GError           **error);
  GSList *       (*list_shortcut_folders)  (GtkFileChooser    *chooser);
  
  /* Signals
   */
  void (*current_folder_changed) (GtkFileChooser *chooser);
  void (*selection_changed)      (GtkFileChooser *chooser);
  void (*update_preview)         (GtkFileChooser *chooser);
  void (*file_activated)         (GtkFileChooser *chooser);
  GtkFileChooserConfirmation (*confirm_overwrite) (GtkFileChooser *chooser);
};

GtkFileSystem *_gtk_file_chooser_get_file_system         (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_set_current_folder_path (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);
GtkFilePath *  _gtk_file_chooser_get_current_folder_path (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_select_path             (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);
void           _gtk_file_chooser_unselect_path           (GtkFileChooser    *chooser,
							  const GtkFilePath *path);
GSList *       _gtk_file_chooser_get_paths               (GtkFileChooser    *chooser);
GtkFilePath *  _gtk_file_chooser_get_preview_path        (GtkFileChooser    *chooser);
gboolean       _gtk_file_chooser_add_shortcut_folder     (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);
gboolean       _gtk_file_chooser_remove_shortcut_folder  (GtkFileChooser    *chooser,
							  const GtkFilePath *path,
							  GError           **error);

/* GtkFileSystemModel private */

typedef struct _FileModelNode           FileModelNode;

struct _GtkFileSystemModel
{
  GObject parent_instance;

  GtkFileSystem  *file_system;
  GtkFileInfoType types;
  FileModelNode  *roots;
  GtkFolder  *root_folder;
  GtkFilePath    *root_path;

  GtkFileSystemModelFilter filter_func;
  gpointer filter_data;

  GSList *idle_clears;
  GSource *idle_clear_source;

  gushort max_depth;

  GSList *pending_handles;
  
  guint show_hidden : 1;
  guint show_folders : 1;
  guint show_files : 1;
  guint folders_only : 1;
  guint has_editable : 1;
};

struct _FileModelNode
{
  GtkFilePath *path;
  FileModelNode *next;

  GtkFileInfo *info;
  GtkFolder *folder;
  
  FileModelNode *children;
  FileModelNode *parent;
  GtkFileSystemModel *model;

  guint ref_count;
  guint n_referenced_children;

  gushort depth;

  guint has_dummy : 1;
  guint is_dummy : 1;
  guint is_visible : 1;
  guint loaded : 1;
  guint idle_clear : 1;
  guint load_pending : 1;
};


G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_PRIVATE_H__ */
