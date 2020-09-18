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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <lightdm.h>

#include "greeter-assistant.h"
#include "greeter-mode-page.h"
#include "greeter-network-page.h"
#include "greeter-vpn-page.h"
#include "greeter-ars-page.h"
#include "greeter-login-page.h"
#include "greeter-message-dialog.h"



struct _GreeterAssistantPrivate {
	GtkWidget *stack;
	GtkWidget *first;
	GtkWidget *forward;
	GtkWidget *backward;
	GtkWidget *title;
	GtkWidget *logo_image;

	GList *pages;
	GreeterPage *current_page;

	GreeterPageManager *manager;
};

typedef GreeterPage *(*PreparePage) (GreeterPageManager *manager);

typedef struct {
  const gchar *page_id;
  PreparePage  prepare_page_func;
} PageData;


static PageData page_table[] = {
	{ "mode",     greeter_prepare_mode_page    },
	{ "network",  greeter_prepare_network_page },
	{ "vpn",      greeter_prepare_vpn_page     },
	{ "login",    greeter_prepare_login_page   },
	{ NULL, NULL }
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterAssistant, greeter_assistant, GTK_TYPE_BOX)



static void
switch_to (GreeterAssistant *assistant, GreeterPage *page)
{
	if (!page)
		return;

	gtk_stack_set_visible_child (GTK_STACK (assistant->priv->stack), GTK_WIDGET (page));
}

static GreeterPage *
find_first_page (GreeterAssistant *assistant)
{
	GList *l = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	l = g_list_first (priv->pages);
	if (l) {
		GreeterPage *page = GREETER_PAGE (l->data);
		if (greeter_page_should_show (page))
			return page;
	}

	return NULL;
}

static GreeterPage *
find_next_page (GreeterAssistant *assistant)
{
	GList *l = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	l = g_list_find (priv->pages, priv->current_page);
	if (l) l = l->next;

	for (; l != NULL; l = l->next) {
		GreeterPage *page = GREETER_PAGE (l->data);
		if (greeter_page_should_show (page))
			return page;
	}

	return NULL;
}

static GreeterPage *
find_prev_page (GreeterAssistant *assistant)
{
	GList *l = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	l = g_list_find (priv->pages, priv->current_page);
	if (l) l = l->prev;

	for (; l != NULL; l = l->prev) {
		GreeterPage *page = GREETER_PAGE (l->data);
		if (greeter_page_should_show (page))
			return page;
	}

	return NULL;
}

static void
set_suggested_action_sensitive (GtkWidget *widget, gboolean sensitive)
{
  gtk_widget_set_sensitive (widget, sensitive);
  if (sensitive)
    gtk_style_context_add_class (gtk_widget_get_style_context (widget), "suggested-action");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "suggested-action");
}

static void
set_navigation_button (GreeterAssistant *assistant, GtkWidget *widget)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	gtk_widget_set_visible (priv->forward, (widget == priv->forward));
}

//static void
//initialize_pages (GList *pages)
//{
//	GList *l = NULL;
//	for (l = pages; l != NULL; l = l->next)
//		greeter_page_initialize (l->data);
//}

static void
update_titlebar (GreeterAssistant *assistant)
{
	const gchar *title;
	GreeterAssistantPrivate *priv = assistant->priv;

	title = greeter_assistant_get_title (assistant);

	if (title)
		gtk_label_set_text (GTK_LABEL (priv->title), title);
}

static void
update_navigation_buttons (GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;
	gboolean is_last_page;

	if (priv->current_page == NULL)
		return;

	gtk_widget_hide (priv->first);

	is_last_page = (find_next_page (assistant) == NULL);

	if (is_last_page) {
		gtk_widget_hide (priv->backward);
		gtk_widget_hide (priv->forward);
		gtk_widget_show (priv->first);
	} else {
		gboolean is_first_page;

		is_first_page = (find_prev_page (assistant) == NULL);

		gtk_widget_set_visible (priv->backward, !is_first_page);

		if (greeter_page_get_complete (priv->current_page)) {
			set_suggested_action_sensitive (priv->forward, TRUE);
		} else {
			set_suggested_action_sensitive (priv->forward, FALSE);
		}
		set_navigation_button (assistant, priv->forward);
	}
}

