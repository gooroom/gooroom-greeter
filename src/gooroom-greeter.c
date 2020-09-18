/*
 * Copyright (C) 2010 - 2011, Robert Ancell <robert.ancell@canonical.com>
 * Copyright (C) 2011, Gunnar Hjalmarsson <ubuntu@gunnar.cc>
 * Copyright (C) 2012 - 2013, Lionel Le Folgoc <mrpouit@ubuntu.com>
 * Copyright (C) 2012, Julien Lavergne <gilir@ubuntu.com>
 * Copyright (C) 2013 - 2015, Simon Steinbeiß <ochosi@shimmerproject.org>
 * Copyright (C) 2013 - 2018, Sean Davis <smd.seandavis@gmail.com>
 * Copyright (C) 2014, Andrew P. <pan.pav.7c5@gmail.com>
 * Copyright (C) 2015 - 2019 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib-unix.h>

#include <locale.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <glib.h>
#include <gtk/gtkx.h>
#include <glib/gslist.h>
#include <signal.h>
#include <upower.h>

#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

#include <libayatana-ido/libayatana-ido.h>
#include <libayatana-indicator/indicator-ng.h>
#include <libayatana-indicator/indicator-object.h>

#include <lightdm.h>

#include "indicator-button.h"
#include "greeterconfiguration.h"
#include "greeterbackground.h"
#include "greeter-assistant.h"
#include "greeter-message-dialog.h"


/* Screen window */
static GtkWidget *main_box;
/* Assistant */
static GtkWidget *assistant;
/* Panel */
static GtkWidget *panel_box;
static GtkWidget *btn_shutdown, *btn_restart, *btn_suspend, *btn_hibernate;
static GtkWidget *indicator_box;

/* Clock */
static gchar *clock_format;

/* Handling monitors backgrounds */
static GreeterBackground *greeter_background;

static UpClient *up_client = NULL;
static GPtrArray *devices = NULL;


enum {
    SYSTEM_SHUTDOWN,
    SYSTEM_RESTART,
    SYSTEM_SUSPEND,
    SYSTEM_HIBERNATE
};

struct SavedFocusData
{
    GtkWidget *widget;
    gint editable_pos;
};



gpointer greeter_save_focus                    (GtkWidget* widget);
void     greeter_restore_focus                 (const gpointer saved_data);

/* Clock */
static gboolean
clock_timeout_thread (gpointer data)
{
    GtkLabel *clock_label = GTK_LABEL (data);

    GDateTime *dt = NULL;

    dt = g_date_time_new_now_local ();
    if (dt) {
        gchar *fm = g_date_time_format (dt, clock_format);
        gchar *markup = g_markup_printf_escaped ("<b><span foreground=\"white\">%s</span></b>", fm);
        gtk_label_set_markup (GTK_LABEL (clock_label), markup);
        g_free (fm);
        g_free (markup);

        g_date_time_unref (dt);
    }

    return TRUE;
}

static void
on_indicator_button_toggled_cb (GtkToggleButton *button, gpointer user_data)
{
	if (!gtk_toggle_button_get_active (button))
		return;

	IndicatorObjectEntry *entry;

	XfceIndicatorButton *indic_button = XFCE_INDICATOR_BUTTON (button);

	entry = xfce_indicator_button_get_entry (indic_button);

	GList *l = NULL;
	GList *children = gtk_container_get_children (GTK_CONTAINER (entry->menu));
	for (l = children; l; l = l->next) {
		GtkWidget *item = GTK_WIDGET (l->data);
		if (item) {
			g_signal_emit_by_name (item, "activate");
			break;
		}
	}

	g_list_free (children);

	g_signal_handlers_block_by_func (button, on_indicator_button_toggled_cb, user_data);
	gtk_toggle_button_set_active (button, FALSE);
	g_signal_handlers_unblock_by_func (button, on_indicator_button_toggled_cb, user_data);
}

