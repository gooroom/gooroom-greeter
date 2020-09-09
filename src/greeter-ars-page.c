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

#define PAGE_ID "ars"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <ctype.h>

#include "greeter-ars-page.h"
#include "libars/gooroom-ars-auth.h"


struct _GreeterARSPagePrivate {
	GtkWidget *title_label;
	GtkWidget *ok_button;
	GtkWidget *req_button;
	GtkWidget *message_label;
	GtkWidget *auth_no_text;
	GtkWidget *auth_no_label;
	GtkWidget *tn_entry;

	gchar     *tran_id;
	gchar     *auth_no;

	guint      timeout_id;

	gboolean   auth_success;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterARSPage, greeter_ars_page, GREETER_TYPE_PAGE);


static gboolean
grab_focus_idle (gpointer data)
{
	gtk_widget_grab_focus (GTK_WIDGET (data));
	
	return FALSE;
}

static gchar *
get_tran_id (void)
{
	GDateTime *dt = NULL;
	gchar *tran_id = NULL;
	gchar *str_time = NULL;

	// tran_id
	dt = g_date_time_new_now_local ();
	str_time = g_date_time_format (dt, "%Y%m%d%H%M%S");
	g_date_time_unref (dt);

	tran_id = g_strdup_printf ("000000%s", str_time);

	g_free (str_time);

	return tran_id;
}

#if 0
static void
sync_complete (GreeterARSPage *page)
{
	greeter_page_set_complete (GREETER_PAGE (page), TRUE);
}
#endif

static GtkWidget *
dialog_new (GtkMessageType  type,
            GtkButtonsType  buttons,
            const gchar    *icon_name,
            const gchar    *title,
            const gchar    *primary_message,
            const gchar    *secondary_message)
{
	GtkWidget *dialog;
	GtkWidget *content_area, *message_area, *parent, *icon;

	dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     type,
                                     buttons,
                                     NULL);

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));

	gtk_box_set_spacing (GTK_BOX (content_area), 0);
	gtk_widget_set_valign (message_area, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (message_area, GTK_ALIGN_START);

	parent = gtk_widget_get_parent (message_area);
	gtk_widget_set_margin_start (parent, 18);
	gtk_widget_set_margin_end (parent, 18);
	gtk_widget_set_margin_top (parent, 12);
	gtk_widget_set_margin_bottom (parent, 12);
	gtk_box_set_spacing (GTK_BOX (parent), 24);

	icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_halign (icon, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (parent), icon);
	gtk_box_reorder_child (GTK_BOX (parent), icon, 0);

	if (title)
		gtk_window_set_title (GTK_WINDOW (dialog), title);

	if (primary_message) {
		gchar *markup;
		markup = g_markup_printf_escaped ("<span weight=\'bold\'>%s</span>", primary_message);
		gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
		g_free (markup);
	}

	if (secondary_message) {
		gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s", secondary_message);
	}

	return dialog;
}

#if 0
static int
close_warning_message_show (void)
{
	int          result;
	GtkWidget   *dialog;
	const gchar *primary_message;
	const gchar *secondary_message;

	primary_message = _("ARS Authentication Cancellation"),
	secondary_message = _("When canceling ARS authentication, you must perform VPN login again.\n"
                          "Would you like to cancel ARS authentication?");

	dialog = dialog_new (GTK_MESSAGE_INFO,
                         GTK_BUTTONS_NONE,
                         "dialog-warning",
                         _("ARS Authentication Cancellation"),
                         primary_message,
                         secondary_message);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            _("Ok"), GTK_RESPONSE_OK,
                            _("Cancel"), GTK_RESPONSE_CANCEL,
                            NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	gtk_widget_show_all (dialog);
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return result;
}
#endif

static void
update_authentication_number (GreeterARSPage *page, const gchar *cn)
{
	gchar *markup;
	GreeterARSPagePrivate *priv = page->priv;

	gtk_widget_hide (priv->message_label);

	markup = g_markup_printf_escaped ("<span font=\'20px\' weight=\'bold\'>%s</span>",
                                      _("Certification Number"));
	gtk_label_set_markup (GTK_LABEL (priv->auth_no_text), markup);
	g_free (markup);

	markup = g_markup_printf_escaped ("<span font=\'95px\' weight=\'bold\'>%s</span>", cn);
	gtk_label_set_markup (GTK_LABEL (priv->auth_no_label), markup);
	g_free (markup);

	gtk_widget_show (priv->auth_no_text);
	gtk_widget_show (priv->auth_no_label);
}

static void
update_status_message (GreeterARSPage *page,
                       const gchar    *message)
{
	gchar *markup = NULL;
	GreeterARSPagePrivate *priv = page->priv;

	if (message) {
		markup = g_markup_printf_escaped ("<span font=\'16px\' weight=\'bold\'>%s</span>", message);
		gtk_label_set_markup (GTK_LABEL (priv->message_label), markup);
		g_free (markup);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->message_label), "");
	}

	gtk_widget_hide (priv->auth_no_text);
	gtk_widget_hide (priv->auth_no_label);
	gtk_widget_show (priv->message_label);
}

