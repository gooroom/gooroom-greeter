desktop_basedir = $(datadir)/images/desktop-base
desktop_base_DATA = gooroom-greeter-bg.jpg

backgroundsdir = $(pkgdatadir)/backgrounds
backgrounds_DATA = \
	logo-bg-001.jpg \
	logo-bg-002.jpg \
	logo-bg-003.jpg \
	logo-bg-004.jpg \
	logo-bg-005.jpg \
	logo-bg-006.jpg \
	logo-bg-007.jpg \
	external-login-bg.png \
	internal-login-bg.png

iconsymbolicstatusdir = $(datadir)/icons/hicolor/scalable/status
iconsymbolicstatus_DATA = \
	mode-internal-symbolic.svg \
	mode-external-symbolic.svg \
	entry-password-symbolic.svg \
	entry-username-symbolic.svg \
	splash-spinner-symbolic.svg \
	arrow-forward-symbolic.svg \
	arrow-backward-symbolic.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated. After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = $(desktop_base_DATA)