static gboolean
find_app_indicators (const gchar *name)
{
    gboolean found = FALSE;

    gchar **app_indicators = config_get_string_list (NULL, "app-indicators", NULL);

    if (app_indicators) {
        guint i;
        for (i = 0; app_indicators[i] != NULL; i++) {
            if (g_strcmp0 (name, app_indicators[i]) == 0) {
                found = TRUE;
                break;
            }
        }
        g_strfreev (app_indicators);
    }

    return found;
}

static void
entry_added (IndicatorObject *io, IndicatorObjectEntry *entry, gpointer user_data)
{
	g_return_if_fail (entry != NULL);

	if ((g_strcmp0 (entry->name_hint, "nm-applet") == 0) ||
        (find_app_indicators (entry->name_hint)))
	{
		GtkWidget *button;
		const gchar *io_name;

		io_name = g_object_get_data (G_OBJECT (io), "io-name");

		button = xfce_indicator_button_new (io, io_name, entry);
		gtk_box_pack_start (GTK_BOX (indicator_box), button, FALSE, FALSE, 0);

		if (entry->image != NULL)
			xfce_indicator_button_set_image (XFCE_INDICATOR_BUTTON (button), entry->image);

		if (entry->label != NULL)
			xfce_indicator_button_set_label (XFCE_INDICATOR_BUTTON (button), entry->label);

		if (g_strcmp0 (entry->name_hint, "gooroom-notice-applet") == 0) {
			g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (on_indicator_button_toggled_cb), user_data);
		} else {
			if (entry->menu != NULL)
				xfce_indicator_button_set_menu (XFCE_INDICATOR_BUTTON (button), entry->menu);
		}

		gtk_widget_show (button);
	}
}

static void
entry_removed (IndicatorObject *io, IndicatorObjectEntry *entry, gpointer user_data)
{
	GList *children, *l = NULL;

	children = gtk_container_get_children (GTK_CONTAINER (indicator_box));
	for (l = children; l; l = l->next) {
		XfceIndicatorButton *child = XFCE_INDICATOR_BUTTON (l->data);
		if (child && (xfce_indicator_button_get_entry (child) == entry)) {
			xfce_indicator_button_destroy (child);
			break;
		}
	}

	g_list_free (children);
}

static gchar *
get_battery_icon_name (double percentage, UpDeviceState state)
{
    gchar *icon_name = NULL;
    const gchar *bat_state;

    switch (state)
    {
        case UP_DEVICE_STATE_CHARGING:
        case UP_DEVICE_STATE_PENDING_CHARGE:
            bat_state = "-charging";
            break;

        case UP_DEVICE_STATE_DISCHARGING:
        case UP_DEVICE_STATE_PENDING_DISCHARGE:
            bat_state = "";
            break;

        case UP_DEVICE_STATE_FULLY_CHARGED:
            bat_state = "-charged";
            break;

        case UP_DEVICE_STATE_EMPTY:
            return g_strdup ("battery-empty");

        default:
            bat_state = NULL;
            break;
    }
    if (!bat_state) {
        return g_strdup ("battery-error");
    }

    if (percentage >= 75) {
        icon_name = g_strdup_printf ("battery-full%s", bat_state);
    } else if (percentage >= 50 && percentage < 75) {
        icon_name = g_strdup_printf ("battery-good%s", bat_state);
    } else if (percentage >= 25 && percentage < 50) {
        icon_name = g_strdup_printf ("battery-medium%s", bat_state);
    } else if (percentage >= 10 && percentage < 25) {
        icon_name = g_strdup_printf ("battery-low%s", bat_state);
    } else {
        icon_name = g_strdup_printf ("battery-caution%s", bat_state);
    }

    return icon_name;
}

