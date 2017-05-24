/*
 * This file is part of hildon-fm package
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

/*
 * hildon-file-system-private.h
 *
 * Functions in this package are internal helpers of the
 * HildonFileSystem and should not be called by
 * applications.
 */

#ifndef __HILDON_FILE_SYSTEM_PRIVATE_H__
#define __HILDON_FILE_SYSTEM_PRIVATE_H__

#include <gtk/gtk.h>
#include "hildon-file-system-common.h"
#include "hildon-file-system-special-location.h"

G_BEGIN_DECLS

#define TREE_ICON_SIZE 26 /* Left side icons */

gboolean 
_hildon_file_system_compare_ignore_last_separator(const char *a, const char *b);

GNode *_hildon_file_system_get_locations(void);

HildonFileSystemSpecialLocation *
_hildon_file_system_get_special_location(GFile *file);

GdkPixbuf *_hildon_file_system_create_image (GtkFileSystem *fs, 
					     GtkWidget *ref_widget,
					     GFileInfo *info, 
					     HildonFileSystemSpecialLocation *location,
					     gint size);

gchar *_hildon_file_system_create_file_name(GFile *path,
  HildonFileSystemSpecialLocation *location, GFileInfo *info);

gchar *_hildon_file_system_create_display_name(GtkFileSystem *fs, 
  GFile *file, HildonFileSystemSpecialLocation *location,
  GFileInfo *info);

GFile *_hildon_file_system_path_for_location(HildonFileSystemSpecialLocation *location);

GtkFileSystemVolume *
_hildon_file_system_get_volume_for_location(GtkFileSystem *fs,
					    HildonFileSystemSpecialLocation *location);

gchar *_hildon_file_system_search_extension(gchar *name,
					    gboolean only_known,
					    gboolean is_folder);
gboolean _hildon_file_system_is_known_extension (const gchar *ext);
long _hildon_file_system_parse_autonumber(const char *start);
void _hildon_file_system_remove_autonumber(char *name);

GdkPixbuf *_hildon_file_system_load_icon_cached(GtkIconTheme *theme, 
  const gchar *name, gint size);

char *hildon_file_system_unescape_string (const char *escaped);

/*#define DEBUG */

#ifdef DEBUG
#define DEBUG_GFILE_URI(fmt, f, ...) { \
  gchar *___uri___ = g_file_get_uri (f);  \
  g_warning ("%s "fmt, __FUNCTION__, ___uri___, ##__VA_ARGS__);\
  g_free (___uri___); \
  }
#else
#define DEBUG_GFILE_URI(f, ...)
#endif

#define WARN_GFILE_URI(fmt, f, ...) { \
  gchar *___uri___ = g_file_get_uri (f);  \
  g_warning (fmt, ___uri___, ##__VA_ARGS__);\
  g_free (___uri___); \
  }

G_END_DECLS

#endif
