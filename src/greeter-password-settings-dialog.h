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

#ifndef __GREETER_PASSWORD_SETTINGS_DIALOG_H__
#define __GREETER_PASSWORD_SETTINGS_DIALOG_H__

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GREETER_TYPE_PASSWORD_SETTINGS_DIALOG            (greeter_password_settings_dialog_get_type ())
#define GREETER_PASSWORD_SETTINGS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GREETER_TYPE_PASSWORD_SETTINGS_DIALOG, GreeterPasswordSettingsDialog))
#define GREETER_PASSWORD_SETTINGS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  GREETER_TYPE_PASSWORD_SETTINGS_DIALOG, GreeterPasswordSettingsDialogClass))
#define GREETER_IS_PASSWORD_SETTINGS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GREETER_TYPE_PASSWORD_SETTINGS_DIALOG))
#define GREETER_IS_PASSWORD_SETTINGS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  GREETER_TYPE_PASSWORD_SETTINGS_DIALOG))
#define GREETER_PASSWORD_SETTINGS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  GREETER_TYPE_PASSWORD_SETTINGS_DIALOG, GreeterPasswordSettingsDialogClass))

typedef struct _GreeterPasswordSettingsDialog GreeterPasswordSettingsDialog;
typedef struct _GreeterPasswordSettingsDialogClass GreeterPasswordSettingsDialogClass;
typedef struct _GreeterPasswordSettingsDialogPrivate GreeterPasswordSettingsDialogPrivate;

struct _GreeterPasswordSettingsDialog {
	GtkDialog dialog;

	GreeterPasswordSettingsDialogPrivate *priv;
};

struct _GreeterPasswordSettingsDialogClass {
	GtkDialogClass parent_class;
};

GType	    greeter_password_settings_dialog_get_type	(void) G_GNUC_CONST;

GtkWidget  *greeter_password_settings_dialog_new		(GtkWidget *parent);


const char *greeter_password_settings_dialog_get_entry_text (GreeterPasswordSettingsDialog *dialog);

void greeter_password_settings_dialog_grab_entry_focus  (GreeterPasswordSettingsDialog *dialog);

void greeter_password_settings_dialog_set_title         (GreeterPasswordSettingsDialog *dialog,
                                                         const gchar                   *title);

void greeter_password_settings_dialog_set_prompt_label  (GreeterPasswordSettingsDialog *dialog,
                                                         const gchar                   *label);

void greeter_password_settings_dialog_set_entry_text    (GreeterPasswordSettingsDialog *dialog,
                                                         const gchar                   *text);

void greeter_password_settings_dialog_set_message_label (GreeterPasswordSettingsDialog *dialog,
                                                         const gchar                   *label);

G_END_DECLS

#endif /* __GREETER_PASSWORD_SETTINGS_DIALOG_H__ */
