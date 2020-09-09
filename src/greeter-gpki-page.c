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

#define PAGE_ID "gpki"

#include "config.h"
#include "greeter-gpki-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <webkit2/webkit2.h>



struct _GreeterGPKIPagePrivate {
	GtkWidget *webview_window;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterGPKIPage, greeter_gpki_page, GREETER_TYPE_PAGE);


static void
sync_complete (GreeterGPKIPage *page)
{
	greeter_page_set_complete (GREETER_PAGE (page), TRUE);
}

static void
greeter_gpki_page_mode_changed (GreeterPage *page,
                                int          mode)
{
	gtk_widget_set_visible (GTK_WIDGET (page), (mode == MODE_ONLINE));
}

#if 0
static gboolean
grab_focus_idle (gpointer data)
{
	gtk_widget_grab_focus (GTK_WIDGET (data));
	
	return FALSE;
}
#endif


static void
web_view_script_message_received_deny_click_cb (WebKitUserContentManager *manager,
                                                WebKitJavascriptResult   *js_result,
                                                gpointer                  user_data)
{
	g_print ("web_view_script_message_received_deny_click_cb\n");
}

static gboolean
webview_draw_cb (GtkWidget *widget,
                 cairo_t   *cr,
                 gpointer   user_data)
{
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 0.0); /* transparent */
	cairo_paint (cr);

	return FALSE;
}

static gboolean
webview_window_draw_cb (GtkWidget *widget,
                        cairo_t   *cr,
                        gpointer   user_data)
{
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0); /* transparent */
	cairo_paint (cr);

	return FALSE;
}

static GtkWidget *
transparent_webview_new (GreeterGPKIPage *page)
{
	GtkWidget *window, *scroll_window, *webview;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

//	gtk_window_set_accept_focus (GTK_WINDOW (window), TRUE);
//	gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
//	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
	if(gdk_screen_is_composited(screen)) {
		GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
		if (visual == NULL)
			visual = gdk_screen_get_system_visual (screen);

		gtk_widget_set_visual (GTK_WIDGET(window), visual);
	}


	webview = webkit_web_view_new();

//	webkit_web_context_set_web_extensions_directory (webkit_web_context_get_default (),
//                                                     WEB_EXTENSIONS_DIR);
	webkit_web_view_load_uri (WEBKIT_WEB_VIEW (webview), "http://kara.softforum.com/AnySign/sample/sign.jsp");
//	webkit_web_view_load_uri (WEBKIT_WEB_VIEW (webview), "file:///home/user/gpki/test1.html");

	const GdkRGBA rgba= {0, 0, 0, 0.0};
	webkit_web_view_set_background_color (WEBKIT_WEB_VIEW (webview), &rgba);

	gtk_container_add (GTK_CONTAINER (window), webview);
//	gtk_widget_set_halign (webview, GTK_ALIGN_CENTER);
//	gtk_widget_set_valign (webview, GTK_ALIGN_CENTER);

//	g_signal_connect (window, "draw", G_CALLBACK (webview_window_draw_cb), NULL);
//	g_signal_connect (webview, "draw", G_CALLBACK (webview_draw_cb), NULL);

	return window;
}

static void
greeter_gpki_page_shown (GreeterPage *greeter_page)
{
#if 0
	GreeterGPKIPage *page = GREETER_GPKI_PAGE (greeter_page);
	GreeterGPKIPagePrivate *priv = page->priv;

	if (priv->webview_window)
		gtk_widget_show_all (priv->webview_window);
#endif
}

static void
greeter_gpki_page_constructed (GObject *object)
{
	GdkMonitor *m;
	GdkRectangle rect;
	GreeterGPKIPage *page = GREETER_GPKI_PAGE (object);
	GreeterGPKIPagePrivate *priv = page->priv;

	G_OBJECT_CLASS (greeter_gpki_page_parent_class)->constructed (object);

	greeter_page_set_title (GREETER_PAGE (page), _("GPKI Authentication"));

	m = gdk_display_get_primary_monitor (gdk_display_get_default ());
	gdk_monitor_get_geometry (m, &rect);

	priv->webview_window = transparent_webview_new (page);
	gtk_widget_set_size_request (priv->webview_window, rect.width, rect.height);
	gtk_widget_hide (priv->webview_window);

	gtk_widget_show (GTK_WIDGET (page));

	sync_complete (page);
}

static void
greeter_gpki_page_dispose (GObject *object)
{
	G_OBJECT_CLASS (greeter_gpki_page_parent_class)->dispose (object);
}

static void
greeter_gpki_page_class_init (GreeterGPKIPageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-gpki-page.ui");

//	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterGPKIPage, webview_window);

	page_class->page_id = PAGE_ID;
	page_class->shown = greeter_gpki_page_shown;
	page_class->mode_changed = greeter_gpki_page_mode_changed;

	object_class->constructed = greeter_gpki_page_constructed;
	object_class->dispose = greeter_gpki_page_dispose;
}

static void
greeter_gpki_page_init (GreeterGPKIPage *page)
{
	page->priv = greeter_gpki_page_get_instance_private (page);

	gtk_widget_init_template (GTK_WIDGET (page));
}

GreeterPage *
greeter_prepare_gpki_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_GPKI_PAGE,
                         "manager", manager,
                         NULL);
}
