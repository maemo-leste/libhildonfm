SUBDIRS = gtkfilesystem

INCLUDES = $(GTK_CFLAGS) $(HILDON_CFLAGS) $(GCONF_CFLAGS) $(HILDON_THUMBNAIL_CFLAGS)	\
	   $(GWCONNECT_CFLAGS) $(DBUSGLIB_CFLAGS) $(GNOMEVFS_CFLAGS)	\
           $(LIBOSSO_CFLAGS)  $(TRACKER_CFLAGS)				\
           $(MCE_CFLAGS) -DLOCALEDIR=\"$(localedir)\"			\
           $(LIBOSSOMIME_CFLAGS) -I$(srcdir)/..				\
           $(DEBUG_CPPFLAGS) -DG_LOG_DOMAIN=\"hildon-fm\"

AM_CFLAGS = $(WARN_CFLAGS) -fPIC
lib_LTLIBRARIES = libhildonfm.la


MAINTAINERCLEANFILES = \
	Makefile.in hildon-fm-enum-types.c hildon-fm-enum-types.h 

EXTRA_DIST = 

libhildonfm_la_LIBADD = $(GTK_LIBS) $(HILDON_LIBS) $(GCONF_LIBS) $(HILDON_THUMBNAIL_LIBS) \
	$(GWCONNECT_LIBS) $(DBUSGLIB_LIBS) $(LIBOSSOMIME_LIBS) $(GNOMEVFS_LIBS) $(TRACKER_LIBS) \
	gtkfilesystem/xdgmime/libxdgmime.la

libhildonfm_la_LDFLAGS = -version-info 2:0:0
libhildonfm_la_SOURCES =			\
	gtkfilesystem/gtkfilechooserutils.c	\
	gtkfilesystem/gtkfilechooserutils.h	\
	gtkfilesystem/gtkfilesystem.c		\
	gtkfilesystem/gtkfilesystemmodel.c	\
	gtkfilesystem/gtkfilesystemmodel.h	\
	gtkfilesystem/gtkfilesystemunix.c	\
	gtkfilesystem/gtkfilesystemunix.h	\
	gtkfilesystem/gtkfilechooserprivate.h	\
	hildon-file-selection.c			\
	hildon-file-system-model.c		\
	hildon-file-chooser-dialog.c		\
	hildon-file-system-storage-dialog.c	\
	hildon-file-system-private.c		\
	hildon-file-system-info.c		\
	hildon-file-system-settings.c		\
	hildon-file-details-dialog.c		\
	hildon-file-system-special-location.c	\
	hildon-file-system-root.c		\
	hildon-file-system-local-device.c	\
	hildon-file-system-remote-device.c	\
	hildon-file-system-dynamic-device.c	\
	hildon-file-system-upnp.c		\
	hildon-file-system-obex.c		\
	hildon-file-system-smb.c		\
	hildon-file-system-voldev.c		\
	hildon-fm1-compat.c			\
	$(hildonfm_HEADERS)			\
	$(hildonfm_built_headers)		\
	$(hildonfm_built_cfiles)

hildonincludeinstdir=$(includedir)/hildon-fm-2/hildon
hildonincludeinst_HEADERS =			\
	hildon-fm.h				\
	hildon-file-selection.h			\
	hildon-file-system-model.h		\
	hildon-file-chooser-dialog.h		\
	hildon-file-system-common.h		\
	hildon-file-system-info.h		\
	hildon-file-details-dialog.h		\
	hildon-file-system-storage-dialog.h	\
	hildon-file-system-dynamic-device.h	\
	hildon-file-system-local-device.h	\
	hildon-file-system-obex.h		\
	hildon-file-system-remote-device.h	\
	hildon-file-system-root.h		\
	hildon-file-system-smb.h		\
	hildon-file-system-special-location.h	\
	hildon-file-system-upnp.h		\
	hildon-file-system-voldev.h		\
	hildon-fm1-compat.h

gtkfilesystemincludeinstdir=$(includedir)/hildon-fm-2/hildon/gtkfilesystem
gtkfilesystemincludeinst_HEADERS = \
	gtkfilesystem/gtkfilesystem.h


headers_to_scan_for_enums = $(hildonincludeinst_HEADERS)
 
# Generate the enums source code, with glib-mkenums:
# This is based on the same Makefile.am stuff in pango:
hildonfm_built_headers = hildon-fm-enum-types.h
hildonfm_built_cfiles = hildon-fm-enum-types.c
 
# Don't build the library until we have built the header that it needs:
$(OBJECTS) $(libhildonfm_la_OBJECTS): $(hildonfm_built_headers)
 
hildon-fm-enum-types.h: @REBUILD@ $(headers_to_scan_for_enums) Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __HILDON_FM_ENUM_TYPES_H__\n#define __HILDON_FM_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define HILDON_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* __HILDON_FM_ENUM_TYPES_H__ */" \
		$(headers_to_scan_for_enums)) > $@

hildon-fm-enum-types.c: @REBUILD@ $(headers_to_scan_for_enums) Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"hildon-fm.h\"\n" \
			--fhead "#include \"hildon-fm-enum-types.h\"\n" \
		      	--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(headers_to_scan_for_enums)) > $@
