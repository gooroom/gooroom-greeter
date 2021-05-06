/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#define PAGE_ID "mode"

#include "config.h"
#include "greeter-mode-page.h"
#include "greeter-message-dialog.h"

#include <glib/gi18n.h>
#include <gio/gio.h>


struct _GreeterModePagePrivate {
	GtkWidget *on_button;
	GtkWidget *off_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterModePage, greeter_mode_page, GREETER_TYPE_PAGE);


static void
set_networking_disable (void)
{
	gchar **argv = NULL;
	const gchar *cmd = "/usr/bin/nmcli networking off";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);

//	gboolean ret;
//    GError *error = NULL;
//    NMClient *nm_client = NULL;
//
//    nm_client = nm_client_new (NULL, &error);
//
//	ret = nm_client_networking_get_enabled (nm_client);
//
//	g_clear_object (&nm_client);
}

static void
vpn_service_reload_done_cb (GPid     pid,
                            gint     status,
                            gpointer user_data)
{
	GreeterPage *page = GREETER_PAGE (user_data);

	g_spawn_close_pid (pid);

	set_networking_disable ();

	greeter_page_manager_hide_splash (page->manager);
}

static gboolean
reload_vpn_service (gpointer user_data)
{
	GPid pid;
	gchar **argv;
	const gchar *cmd, *message;
	GreeterModePage *page = GREETER_MODE_PAGE (user_data);
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	message = _("Initializing Settings for VPN.\nPlease wait.");

	greeter_page_manager_show_splash (manager, GREETER_PAGE (page)->parent, message);

	cmd = "/bin/systemctl restart gooroom-vpn-daemon.service";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	if (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL))
		g_child_watch_add (pid, (GChildWatchFunc) vpn_service_reload_done_cb, page);

	g_strfreev (argv);

	return FALSE;
}

static void
mode_button_toggled_cb (GtkToggleButton *button,
                        gpointer         user_data)
{
	int mode = MODE_OFFLINE;
	GreeterModePage *page = GREETER_MODE_PAGE (user_data);
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	if (!gtk_toggle_button_get_active (button))
		return;

	if (GTK_WIDGET (button) == page->priv->on_button)
		mode = MODE_ONLINE;

	greeter_page_manager_set_mode (manager, mode);
}

static void
greeter_mode_page_finalize (GObject *object)
{
	G_OBJECT_CLASS (greeter_mode_page_parent_class)->finalize (object);
}

static void
greeter_mode_page_shown (GreeterPage *page)
{
	GreeterModePage *self = GREETER_MODE_PAGE (page);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->on_button), TRUE);

	greeter_page_manager_set_mode (page->manager, MODE_ONLINE);

	g_idle_add ((GSourceFunc)reload_vpn_service, page);
}

static gboolean
greeter_mode_page_should_show (GreeterPage *page)
{
	return TRUE;
}

static gboolean
greeter_mode_page_key_press_event (GreeterPage *page,
                                   GdkEventKey *event)
{
	GreeterModePagePrivate *priv = GREETER_MODE_PAGE (page)->priv;

	if ((event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down)) {
		GtkWidget *active_button = NULL;
		if (event->keyval == GDK_KEY_Up &&
            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->on_button))) {
			active_button = priv->on_button;
		}

		if (event->keyval == GDK_KEY_Down &&
            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->off_button))) {
			active_button = priv->off_button;
		}

		if (active_button)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_button), TRUE);

		return TRUE;
	}

	return FALSE;
}

static void
greeter_mode_page_init (GreeterModePage *page)
{
	GreeterModePagePrivate *priv;

	priv = page->priv = greeter_mode_page_get_instance_private (page);

	gtk_widget_init_template (GTK_WIDGET (page));

	greeter_page_set_title (GREETER_PAGE (page), _("System operation mode setting"));

	g_signal_connect (G_OBJECT (priv->on_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	g_signal_connect (G_OBJECT (priv->off_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	greeter_page_set_complete (GREETER_PAGE (page), TRUE);

	gtk_widget_show (GTK_WIDGET (page));
}

static void
greeter_mode_page_class_init (GreeterModePageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-mode-page.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, on_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, off_button);

	page_class->page_id = PAGE_ID;
	page_class->shown = greeter_mode_page_shown;
	page_class->should_show = greeter_mode_page_should_show;
	page_class->key_press_event = greeter_mode_page_key_press_event;

	object_class->finalize = greeter_mode_page_finalize;
}

GreeterPage *
greeter_prepare_mode_page (GreeterPageManager *manager,
                           GtkWidget          *parent)
{
	return g_object_new (GREETER_TYPE_MODE_PAGE,
                         "manager", manager,
                         "parent", parent,
                         NULL);
}