static void
on_power_device_changed_cb (UpDevice *device, GParamSpec *pspec, gpointer data)
{
    GtkImage *bat_tray = GTK_IMAGE (data);

    gchar *icon_name;
    gdouble percentage;
    UpDeviceState state;

    g_object_get (device,
                  "state", &state,
                  "percentage", &percentage,
                  NULL);

/* Sometimes the reported state is fully charged but battery is at 99%,
 * refusing to reach 100%. In these cases, just assume 100%.
 */
    if (state == UP_DEVICE_STATE_FULLY_CHARGED &&
        (100.0 - percentage <= 1.0))
      percentage = 100.0;

    icon_name = get_battery_icon_name (percentage, state);

    gtk_image_set_from_icon_name (bat_tray,
                                  icon_name,
                                  GTK_ICON_SIZE_BUTTON);

    gtk_image_set_pixel_size (bat_tray, 22);

    g_free (icon_name);
}

static void
updevice_added_cb (UpDevice *device)
{
    gboolean is_present = FALSE;
    guint device_type = UP_DEVICE_KIND_UNKNOWN;

    g_object_get (device, "kind", &device_type, NULL);
    g_object_get (device, "is-present", &is_present, NULL);

    if (device_type == UP_DEVICE_KIND_BATTERY && is_present) {
		GtkWidget *image = gtk_image_new_from_icon_name ("battery-full-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start (GTK_BOX (indicator_box), image, FALSE, FALSE, 0);
		gtk_widget_show (image);

		g_object_set_data (G_OBJECT (image), "updevice", device);

		on_power_device_changed_cb (device, NULL, image);
		g_signal_connect (device, "notify", G_CALLBACK (on_power_device_changed_cb), image);
    }
}

static void
on_up_client_device_added_cb (UpClient *upclient, UpDevice *device, gpointer data)
{
	updevice_added_cb (device);
}

static void
updevice_removed_cb (GtkWidget *widget, gpointer data)
{
    UpDevice *removed_device = (UpDevice *)data;

    UpDevice *device = (UpDevice*)g_object_get_data (G_OBJECT (widget), "updevice");

    if (device != removed_device)
        return;

    gtk_container_remove (GTK_CONTAINER (indicator_box), widget);

    gtk_widget_destroy (widget);
}

static void
on_up_client_device_removed_cb (UpClient *upclient, UpDevice *device, gpointer data)
{
    gtk_container_foreach (GTK_CONTAINER (indicator_box), updevice_removed_cb, device);
}

static void
show_command_dialog (const gchar* icon, const gchar* title, const gchar* message, int type)
{
	GList *items, *l = NULL;
	gint res, logged_in_users = 0;
	gchar *new_message = NULL;
	GtkWidget *dialog, *toplevel;

	items = lightdm_user_list_get_users (lightdm_user_list_get_instance ());
	for (l = items; l; l = l->next) {
		LightDMUser *user = l->data;
		if (lightdm_user_get_logged_in (user))
			logged_in_users++;
	}

    /* Check if there are still users logged in, count them and if so, display a warning */
	if (logged_in_users > 0) {
		gchar *warning = g_strdup_printf (ngettext ("Warning: There is still %d user logged in.",
                                          "Warning: There are still %d users logged in.",
                                          logged_in_users), logged_in_users);

		new_message = g_strdup_printf ("%s\n%s", warning, message);
		g_free (warning);
	} else {
		new_message = g_strdup (message);
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (main_box));

	dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
                                         icon,
                                         title,
                                         new_message);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            _("_Ok"), GTK_RESPONSE_OK,
                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                            NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	gtk_widget_show (dialog);
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

    g_free (new_message);

	if (res != GTK_RESPONSE_OK)
		return;

	switch (type)
	{
		case SYSTEM_SHUTDOWN:
			lightdm_shutdown (NULL);
		break;

		case SYSTEM_RESTART:
			lightdm_restart (NULL);
		break;

		case SYSTEM_SUSPEND:
			lightdm_suspend (NULL);
		break;

		case SYSTEM_HIBERNATE:
			lightdm_hibernate (NULL);
		break;

		default:
			return;
	}
}

