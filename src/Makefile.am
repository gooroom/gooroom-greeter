sbin_PROGRAMS = gooroom-greeter

BUILT_SOURCES = \
	greeter-resources.c \
	greeter-resources.h

gooroom_greeter_SOURCES = \
	$(BUILT_SOURCES) \
	indicator-button.h	\
	indicator-button.c	\
	gooroom-greeter.c \
	greeterbackground.h \
	greeterbackground.c \
	greeterconfiguration.h \
	greeterconfiguration.c \
	greeter-assistant.h \
	greeter-assistant.c \
	greeter-page.h \
	greeter-page.c \
	greeter-page-manager.h \
	greeter-page-manager.c \
	greeter-mode-page.h \
	greeter-mode-page.c \
	greeter-network-page.h \
	greeter-network-page.c \
	network-dialogs.h \
	network-dialogs.c \
	greeter-vpn-page.h \
	greeter-vpn-page.c \
	greeter-login-page.h \
	greeter-login-page.c \
	greeter-password-settings-dialog.h \
	greeter-password-settings-dialog.c \
	splash-window.h \
	splash-window.c \
	greeter-message-dialog.h \
	greeter-message-dialog.c

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DHAVE_CONFIG_H \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPKGDATA_DIR=\"$(pkgdatadir)\" \
	-DCONFIG_FILE=\"$(sysconfdir)/lightdm/gooroom-greeter.conf\" \
	-DINDICATOR_DIR=\"$(INDICATORDIR)\"	\
	-DGOOROOM_SPLASH=\"$(libdir)/gooroom-splash/gooroom-splash\" \
	-DGOOROOM_NOTIFYD=\"$(libdir)/gooroom-notifyd/gooroom-notifyd\" \
	-DVPN_SERVICE_CONFIG_DIR=\"$(datadir)/gooroom-vpn-service/services\" \
	-DGOOROOM_VPN_SERVICE_CONFIG_FILE=\"$(sysconfdir)/gooroom-vpn-service/vpn.conf\" \
	$(WARN_CFLAGS)

gooroom_greeter_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
    $(LIBNM_CFLAGS) \
    $(LIBNMA_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(UPOWER_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(LIGHTDMGOBJECT_CFLAGS) \
	$(LIBX11_CFLAGS) \
    $(AYATANA_INDICATOR_NG_CFLAGS)

gooroom_greeter_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
    $(LIBNM_LIBS) \
    $(LIBNMA_LIBS) \
	$(GMODULE_LIBS) \
	$(UPOWER_LIBS) \
	$(GTHREAD_LIBS) \
	$(LIGHTDMGOBJECT_LIBS) \
	$(LIBX11_LIBS) \
	$(AYATANA_INDICATOR_NG_LIBS) \
	-lm

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gresource.xml)
greeter-resources.c: gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name greeter $<
greeter-resources.h: gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name greeter $<

DISTCLEANFILES = \
	$(BUILT_SOURCES)
