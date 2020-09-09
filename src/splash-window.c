/*
 * Copyright (C) 2019 Gooroom <gooroom@gooroom.kr>.
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "splash-window.h"


struct _SplashWindowPrivate
{
	GtkWidget *message_label;
	GtkWidget *spinner;
};



G_DEFINE_TYPE_WITH_PRIVATE (SplashWindow, splash_window, GTK_TYPE_WINDOW);


//static gboolean
//splash_window_draw (GtkWidget *widget,
//                    cairo_t   *cr)
//{
//	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
//	cairo_set_source_rgba (cr, 255, 0.0, 0.0, 0.1); /* transparent */
//	cairo_paint (cr);
//
//	if (GTK_WIDGET_CLASS (splash_window_parent_class)->draw)
//		return GTK_WIDGET_CLASS (splash_window_parent_class)->draw (widget, cr);
//
//	return FALSE;
//}

static void
splash_window_finalize (GObject *object)
{
	G_OBJECT_CLASS (splash_window_parent_class)->finalize (object);
}

static void
splash_window_init (SplashWindow *window)
{
	window->priv = splash_window_get_instance_private (window);

	gtk_widget_init_template (GTK_WIDGET (window));

	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
//	gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
//	gtk_window_fullscreen (GTK_WINDOW (window));
	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
	if (gdk_screen_is_composited (screen)) {
		GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
		if (visual == NULL)
			visual = gdk_screen_get_system_visual (screen);

		gtk_widget_set_visual (GTK_WIDGET(window), visual);
	}
}

static void
splash_window_class_init (SplashWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
//	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/splash-window.ui");

	object_class->finalize = splash_window_finalize;

//	widget_class->draw = splash_window_draw;

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  SplashWindow, message_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  SplashWindow, spinner);
}


SplashWindow *
splash_window_new (GtkWindow *parent)
{
	GObject *result;

	result = g_object_new (SPLASH_TYPE_WINDOW,
                           "transient-for", parent,
//                           "type", GTK_WINDOW_POPUP,
                           NULL);

	return SPLASH_WINDOW (result);
}

void
splash_window_show (SplashWindow *window)
{
	g_return_if_fail (SPLASH_IS_WINDOW (window));

	gtk_spinner_start (GTK_SPINNER (window->priv->spinner));

	gtk_widget_show (GTK_WIDGET (window));
}

void
splash_window_destroy (SplashWindow *window)
{
	g_return_if_fail (SPLASH_IS_WINDOW (window));

	gtk_spinner_stop (GTK_SPINNER (window->priv->spinner));

	gtk_widget_destroy (GTK_WIDGET (window));
}

void
splash_window_set_message_label (SplashWindow *window,
                                 const char   *message)
{
	SplashWindowPrivate *priv = window->priv;

	if (message) {
		gtk_label_set_text (GTK_LABEL (priv->message_label), message);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->message_label), "");
	}
}
