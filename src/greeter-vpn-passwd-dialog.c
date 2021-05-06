/*
 * Copyright (C) 2015-2020 Gooroom <gooroom@gooroom.kr>
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
#include <glib/gi18n.h>

#include "pw-utils.h"
#include "greeter-vpn-passwd-dialog.h"


#define VALIDATION_TIMEOUT 100

struct _GreeterVPNPasswdDialogPrivate {
	GtkWidget *parent;
	GtkWidget *old_pw_entry;
	GtkWidget *new_pw_entry;
	GtkWidget *new_verify_pw_entry;
	GtkWidget *error_label;
	GtkWidget *ok_button;

	gboolean is_valid_old_pw;
	gboolean is_valid_new_pw;
	gboolean is_valid_new_verify_pw;

	guint validation_timeout_id;

	gchar *cur_password;
	gchar *new_password;
};


enum {
	PROP_0,
	PROP_PASSWORD
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterVPNPasswdDialog, greeter_vpn_passwd_dialog, GTK_TYPE_DIALOG)



static void
set_parent (GreeterVPNPasswdDialog *dialog,
            GtkWidget              *parent)
{
	if (parent)
		dialog->priv->parent = parent;
}

static void
set_entry_validation_checkmark (GtkEntry *entry)
{
	g_object_set (entry, "caps-lock-warning", FALSE, NULL);

	gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "object-select-symbolic");
}

static void
clear_entry_validation_error (GtkEntry *entry)
{
    gboolean warning;

    g_object_get (entry, "caps-lock-warning", &warning, NULL);

    if (warning)
        return;

    gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    g_object_set (entry, "caps-lock-warning", TRUE, NULL);
}

static void
update_validation (GreeterVPNPasswdDialog *dialog)
{
	gboolean valid = FALSE;
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	valid = (priv->is_valid_old_pw &&
             priv->is_valid_new_pw &&
             priv->is_valid_new_verify_pw);

	g_clear_pointer (&priv->new_password, g_free);

	if (valid) {
		priv->new_password = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->new_pw_entry)));
		gtk_widget_set_sensitive (priv->ok_button, TRUE);
	} else {
		gtk_widget_set_sensitive (priv->ok_button, FALSE);
	}
}

static gboolean
validate_cb (gpointer data)
{
	const gchar *old_pw, *new_pw, *new_verify_pw;
	GreeterVPNPasswdDialog *dialog = GREETER_VPN_PASSWD_DIALOG (data);
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	g_clear_handle_id (&priv->validation_timeout_id, g_source_remove);

	priv->validation_timeout_id = 0;
	priv->is_valid_old_pw = FALSE;
	priv->is_valid_new_pw = FALSE;
	priv->is_valid_new_verify_pw = FALSE;

	old_pw = gtk_entry_get_text (GTK_ENTRY (priv->old_pw_entry));
	new_pw = gtk_entry_get_text (GTK_ENTRY (priv->new_pw_entry));
	new_verify_pw = gtk_entry_get_text (GTK_ENTRY (priv->new_verify_pw_entry));

	/* check old password */
	if (strlen (old_pw) > 0) {
		priv->is_valid_old_pw = g_str_equal (old_pw, priv->cur_password);
		if (priv->is_valid_old_pw) {
			set_entry_validation_checkmark (GTK_ENTRY (priv->old_pw_entry));
			gtk_widget_set_sensitive (priv->new_pw_entry, TRUE);
			gtk_widget_set_sensitive (priv->new_verify_pw_entry, TRUE);
		} else {
			gtk_widget_set_sensitive (priv->new_pw_entry, FALSE);
			gtk_widget_set_sensitive (priv->new_verify_pw_entry, FALSE);
			gtk_label_set_label (GTK_LABEL (priv->error_label), _("The old passwords do not match."));
			goto done;
		}
	}

	/* check new password */
	if (strlen (new_pw) > 0) {
		const gchar *hint;
		gint strength_level;

		pw_strength (new_pw, old_pw, NULL, &hint, &strength_level);

		priv->is_valid_new_pw = (strlen (new_pw) && strength_level > 1);
		if (priv->is_valid_new_pw) {
			set_entry_validation_checkmark (GTK_ENTRY (priv->new_pw_entry));
		} else {
			if (hint) {
				gtk_label_set_text (GTK_LABEL (priv->error_label), hint);
			}
			goto done;
		}
	}

	/* check new verify password */
	if (strlen (new_verify_pw) > 0) {
		priv->is_valid_new_verify_pw = g_str_equal (new_pw, new_verify_pw);
		if (priv->is_valid_new_verify_pw) {
			set_entry_validation_checkmark (GTK_ENTRY (priv->new_verify_pw_entry));
		} else {
			gtk_label_set_label (GTK_LABEL (priv->error_label), _("The passwords do not match."));
			goto done;
		}
	}

