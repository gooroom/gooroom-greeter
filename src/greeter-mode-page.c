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
set_networking_enable (void)
{
	g_spawn_command_line_sync ("/usr/bin/nmcli networking on", NULL, NULL, NULL, NULL);

#if 0
	gchar *cmd;

	cmd = g_strdup_printf ("%s --action on", GREETER_NETWORK_CONTROL_HELPER);
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
	g_free (cmd);
#endif


#if 0
	gboolean ret;
    GError *error = NULL;
    NMClient *nm_client = NULL;

    nm_client = nm_client_new (NULL, &error);

	ret = nm_client_networking_get_enabled (nm_client);

	g_clear_object (&nm_client);
#endif
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

//static void
//greeter_mode_page_out (GreeterPage *page,
//                       gboolean     next)
//{
//	if (!next)
//		return;
//
//	set_networking_enable (greeter_page_manager_get_mode (page->manager) == MODE_ONLINE);
//}

static void
greeter_mode_page_shown (GreeterPage *page)
{
	GreeterModePage *self = GREETER_MODE_PAGE (page);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->on_button), TRUE);

	greeter_page_manager_set_mode (page->manager, MODE_ONLINE);
}

static gboolean
greeter_mode_page_should_show (GreeterPage *page)
{
	return TRUE;
}

//static gboolean
//greeter_mode_page_key_press_event (GtkWidget   *widget,
//                                   GdkEventKey *event)
//{
//	GreeterModePage *page = GREETER_MODE_PAGE (widget);
//	GreeterModePagePrivate *priv = page->priv;
//
//	if (event->keyval == GDK_KEY_Return &&
//		greeter_page_get_complete (GREETER_PAGE (page))) {
//		greeter_page_manager_go_next (GREETER_PAGE (page)->manager);
//		return TRUE;
//	}
//
//	if ((event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down)) {
//		GtkWidget *active_button = NULL;
//		if (event->keyval == GDK_KEY_Up &&
//            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->on_button))) {
//			active_button = priv->on_button;
//		}
//
//		if (event->keyval == GDK_KEY_Down &&
//            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->off_button))) {
//			active_button = priv->off_button;
//		}
//
//		if (active_button)
//			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_button), TRUE);
//
//		return TRUE;
//	}
//
//	return GTK_WIDGET_CLASS (greeter_mode_page_parent_class)->key_press_event (widget, event);
//}

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

	set_networking_enable ();

	greeter_page_set_complete (GREETER_PAGE (page), TRUE);

	gtk_widget_show (GTK_WIDGET (page));
}

static void
greeter_mode_page_class_init (GreeterModePageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
//	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);


	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-mode-page.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, on_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, off_button);

	page_class->page_id = PAGE_ID;
//	page_class->out = greeter_mode_page_out;
	page_class->shown = greeter_mode_page_shown;
	page_class->should_show = greeter_mode_page_should_show;

//	widget_class->key_press_event = greeter_mode_page_key_press_event;

	object_class->finalize = greeter_mode_page_finalize;
}

GreeterPage *
greeter_prepare_mode_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_MODE_PAGE,
                         "manager", manager,
                         NULL);
}