static void
on_command_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	const char *img, *title, *msg;
	int type = GPOINTER_TO_INT (user_data);

	switch (type) {
		case SYSTEM_SHUTDOWN:
			img = "system-shutdown-symbolic";
			title = _("System Shutdown");
			msg = _("Are you sure you want to close all programs and shut down the computer?");
			break;

		case SYSTEM_RESTART:
			img = "system-restart-symbolic";
			title = _("System Restart");
			msg = _("Are you sure you want to close all programs and restart the computer?");
			break;

		case SYSTEM_SUSPEND:
			img = "system-suspend-symbolic";
			title = _("System Suspend");
			msg = _("Are you sure you want to suspend the computer?");
			break;

		case SYSTEM_HIBERNATE:
			img = "system-hibernate-symbolic";
			title = _("System Hibernate");
			msg = _("Are you sure you want to hibernate the computer?");
			break;

		default:
			return;
	}

	show_command_dialog (img, title, msg, type);
}

static void
dbus_update_activation_environment (void)
{
	gchar **argv = NULL;
	const gchar *cmd = "/usr/bin/dbus-update-activation-environment --systemd DBUS_SESSION_BUS_ADDRESS DISPLAY XAUTHORITY";
//	const gchar *cmd = "/usr/bin/dbus-update-activation-environment --systemd -all";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
}