done:
	update_validation (dialog);

	return FALSE;
}

static void
old_pw_entry_changed_cb (GreeterVPNPasswdDialog *dialog)
{
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	gtk_label_set_text (GTK_LABEL (priv->error_label), "");

	clear_entry_validation_error (GTK_ENTRY (priv->old_pw_entry));

	g_clear_handle_id (&priv->validation_timeout_id, g_source_remove);
	priv->validation_timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate_cb, dialog);
}

static void
new_pw_entry_changed_cb (GreeterVPNPasswdDialog *dialog)
{
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	gtk_label_set_text (GTK_LABEL (priv->error_label), "");

	clear_entry_validation_error (GTK_ENTRY (priv->new_pw_entry));
	clear_entry_validation_error (GTK_ENTRY (priv->new_verify_pw_entry));

	g_clear_handle_id (&priv->validation_timeout_id, g_source_remove);
	priv->validation_timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate_cb, dialog);
}

static void
new_verify_pw_entry_changed_cb (GreeterVPNPasswdDialog *dialog)
{
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	gtk_label_set_text (GTK_LABEL (priv->error_label), "");

	clear_entry_validation_error (GTK_ENTRY (priv->new_verify_pw_entry));

	g_clear_handle_id (&priv->validation_timeout_id, g_source_remove);
	priv->validation_timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate_cb, dialog);
}

