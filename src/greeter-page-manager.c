/*
 * Copyright (C) 2015 - 2020 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <glib.h>

#include "splash-window.h"
#include "greeter-page-manager.h"

#define MODE_ONLINE_STRING  "ONLINE"
#define MODE_OFFLINE_STRING "OFFLINE"
#define MODE_SAVE_FILE_PATH "/var/lib/lightdm/mode"

enum {
	GO_NEXT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


struct _GreeterPageManagerPrivate {
	SplashWindow *splash;

	int           mode;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterPageManager, greeter_page_manager, G_TYPE_OBJECT)


//static void
//get_monitor_geometry (GtkWidget    *widget,
//                      GdkRectangle *geometry)
//{
//	GdkDisplay *d;
//	GdkWindow  *w;
//	GdkMonitor *m;
//
//	d = gdk_display_get_default ();
//	w = gtk_widget_get_window (widget);
//	m = gdk_display_get_monitor_at_window (d, w);
//
//	gdk_monitor_get_geometry (m, geometry);
//}

static void
greeter_page_manager_finalize (GObject *object)
{
	G_OBJECT_CLASS (greeter_page_manager_parent_class)->finalize (object);
}

static void
greeter_page_manager_init (GreeterPageManager *manager)
{
	manager->priv = greeter_page_manager_get_instance_private (manager);

	manager->priv->mode = MODE_ONLINE;
}

static void
greeter_page_manager_class_init (GreeterPageManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = greeter_page_manager_finalize;

	signals[GO_NEXT] = g_signal_new ("go-next",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, go_next),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

GreeterPageManager *
greeter_page_manager_new (void)
{
	GObject *result;

	result = g_object_new (GREETER_TYPE_PAGE_MANAGER, NULL);

	return GREETER_PAGE_MANAGER (result);
}

void
greeter_page_manager_set_mode (GreeterPageManager *manager,
                               int                 mode)
{
	GError *error = NULL;
	const char *contents;

	contents = (mode == MODE_ONLINE) ? MODE_ONLINE_STRING : MODE_OFFLINE_STRING;

	g_file_set_contents (MODE_SAVE_FILE_PATH, contents, -1, &error);
	if (error) {
		g_error_free (error);
	}

	manager->priv->mode = mode;
}

int
greeter_page_manager_get_mode (GreeterPageManager *manager)
{
	int mode;
	GError *error = NULL;
	gchar *contents = NULL;

	g_file_get_contents (MODE_SAVE_FILE_PATH, &contents, NULL, &error);
	if (error) {
		g_error_free (error);
		mode = manager->priv->mode;
		goto out;
	}

	mode = g_str_equal (contents, MODE_ONLINE_STRING) ? MODE_ONLINE : MODE_OFFLINE;

out:
	g_free (contents);
	return mode;
}

void
greeter_page_manager_go_next (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[GO_NEXT], 0);
}

void
greeter_page_manager_show_splash (GreeterPageManager *manager,
                                  GtkWidget          *parent,
                                  const char         *message)
{
//	GdkRectangle geometry;
	GreeterPageManagerPrivate *priv = manager->priv;

//	get_monitor_geometry (parent, &geometry);

	greeter_page_manager_hide_splash (manager);

	priv->splash = splash_window_new (GTK_WINDOW (parent));
//	gtk_widget_set_size_request (GTK_WIDGET (priv->splash), geometry.width, geometry.height);
//	gtk_window_move (GTK_WINDOW (priv->splash), geometry.x, geometry.y);
	splash_window_set_message_label (SPLASH_WINDOW (priv->splash), message);

	splash_window_show (priv->splash);
}

void
greeter_page_manager_hide_splash (GreeterPageManager *manager)
{
	GreeterPageManagerPrivate *priv = manager->priv;

	if (priv->splash) {
		splash_window_destroy (priv->splash);
		priv->splash = NULL;
	}
}
