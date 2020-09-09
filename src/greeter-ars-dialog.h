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

#ifndef __GREETER_ARS_DIALOG_H__
#define __GREETER_ARS_DIALOG_H__

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GREETER_TYPE_ARS_DIALOG            (greeter_ars_dialog_get_type ())
#define GREETER_ARS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GREETER_TYPE_ARS_DIALOG, GreeterARSDialog))
#define GREETER_ARS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  GREETER_TYPE_ARS_DIALOG, GreeterARSDialogClass))
#define GREETER_IS_ARS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GREETER_TYPE_ARS_DIALOG))
#define GREETER_IS_ARS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  GREETER_TYPE_ARS_DIALOG))
#define GREETER_ARS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  GREETER_TYPE_ARS_DIALOG, GreeterARSDialogClass))

typedef struct _GreeterARSDialog GreeterARSDialog;
typedef struct _GreeterARSDialogClass GreeterARSDialogClass;
typedef struct _GreeterARSDialogPrivate GreeterARSDialogPrivate;

struct _GreeterARSDialog {
	GtkDialog dialog;

	GreeterARSDialogPrivate *priv;
};

struct _GreeterARSDialogClass {
	GtkDialogClass parent_class;
};

GType	    greeter_ars_dialog_get_type	(void) G_GNUC_CONST;

GtkWidget  *greeter_ars_dialog_new		(GtkWindow  *parent);

G_END_DECLS

#endif /* __GREETER_ARS_DIALOG_H__ */