static void
pw_entry_activated_cb (GreeterVPNPasswdDialog *dialog)
{
	gboolean valid;
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	valid = (priv->is_valid_old_pw &&
             priv->is_valid_new_pw &&
             priv->is_valid_new_verify_pw);

	if (valid)
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
greeter_vpn_passwd_dialog_size_allocate (GtkWidget     *widget,
                                         GtkAllocation *allocation)
{
	GreeterVPNPasswdDialog *dialog = GREETER_VPN_PASSWD_DIALOG (widget);
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	if (GTK_WIDGET_CLASS (greeter_vpn_passwd_dialog_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (greeter_vpn_passwd_dialog_parent_class)->size_allocate (widget, allocation);

	if (!gtk_widget_get_realized (widget))
		return;

	if (priv->parent) {
		GtkWidget *toplevel;
		gint x, y, p_w, p_h;

		toplevel = gtk_widget_get_toplevel (priv->parent);

		gtk_widget_translate_coordinates (priv->parent, toplevel, 0, 0, &x, &y);
		gtk_widget_get_size_request (priv->parent, &p_w, &p_h);

		gtk_window_move (GTK_WINDOW (dialog),
                         x + (p_w - allocation->width) / 2,
                         y + (p_h - allocation->height) / 2 );
	}
}

static void
greeter_vpn_passwd_dialog_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	GreeterVPNPasswdDialog *dialog = GREETER_VPN_PASSWD_DIALOG (object);
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	switch (prop_id)
	{
		case PROP_PASSWORD:
			g_clear_pointer (&priv->cur_password, g_free);
			priv->cur_password = g_value_dup_string (value);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_vpn_passwd_dialog_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	GreeterVPNPasswdDialog *dialog = GREETER_VPN_PASSWD_DIALOG (object);
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

	switch (prop_id)
	{
		case PROP_PASSWORD:
			g_value_set_string (value, priv->cur_password);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_vpn_passwd_dialog_finalize (GObject *object)
{
	GreeterVPNPasswdDialog *dialog = GREETER_VPN_PASSWD_DIALOG (object);

	g_clear_pointer (&dialog->priv->cur_password, g_free);
	g_clear_pointer (&dialog->priv->new_password, g_free);

	G_OBJECT_CLASS (greeter_vpn_passwd_dialog_parent_class)->finalize (object);
}

static void
greeter_vpn_passwd_dialog_constructed (GObject *object)
{
	GreeterVPNPasswdDialog *dialog = GREETER_VPN_PASSWD_DIALOG (object);
	GreeterVPNPasswdDialogPrivate *priv = dialog->priv;

    G_OBJECT_CLASS (greeter_vpn_passwd_dialog_parent_class)->constructed (object);

	gtk_label_set_text (GTK_LABEL (priv->error_label), "");
	gtk_widget_set_sensitive (priv->new_pw_entry, FALSE);
	gtk_widget_set_sensitive (priv->new_verify_pw_entry, FALSE);

	g_signal_connect_swapped (priv->old_pw_entry, "changed",
                              G_CALLBACK (old_pw_entry_changed_cb), dialog);
	g_signal_connect_swapped (priv->new_pw_entry, "changed",
                              G_CALLBACK (new_pw_entry_changed_cb), dialog);
	g_signal_connect_swapped (priv->new_verify_pw_entry, "changed",
                              G_CALLBACK (new_verify_pw_entry_changed_cb), dialog);
	g_signal_connect_swapped (priv->new_pw_entry, "activate",
                              G_CALLBACK (pw_entry_activated_cb), dialog);
	g_signal_connect_swapped (priv->new_verify_pw_entry, "activate",
                              G_CALLBACK (pw_entry_activated_cb), dialog);

	update_validation (dialog);
}

static void
greeter_vpn_passwd_dialog_init (GreeterVPNPasswdDialog *dialog)
{
	dialog->priv = greeter_vpn_passwd_dialog_get_instance_private (dialog);

	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), TRUE);
	gtk_widget_set_app_paintable (GTK_WIDGET (dialog), TRUE);

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (dialog));
	if(gdk_screen_is_composited (screen)) {
		GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
		if (visual == NULL)
			visual = gdk_screen_get_system_visual (screen);

		gtk_widget_set_visual (GTK_WIDGET (dialog), visual);
	}
}

static void
greeter_vpn_passwd_dialog_class_init (GreeterVPNPasswdDialogClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = greeter_vpn_passwd_dialog_constructed;
	object_class->get_property = greeter_vpn_passwd_dialog_get_property;
	object_class->set_property = greeter_vpn_passwd_dialog_set_property;
	object_class->finalize     = greeter_vpn_passwd_dialog_finalize;

	widget_class->size_allocate = greeter_vpn_passwd_dialog_size_allocate;

	g_object_class_install_property
		(object_class, PROP_PASSWORD,
		 g_param_spec_string ("password", "", "",
			 NULL,
			 G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
			"/kr/gooroom/greeter/greeter-vpn-passwd-dialog.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterVPNPasswdDialog, old_pw_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterVPNPasswdDialog, new_pw_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterVPNPasswdDialog, new_verify_pw_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterVPNPasswdDialog, error_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterVPNPasswdDialog, ok_button);
}

GtkWidget *
greeter_vpn_passwd_dialog_new (GtkWidget   *parent,
                               const gchar *password)
{
	GObject *object;

	object = g_object_new (GREETER_TYPE_VPN_PASSWD_DIALOG,
                           "password", password,
                           "use-header-bar", FALSE,
                           "transient-for", NULL,
                           NULL);

	set_parent (GREETER_VPN_PASSWD_DIALOG (object), parent);

	return GTK_WIDGET (object);
}

gchar *
greeter_vpn_passwd_dialog_get_new_password (GreeterVPNPasswdDialog *dialog)
{
	g_return_val_if_fail (GREETER_IS_VPN_PASSWD_DIALOG (dialog), NULL);

	return dialog->priv->new_password;
}
