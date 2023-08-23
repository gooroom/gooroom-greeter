desktop_basedir = $(datadir)/images/desktop-base
desktop_base_DATA = gooroom-greeter-bg.png

iconsymbolicstatusdir = $(datadir)/icons/hicolor/scalable/status
iconsymbolicstatus_DATA = \
	greeter-spinner-symbolic.svg \
	greeter-checkbox-checked.svg \
	greeter-checkbox-unchecked.svg \
	greeter-checkbox-unchecked-disabled.svg \
	greeter-arrow-symbolic.svg

iconsymbolicactionsdir = $(datadir)/icons/hicolor/scalable/actions
iconsymbolicactions_DATA = \
	greeter-system-reboot-symbolic.svg \
	greeter-system-suspend-symbolic.svg \
	greeter-system-shutdown-symbolic.svg \
	greeter-system-hibernate-symbolic.svg

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

#EXTRA_DIST = $(desktop_base_DATA)