static gboolean
authentication_query_cb (GreeterARSPage *page)
{
	gboolean ret = FALSE;
	gchar *rc = NULL, *msg = NULL;
	GError *error = NULL;
	GreeterARSPagePrivate *priv = page->priv;

	rc = gooroom_ars_confirm (priv->tran_id, TRUE, &error);

g_print ("result code = %s\n", rc);

	if (g_str_equal (rc, "0000")) {
		priv->auth_success = TRUE;
		msg = g_strdup (_("Authentication Success"));
		ret = FALSE;
	} else if (g_str_equal (rc, "0001")) {
		update_authentication_number (page, priv->auth_no);
		ret = TRUE;
	} else if (g_str_equal (rc, "4003")) {
		msg = g_strdup (_("The input time has been exceeded.\nPlease try again."));
		ret = FALSE;
	} else if (g_str_equal (rc, "4011")) {
		msg = g_strdup (_("Phone is busy.\nPlease try again later."));
		ret = FALSE;
	} else if (g_str_equal (rc, "4012") ||
               g_str_equal (rc, "4013") ||
               g_str_equal (rc, "4101")) {
		msg = g_strdup (_("Invalid Phone Number"));
		ret = FALSE;
	} else if (g_str_equal (rc, "4014")) {
		msg = g_strdup (_("No response.\nPlease try again later."));
		ret = FALSE;
	} else if (g_str_equal (rc, "4017")) {
		msg = g_strdup (_("Entered the wrong authentication number.\nPlease try again."));
		ret = FALSE;
	} else {
		msg = g_strdup (_("Authentication Failure.\nPlease try again."));
		ret = FALSE;
	}

	if (msg) {
		update_status_message (page, msg);
		g_free (msg);
	}

	if (!ret) {
		if (priv->timeout_id > 0) {
			g_source_remove (priv->timeout_id);
			priv->timeout_id = 0;
		}

		/* ARS Authentication is success */
		if (priv->auth_success) {
			gtk_widget_show (priv->ok_button);
			gtk_widget_set_sensitive (priv->req_button, FALSE);
		} else {
			gtk_widget_hide (priv->ok_button);
			const char *text = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));
			gtk_widget_set_sensitive (priv->req_button, strlen (text) > 0);
		}
	}

	return ret;
}

static void
request_authentication (GreeterARSPage *page)
{
	const gchar *tn;
	GError *error = NULL;
	GreeterARSPagePrivate *priv = page->priv;

	gtk_widget_hide (priv->ok_button);
	gtk_widget_set_sensitive (priv->req_button, FALSE);

	if (priv->tran_id) {
		g_free (priv->tran_id);
		priv->tran_id = NULL;
	}

	if (priv->auth_no) {
		g_free (priv->auth_no);
		priv->auth_no = NULL;
	}

	priv->auth_success = FALSE;

	update_status_message (page, _("Calling for ARS authentication"));

	// phone number
	tn = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));
	// tran_id
	priv->tran_id = get_tran_id ();

	priv->auth_no = gooroom_ars_authentication (priv->tran_id, tn, TRUE, &error);
	if (priv->auth_no) {
		priv->timeout_id = g_timeout_add (1000, (GSourceFunc) authentication_query_cb, page);
	} else {
		update_status_message (page, NULL);

		const char *text = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));
		gtk_widget_set_sensitive (priv->req_button, strlen (text) > 0);
	}
}


#if 0
static void
ok_button_clicked_cb (GtkButton *button,
                      gpointer   user_data)
{
	GreeterARSPage *page = GREETER_ARS_PAGE (user_data);
}

static void
cancel_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
	GreeterARSPage *page = GREETER_ARS_PAGE (user_data);
	GreeterARSPagePrivate *priv = page->priv;

	if (priv->timeout_id > 0)
		return;

	if (priv->auth_success) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
		return;
	}

	if (close_warning_message_show () == GTK_RESPONSE_OK)
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}
#endif

static void
request_button_clicked_cb (GtkButton *button,
                           gpointer   user_data)
{
	GreeterARSPage *page = GREETER_ARS_PAGE (user_data);
	GreeterARSPagePrivate *priv = page->priv;

	if (priv->timeout_id == 0)
		request_authentication (GREETER_ARS_PAGE (user_data));
}

static void
tn_entry_activate_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GreeterARSPage *page = GREETER_ARS_PAGE (user_data);
	GreeterARSPagePrivate *priv = page->priv;

	if (priv->timeout_id == 0 && gtk_widget_get_sensitive (priv->req_button))
		request_authentication (GREETER_ARS_PAGE (user_data));
}

