/*
 * Copyright (C) 2015 - 2020 Gooroom <gooroom@gooroom.kr>
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

#include "config.h"

#include <glib.h>

#include "splash-window.h"
#include "greeter-page-manager.h"

#define MODE_EXTERNAL_STRING  "EXTERNAL"
#define MODE_INTERNAL_STRING  "INTERNAL"
#define MODE_SAVE_FILE_PATH   "/var/tmp/lightdm.mode"

enum {
	GO_NEXT,
	GO_PREV,
	GO_FIRST,
	RELOAD,
	LAST_SIGNAL
};

//enum
//{
//    PROP_0,
//    PROP_NETWORK_AVAILABLE,
//    PROP_LAST,
//};

static guint signals[LAST_SIGNAL];
//static GParamSpec *obj_props[PROP_LAST];


struct _GreeterPageManagerPrivate {
	SplashWindow *splash;

	gboolean      network_available;
	gboolean      is_vpn_logined;

	int           mode;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterPageManager, greeter_page_manager, G_TYPE_OBJECT)


static void
greeter_page_manager_finalize (GObject *object)
{
	G_OBJECT_CLASS (greeter_page_manager_parent_class)->finalize (object);
}

static void
greeter_page_manager_init (GreeterPageManager *manager)
{
	manager->priv = greeter_page_manager_get_instance_private (manager);

	manager->priv->mode = MODE_INTERNAL;
	manager->priv->network_available = FALSE;
	manager->priv->is_vpn_logined = FALSE;
}

static void
greeter_page_manager_class_init (GreeterPageManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = greeter_page_manager_finalize;

//	obj_props[PROP_NETWORK_AVAILABLE] =
//		g_param_spec_boolean ("network-available", "", "", FALSE,
//				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);


	signals[GO_NEXT] = g_signal_new ("go-next",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, go_next),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

	signals[GO_PREV] = g_signal_new ("go-prev",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, go_prev),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

	signals[GO_FIRST] = g_signal_new ("go-first",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, go_first),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

	signals[RELOAD] = g_signal_new ("reload",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, reload),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}

GreeterPageManager *
greeter_page_manager_new (void)
{
	GObject *result;

	result = g_object_new (GREETER_TYPE_PAGE_MANAGER, NULL);

	return GREETER_PAGE_MANAGER (result);
}

void
greeter_page_manager_set_mode (GreeterPageManager *manager,
                               int                 mode)
{
	GError *error = NULL;
	const char *contents;

	contents = (mode == MODE_INTERNAL) ? MODE_INTERNAL_STRING : MODE_EXTERNAL_STRING;

	g_file_set_contents (MODE_SAVE_FILE_PATH, contents, -1, &error);
	if (error) {
		g_warning ("Error attepting to write %s file : %s", MODE_SAVE_FILE_PATH, error->message);
		g_error_free (error);
	}

	manager->priv->mode = mode;
}

int
greeter_page_manager_get_mode (GreeterPageManager *manager)
{
	int mode;
	gsize len, i;
	GError *error = NULL;
	gchar *contents = NULL;

	g_file_get_contents (MODE_SAVE_FILE_PATH, &contents, &len, &error);
	if (error) {
		g_error_free (error);
		mode = manager->priv->mode;
		goto out;
	}

	for (i = 0; i < len; i++) {
		if (contents[i] == '\n')
			contents[i] = '\0';
	}

	mode = g_str_equal (contents, MODE_INTERNAL_STRING) ? MODE_INTERNAL : MODE_EXTERNAL;

out:
	g_free (contents);

	return mode;
}

void
greeter_page_manager_set_is_vpn_logined (GreeterPageManager *manager,
                                         gboolean            is_logined)
{
	manager->priv->is_vpn_logined = is_logined;
}

gboolean
greeter_page_manager_get_is_vpn_logined (GreeterPageManager *manager)
{
	return manager->priv->is_vpn_logined;
}

void
greeter_page_manager_set_network_available (GreeterPageManager *manager,
                                            gboolean            available)
{
	manager->priv->network_available = available;

//	g_object_notify_by_pspec (G_OBJECT (manager), obj_props[PROP_NETWORK_AVAILABLE]);
}

gboolean
greeter_page_manager_get_network_available (GreeterPageManager *manager)
{
	return manager->priv->network_available;
}

void
greeter_page_manager_go_next (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[GO_NEXT], 0);
}

void
greeter_page_manager_go_prev (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[GO_PREV], 0);
}

void
greeter_page_manager_go_first (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[GO_FIRST], 0);
}

void
greeter_page_manager_reload (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[RELOAD], 0);
}

void
greeter_page_manager_show_splash (GreeterPageManager *manager,
                                  GtkWidget          *parent,
                                  const char         *message,
                                  const char         *theme)
{
	GreeterPageManagerPrivate *priv = manager->priv;

	greeter_page_manager_hide_splash (manager);

	priv->splash = splash_window_new (GTK_WINDOW (parent));
	splash_window_set_message_label (SPLASH_WINDOW (priv->splash), message);
	splash_window_set_theme (SPLASH_WINDOW (priv->splash), theme);

	splash_window_show (priv->splash);
}

void
greeter_page_manager_hide_splash (GreeterPageManager *manager)
{
	GreeterPageManagerPrivate *priv = manager->priv;

	if (priv->splash) {
		splash_window_destroy (priv->splash);
		priv->splash = NULL;
	}
}
