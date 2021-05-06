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

#ifndef __GREETER_MESSAGE_DIALOG_H__
#define __GREETER_MESSAGE_DIALOG_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GREETER_TYPE_MESSAGE_DIALOG            (greeter_message_dialog_get_type ())
#define GREETER_MESSAGE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GREETER_TYPE_MESSAGE_DIALOG, GreeterMessageDialog))
#define GREETER_MESSAGE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  GREETER_TYPE_MESSAGE_DIALOG, GreeterMessageDialogClass))
#define GREETER_IS_MESSAGE_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GREETER_TYPE_MESSAGE_DIALOG))
#define GREETER_IS_MESSAGE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  GREETER_TYPE_MESSAGE_DIALOG))
#define GREETER_MESSAGE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  GREETER_TYPE_MESSAGE_DIALOG, GreeterMessageDialogClass))

typedef struct _GreeterMessageDialog GreeterMessageDialog;
typedef struct _GreeterMessageDialogClass GreeterMessageDialogClass;
typedef struct _GreeterMessageDialogPrivate GreeterMessageDialogPrivate;

struct _GreeterMessageDialog {
	GtkDialog dialog;

	GreeterMessageDialogPrivate *priv;
};

struct _GreeterMessageDialogClass {
	GtkDialogClass parent_class;
};

GType	    greeter_message_dialog_get_type	(void) G_GNUC_CONST;

GtkWidget  *greeter_message_dialog_new		(GtkWidget  *parent,
                                             const char *icon,
                                             const char *title,
                                             const char *message); 

void greeter_message_dialog_set_title       (GreeterMessageDialog *dialog,
                                             const char           *title);
void greeter_message_dialog_set_message     (GreeterMessageDialog *dialog,
                                             const char           *message);
void greeter_message_dialog_set_icon        (GreeterMessageDialog *dialog,
                                             const char           *icon);

G_END_DECLS

#endif /* __GREETER_MESSAGE_DIALOG_H__ */