static void
update_current_page (GreeterAssistant *assistant,
                     GreeterPage      *page)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (priv->current_page == page)
		return;

	priv->current_page = page;

	update_titlebar (assistant);
	update_navigation_buttons (assistant);
	gtk_widget_grab_focus (priv->forward);

//	gboolean is_first_page = FALSE;
//	is_first_page = (find_prev_page (assistant) == NULL);
//	if (is_first_page)
//		initialize_pages (priv->pages);

	if (page)
		greeter_page_shown (page);
}

static void
go_first_button_cb (GtkWidget *button,
                    gpointer   user_data)
{
	int res;
	const gchar *title;
	const gchar *message;
	GtkWidget *dialog, *toplevel;
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	if (greeter_page_manager_get_mode (assistant->priv->manager) == MODE_ONLINE) {
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (assistant));

		title = _("VPN Connection Termination Warning");
		message = _("If you return to the first step, the VPN connection is terminated and "
                    "you need to proceed with the authentication process again."
                    "Would you like to continue anyway?");

		dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
				"network-vpn-symbolic",
				title,
				message);

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Ok"), GTK_RESPONSE_OK,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				NULL);

		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

		res = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (res == GTK_RESPONSE_CANCEL)
			return;
	}

	greeter_assistant_first_page (assistant);
}

static void
go_forward_button_cb (GtkWidget *button,
                      gpointer   user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	greeter_assistant_next_page (assistant);
}

static void
go_backward_button_cb (GtkWidget *button,
                       gpointer   user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	greeter_assistant_prev_page (assistant);
}

static void
page_notify_cb (GreeterPage      *page,
                GParamSpec       *pspec,
                GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (page != priv->current_page)
		return;

	if (strcmp (pspec->name, "title") == 0) {
		update_titlebar (assistant);
	} else {
		update_navigation_buttons (assistant);
	}
}

static void
current_page_changed_cb (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
	GtkWidget *new_page = gtk_stack_get_visible_child (GTK_STACK (gobject));

	update_current_page (assistant, GREETER_PAGE (new_page));
}

static gboolean
change_current_page_idle (gpointer user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	current_page_changed_cb (G_OBJECT (assistant->priv->stack), NULL, assistant);

	return FALSE;
}

static void
go_next_page_cb (GreeterPageManager *manager,
                 gpointer            user_data)
{
	greeter_assistant_next_page (GREETER_ASSISTANT (user_data));
}

static void
go_first_page_cb (GreeterPageManager *manager,
                  gpointer            user_data)
{
	greeter_assistant_first_page (GREETER_ASSISTANT (user_data));
}

static void
greeter_assistant_ui_setup (GreeterAssistant *assistant)
{
	GdkPixbuf *pixbuf = NULL;

	pixbuf = gdk_pixbuf_new_from_resource_at_scale ("/kr/gooroom/greeter/logo",
                                                    102, 44, FALSE, NULL);
	if (pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (assistant->priv->logo_image), pixbuf);
		g_object_unref (pixbuf);
	}
}

#if 0
static void
greeter_assistant_realize (GtkWidget *widget)
{
	gint pref_w, pref_h;
//	GreeterAssistant *assistant = GREETER_ASSISTANT (widget);

	if (GTK_WIDGET_CLASS (greeter_assistant_parent_class)->realize)
		GTK_WIDGET_CLASS (greeter_assistant_parent_class)->realize (widget);

	gtk_widget_get_preferred_width (widget, NULL, &pref_w);
	gtk_widget_get_preferred_height (widget, NULL, &pref_h);
}
#endif

static void
greeter_assistant_finalize (GObject *object)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (object);
	GreeterAssistantPrivate *priv = assistant->priv;

	g_clear_object (&priv->manager);

	if (priv->pages) {
		g_list_free (priv->pages);
		priv->pages = NULL;
	}

	G_OBJECT_CLASS (greeter_assistant_parent_class)->finalize (object);
}

