/*
 * Copyright (C) 2015-2020 Gooroom <gooroom@gooroom.kr>
 * Copyright (C) 2004-2008 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008-2011 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <glib/gi18n.h>

#include "greeter-connection-editor-window.h"


struct _GreeterConnectionEditorWindowPrivate
{
	GdkMonitor   *monitor;

	GdkRectangle  geometry;

	gchar        *uuid;
};


enum {
	PROP_0,
	PROP_UUID,
	PROP_MONITOR
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterConnectionEditorWindow, greeter_connection_editor_window, GTK_TYPE_WINDOW)



static void
update_geometry (GreeterConnectionEditorWindow *window)
{
	GdkRectangle geometry;

	if (!GDK_IS_MONITOR (window->priv->monitor))
		return;

	gdk_monitor_get_geometry (window->priv->monitor, &geometry);

	window->priv->geometry.x = geometry.x;
	window->priv->geometry.y = geometry.y;
	window->priv->geometry.width = geometry.width;
	window->priv->geometry.height = geometry.height;
}

/* copied from panel-toplevel.c */
static void
greeter_connection_editor_window_move_resize_window (GreeterConnectionEditorWindow *window,
                                                     gboolean                       move,
                                                     gboolean                       resize)
{
	GdkWindow *gdkwindow;

	gdkwindow = gtk_widget_get_window (GTK_WIDGET (window));

	if (move && resize) {
		gdk_window_move_resize (gdkwindow,
                                window->priv->geometry.x,
                                window->priv->geometry.y,
                                window->priv->geometry.width,
                                window->priv->geometry.height);
	} else if (move) {
		gdk_window_move (gdkwindow,
                         window->priv->geometry.x,
                         window->priv->geometry.y);
	} else if (resize) {
		gdk_window_resize (gdkwindow,
                           window->priv->geometry.width,
                           window->priv->geometry.height);
	}
}

static void
process_watch_cb (GPid pid, gint status, gpointer user_data)
{
	g_spawn_close_pid (pid);

	gtk_widget_destroy (GTK_WIDGET (user_data));
}

static gboolean
launch_nm_connection_editor_cb (gpointer user_data)
{
	GPid pid;
	gchar *cmd = NULL;
	gchar **argv = NULL, **envp = NULL;
	GreeterConnectionEditorWindow *window = GREETER_CONNECTION_EDITOR_WINDOW (user_data);

	envp = g_get_environ ();
	cmd = g_strdup_printf ("/usr/bin/nm-connection-editor --edit=%s", window->priv->uuid);

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	// Spawn child process.
	if (g_spawn_async_with_pipes (NULL, argv, envp, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
                                  NULL, &pid, NULL, NULL, NULL, NULL))
	{
		g_child_watch_add (pid, process_watch_cb, window);
	}

	g_strfreev (argv);
	g_strfreev (envp);

	g_free (cmd);

	return FALSE;
}

static void
greeter_connection_editor_window_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
	GreeterConnectionEditorWindow *self;

	self = GREETER_CONNECTION_EDITOR_WINDOW (object);

	switch (prop_id) {
        case PROP_UUID:
			self->priv->uuid = g_value_dup_string (value);
        break;
		case PROP_MONITOR:
			self->priv->monitor = g_value_get_pointer (value);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_connection_editor_window_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
	GreeterConnectionEditorWindow *self;

	self = GREETER_CONNECTION_EDITOR_WINDOW (object);

	switch (prop_id) {
		case PROP_UUID:
			g_value_set_string (value, self->priv->uuid);
		break;
		case PROP_MONITOR:
			g_value_set_pointer (value, (gpointer) self->priv->monitor);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_connection_editor_window_real_size_allocate (GtkWidget     *widget,
                                                     GtkAllocation *allocation)
{
	GreeterConnectionEditorWindow *window;

	window = GREETER_CONNECTION_EDITOR_WINDOW (widget);

	if (GTK_WIDGET_CLASS (greeter_connection_editor_window_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (greeter_connection_editor_window_parent_class)->size_allocate (widget, allocation);

	if (!gtk_widget_get_realized (widget))
		return;

	update_geometry (window);

	greeter_connection_editor_window_move_resize_window (GREETER_CONNECTION_EDITOR_WINDOW (widget), TRUE, TRUE);
}

static void
greeter_connection_editor_window_finalize (GObject *object)
{
	GreeterConnectionEditorWindow *self = GREETER_CONNECTION_EDITOR_WINDOW (object);

	g_clear_pointer (&self->priv->uuid, g_free);

	G_OBJECT_CLASS (greeter_connection_editor_window_parent_class)->finalize (object);
}

static void
greeter_connection_editor_window_constructed (GObject *object)
{
	GreeterConnectionEditorWindow *window = GREETER_CONNECTION_EDITOR_WINDOW (object);

    G_OBJECT_CLASS (greeter_connection_editor_window_parent_class)->constructed (object);

	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_keep_below (GTK_WINDOW (window), TRUE);
	gtk_window_fullscreen (GTK_WINDOW (window));

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
	if(gdk_screen_is_composited (screen)) {
		GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
		if (visual == NULL)
			visual = gdk_screen_get_system_visual (screen);

		gtk_widget_set_visual (GTK_WIDGET (window), visual);
	}

	g_idle_add ((GSourceFunc)launch_nm_connection_editor_cb, window);
}

static void
greeter_connection_editor_window_init (GreeterConnectionEditorWindow *window)
{
	GreeterConnectionEditorWindowPrivate *priv;
	priv = window->priv = greeter_connection_editor_window_get_instance_private (window);

	priv->geometry.x      = -1;
	priv->geometry.y      = -1;
	priv->geometry.width  = -1;
	priv->geometry.height = -1;
}

static void
greeter_connection_editor_window_class_init (GreeterConnectionEditorWindowClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = greeter_connection_editor_window_constructed;
	object_class->get_property = greeter_connection_editor_window_get_property;
	object_class->set_property = greeter_connection_editor_window_set_property;
	object_class->finalize     = greeter_connection_editor_window_finalize;

	widget_class->size_allocate  = greeter_connection_editor_window_real_size_allocate;

	g_object_class_install_property
		(object_class, PROP_MONITOR,
		 g_param_spec_pointer ("monitor",
			 "Gdk monitor",
			 "The monitor (in terms of Gdk) which the window is on",
			 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property
		(object_class, PROP_UUID,
		 g_param_spec_string ("uuid", "", "",
			 NULL,
			 G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

GreeterConnectionEditorWindow *
greeter_connection_editor_window_new (GdkMonitor *monitor,
                                      gchar      *uuid)
{
	GObject *result;

	result = g_object_new (GREETER_TYPE_CONNECTION_EDITOR_WINDOW,
                           "monitor", monitor,
                           "uuid", uuid,
                           "app-paintable", TRUE,
                           NULL);

	return GREETER_CONNECTION_EDITOR_WINDOW (result);
}
