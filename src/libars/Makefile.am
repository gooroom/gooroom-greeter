noinst_LTLIBRARIES = libars.la

AM_CPPFLAGS = \
	-I$(top_builddir)

#BUILT_SOURCES = \
#	resources.c \
#	resources.h

#resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gresource.xml)
#resources.c: gresource.xml $(resource_files)
#	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
#resources.h: gresource.xml $(resource_files)
#	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<

libars_la_SOURCES = \
	ars-infos.h \
	gooroom-ars-auth.h	\
	gooroom-ars-auth.c

libars_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(CURL_CFLAGS)

libars_la_LIBADD = \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(CURL_LIBS)

libars_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

#EXTRA_DIST =            \
#	gresource.xml \
#	$(resource_files)   \
#	$(NULL)

#CLEANFILES = $(BUILT_SOURCES)

