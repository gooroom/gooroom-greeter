/*
 * Copyright (C) 2015-2019 Gooroom <gooroom@gooroom.kr>
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>


static gchar *action = NULL;

static GOptionEntry option_entries[] =
{
	{ "action", 'a', 0, G_OPTION_ARG_STRING, &action, NULL, NULL },
    { NULL }
};

int
main (int argc, char **argv)
{
	gboolean retval;
	GError *error = NULL;
    gchar *nmcli = NULL;
	GOptionContext *context;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, NULL);
	retval = g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	/* parse options */
	if (!retval) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	if (!action) {
		g_warning ("No action was specified.");
		return EXIT_FAILURE;
	}

	if (g_str_equal (action, "on")) {
	} else if (g_str_equal (action, "off")) {
	} else {
		g_warning ("Invalid action.");
		return EXIT_FAILURE;
	}

	nmcli = g_find_program_in_path ("nmcli");
	if (nmcli) {
		gchar *cmd = g_strdup_printf ("%s networking %s", nmcli, action);
		g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
		g_free (cmd);
	} else {
		g_warning ("nmcli command not found.");
	}
	g_free (nmcli);
	
	return EXIT_SUCCESS;
}
