/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <ctype.h>

#include "greeter-ars-dialog.h"
#include "libars/gooroom-ars-auth.h"

struct _GreeterARSDialogPrivate {
	GtkWidget *title_label;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
//	GtkWidget *retry_button;
	GtkWidget *req_ars_button;
	GtkWidget *ars_message_label;
	GtkWidget *auth_no_text;
	GtkWidget *auth_no_label;
	GtkWidget *tn_entry;

	gchar     *ars_tran_id;
	gchar     *ars_auth_no;

	guint      ars_timeout_id;
	gboolean   auth_success;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterARSDialog, greeter_ars_dialog, GTK_TYPE_DIALOG);



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

static void
update_authentication_number (GreeterARSDialog *dialog, const gchar *cn)
{
	gchar *markup;
	GreeterARSDialogPrivate *priv = dialog->priv;

	gtk_widget_hide (priv->ars_message_label);

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
update_status_message (GreeterARSDialog *dialog, const gchar *message)
{
	gchar *markup = NULL;
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (message) {
		markup = g_markup_printf_escaped ("<span font=\'16px\' weight=\'bold\'>%s</span>", message);
		gtk_label_set_markup (GTK_LABEL (priv->ars_message_label), markup);
		g_free (markup);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->ars_message_label), "");
	}

	gtk_widget_hide (priv->auth_no_text);
	gtk_widget_hide (priv->auth_no_label);
	gtk_widget_show (priv->ars_message_label);
}

static gboolean
query_ars_auth_status (GreeterARSDialog *dialog)
{
	gboolean ret = FALSE;
	gchar *rc = NULL, *msg = NULL;
	GError *error = NULL;
	GreeterARSDialogPrivate *priv = dialog->priv;

	rc = gooroom_ars_confirm (priv->ars_tran_id, TRUE, &error);

g_print ("result code = %s\n", rc);

	if (g_str_equal (rc, "0000")) {
		priv->auth_success = TRUE;
		msg = g_strdup (_("Authentication Success"));
		ret = FALSE;
	} else if (g_str_equal (rc, "0001")) {
		update_authentication_number (dialog, priv->ars_auth_no);
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
		update_status_message (dialog, msg);
		g_free (msg);
	}

	if (!ret) {
		if (priv->ars_timeout_id > 0) {
			g_source_remove (priv->ars_timeout_id);
			priv->ars_timeout_id = 0;
		}

//		gtk_widget_set_sensitive (priv->ok_button, priv->auth_success);
		if (priv->auth_success) {
			gtk_widget_show (priv->ok_button);
			gtk_widget_set_sensitive (priv->req_ars_button, FALSE);
		} else {
			gtk_widget_hide (priv->ok_button);
			const char *text = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));
			gtk_widget_set_sensitive (priv->req_ars_button, strlen (text) > 0);
		}
	}

	return ret;
}

static void
request_ars_authentication (GreeterARSDialog *dialog)
{
	GDateTime *dt;
	const gchar *tn;
	gchar *str_time = NULL;
	GError *error = NULL;
	GreeterARSDialogPrivate *priv = dialog->priv;

//	gtk_widget_set_sensitive (priv->ok_button, FALSE);
//	gtk_widget_set_sensitive (priv->retry_button, FALSE);
	gtk_widget_hide (priv->ok_button);
	gtk_widget_set_sensitive (priv->req_ars_button, FALSE);

	update_status_message (dialog, _("Calling for ARS authentication"));

	if (priv->ars_tran_id) {
		g_free (priv->ars_tran_id);
		priv->ars_tran_id = NULL;
	}

	if (priv->ars_auth_no) {
		g_free (priv->ars_auth_no);
		priv->ars_auth_no = NULL;
	}

	priv->auth_success = FALSE;

	// phone number
	tn = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));

	// tran_id
	dt = g_date_time_new_now_local ();
	str_time = g_date_time_format (dt, "%Y%m%d%H%M%S");
	g_date_time_unref (dt);

	priv->ars_tran_id = g_strdup_printf ("000000%s", str_time);

	priv->ars_auth_no = gooroom_ars_authentication (priv->ars_tran_id, tn, TRUE, &error);
	if (priv->ars_auth_no) {
		priv->ars_timeout_id = g_timeout_add (1000, (GSourceFunc) query_ars_auth_status, dialog);
	} else {
		update_status_message (dialog, "");

//		gtk_widget_set_sensitive (priv->ok_button, TRUE);
//		gtk_widget_set_sensitive (priv->retry_button, TRUE);
		const char *text = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));
		gtk_widget_set_sensitive (priv->req_ars_button, strlen (text) > 0);
	}
}

#if 0
static gboolean
dialog_response_ok_cb (gpointer user_data)
{
	gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_OK);

	return FALSE;
}
#endif


#if 0
static void
retry_button_clicked_cb (GtkButton *button,
                         gpointer   user_data)
{
	request_ars_authentication (GREETER_ARS_DIALOG (user_data));
}
#endif

static void
ok_button_clicked_cb (GtkButton *button,
                      gpointer   user_data)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);

	if (dialog->priv->auth_success)
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

