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
	GtkWidget *main_box;
	GtkWidget *message_label;
	GtkWidget *spinner;
};



G_DEFINE_TYPE_WITH_PRIVATE (SplashWindow, splash_window, GTK_TYPE_WINDOW);



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

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/splash-window.ui");

	object_class->finalize = splash_window_finalize;

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), SplashWindow, main_box);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), SplashWindow, message_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), SplashWindow, spinner);
}


SplashWindow *
splash_window_new (GtkWindow *parent)
{
	GObject *result;

	result = g_object_new (SPLASH_TYPE_WINDOW,
                           "transient-for", parent,
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

void
splash_window_set_theme (SplashWindow *window,
                         const char   *theme)
{
	GtkStyleContext *style_box;
	SplashWindowPrivate *priv = window->priv;

	style_box = gtk_widget_get_style_context (GTK_WIDGET (priv->main_box));

	if (theme) {
		gtk_style_context_add_class (style_box, theme);
		if (g_str_equal (theme, "internal")) {
			gtk_style_context_remove_class (style_box, "external");
		} else {
			gtk_style_context_remove_class (style_box, "internal");
		}
	} else {
		gtk_style_context_remove_class (style_box, "internal");
		gtk_style_context_remove_class (style_box, "external");
	}
}
