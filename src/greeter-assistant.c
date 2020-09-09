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
//#include "greeter-gpki-page.h"
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

#if 0
static int
get_page_index (GreeterPage *page)
{
	int page_index = 0;

	if (page) {
		PageData *page_data = NULL;
	    for (page_data = page_table; page_data->page_id != NULL; ++page_data) {
			g_print ("page_id = %s\n", page_data->page_id);

			++page_index;

			if (g_str_equal (page_data->page_id, GREETER_PAGE_GET_CLASS (page)->page_id))
				break;
		}
	}

	return page_index;
}

static void
widget_destroyed (GtkWidget *widget, GreeterAssistant *assistant)
{
	GreeterPage *page = GREETER_PAGE (widget);
	GreeterAssistantPrivate *priv = assistant->priv;

	priv->pages = g_list_delete_link (priv->pages, page);
	if (page == priv->current_page)
		priv->current_page = NULL;

//	g_slice_free (GreeterAssistantPagePrivate, page->assistant_priv);
//	page->assistant_priv = NULL;
}
#endif

//static void
//visible_child_changed (GreeterAssistant *assistant)
//{
//	g_signal_emit (assistant, signals[PAGE_CHANGED], 0);
//}

//static void
//destroy_pages_after (GreeterAssistant *assistant, GreeterPage *page)
//{
//	GList *pages, *l, *next;
//
//	pages = greeter_assistant_get_all_pages (assistant);
//
//	for (l = pages; l != NULL; l = l->next)
//		if (l->data == page)
//			break;
//
//	l = l->next;
//	for (; l != NULL; l = next) {
//		next = l->next;
//		gtk_widget_destroy (GTK_WIDGET (l->data));
//	}
//}
//

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

#if 0
static void
switch_to_next_page (GreeterAssistant *assistant)
{
	switch_to (assistant, find_next_page (assistant));
}
#endif

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
//
//
//static void
//on_apply_done (GreeterPage *page,
//               gboolean valid,
//               gpointer user_data)
//{
//	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
//
//	if (valid)
//		switch_to_next_page (assistant);
//}
//
//static void
//rebuild_pages (GreeterAssistant *assistant)
//{
//	PageData *page_data;
//	GreeterPage *page;
////  GreeterAssistant *assistant;
//	GreeterPage *current_page;
////  gchar **skip_pages;
////  gboolean is_new_user, skipped;
//	GreeterAssistantPrivate *priv = assistant->priv;
//
////  assistant = greeter_driver_get_assistant (driver);
//	current_page = greeter_assistant_get_current_page (assistant);
//	page_data = page_table;
//
////  g_ptr_array_free (skipped_pages, TRUE);
////  skipped_pages = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_widget_destroy);
//
//  if (current_page != NULL) {
//    destroy_pages_after (assistant, current_page);
//
//    for (page_data = page_table; page_data->page_id != NULL; ++page_data)
//      if (g_str_equal (page_data->page_id, GREETER_PAGE_GET_CLASS (current_page)->page_id))
//        break;
//
//    ++page_data;
//  }
//
////  is_new_user = (greeter_driver_get_mode (driver) == GREETER_DRIVER_MODE_NEW_USER);
////  skip_pages = pages_to_skip_from_file (is_new_user);
//
//	for (; page_data->page_id != NULL; ++page_data) {
//g_print ("page_id = %s\n", page_data->page_id);
////	  skipped = FALSE;
//
////	  if ((page_data->new_user_only && !is_new_user) ||
////			  (should_skip_page (driver, page_data->page_id, skip_pages)))
////		  skipped = TRUE;
//
//		page = page_data->prepare_page_func (priv->manager);
//		if (!page)
//			continue;
//
////	  if (skipped && greeter_page_skip (page))
////		  g_ptr_array_add (skipped_pages, page);
////	  else
////		  greeter_driver_add_page (driver, page);
//
//		greeter_assistant_add_page (assistant, page);
//	}
//
////	g_strfreev (skip_pages);
//}
//

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

//static void
//update_applying_state (GreeterAssistant *assistant)
//{
//	gboolean applying = FALSE;
//	gboolean is_first_page = FALSE;
//
//	GreeterAssistantPrivate *priv = greeter_assistant_get_instance_private (assistant);
//	if (priv->current_page)
//	{
//		applying = greeter_page_get_applying (priv->current_page);
//		is_first_page = (find_prev_page (assistant) == NULL);
//	}
//	gtk_widget_set_sensitive (priv->forward, !applying);
//	gtk_widget_set_visible (priv->backward, !applying && !is_first_page);
////	gtk_widget_set_visible (priv->cancel, applying);
////	gtk_widget_set_visible (priv->spinner, applying);
//
////	if (applying)
////		gtk_spinner_start (GTK_SPINNER (priv->spinner));
////	else
////		gtk_spinner_stop (GTK_SPINNER (priv->spinner));
//}


