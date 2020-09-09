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

#include <glib/gi18n.h>
#include <gio/gio.h>


struct _GreeterModePagePrivate {
	GtkWidget *on_button;
	GtkWidget *off_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterModePage, greeter_mode_page, GREETER_TYPE_PAGE);



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

/*
    GtkWidget *toplevel;
    const char *message;
    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
    message = _("Authentication is in progress.\nPlease wait...");

    greeter_page_manager_show_splash (manager, toplevel, message);
*/
}

#if 0
static void
setup_page_ui (GreeterModePage *page)
{
//	GreeterModePagePrivate *priv = page->priv;

//	gtk_button_set_relief (GTK_BUTTON (priv->on_button), GTK_RELIEF_NONE);
//	gtk_button_set_relief (GTK_BUTTON (priv->off_button), GTK_RELIEF_NONE);

//	gchar *markup;
//	markup = g_markup_printf_escaped ("<span font=\"30px\" weight=\"bold\">%s</span>", _("ONLINE"));
//	gtk_label_set_markup (GTK_LABEL (priv->on_label), markup);
//	g_free (markup);
//
//	markup = g_markup_printf_escaped ("<span font=\"30px\" weight=\"bold\">%s</span>", _("OFFLINE"));
//	gtk_label_set_markup (GTK_LABEL (priv->off_label), markup);
//	g_free (markup);
}

static void
greeter_mode_page_constructed (GObject *object)
{
	gchar *markup = NULL;
	GreeterModePage *page = GREETER_MODE_PAGE (object);
	GreeterModePagePrivate *priv = page->priv;

	G_OBJECT_CLASS (greeter_mode_page_parent_class)->constructed (object);

	greeter_page_set_title (GREETER_PAGE (page), _("Which mode would you like to use?"));

	markup = g_markup_printf_escaped ("<span font=\"30px\" weight=\"bold\">%s</span>", _("Online"));
	gtk_label_set_markup (GTK_LABEL (priv->on_label), markup);
	g_free (markup);

	markup = g_markup_printf_escaped ("<span font=\"30px\" weight=\"bold\">%s</span>", _("Offline"));
	gtk_label_set_markup (GTK_LABEL (priv->off_label), markup);
	g_free (markup);

	g_signal_connect (G_OBJECT (priv->on_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	g_signal_connect (G_OBJECT (priv->off_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	gtk_widget_show (GTK_WIDGET (page));

	greeter_page_set_complete (GREETER_PAGE (page), TRUE);
}

static void
greeter_mode_page_finalize (GObject *object)
{
//	GreeterModePage *page = GREETER_MODE_PAGE (object);
//	GreeterModePagePrivate *priv = page->priv;

	G_OBJECT_CLASS (greeter_mode_page_parent_class)->finalize (object);
}

static void
greeter_mode_page_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
	gint size, pref_w, pref_h;
	GreeterModePage *page = GREETER_MODE_PAGE (widget);
	GreeterModePagePrivate *priv = page->priv;

	if (GTK_WIDGET_CLASS (greeter_mode_page_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (greeter_mode_page_parent_class)->size_allocate (widget, allocation);

	gtk_widget_get_preferred_width (GTK_WIDGET (priv->on_button), NULL, &pref_w);
	gtk_widget_get_preferred_height (GTK_WIDGET (priv->on_button), NULL, &pref_h);

g_print ("button size width = %d\n", pref_w);
g_print ("button size height = %d\n", pref_h);

	size = MAX (pref_w, pref_h);

	gtk_widget_set_size_request (priv->on_button, size, size);
	gtk_widget_set_size_request (priv->off_button, size, size);
}

static void
greeter_mode_page_realize (GtkWidget *widget)
{
	gint size, pref_w, pref_h;
	GreeterModePage *page = GREETER_MODE_PAGE (widget);
	GreeterModePagePrivate *priv = page->priv;

	gtk_widget_get_preferred_width (GTK_WIDGET (priv->on_button), NULL, &pref_w);
	gtk_widget_get_preferred_height (GTK_WIDGET (priv->on_button), NULL, &pref_h);

g_print ("button size width = %d\n", pref_w);
g_print ("button size height = %d\n", pref_h);

	size = MAX (pref_w, pref_h);

	gtk_widget_set_size_request (priv->on_button, size, size);
	gtk_widget_set_size_request (priv->off_button, size, size);

	if (GTK_WIDGET_CLASS (greeter_mode_page_parent_class)->realize)
		GTK_WIDGET_CLASS (greeter_mode_page_parent_class)->realize (widget);
}
#endif

static void
greeter_mode_page_out (GreeterPage *page,
                       gboolean     next)
{
	g_print ("greeter_mode_page_out : %d\n", next);
}

static void
greeter_mode_page_shown (GreeterPage *page)
{
	int mode = MODE_ONLINE;
	GreeterModePage *self = GREETER_MODE_PAGE (page);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->off_button)))
		mode = MODE_OFFLINE;

	greeter_page_manager_set_mode (page->manager, mode);
}

static gboolean
greeter_mode_page_should_show (GreeterPage *page)
{
	return TRUE;
}

static void
greeter_mode_page_init (GreeterModePage *page)
{
	GreeterModePagePrivate *priv;

	priv = page->priv = greeter_mode_page_get_instance_private (page);

	gtk_widget_init_template (GTK_WIDGET (page));

	// 시스템 동작 모드 설정
	greeter_page_set_title (GREETER_PAGE (page), _("System operation mode setting"));

//	setup_page_ui (page);

	g_signal_connect (G_OBJECT (priv->on_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	g_signal_connect (G_OBJECT (priv->off_button), "toggled",
                      G_CALLBACK (mode_button_toggled_cb), page);

	gtk_widget_show (GTK_WIDGET (page));

	greeter_page_set_complete (GREETER_PAGE (page), TRUE);
}

static void
greeter_mode_page_class_init (GreeterModePageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
//	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
//	GObjectClass *object_class = G_OBJECT_CLASS (klass);


	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-mode-page.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, on_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterModePage, off_button);

	page_class->page_id = PAGE_ID;
	page_class->out = greeter_mode_page_out;
	page_class->shown = greeter_mode_page_shown;
	page_class->should_show = greeter_mode_page_should_show;

//	object_class->constructed = greeter_mode_page_constructed;
//	object_class->finalize = greeter_mode_page_finalize;
//	widget_class->size_allocate = greeter_mode_page_size_allocate;
//	widget_class->realize = greeter_mode_page_realize;
}

GreeterPage *
greeter_prepare_mode_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_MODE_PAGE,
                         "manager", manager,
                         NULL);
}
