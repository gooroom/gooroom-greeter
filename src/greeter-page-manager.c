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

#define MODE_ONLINE_STRING  "ONLINE"
#define MODE_OFFLINE_STRING "OFFLINE"
#define MODE_SAVE_FILE_PATH "/var/tmp/lightdm.mode"

enum {
	GO_NEXT,
	GO_FIRST,
	MODE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


struct _GreeterPageManagerPrivate {
	GtkWidget *splash;

	int        mode;
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

	manager->priv->mode = MODE_ONLINE;
}

static void
greeter_page_manager_class_init (GreeterPageManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = greeter_page_manager_finalize;

	signals[GO_NEXT] = g_signal_new ("go-next",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, go_next),
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

	signals[MODE_CHANGED] = g_signal_new ("mode-changed",
                                     GREETER_TYPE_PAGE_MANAGER,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GreeterPageManagerClass, mode_changed),
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__INT,
                                     G_TYPE_NONE, 1, G_TYPE_INT);
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

	contents = (mode == MODE_ONLINE) ? MODE_ONLINE_STRING : MODE_OFFLINE_STRING;

	g_file_set_contents (MODE_SAVE_FILE_PATH, contents, -1, &error);
	if (error) {
		g_warning ("Error attepting to write %s file : %s", MODE_SAVE_FILE_PATH, error->message);
		g_error_free (error);
	}

	manager->priv->mode = mode;

	g_signal_emit (G_OBJECT (manager), signals[MODE_CHANGED], 0, mode);
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

	mode = g_str_equal (contents, MODE_ONLINE_STRING) ? MODE_ONLINE : MODE_OFFLINE;

out:
	g_free (contents);

	return mode;
}

void
greeter_page_manager_go_next (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[GO_NEXT], 0);
}

void
greeter_page_manager_go_first (GreeterPageManager *manager)
{
	g_signal_emit (G_OBJECT (manager), signals[GO_FIRST], 0);
}

void
greeter_page_manager_show_splash (GreeterPageManager *manager,
                                  GtkWidget          *parent,
                                  const char         *message)
{
	GreeterPageManagerPrivate *priv = manager->priv;

	greeter_page_manager_hide_splash (manager);

	priv->splash = splash_window_new (parent);
	splash_window_set_message_label (SPLASH_WINDOW (priv->splash), message);
	splash_window_show (SPLASH_WINDOW (priv->splash));
}

void
greeter_page_manager_hide_splash (GreeterPageManager *manager)
{
	GreeterPageManagerPrivate *priv = manager->priv;

	if (priv->splash) {
		splash_window_destroy (SPLASH_WINDOW (priv->splash));
		priv->splash = NULL;
	}
}

void
greeter_page_manager_update_splash_message (GreeterPageManager *manager,
                                            const char         *message)
{
	GreeterPageManagerPrivate *priv = manager->priv;

	if (priv->splash)
		splash_window_set_message_label (SPLASH_WINDOW (priv->splash), message);
}
