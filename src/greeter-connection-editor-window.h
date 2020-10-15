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

#ifndef __GREETER_CONNECTION_EDITOR_WINDOW_H__
#define __GREETER_CONNECTION_EDITOR_WINDOW_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GREETER_TYPE_CONNECTION_EDITOR_WINDOW         (greeter_connection_editor_window_get_type ())
#define GREETER_CONNECTION_EDITOR_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GREETER_TYPE_CONNECTION_EDITOR_WINDOW, GreeterConnectionEditorWindow))
#define GREETER_CONNECTION_EDITOR_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GREETER_TYPE_CONNECTION_EDITOR_WINDOW, GreeterConnectionEditorWindowClass))
#define GREETER_IS_CONNECTION_EDITOR_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GREETER_TYPE_CONNECTION_EDITOR_WINDOW))
#define GREETER_IS_CONNECTION_EDITOR_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GREETER_TYPE_CONNECTION_EDITOR_WINDOW))
#define GREETER_CONNECTION_EDITOR_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GREETER_CONNECTION_EDITOR_WINDOW, GreeterConnectionEditorWindowClass))

typedef struct _GreeterConnectionEditorWindow        GreeterConnectionEditorWindow;
typedef struct _GreeterConnectionEditorWindowClass   GreeterConnectionEditorWindowClass;
typedef struct _GreeterConnectionEditorWindowPrivate GreeterConnectionEditorWindowPrivate;


struct _GreeterConnectionEditorWindow
{
	GtkWindow  __parent__;

	GreeterConnectionEditorWindowPrivate *priv;
};

struct _GreeterConnectionEditorWindowClass
{
	GtkWindowClass   __parent_class__;
};

GType                           greeter_connection_editor_window_get_type (void);

GreeterConnectionEditorWindow  *greeter_connection_editor_window_new (GdkMonitor *monitor,
                                                                      gchar      *uuid);

G_END_DECLS

#endif /* __GREETER_CONNECTION_EDITOR_WINDOW_H */