static void
wm_start (void)
{
	gchar **argv = NULL, **envp = NULL;
	const gchar *cmd = "/usr/bin/metacity";

	GSettings *settings = g_settings_new ("org.gnome.desktop.wm.preferences");
	g_settings_set_enum (settings, "action-right-click-titlebar", 5);
	g_object_unref (settings);

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	envp = g_get_environ ();

	g_spawn_async (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
	g_strfreev (envp);
}

static void
gf_start (void)
{
	gchar **argv = NULL, **envp = NULL;
	const gchar *cmd = "/usr/bin/gnome-flashback";

	GSettings *settings = g_settings_new ("org.gnome.gnome-flashback");
	g_settings_set_boolean (settings, "automount-manager", FALSE);
	g_settings_set_boolean (settings, "idle-monitor", FALSE);
	g_settings_set_boolean (settings, "polkit", FALSE);
	g_settings_set_boolean (settings, "shell", FALSE);
	g_settings_set_boolean (settings, "a11y-keyboard", FALSE);
	g_settings_set_boolean (settings, "audio-device-selection", FALSE);
	g_settings_set_boolean (settings, "bluetooth-applet", FALSE);
	g_settings_set_boolean (settings, "desktop-background", FALSE);
	g_settings_set_boolean (settings, "end-session-dialog", FALSE);
	g_settings_set_boolean (settings, "input-settings", FALSE);
	g_settings_set_boolean (settings, "input-sources", FALSE);
	g_settings_set_boolean (settings, "notifications", FALSE);
	g_settings_set_boolean (settings, "power-applet", FALSE);
	g_settings_set_boolean (settings, "screencast", FALSE);
	g_settings_set_boolean (settings, "screensaver", FALSE);
	g_settings_set_boolean (settings, "screenshot", FALSE);
	g_settings_set_boolean (settings, "sound-applet", FALSE);
	g_settings_set_boolean (settings, "status-notifier-watcher", FALSE);
	g_settings_set_boolean (settings, "workarounds", FALSE);
	g_object_unref (settings);

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	envp = g_get_environ ();

	g_spawn_async (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
	g_strfreev (envp);
}

static void
notify_service_start (void)
{
	const gchar *cmd;
	gchar **argv = NULL, **envp = NULL;

	cmd = "/usr/bin/gsettings set apps.gooroom-notifyd notify-location 2";
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

	cmd = "/usr/bin/gsettings set apps.gooroom-notifyd do-not-disturb true";
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

	g_shell_parse_argv (GOOROOM_NOTIFYD, NULL, &argv, NULL);

	envp = g_get_environ ();

	g_spawn_async (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
	g_strfreev (envp);
}

static void
indicator_application_service_start (void)
{
	gchar **argv = NULL, **envp = NULL;
	const gchar *cmd = "systemctl --user start ayatana-indicator-application";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	envp = g_get_environ ();

	g_spawn_async (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
}

static void
network_indicator_application_start (void)
{
	const gchar *cmd;
	gchar **argv = NULL, **envp = NULL;

	cmd = "/usr/bin/gsettings set org.gnome.nm-applet disable-connected-notifications true";
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

	cmd = "/usr/bin/gsettings set org.gnome.nm-applet disable-disconnected-notifications true";
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

	cmd = "/usr/bin/gsettings set org.gnome.nm-applet suppress-wireless-networks-available true";
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);

	cmd = "nm-applet";
	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	envp = g_get_environ ();

	g_spawn_async (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
	g_strfreev (envp);
}

static void
other_indicator_application_start (void)
{
	gchar **envp = NULL;
	gchar **app_indicators = config_get_string_list (NULL, "app-indicators", NULL);

	if (!app_indicators)
		return;

	envp = g_get_environ ();

	guint i;
	for (i = 0; app_indicators[i] != NULL; i++) {
		gchar **argv = NULL;
		g_shell_parse_argv (app_indicators[i], NULL, &argv, NULL);
		g_spawn_async (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
		g_strfreev (argv);
	}

	g_strfreev (envp);
	g_strfreev (app_indicators);
}

static void
load_clock_indicator (void)
{
	GtkWidget *clock_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (indicator_box), clock_label, FALSE, FALSE, 0);
	gtk_widget_show_all (clock_label);

	/* update clock */
	clock_timeout_thread (clock_label);
	gdk_threads_add_timeout (1000, (GSourceFunc) clock_timeout_thread, clock_label);
}

static void
load_battery_indicator (void)
{
	guint i;

	up_client = up_client_new ();
	devices = up_client_get_devices2 (up_client);

	if (devices) {
		for ( i = 0; i < devices->len; i++) {
			UpDevice *device = g_ptr_array_index (devices, i);

			updevice_added_cb (device);
		}
	}

	g_signal_connect (up_client, "device-added", G_CALLBACK (on_up_client_device_added_cb), NULL);
	g_signal_connect (up_client, "device-removed", G_CALLBACK (on_up_client_device_removed_cb), NULL);
}

static void
load_module (const gchar *name)
{
	gchar                *path;
	IndicatorObject      *io;
	GList                *entries, *l = NULL;

	path = g_build_filename (INDICATOR_DIR, name, NULL);
	io = indicator_object_new_from_file (path);
	g_free (path);

	g_object_set_data (G_OBJECT (io), "io-name", g_strdup (name));

	g_signal_connect (G_OBJECT (io), INDICATOR_OBJECT_SIGNAL_ENTRY_ADDED, G_CALLBACK (entry_added), NULL);
	g_signal_connect (G_OBJECT (io), INDICATOR_OBJECT_SIGNAL_ENTRY_REMOVED, G_CALLBACK (entry_removed), NULL);

	entries = indicator_object_get_entries (io);

	for (l = entries; l; l = l->next) {
		IndicatorObjectEntry *ioe = (IndicatorObjectEntry *)l->data;
		entry_added (io, ioe, NULL);
	}

	g_list_free (entries);
}


static void
load_application_indicator (void)
{
	/* load application indicator */
	if (g_file_test (INDICATOR_DIR, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
		GDir *dir = g_dir_open (INDICATOR_DIR, 0, NULL);

		const gchar *name;
		while ((name = g_dir_read_name (dir)) != NULL) {
			if (!name || !g_str_has_suffix (name, G_MODULE_SUFFIX))
				continue;

			if (!g_str_equal (name, "libayatana-application.so"))
				continue;

			load_module (name);
		}
		g_dir_close (dir);
	}
}

static void
load_indicators (void)
{
	load_clock_indicator ();
	load_battery_indicator ();
	load_application_indicator ();
}

static void
load_commands (void)
{
	gtk_widget_set_visible (btn_shutdown, lightdm_get_can_shutdown());
	gtk_widget_set_visible (btn_restart, lightdm_get_can_restart());
	gtk_widget_set_visible (btn_suspend, lightdm_get_can_suspend());
	gtk_widget_set_visible (btn_hibernate, lightdm_get_can_hibernate());

	g_signal_connect (G_OBJECT (btn_shutdown), "clicked",
                      G_CALLBACK (on_command_button_clicked_cb), GINT_TO_POINTER (SYSTEM_SHUTDOWN));
	g_signal_connect (G_OBJECT (btn_restart), "clicked",
                      G_CALLBACK (on_command_button_clicked_cb), GINT_TO_POINTER (SYSTEM_RESTART));
	g_signal_connect (G_OBJECT (btn_suspend), "clicked",
                      G_CALLBACK (on_command_button_clicked_cb), GINT_TO_POINTER (SYSTEM_SUSPEND));
	g_signal_connect (G_OBJECT (btn_hibernate), "clicked",
                      G_CALLBACK (on_command_button_clicked_cb), GINT_TO_POINTER (SYSTEM_HIBERNATE));
}

static void
read_monitor_configuration (const gchar *group, const gchar *name)
{
    g_debug ("[Configuration] Monitor configuration found: '%s'", name);

    gchar *background = config_get_string (group, CONFIG_KEY_BACKGROUND, NULL);
    greeter_background_set_monitor_config (greeter_background, name, background,
                                           config_get_bool (group, CONFIG_KEY_USER_BACKGROUND, -1),
                                           config_get_bool (group, CONFIG_KEY_LAPTOP, -1),
                                           config_get_int  (group, CONFIG_KEY_T_DURATION, -1),
                                           config_get_enum (group, CONFIG_KEY_T_TYPE,
                                                            TRANSITION_TYPE_FALLBACK,
                                                            "none",         TRANSITION_TYPE_NONE,
                                                            "linear",       TRANSITION_TYPE_LINEAR,
                                                            "ease-in-out",  TRANSITION_TYPE_EASE_IN_OUT, NULL));
    g_free (background);
}

gpointer
greeter_save_focus (GtkWidget* widget)
{
    GtkWidget *window = gtk_widget_get_toplevel(widget);
    if (!GTK_IS_WINDOW (window))
        return NULL;

    struct SavedFocusData *data = g_new0 (struct SavedFocusData, 1);
    data->widget = gtk_window_get_focus (GTK_WINDOW (window));
    data->editable_pos = GTK_IS_EDITABLE(data->widget) ? gtk_editable_get_position (GTK_EDITABLE (data->widget)) : -1;

    return data;
}

void
greeter_restore_focus (const gpointer saved_data)
{
    struct SavedFocusData *data = saved_data;

    if (!saved_data || !GTK_IS_WIDGET (data->widget))
        return;

    gtk_widget_grab_focus (data->widget);
    if (GTK_IS_EDITABLE(data->widget) && data->editable_pos > -1)
        gtk_editable_set_position(GTK_EDITABLE(data->widget), data->editable_pos);
}

static void
sigterm_cb (gpointer user_data)
{
    gboolean is_callback = GPOINTER_TO_INT (user_data);

    if (is_callback)
        g_debug ("SIGTERM received");

    if (is_callback)
    {
        gtk_main_quit ();
        #ifdef KILL_ON_SIGTERM
        /* LP: #1445461 */
        g_debug ("Killing greeter with exit()...");
        exit (EXIT_SUCCESS);
        #endif
    }
}

static void
assistant_realize_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GdkMonitor *m;
	GdkRectangle geometry;
	gint panel_height = 0, pref_w = 0, pref_h = 0;

	GreeterAssistant *assistant = GREETER_ASSISTANT (widget);

	m = gdk_display_get_primary_monitor (gdk_display_get_default ());

	gdk_monitor_get_geometry (m, &geometry);

	gtk_widget_get_preferred_height (panel_box, NULL, &panel_height);
	gtk_widget_get_preferred_height (GTK_WIDGET (assistant), NULL, &pref_h);
	gtk_widget_get_preferred_width (GTK_WIDGET (assistant), NULL, &pref_w);

	int max_width = geometry.width;
	int max_height = geometry.height - panel_height;

	pref_w = (pref_w > max_width) ? max_width : pref_w;
	pref_h = (pref_h > max_height) ? max_height : pref_h;

	gtk_widget_set_size_request (GTK_WIDGET (assistant), pref_w, pref_h);

	g_debug ("preferred width = %d", pref_w);
	g_debug ("preferred height = %d", pref_h);
}

static void
apply_gtk_config (void)
{
	GError *error = NULL;
	GKeyFile *keyfile = NULL;
	gchar *gtk_settings_ini = NULL;
	gchar *gtk_settings_dir = NULL;

	gtk_settings_dir = g_build_filename (g_get_user_config_dir (), "gtk-3.0", NULL);
	if (g_mkdir_with_parents (gtk_settings_dir, 0775) < 0) {
		g_warning ("Failed to create directory %s", gtk_settings_dir);
		g_free (gtk_settings_dir);
		return;
	}

	gtk_settings_ini = g_build_filename (gtk_settings_dir, "settings.ini", NULL);

	keyfile = g_key_file_new ();

	if (error == NULL)
	{
		gchar *value;

		/* Set GTK+ settings */
		value = config_get_string (NULL, CONFIG_KEY_THEME, NULL);
		if (value)
		{
			g_key_file_set_string (keyfile, "Settings", "gtk-theme-name", value);
			g_free (value);
		}

		value = config_get_string (NULL, CONFIG_KEY_ICON_THEME, NULL);
		if (value)
		{
			g_key_file_set_string (keyfile, "Settings", "gtk-icon-theme-name", value);
			g_free (value);
		}

		value = config_get_string (NULL, CONFIG_KEY_FONT, "Sans 10");
		if (value)
		{
			g_key_file_set_string (keyfile, "Settings", "gtk-font-name", value);
			g_free (value);
		}

		if (config_has_key (NULL, CONFIG_KEY_DPI))
		{
			gint dpi = 1024 * config_get_int (NULL, CONFIG_KEY_DPI, 96);
			g_key_file_set_integer (keyfile, "Settings", "gtk-xft-dpi", dpi);
		}

		if (config_has_key (NULL, CONFIG_KEY_ANTIALIAS))
		{
			gboolean antialias = config_get_bool (NULL, CONFIG_KEY_ANTIALIAS, FALSE);
			g_key_file_set_boolean (keyfile, "Settings", "gtk-xft-antialias", antialias);
		}

		value = config_get_string (NULL, CONFIG_KEY_HINT_STYLE, NULL);
		if (value)
		{
			g_key_file_set_string (keyfile, "Settings", "gtk-xft-hintstyle", value);
			g_free (value);
		}

		value = config_get_string (NULL, CONFIG_KEY_RGBA, NULL);
		if (value)
		{
			g_key_file_set_string (keyfile, "Settings", "gtk-xft-rgba", value);
			g_free (value);
		}
		g_key_file_save_to_file (keyfile, gtk_settings_ini, NULL);
	} else {
		g_error_free (error);
	}

	g_free (gtk_settings_dir);
	g_free (gtk_settings_ini);
	g_key_file_free (keyfile);
}

int
main (int argc, char **argv)
{
	GtkBuilder *builder;
	int ret = EXIT_SUCCESS;

	/* LP: #1024482 */
	g_setenv ("GDK_CORE_DEVICE_EVENTS", "1", TRUE);
	g_setenv ("GTK_MODULES", "atk-bridge", FALSE);

	/* Make nm-applet hide items the user does not have permissions to interact with */
	g_setenv ("NM_APPLET_HIDE_POLICY_ITEMS", "1", TRUE);

	dbus_update_activation_environment ();

	g_unix_signal_add (SIGTERM, (GSourceFunc)sigterm_cb, /* is_callback */ GINT_TO_POINTER (TRUE));

	/* Initialize i18n */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* init gtk */
	gtk_init (&argc, &argv);

	config_init ();
	apply_gtk_config ();

	/* Starting window manager */
	wm_start ();

	/* Starting gnome-flashback */
	gf_start ();

	/* Set default cursor */
	gdk_window_set_cursor (gdk_get_default_root_window (),
                           gdk_cursor_new_for_display (gdk_display_get_default (),
                           GDK_LEFT_PTR));

	builder = gtk_builder_new_from_resource ("/kr/gooroom/greeter/gooroom-greeter.ui");

	/* main box */
	main_box = GTK_WIDGET (gtk_builder_get_object (builder, "main_box"));
	/* bottom panel */
	panel_box = GTK_WIDGET (gtk_builder_get_object (builder, "panel_box"));
	/* indicator box in panel */
	indicator_box = GTK_WIDGET (gtk_builder_get_object (builder, "indicator_box"));

	/* command buttons */
	btn_shutdown = GTK_WIDGET (gtk_builder_get_object (builder, "btn_shutdown"));
	btn_restart = GTK_WIDGET (gtk_builder_get_object (builder, "btn_restart"));
	btn_suspend = GTK_WIDGET (gtk_builder_get_object (builder, "btn_suspend"));
	btn_hibernate = GTK_WIDGET (gtk_builder_get_object (builder, "btn_hibernate"));

	/* Background */
	greeter_background = greeter_background_new (main_box);

	assistant = greeter_assistant_new ();
	gtk_widget_set_halign (assistant, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (assistant, GTK_ALIGN_CENTER);

	gtk_box_pack_start (GTK_BOX (main_box), assistant, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (assistant), "realize", G_CALLBACK (assistant_realize_cb), NULL);

	clock_format = config_get_string (NULL, CONFIG_KEY_CLOCK_FORMAT, "%F      %p %I:%M");

	GtkCssProvider *provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_resource (provider, "/kr/gooroom/greeter/theme.css");
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);

	read_monitor_configuration (CONFIG_GROUP_DEFAULT, GREETER_BACKGROUND_DEFAULT);

	gchar **config_group;
	gchar **config_groups = config_get_groups (CONFIG_GROUP_MONITOR);
	for (config_group = config_groups; *config_group; ++config_group)
	{
		const gchar *name = *config_group + sizeof (CONFIG_GROUP_MONITOR);
		while (*name && g_ascii_isspace (*name))
			++name;

		read_monitor_configuration (*config_group, name);
	}
	g_strfreev (config_groups);

	greeter_background_connect (greeter_background, gdk_screen_get_default ());

	load_commands ();
	load_indicators ();

	/* Start the indicator applications service */
	indicator_application_service_start ();
	network_indicator_application_start ();
	other_indicator_application_start ();
	notify_service_start ();

	gtk_widget_show (main_box);

	gtk_main ();

	if (devices) {
		g_ptr_array_foreach (devices, (GFunc) g_object_unref, NULL);
		g_clear_pointer (&devices, g_ptr_array_unref);
	}

	if (up_client)
		g_clear_object (&up_client);

	sigterm_cb (GINT_TO_POINTER (FALSE));

    return ret;
}
