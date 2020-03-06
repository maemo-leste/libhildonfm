MAINTAINERCLEANFILES = Makefile.in

TEST_PROGS = 
noinst_PROGRAMS	= $(TEST_PROGS)
tests_ldadd = $(GTK_LIBS) $(HILDON_LIBS) $(GIO_LIBS) $(GCONF_LIBS) \
              $(top_builddir)/hildon-fm/libhildonfm.la $(HILDON_THUMBNAIL_LIBS)
tests_ldflags = -module -avoid-version
tests_cflags = $(GTK_CFLAGS) $(HILDON_CFLAGS) $(GCONF_CFLAGS) $(GIO_CFLAGS) \
               -I$(top_srcdir)/hildon-fm $(WARN_CFLAGS)

TEST_PROGS += check_test
check_test_SOURCES = check_test.c
check_test_LDADD = $(tests_ldadd)
check_test_LDFLAGS = $(tests_ldflags)
check_test_CFLAGS = $(tests_cflags)

TEST_PROGS += file_selection
file_selection_SOURCES = check-hildonfm-file-selection.c
file_selection_LDADD = $(tests_ldadd)
file_selection_LDFLAGS = $(tests_ldflags)
file_selection_CFLAGS = $(tests_cflags)

TEST_PROGS += file_chooser_dialog
file_chooser_dialog_SOURCES = check-hildonfm-file-chooser-dialog.c
file_chooser_dialog_LDADD = $(tests_ldadd)
file_chooser_dialog_LDFLAGS = $(tests_ldflags)
file_chooser_dialog_CFLAGS = $(tests_cflags)

TEST_PROGS += file_details_dialog
file_details_dialog_SOURCES = check-hildonfm-file-details-dialog.c
file_details_dialog_LDADD = $(tests_ldadd)
file_details_dialog_LDFLAGS = $(tests_ldflags)
file_details_dialog_CFLAGS = $(tests_cflags)

TEST_PROGS += file_system_model
file_system_model_SOURCES = check-hildonfm-file-system-model.c
file_system_model_LDADD = $(tests_ldadd)
file_system_model_LDFLAGS = $(tests_ldflags)
file_system_model_CFLAGS = $(tests_cflags)

TEST_PROGS += file_system_private
file_system_private_SOURCES = check-hildonfm-file-system-private.c
file_system_private_LDADD = $(tests_ldadd)
file_system_private_LDFLAGS = $(tests_ldflags)
file_system_private_CFLAGS = $(tests_cflags)

TEST_PROGS += file_system_settings
file_system_settings_SOURCES = check-hildonfm-file-system-settings.c
file_system_settings_LDADD = $(tests_ldadd)
file_system_settings_LDFLAGS = $(tests_ldflags)
file_system_settings_CFLAGS = $(tests_cflags)

TEST_PROGS += file_system_special_location
file_system_special_location_SOURCES = check-hildonfm-file-system-special-location.c
file_system_special_location_LDADD = $(tests_ldadd)
file_system_special_location_LDFLAGS = $(tests_ldflags)
file_system_special_location_CFLAGS = $(tests_cflags)

TEST_PROGS += file_system_storage_dialog
file_system_storage_dialog_SOURCES = check-hildonfm-file-system-storage-dialog.c
file_system_storage_dialog_LDADD = $(tests_ldadd)
file_system_storage_dialog_LDFLAGS = $(tests_ldflags)
file_system_storage_dialog_CFLAGS = $(tests_cflags)

TEST_PROGS += file_system_voldev
file_system_voldev_SOURCES = check-hildonfm-file-system-voldev.c
file_system_voldev_LDADD = $(tests_ldadd)
file_system_voldev_LDFLAGS = $(tests_ldflags)
file_system_voldev_CFLAGS = $(tests_cflags)

test: ${TEST_PROGS}
	for i in ${TEST_PROGS}; do echo "Running $$i..."; "${top_builddir}/tests/$$i"; done

check-local: test