#if 0
	gchar *rc;
	GError *error = NULL;
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (!priv->ars_tran_id) {
		gtk_label_set_text (GTK_LABEL (priv->ars_message_label),
                            _("Internal Error : No tran_id for ARS authentication"));
		return;
	}

	rc = gooroom_ars_confirm (priv->ars_tran_id, TRUE, &error);

	if (g_str_equal (rc, "0000") && !error) {
		g_idle_add ((GSourceFunc) dialog_response_ok_cb, dialog);
	} else {
		if (error) {
			gchar *msg = NULL;
			if (error->code == ARS_ERROR_CODE_4003)
				msg = g_strdup (_("The input time has been exceeded. Please try again."));
			else
				msg = g_strdup_printf ("Error Code : %d", error->code);

			gtk_label_set_text (GTK_LABEL (priv->ars_message_label), msg);
			g_free (msg);

			g_error_free (error);
		}
	}

retry:
	g_free (rc);
#endif
}

static void
cancel_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (priv->ars_timeout_id > 0)
		return;

	if (priv->auth_success) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
		return;
	}

	if (close_warning_message_show () == GTK_RESPONSE_OK)
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
request_ars_button_clicked_cb (GtkButton *button,
                               gpointer   user_data)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (priv->ars_timeout_id == 0)
		request_ars_authentication (GREETER_ARS_DIALOG (user_data));
}

static void
tn_entry_activate_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (priv->ars_timeout_id == 0 && gtk_widget_get_sensitive (priv->req_ars_button))
		request_ars_authentication (GREETER_ARS_DIALOG (user_data));
}

static void
tn_entry_changed_cb (GtkWidget *widget,
                     gpointer   user_data)
{
	const char *text;
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterARSDialogPrivate *priv = dialog->priv;

	text = gtk_entry_get_text (GTK_ENTRY (priv->tn_entry));
g_print ("text = %s\n", text);

	gtk_widget_set_sensitive (priv->req_ars_button, strlen (text) > 0);
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
greeter_ars_dialog_close (GtkDialog *dialog)
{
#if 0
	GreeterARSDialogPrivate *priv = GREETER_ARS_DIALOG (dialog)->priv;

	if (priv->ars_timeout_id > 0)
		return;

	if (priv->auth_success) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
		return;
	}

	GTK_DIALOG_CLASS (greeter_ars_dialog_parent_class)->close (dialog);
#endif
}

#if 0
static void
greeter_ars_dialog_response (GtkDialog *dialog,
                             gint response_id)
{
//	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (dialog);
//	GreeterARSDialogPrivate *priv = GREETER_ARS_DIALOG (dialog)->priv;
g_print ("greeter_ars_dialog_response id = %d\n", response_id);

	if (response_id == GTK_RESPONSE_CANCEL ||
        response_id == GTK_RESPONSE_DELETE_EVENT) {
//		gtk_dialog_response (dialog, GTK_RESPONSE_NONE);
	}
}
#endif

static void
greeter_ars_dialog_finalize (GObject *object)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (object);
	GreeterARSDialogPrivate *priv = dialog->priv;

	g_free (priv->ars_tran_id);
	g_free (priv->ars_auth_no);

	if (priv->ars_timeout_id > 0) {
		g_source_remove (priv->ars_timeout_id);
		priv->ars_timeout_id = 0;
	}

	G_OBJECT_CLASS (greeter_ars_dialog_parent_class)->finalize (object);
}

static void
greeter_ars_dialog_class_init (GreeterARSDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->finalize = greeter_ars_dialog_finalize;

	dialog_class->close = greeter_ars_dialog_close;
//	dialog_class->response = greeter_ars_dialog_response;


	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-ars-dialog.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, req_ars_button);
//	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
//                                                  GreeterARSDialog, retry_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, ok_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, cancel_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, tn_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, ars_message_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, auth_no_text);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, auth_no_label);
}

static void
greeter_ars_dialog_init (GreeterARSDialog *dialog)
{
	GreeterARSDialogPrivate *priv;

	priv = dialog->priv = greeter_ars_dialog_get_instance_private (dialog);

	priv->ars_tran_id = NULL;
	priv->ars_auth_no = NULL;
	priv->ars_timeout_id = 0;
	priv->auth_success = FALSE;

	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_widget_set_sensitive (priv->req_ars_button, FALSE);

	g_signal_connect (G_OBJECT (priv->req_ars_button), "clicked",
                      G_CALLBACK (request_ars_button_clicked_cb), dialog);

//	g_signal_connect (G_OBJECT (priv->retry_button), "clicked",
//                      G_CALLBACK (retry_button_clicked_cb), dialog);

	g_signal_connect (G_OBJECT (priv->ok_button), "clicked",
                      G_CALLBACK (ok_button_clicked_cb), dialog);

	g_signal_connect (G_OBJECT (priv->cancel_button), "clicked",
                      G_CALLBACK (cancel_button_clicked_cb), dialog);

	g_signal_connect (G_OBJECT (priv->tn_entry), "activate",
                      G_CALLBACK (tn_entry_activate_cb), dialog);

	g_signal_connect (G_OBJECT (priv->tn_entry), "changed",
                      G_CALLBACK (tn_entry_changed_cb), dialog);

	g_signal_connect (G_OBJECT (priv->tn_entry), "insert-text",
                      G_CALLBACK (tn_entry_insert_text_cb), dialog);

}

GtkWidget *
greeter_ars_dialog_new (GtkWindow *parent)
{
	GObject *dialog;

	dialog = g_object_new (GREETER_TYPE_ARS_DIALOG,
                           "use-header-bar", FALSE,
                           "transient-for", parent,
                           NULL);

//	gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

	return GTK_WIDGET (dialog);
}