static void
tn_entry_changed_cb (GtkWidget *widget,
                     gpointer   user_data)
{
	const char *text;
	GreeterARSPage *page = GREETER_ARS_PAGE (user_data);
	GreeterARSPagePrivate *priv = page->priv;

	text = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));

	gtk_widget_set_sensitive (priv->req_button, strlen (text) > 0);
}

static void
tn_entry_insert_text_cb (GtkEditable *editable,
                         const gchar *text,
                         gint         length,
                         gint        *position,
                         gpointer     user_data)
{
	int i;

	for (i = 0; i < length; i++) {
		if (!isdigit(text[i])) {
			g_signal_stop_emission_by_name (G_OBJECT (editable), "insert-text");
			return;
		}
	}
}

static void
greeter_ars_page_shown (GreeterPage *page)
{
	GreeterARSPagePrivate *priv = GREETER_ARS_PAGE (page)->priv;

	update_status_message (GREETER_ARS_PAGE (page), NULL);

	gtk_widget_set_sensitive (priv->req_button, FALSE);

	gtk_entry_set_text (GTK_ENTRY (priv->tn_entry), "");

	g_idle_add ((GSourceFunc) grab_focus_idle, priv->tn_entry);
}

static void
greeter_ars_page_mode_changed (GreeterPage *page,
                               int          mode)
{
	gtk_widget_set_visible (GTK_WIDGET (page), (mode == MODE_ONLINE));
}

static void
greeter_ars_page_finalize (GObject *object)
{
	GreeterARSPage *self = GREETER_ARS_PAGE (object);
	GreeterARSPagePrivate *priv = self->priv;

	g_free (priv->tran_id);
	g_free (priv->auth_no);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	G_OBJECT_CLASS (greeter_ars_page_parent_class)->finalize (object);
}

#if 0
static void
greeter_ars_page_constructed (GObject *object)
{
	GreeterARSPage *self = GREETER_ARS_PAGE (object);
	GreeterARSPagePrivate *priv = self->priv;

	G_OBJECT_CLASS (greeter_ars_page_parent_class)->constructed (object);

	greeter_page_set_title (GREETER_PAGE (self), _("ARS Authentication"));

	gtk_widget_set_sensitive (priv->req_button, FALSE);

	g_signal_connect (G_OBJECT (priv->req_button), "clicked",
                      G_CALLBACK (request_button_clicked_cb), self);

//	g_signal_connect (G_OBJECT (priv->ok_button), "clicked",
//                      G_CALLBACK (ok_button_clicked_cb), self);

	g_signal_connect (G_OBJECT (priv->tn_entry), "activate",
                      G_CALLBACK (tn_entry_activate_cb), self);

	g_signal_connect (G_OBJECT (priv->tn_entry), "changed",
                      G_CALLBACK (tn_entry_changed_cb), self);

	g_signal_connect (G_OBJECT (priv->tn_entry), "insert-text",
                      G_CALLBACK (tn_entry_insert_text_cb), self);

	gtk_widget_show (GTK_WIDGET (self));
}
#endif

static void
greeter_ars_page_init (GreeterARSPage *page)
{
	GreeterARSPagePrivate *priv;

	priv = page->priv = greeter_ars_page_get_instance_private (page);

	priv->tran_id = NULL;
	priv->auth_no = NULL;
	priv->timeout_id = 0;
	priv->auth_success = FALSE;

	gtk_widget_init_template (GTK_WIDGET (page));

	greeter_page_set_title (GREETER_PAGE (page), _("ARS Authentication"));

	g_signal_connect (G_OBJECT (priv->req_button), "clicked",
                      G_CALLBACK (request_button_clicked_cb), page);

//	g_signal_connect (G_OBJECT (priv->ok_button), "clicked",
//                      G_CALLBACK (ok_button_clicked_cb), page);

	g_signal_connect (G_OBJECT (priv->tn_entry), "activate",
                      G_CALLBACK (tn_entry_activate_cb), page);

	g_signal_connect (G_OBJECT (priv->tn_entry), "changed",
                      G_CALLBACK (tn_entry_changed_cb), page);

	g_signal_connect (G_OBJECT (priv->tn_entry), "insert-text",
                      G_CALLBACK (tn_entry_insert_text_cb), page);

	gtk_widget_show (GTK_WIDGET (page));
}

static void
greeter_ars_page_class_init (GreeterARSPageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-ars-page.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSPage, req_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSPage, ok_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSPage, tn_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSPage, message_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSPage, auth_no_text);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSPage, auth_no_label);

	page_class->page_id = PAGE_ID;
	page_class->shown = greeter_ars_page_shown;
	page_class->mode_changed = greeter_ars_page_mode_changed;

//	object_class->constructed = greeter_ars_page_constructed;
	object_class->finalize = greeter_ars_page_finalize;
}

GreeterPage *
greeter_prepare_ars_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_ARS_PAGE,
                         "manager", manager,
                         NULL);
}
