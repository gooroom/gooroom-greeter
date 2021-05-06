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
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <ctype.h>

#include "greeter-ars-dialog.h"
#include "readable-phone-number.h"

struct _GreeterARSDialogPrivate {
	GtkWidget *parent;
	GtkWidget *desc_label;
	GtkWidget *auth_num_label;
	GtkWidget *phone_num_label;
	GtkWidget *left_time_label;

	gint left_secs;
	gint init_left_secs;

	guint timer_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterARSDialog, greeter_ars_dialog, GTK_TYPE_DIALOG);


static void
set_parent (GreeterARSDialog *dialog,
            GtkWidget        *parent)
{
	if (parent)
		dialog->priv->parent = parent;
}

static gboolean
timer_cb (gpointer user_data)
{
	gchar *text = NULL;
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (user_data);
	GreeterARSDialogPrivate *priv = dialog->priv;
	if (priv->left_secs <= 0) {
		gtk_label_set_text (GTK_LABEL (priv->left_time_label), _("Input Timeout"));
		return FALSE;
	}

	--priv->left_secs;

	text = g_strdup_printf (_("%02d:%02d"), priv->left_secs / 60, priv->left_secs % 60);
	gtk_label_set_text (GTK_LABEL (priv->left_time_label), text);
	g_free (text);

	return TRUE;
}

static void
greeter_ars_dialog_show (GtkWidget *widget)
{
	gchar *text = NULL;
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (widget);
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (GTK_WIDGET_CLASS (greeter_ars_dialog_parent_class)->show)
		GTK_WIDGET_CLASS (greeter_ars_dialog_parent_class)->show (widget);

	priv->left_secs = priv->init_left_secs;

	text = g_strdup_printf (_("%02d:%02d"), priv->left_secs / 60, priv->left_secs % 60);
	gtk_label_set_text (GTK_LABEL (priv->left_time_label), text);
	g_free (text);

	// start timer
	priv->timer_id = g_timeout_add (1000, (GSourceFunc)timer_cb, dialog);
}

static void
greeter_ars_dialog_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (widget);
	GreeterARSDialogPrivate *priv = dialog->priv;

	if (GTK_WIDGET_CLASS (greeter_ars_dialog_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (greeter_ars_dialog_parent_class)->size_allocate (widget, allocation);

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
greeter_ars_dialog_close (GtkDialog *dialog)
{
//	GTK_DIALOG_CLASS (greeter_ars_dialog_parent_class)->close (dialog);
}

static void
greeter_ars_dialog_finalize (GObject *object)
{
	GreeterARSDialog *dialog = GREETER_ARS_DIALOG (object);

    g_clear_handle_id (&dialog->priv->timer_id, g_source_remove);
	dialog->priv->timer_id = 0;

	G_OBJECT_CLASS (greeter_ars_dialog_parent_class)->finalize (object);
}

static void
greeter_ars_dialog_class_init (GreeterARSDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->finalize = greeter_ars_dialog_finalize;

	widget_class->size_allocate = greeter_ars_dialog_size_allocate;
	widget_class->show = greeter_ars_dialog_show;

	dialog_class->close = greeter_ars_dialog_close;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-ars-dialog.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, desc_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, phone_num_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, auth_num_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterARSDialog, left_time_label);
}

static void
greeter_ars_dialog_init (GreeterARSDialog *dialog)
{
	GreeterARSDialogPrivate *priv;
	priv = dialog->priv = greeter_ars_dialog_get_instance_private (dialog);

	priv->timer_id = 0;
	priv->init_left_secs = 120;  //default 120 secs

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

	if (g_file_test (MOFA_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
		GKeyFile *keyfile = g_key_file_new ();
		g_key_file_load_from_file (keyfile, MOFA_CONFIG_FILE, G_KEY_FILE_NONE, NULL);
		priv->init_left_secs = g_key_file_get_integer (keyfile,
                                                       "Settings",
                                                       "ARS_AUTH_NUM_INPUT_TIMEOUT",
                                                       NULL);
		g_key_file_unref (keyfile);
	} else {
		g_warning ("Failed to load mofa.conf file: %s", MOFA_CONFIG_FILE);
	}
}

GtkWidget *
greeter_ars_dialog_new (GtkWidget   *parent,
                        const gchar *phone_num,
                        const gchar *auth_num)
{
	GObject *object;

	object = g_object_new (GREETER_TYPE_ARS_DIALOG,
                           "use-header-bar", FALSE,
                           "transient-for", NULL,
                           NULL);

	set_parent (GREETER_ARS_DIALOG (object), parent);
	greeter_ars_dialog_set_phone_number (GREETER_ARS_DIALOG (object), phone_num);
	greeter_ars_dialog_set_authentication_number (GREETER_ARS_DIALOG (object), auth_num);

	return GTK_WIDGET (object);
}

void
greeter_ars_dialog_set_phone_number (GreeterARSDialog *dialog,
                                     const gchar      *phone_num)
{
	const gchar *text;
	gchar *r_phone_num = NULL;

	if (phone_num) {
		r_phone_num = get_readable_phone_number (phone_num);
		text = r_phone_num ? r_phone_num : phone_num;
	} else {
		text = NULL;
	}

	gtk_label_set_text (GTK_LABEL (dialog->priv->phone_num_label), text ? text : _("Unknown"));

	g_clear_pointer (&r_phone_num, g_free);
}

void
greeter_ars_dialog_set_authentication_number (GreeterARSDialog *dialog,
                                              const gchar      *auth_num)
{
	gtk_label_set_text (GTK_LABEL (dialog->priv->auth_num_label), auth_num ? auth_num : _("Unknown"));
}
