/*
 * Copyright 
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

#ifndef __SPLASH_WINDOW_H__
#define __SPLASH_WINDOW_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SPLASH_TYPE_WINDOW         (splash_window_get_type ())
#define SPLASH_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SPLASH_TYPE_WINDOW, SplashWindow))
#define SPLASH_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SPLASH_TYPE_WINDOW, SplashWindowClass))
#define SPLASH_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SPLASH_TYPE_WINDOW))
#define SPLASH_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SPLASH_TYPE_WINDOW))
#define SPLASH_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SPLASH_TYPE_WINDOW, SplashWindowClass))

typedef struct _SplashWindow SplashWindow;
typedef struct _SplashWindowClass SplashWindowClass;
typedef struct _SplashWindowPrivate SplashWindowPrivate;

struct _SplashWindow {
	GtkWindow __parent__;

	SplashWindowPrivate *priv;
};

struct _SplashWindowClass {
	GtkWindowClass __parent_class__;
};

GType          splash_window_get_type          (void); G_GNUC_CONST

SplashWindow  *splash_window_new               (GtkWindow *parent);

void           splash_window_show              (SplashWindow *window);
void           splash_window_destroy           (SplashWindow *window);
void           splash_window_set_message_label (SplashWindow *window,
                                                const char   *message);

G_END_DECLS

#endif /* __SPLASH_WINDOW_H__ */
