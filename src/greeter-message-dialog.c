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

#include "greeter-message-dialog.h"

struct _GreeterMessageDialogPrivate {
	GtkWidget *parent;
	GtkWidget *icon_image;
	GtkWidget *title_label;
	GtkWidget *message_label;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterMessageDialog, greeter_message_dialog, GTK_TYPE_DIALOG);



static void
set_parent (GreeterMessageDialog *dialog,
            GtkWidget            *parent)
{
	if (parent)
		dialog->priv->parent = parent;
}

static void
greeter_message_dialog_size_allocate (GtkWidget     *widget,
                                      GtkAllocation *allocation)
{
	GreeterMessageDialog *dialog = GREETER_MESSAGE_DIALOG (widget);
	GreeterMessageDialogPrivate *priv = dialog->priv;

	if (GTK_WIDGET_CLASS (greeter_message_dialog_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (greeter_message_dialog_parent_class)->size_allocate (widget, allocation);

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
greeter_message_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (greeter_message_dialog_parent_class)->finalize (object);
}

static void
greeter_message_dialog_init (GreeterMessageDialog *dialog)
{
	PangoAttrList *attrs;
	PangoAttribute *attr;
	GreeterMessageDialogPrivate *priv;
	priv = dialog->priv = greeter_message_dialog_get_instance_private (dialog);

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

	attrs = pango_attr_list_new ();
	attr = pango_attr_rise_new (8000);
	pango_attr_list_insert (attrs, attr);
	gtk_label_set_attributes (GTK_LABEL (priv->message_label), attrs);
}

static void
greeter_message_dialog_class_init (GreeterMessageDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = greeter_message_dialog_finalize;
	widget_class->size_allocate = greeter_message_dialog_size_allocate;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-message-dialog.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                  GreeterMessageDialog, icon_image);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                 GreeterMessageDialog, title_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass),
                                                 GreeterMessageDialog, message_label);
}

GtkWidget *
greeter_message_dialog_new (GtkWidget  *parent,
                            const char *icon,
                            const char *title,
                            const char *message)
{
	GObject *dialog;

	dialog = g_object_new (GREETER_TYPE_MESSAGE_DIALOG,
                           "use-header-bar", FALSE,
                           "transient-for", NULL,
                           NULL);

	set_parent (GREETER_MESSAGE_DIALOG (dialog), parent);
	greeter_message_dialog_set_icon (GREETER_MESSAGE_DIALOG (dialog), icon);
	greeter_message_dialog_set_title (GREETER_MESSAGE_DIALOG (dialog), title);
	greeter_message_dialog_set_message (GREETER_MESSAGE_DIALOG (dialog), message);

	return GTK_WIDGET (dialog);
}

void
greeter_message_dialog_set_title (GreeterMessageDialog *dialog,
                                  const char           *title)
{
	if (title) {
		gtk_widget_show (dialog->priv->title_label);
		gtk_label_set_text (GTK_LABEL (dialog->priv->title_label), title);
	} else {
		gtk_widget_hide (dialog->priv->title_label);
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
	if (icon) {
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->icon_image),
                                      icon, GTK_ICON_SIZE_DIALOG);

		gtk_widget_show (dialog->priv->icon_image);
	} else {
		gtk_widget_hide (dialog->priv->icon_image);
	}
}
