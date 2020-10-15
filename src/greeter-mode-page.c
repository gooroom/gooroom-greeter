/*
 * Copyright (C) 2015-2020 Gooroom <gooroom@gooroom.kr>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "greeter-mode-page.h"
#include "greeter-message-dialog.h"

#define PAGE_ID "STEP 1"

struct _GreeterModePagePrivate {
	GtkWidget *mode_internal_button;
	GtkWidget *mode_external_button;
	GtkWidget *forward_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterModePage, greeter_mode_page, GREETER_TYPE_PAGE);


//static void
//set_networking_enable (gboolean enabled)
//{
//	gchar *cmd = NULL;
//	const gchar *on_off;
//
//	on_off = enabled ? "on" : "off";
//
//	cmd = g_strdup_printf ("/usr/bin/nmcli networking %s", on_off);
//
//	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
//
//	g_free (cmd);
//}

static void
mode_button_toggled_cb (GtkToggleButton *button,
                        gpointer         user_data)
{
	int mode = MODE_EXTERNAL;
	GreeterModePage *page = GREETER_MODE_PAGE (user_data);
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	if (!gtk_toggle_button_get_active (button))
		return;

	if (GTK_WIDGET (button) == page->priv->mode_internal_button)
		mode = MODE_INTERNAL;

	greeter_page_manager_set_mode (manager, mode);
}

static void
forward_button_clicked_cb (GtkWidget *button,
                           gpointer   user_data)
{
//	set_networking_enable (TRUE);

	greeter_page_manager_go_next (GREETER_PAGE (user_data)->manager);
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

//	set_networking_enable (FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->mode_internal_button), TRUE);

	greeter_page_manager_set_mode (page->manager, MODE_INTERNAL);

	gtk_widget_grab_focus (self->priv->forward_button);
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
//            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mode_internal_button))) {
//			active_button = priv->mode_internal_button;
//		}
//
//		if (event->keyval == GDK_KEY_Down &&
//            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mode_external_button))) {
//			active_button = priv->mode_external_button;
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

	greeter_page_set_title (GREETER_PAGE (page), _("Selecting Connection Environment"));

	g_signal_connect (G_OBJECT (priv->mode_internal_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	g_signal_connect (G_OBJECT (priv->mode_external_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	g_signal_connect (G_OBJECT (priv->forward_button), "clicked",
                      G_CALLBACK (forward_button_clicked_cb), page);

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

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, mode_internal_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, mode_external_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, forward_button);

	page_class->page_id = PAGE_ID;
	page_class->shown = greeter_mode_page_shown;
	page_class->should_show = greeter_mode_page_should_show;

	object_class->finalize = greeter_mode_page_finalize;
}

GreeterPage *
greeter_prepare_mode_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_MODE_PAGE,
                         "manager", manager,
                         NULL);
}