static void
greeter_assistant_init (GreeterAssistant *assistant)
{
	PageData *page_data;
	GreeterAssistantPrivate *priv;

	priv = assistant->priv = greeter_assistant_get_instance_private (assistant);

	gtk_widget_init_template (GTK_WIDGET (assistant));

	greeter_assistant_ui_setup (assistant);

	priv->manager = greeter_page_manager_new ();

	page_data = page_table;
	for (; page_data->page_id != NULL; ++page_data) {
		GreeterPage *page = page_data->prepare_page_func (priv->manager);
		if (!page)
			continue;

		greeter_assistant_add_page (assistant, page);
	}

	g_signal_connect (priv->manager, "go-next", G_CALLBACK (go_next_page_cb), assistant);
	g_signal_connect (priv->manager, "go-first", G_CALLBACK (go_first_page_cb), assistant);

	g_signal_connect (priv->stack, "notify::visible-child",
                      G_CALLBACK (current_page_changed_cb), assistant);

	g_signal_connect (priv->first, "clicked", G_CALLBACK (go_first_button_cb), assistant);
	g_signal_connect (priv->forward, "clicked", G_CALLBACK (go_forward_button_cb), assistant);
	g_signal_connect (priv->backward, "clicked", G_CALLBACK (go_backward_button_cb), assistant);

	g_idle_add ((GSourceFunc) change_current_page_idle, assistant);
}

static void
greeter_assistant_class_init (GreeterAssistantClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
//	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->finalize = greeter_assistant_finalize;
//	widget_class->realize = greeter_assistant_realize;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-assistant.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, stack);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, first);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, forward);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, backward);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, title);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, logo_image);
}

GtkWidget *
greeter_assistant_new (void)
{
	return g_object_new (GREETER_TYPE_ASSISTANT, NULL);
}

void
greeter_assistant_add_page (GreeterAssistant *assistant,
                            GreeterPage      *page)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	priv->pages = g_list_append (priv->pages, page);

	g_signal_connect (page, "notify", G_CALLBACK (page_notify_cb), assistant);

	gtk_container_add (GTK_CONTAINER (priv->stack), GTK_WIDGET (page));

	gtk_widget_set_halign (GTK_WIDGET (page), GTK_ALIGN_FILL);
	gtk_widget_set_valign (GTK_WIDGET (page), GTK_ALIGN_FILL);
}

GreeterPage *
greeter_assistant_get_current_page (GreeterAssistant *assistant)
{
	return assistant->priv->current_page;
}

const gchar *
greeter_assistant_get_title (GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (priv->current_page != NULL)
		return greeter_page_get_title (priv->current_page);
	else
		return "";
}

void
greeter_assistant_first_page (GreeterAssistant *assistant)
{
	GreeterPage *first_page;
	GreeterAssistantPrivate *priv = assistant->priv;

	first_page = find_first_page (assistant);

	if (first_page && priv->current_page &&
        (priv->current_page != first_page)) {

		GList *l = g_list_first (priv->pages);
		for (l = l->next; l; l = l->next) {
			GreeterPage *page = GREETER_PAGE (l->data);
			if (greeter_page_should_show (page))
				greeter_page_out (page, FALSE);
		}
	}

	switch_to (assistant, first_page);
}

void
greeter_assistant_next_page (GreeterAssistant *assistant)
{
	GreeterPage *next_page;
	GreeterAssistantPrivate *priv = assistant->priv;

	next_page = find_next_page (assistant);

	if (next_page && priv->current_page &&
        (priv->current_page != next_page)) {
		greeter_page_out (priv->current_page, TRUE);
	}

	switch_to (assistant, next_page);
}

void
greeter_assistant_prev_page (GreeterAssistant *assistant)
{
	GreeterPage *prev_page;
	GreeterAssistantPrivate *priv = assistant->priv;

	prev_page = find_prev_page (assistant);

	if (prev_page && priv->current_page &&
        (priv->current_page != prev_page)) {
		greeter_page_out (priv->current_page, FALSE);
	}

	switch_to (assistant, prev_page);
}
