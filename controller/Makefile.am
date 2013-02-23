bin_PROGRAMS = seafile-controller

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/lib \
	@GLIB2_CFLAGS@ \
	@CCNET_CFLAGS@ \
	-Wall

noinst_HEADERS = seafile-controller.h application.h ../common/log.h

seafile_controller_SOURCES = seafile-controller.c application.c ../common/log.c

seafile_controller_LDADD = \
	@CCNET_LIBS@ \
	$(top_builddir)/lib/libseafile_common.la \
	@GLIB2_LIBS@

seafile_controller_LDFLAGS = @STATIC_COMPILE@
