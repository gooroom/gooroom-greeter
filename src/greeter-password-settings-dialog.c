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

#include "greeter-password-settings-dialog.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

struct _GreeterPasswordSettingsDialogPrivate {
	GtkWidget *parent;
	GtkWidget *title_label;
	GtkWidget *prompt_label;
	GtkWidget *prompt_entry;
	GtkWidget *message_label;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterPasswordSettingsDialog, greeter_password_settings_dialog, GTK_TYPE_DIALOG);


static void
set_parent (GreeterPasswordSettingsDialog *dialog,
            GtkWidget                     *parent)
{
	if (parent)
		dialog->priv->parent = parent;
}

static void
block_response (GreeterPasswordSettingsDialog *dialog,
                gboolean                       blocked)
{
	gtk_widget_set_sensitive (dialog->priv->ok_button, !blocked);
	gtk_widget_set_sensitive (dialog->priv->prompt_entry, !blocked);
}

static void
prompt_entry_changed_cb (GtkWidget *widget,
                         gpointer   user_data)
{
	const char *text;
	GreeterPasswordSettingsDialog *dialog = GREETER_PASSWORD_SETTINGS_DIALOG (user_data);
	GreeterPasswordSettingsDialogPrivate *priv = dialog->priv;

	text = gtk_entry_get_text (GTK_ENTRY (priv->prompt_entry));

	if (strlen (text) > 0) {
		gtk_widget_set_sensitive (priv->ok_button, TRUE);
		gtk_label_set_text (GTK_LABEL (priv->message_label), "");
	} else {
		gtk_widget_set_sensitive (priv->ok_button, FALSE);
	}
}

static void
prompt_entry_activate_cb (GtkWidget *widget,
                          gpointer   user_data)
{
	GreeterPasswordSettingsDialog *dialog = GREETER_PASSWORD_SETTINGS_DIALOG (user_data);

	block_response (dialog, TRUE);

	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
ok_button_clicked_cb (GtkButton *widget,
                      gpointer   user_data)
{
	GreeterPasswordSettingsDialog *dialog = GREETER_PASSWORD_SETTINGS_DIALOG (user_data);

	block_response (dialog, TRUE);

	gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_OK);
}

static void
cancel_button_clicked_cb (GtkButton *widget,
                          gpointer   user_data)
{
	gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_CANCEL);
}

static void
greeter_password_settings_dialog_size_allocate (GtkWidget     *widget,
                                                GtkAllocation *allocation)
{
	GreeterPasswordSettingsDialog *dialog = GREETER_PASSWORD_SETTINGS_DIALOG (widget);
	GreeterPasswordSettingsDialogPrivate *priv = dialog->priv;

	if (GTK_WIDGET_CLASS (greeter_password_settings_dialog_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (greeter_password_settings_dialog_parent_class)->size_allocate (widget, allocation);

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
greeter_password_settings_dialog_init (GreeterPasswordSettingsDialog *dialog)
{
	GreeterPasswordSettingsDialogPrivate *priv;

	priv = dialog->priv = greeter_password_settings_dialog_get_instance_private (dialog);

	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_window_set_decorated (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), TRUE);
	gtk_widget_set_app_paintable (GTK_WIDGET (dialog), TRUE);

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (dialog));
	if (gdk_screen_is_composited (screen)) {
		GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
		if (visual == NULL)
			visual = gdk_screen_get_system_visual (screen);

		gtk_widget_set_visual (GTK_WIDGET (dialog), visual);
	}

	g_signal_connect (G_OBJECT (priv->prompt_entry), "activate",
                      G_CALLBACK (prompt_entry_activate_cb), dialog);
    g_signal_connect (G_OBJECT (priv->prompt_entry), "changed",
                      G_CALLBACK (prompt_entry_changed_cb), dialog);
    g_signal_connect (G_OBJECT (priv->ok_button), "clicked",
                      G_CALLBACK (ok_button_clicked_cb), dialog);
    g_signal_connect (G_OBJECT (priv->cancel_button), "clicked",
                      G_CALLBACK (cancel_button_clicked_cb), dialog);

	block_response (dialog, TRUE);
}

static void
greeter_password_settings_dialog_class_init (GreeterPasswordSettingsDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->size_allocate = greeter_password_settings_dialog_size_allocate;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-password-settings-dialog.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterPasswordSettingsDialog, ok_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterPasswordSettingsDialog, cancel_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterPasswordSettingsDialog, title_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterPasswordSettingsDialog, prompt_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterPasswordSettingsDialog, prompt_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterPasswordSettingsDialog, message_label);
}

GtkWidget *
greeter_password_settings_dialog_new (GtkWidget *parent)
{
	GObject *dialog;

	dialog = g_object_new (GREETER_TYPE_PASSWORD_SETTINGS_DIALOG,
                           "use-header-bar", FALSE,
                           "transient-for”", NULL,
                           NULL);

	set_parent (GREETER_PASSWORD_SETTINGS_DIALOG (dialog), parent);

	return GTK_WIDGET (dialog);
}

void
greeter_password_settings_dialog_set_title (GreeterPasswordSettingsDialog *dialog,
                                            const gchar                   *title)
{
	if (title)
		gtk_label_set_text (GTK_LABEL (dialog->priv->title_label), title);
}

void
greeter_password_settings_dialog_set_prompt_label (GreeterPasswordSettingsDialog *dialog,
                                                   const gchar                   *label)
{
	if (label)
		gtk_label_set_text (GTK_LABEL (dialog->priv->prompt_label), label);
}

void
greeter_password_settings_dialog_set_entry_text (GreeterPasswordSettingsDialog *dialog,
                                                 const gchar                   *text)
{
	block_response (dialog, FALSE);

	if (text)
		gtk_entry_set_text (GTK_ENTRY (dialog->priv->prompt_entry), text);

	prompt_entry_changed_cb (dialog->priv->prompt_entry, dialog);
}

void
greeter_password_settings_dialog_set_message_label (GreeterPasswordSettingsDialog *dialog,
                                                    const gchar                   *label)
{
	if (label)
		gtk_label_set_text (GTK_LABEL (dialog->priv->message_label), label);
}

void
greeter_password_settings_dialog_grab_entry_focus (GreeterPasswordSettingsDialog *dialog)
{
	if (dialog->priv->prompt_entry)
		gtk_widget_grab_focus (GTK_WIDGET (dialog->priv->prompt_entry));
}

const char *
greeter_password_settings_dialog_get_entry_text (GreeterPasswordSettingsDialog *dialog)
{
	if (dialog->priv->prompt_entry)
		return gtk_entry_get_text (GTK_ENTRY (dialog->priv->prompt_entry));

	return NULL;
}
