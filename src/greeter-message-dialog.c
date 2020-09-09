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

#include "greeter-message-dialog.h"

struct _GreeterMessageDialogPrivate {
//	GtkWidget *icon_image;
	GtkWidget *title_label;
	GtkWidget *message_label;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterMessageDialog, greeter_message_dialog, GTK_TYPE_DIALOG);


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
ok_button_clicked_cb (GtkButton *button,
                      gpointer   user_data)
{
	GreeterMessageDialog *dialog = GREETER_ARS_DIALOG (user_data);

	if (dialog->priv->auth_success)
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
cancel_button_clicked_cb (GtkButton *button,
                          gpointer   user_data)
{
	GreeterMessageDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterMessageDialogPrivate *priv = dialog->priv;

	if (priv->message_timeout_id > 0)
		return;

	if (priv->auth_success) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
		return;
	}

	if (close_warning_message_show () == GTK_RESPONSE_OK)
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
greeter_message_dialog_add_buttons_valist (GtkDialog      *dialog,
                                           const gchar    *first_button_text,
                                           va_list         args)
{
	const gchar* text;
	gint response_id;

	if (first_button_text == NULL)
		return;

	text = first_button_text;
	response_id = va_arg (args, gint);

	while (text != NULL)
	{
		gtk_dialog_add_button (dialog, text, response_id);

		text = va_arg (args, gchar*);
		if (text == NULL)
			break;
		response_id = va_arg (args, int);
	}
}
#endif


static void
greeter_message_dialog_close (GtkDialog *dialog)
{
}

#if 0
static void
greeter_message_dialog_response (GtkDialog *dialog,
                             gint response_id)
{
//	GreeterMessageDialog *dialog = GREETER_ARS_DIALOG (dialog);
//	GreeterMessageDialogPrivate *priv = GREETER_ARS_DIALOG (dialog)->priv;
g_print ("greeter_message_dialog_response id = %d\n", response_id);

	if (response_id == GTK_RESPONSE_CANCEL ||
        response_id == GTK_RESPONSE_DELETE_EVENT) {
//		gtk_dialog_response (dialog, GTK_RESPONSE_NONE);
	}
}
#endif

#if 0
static void
greeter_message_dialog_finalize (GObject *object)
{
	GreeterMessageDialog *dialog = GREETER_ARS_DIALOG (object);
	GreeterMessageDialogPrivate *priv = dialog->priv;

	G_OBJECT_CLASS (greeter_message_dialog_parent_class)->finalize (object);
}
#endif

static void
greeter_message_dialog_init (GreeterMessageDialog *dialog)
{
	dialog->priv = greeter_message_dialog_get_instance_private (dialog);

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
}

static void
greeter_message_dialog_class_init (GreeterMessageDialogClass *klass)
{
//	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

//	gobject_class->finalize = greeter_message_dialog_finalize;

	dialog_class->close = greeter_message_dialog_close;
//	dialog_class->response = greeter_message_dialog_response;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-message-dialog.ui");

//	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
//                                                  GreeterMessageDialog, icon_image);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                 GreeterMessageDialog, title_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                 GreeterMessageDialog, message_label);
}

GtkWidget *
greeter_message_dialog_new (GtkWindow *parent,
                            const char *icon,
                            const char *title,
                            const char *message)
{
	GreeterMessageDialog *dialog;

	dialog = GREETER_MESSAGE_DIALOG (g_object_new (GREETER_TYPE_MESSAGE_DIALOG,
                                                   "use-header-bar", FALSE,
                                                   "transient-for", parent,
                                                   NULL));

	greeter_message_dialog_set_icon (dialog, icon);
	greeter_message_dialog_set_title (dialog, title);
	greeter_message_dialog_set_message (dialog, message);

	return GTK_WIDGET (dialog);
}

void
greeter_message_dialog_set_title (GreeterMessageDialog *dialog,
                                  const char           *title)
{
	if (title) {
		gtk_label_set_text (GTK_LABEL (dialog->priv->title_label), title);
/*
		gchar *markup;
		markup = g_markup_printf_escaped ("<span font='18px' weight='bold'>%s</span>", title);
		gtk_label_set_markup (GTK_LABEL (dialog->priv->title_label), markup);
		g_free (markup);
*/
	}
}

void
greeter_message_dialog_set_message (GreeterMessageDialog *dialog,
                                    const char           *message)
{
	if (message)
		gtk_label_set_text (GTK_LABEL (dialog->priv->message_label), message);
}

void
greeter_message_dialog_set_icon (GreeterMessageDialog *dialog,
                                 const char           *icon)
{
#if 0
	if (icon)
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->icon_image),
                                      icon, GTK_ICON_SIZE_DIALOG);
#endif
}
