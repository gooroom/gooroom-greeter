/*
 * Copyright (C) 2015-2020 Gooroom <gooroom@gooroom.kr>
 * Copyright (C) 2004-2005 William Jon McCann <mccann@jhu.edu>
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
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef __GREETER_VPN_PASSWD_DIALOG_H__
#define __GREETER_VPN_PASSWD_DIALOG_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GREETER_TYPE_VPN_PASSWD_DIALOG         (greeter_vpn_passwd_dialog_get_type ())
#define GREETER_VPN_PASSWD_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GREETER_TYPE_VPN_PASSWD_DIALOG, GreeterVPNPasswdDialog))
#define GREETER_VPN_PASSWD_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GREETER_TYPE_VPN_PASSWD_DIALOG, GreeterVPNPasswdDialogClass))
#define GREETER_IS_VPN_PASSWD_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GREETER_TYPE_VPN_PASSWD_DIALOG))
#define GREETER_IS_VPN_PASSWD_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GREETER_TYPE_VPN_PASSWD_DIALOG))
#define GREETER_VPN_PASSWD_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GREETER_VPN_PASSWD_DIALOG, GreeterVPNPasswdDialogClass))

typedef struct _GreeterVPNPasswdDialog        GreeterVPNPasswdDialog;
typedef struct _GreeterVPNPasswdDialogClass   GreeterVPNPasswdDialogClass;
typedef struct _GreeterVPNPasswdDialogPrivate GreeterVPNPasswdDialogPrivate;


struct _GreeterVPNPasswdDialog
{
	GtkDialog  __parent__;

	GreeterVPNPasswdDialogPrivate *priv;
};

struct _GreeterVPNPasswdDialogClass
{
	GtkDialogClass   __parent_class__;
};

GType      greeter_vpn_passwd_dialog_get_type (void);

GreeterVPNPasswdDialog *greeter_vpn_passwd_dialog_new (const gchar *password);

gchar     *greeter_vpn_passwd_dialog_get_new_password (GreeterVPNPasswdDialog *dialog);

G_END_DECLS

#endif /* __GREETER_VPN_PASSWD_DIALOG_H */
