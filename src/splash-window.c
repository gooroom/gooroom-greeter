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
	GtkWidget *parent;
	GtkWidget *spinner;
	GtkWidget *message_label;
};



G_DEFINE_TYPE_WITH_PRIVATE (SplashWindow, splash_window, GTK_TYPE_WINDOW);


static void
set_parent (SplashWindow *window,
            GtkWidget    *parent)
{
	if (parent)
		window->priv->parent = parent;
}

static void
splash_window_finalize (GObject *object)
{
	G_OBJECT_CLASS (splash_window_parent_class)->finalize (object);
}

static void
splash_window_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
	SplashWindow *window = SPLASH_WINDOW (widget);
	SplashWindowPrivate *priv = window->priv;

	if (!gtk_widget_get_realized (widget))
		return;

	if (priv->parent) {
		GtkWidget *toplevel;
		gint x, y, p_w, p_h;

		toplevel = gtk_widget_get_toplevel (priv->parent);

		gtk_widget_translate_coordinates (priv->parent, toplevel, 0, 0, &x, &y);
		gtk_widget_get_size_request (priv->parent, &p_w, &p_h);

		gtk_window_move (GTK_WINDOW (window),
                         x + (p_w - allocation->width) / 2,
                         y + (p_h - allocation->height) / 2 );
	}

	if (GTK_WIDGET_CLASS (splash_window_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (splash_window_parent_class)->size_allocate (widget, allocation);
}

static void
splash_window_init (SplashWindow *window)
{
	PangoAttrList *attrs;
	PangoAttribute *attr;
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

	attrs = pango_attr_list_new ();
	attr = pango_attr_rise_new (8000);
	pango_attr_list_insert (attrs, attr);

	gtk_label_set_attributes (GTK_LABEL (window->priv->message_label), attrs);
}

static void
splash_window_class_init (SplashWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = splash_window_finalize;
	widget_class->size_allocate = splash_window_size_allocate;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/splash-window.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  SplashWindow, message_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  SplashWindow, spinner);
}

GtkWidget *
splash_window_new (GtkWidget *parent)
{
	GObject *object;

	object = g_object_new (SPLASH_TYPE_WINDOW,
                           "transient-for", NULL,
                           NULL);

	set_parent (SPLASH_WINDOW (object), parent);

	return GTK_WIDGET (object);
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
	g_return_if_fail (SPLASH_IS_WINDOW (window));

	SplashWindowPrivate *priv = window->priv;

	if (message) {
		gtk_label_set_text (GTK_LABEL (priv->message_label), message);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->message_label), "");
	}
}