static void
update_navigation_buttons (GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;
	gboolean is_last_page;

	if (priv->current_page == NULL)
		return;

	gtk_widget_hide (priv->first);

	is_last_page = (find_next_page (assistant) == NULL);

	g_print ("is last page = %d\n", is_last_page);

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
go_first_cb (GtkWidget *button, gpointer user_data)
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
				"dialog-information",
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
go_forward_cb (GtkWidget *button, gpointer user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	greeter_assistant_next_page (assistant);
}

static void
go_backward_cb (GtkWidget *button, gpointer user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	greeter_assistant_prev_page (assistant);
}

//static void
//do_cancel_cb (GtkWidget *button, gpointer user_data)
//{
//	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
//	GreeterAssistantPrivate *priv = assistant->priv;
//	if (priv->current_page)
//		greeter_page_apply_cancel (priv->current_page);
//}
//
static void
page_notify_cb (GreeterPage      *page,
                GParamSpec       *pspec,
                GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (page != priv->current_page)
		return;

	if (strcmp (pspec->name, "title") == 0)
	{
//		g_object_notify_by_pspec (G_OBJECT (assistant), obj_props[PROP_TITLE]);
		update_titlebar (assistant);
	}
//	else if (strcmp (pspec->name, "applying") == 0)
//	{
//		update_applying_state (assistant);
//	}
	else
	{
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

#if 0
static void
greeter_assistant_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (object);
	GreeterAssistantPrivate *priv = assistant->priv;

	switch (prop_id)
	{
		case PROP_GREETER:
			priv->greeter = g_value_get_pointer (value);
		break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_assistant_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (object);
	GreeterAssistantPrivate *priv = assistant->priv;

	switch (prop_id)
	{
		case PROP_GREETER:
			g_value_set_pointer (value, (gpointer) priv->greeter);
		break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
#endif

static void
greeter_assistant_ui_setup (GreeterAssistant *assistant)
{
	GdkPixbuf *pixbuf = NULL;

	pixbuf = gdk_pixbuf_new_from_resource_at_scale ("/kr/gooroom/greeter/gooroom-logo",
                                                    72, 36, FALSE, NULL);
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

g_print ("assistant width = %d\n", pref_w);
g_print ("assistant height = %d\n", pref_h);
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

	g_signal_connect (priv->stack, "notify::visible-child",
                      G_CALLBACK (current_page_changed_cb), assistant);

	g_signal_connect (priv->first, "clicked", G_CALLBACK (go_first_cb), assistant);
	g_signal_connect (priv->forward, "clicked", G_CALLBACK (go_forward_cb), assistant);
	g_signal_connect (priv->backward, "clicked", G_CALLBACK (go_backward_cb), assistant);

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

//	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, cancel);
//  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, main_layout);
//  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, spinner);
//  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, titlebar);
//	gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), visible_child_changed);

//	gobject_class->get_property = greeter_assistant_get_property;
//	gobject_class->set_property = greeter_assistant_set_property;

#if 0
	obj_props[PROP_GREETER] =
		g_param_spec_pointer ("greeter", "", "",
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);


	obj_props[PROP_TITLE] = g_param_spec_string ("title",
                                                 "", "",
                                                 NULL,
                                                 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * GreeterAssistant::page-changed:
	 * @assistant: the #GreeterAssistant
	 *
	 * The ::page-changed signal is emitted when the visible page
	 * changed.
	 */
	signals[PAGE_CHANGED] = g_signal_new ("page-changed",
                                          G_TYPE_FROM_CLASS (gobject_class),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 0);

	g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
#endif
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

//	g_signal_connect (page, "destroy", G_CALLBACK (widget_destroyed), assistant);
	g_signal_connect (page, "notify", G_CALLBACK (page_notify_cb), assistant);

	gtk_container_add (GTK_CONTAINER (priv->stack), GTK_WIDGET (page));

	gtk_widget_set_halign (GTK_WIDGET (page), GTK_ALIGN_FILL);
	gtk_widget_set_valign (GTK_WIDGET (page), GTK_ALIGN_FILL);

//	update_navigation_buttons (assistant);
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
