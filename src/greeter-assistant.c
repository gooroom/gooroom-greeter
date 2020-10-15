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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <math.h>


#include "greeter-assistant.h"
#include "greeter-mode-page.h"
#include "greeter-network-page.h"
#include "greeter-login-page.h"
#include "greeter-message-dialog.h"

#define INTERNAL_LOGIN_BG_FILENAME "internal-login-bg.png"
#define EXTERNAL_LOGIN_BG_FILENAME "external-login-bg.png"


struct _GreeterAssistantPrivate {
	GtkWidget *stack;
	GtkWidget *first;
	GtkWidget *forward;
	GtkWidget *backward;
	GtkWidget *logo_box;
	GtkWidget *step_box;
	GtkWidget *brand_label;
	GtkWidget *help_button_type1;
	GtkWidget *help_button_type2;
	GtkWidget *step;
	GtkWidget *title;
	GtkWidget *iti_label;
	GtkWidget *eti_label;
	GdkPixbuf *logo_bg_pixbuf;
	GdkPixbuf *internal_login_bg;
	GdkPixbuf *external_login_bg;

	GList *pages;
	GreeterPage *current_page;

	GreeterPageManager *manager;
};

typedef GreeterPage *(*PreparePage) (GreeterPageManager *manager);

typedef struct {
  const gchar *page_id;
  PreparePage  prepare_page_func;
} PageData;


static PageData page_table[] = {
	{ "mode",     greeter_prepare_mode_page    },
	{ "network",  greeter_prepare_network_page },
	{ "login",    greeter_prepare_login_page   },
	{ NULL, NULL }
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterAssistant, greeter_assistant, GTK_TYPE_BOX)



static void
launch_help (void)
{
	gchar **argv = NULL;
	const gchar *cmd = "/usr/bin/gooroom-guide";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_strfreev (argv);
}

static void
switch_to (GreeterAssistant *assistant, GreeterPage *page)
{
	if (!page)
		return;

	gtk_stack_set_visible_child (GTK_STACK (assistant->priv->stack), GTK_WIDGET (page));
}

static GreeterPage *
find_first_page (GreeterAssistant *assistant)
{
	GList *l = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	l = g_list_first (priv->pages);
	if (l) {
		GreeterPage *page = GREETER_PAGE (l->data);
		if (greeter_page_should_show (page))
			return page;
	}

	return NULL;
}

static GreeterPage *
find_next_page (GreeterAssistant *assistant)
{
	GList *l = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	l = g_list_find (priv->pages, priv->current_page);
	if (l) l = l->next;

	for (; l != NULL; l = l->next) {
		GreeterPage *page = GREETER_PAGE (l->data);
		if (greeter_page_should_show (page))
			return page;
	}

	return NULL;
}

static GreeterPage *
find_prev_page (GreeterAssistant *assistant)
{
	GList *l = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	l = g_list_find (priv->pages, priv->current_page);
	if (l) l = l->prev;

	for (; l != NULL; l = l->prev) {
		GreeterPage *page = GREETER_PAGE (l->data);
		if (greeter_page_should_show (page))
			return page;
	}

	return NULL;
}

static void
remove_style_class (GreeterAssistant *assistant, const gchar *classname)
{
	GtkStyleContext *style, *style_title, *style_step_box;
	GtkStyleContext *style_first, *style_forward, *style_backward;
	GreeterAssistantPrivate *priv = assistant->priv;

	style = gtk_widget_get_style_context (GTK_WIDGET (assistant));
	style_first = gtk_widget_get_style_context (GTK_WIDGET (priv->first));
	style_forward = gtk_widget_get_style_context (GTK_WIDGET (priv->forward));
	style_backward = gtk_widget_get_style_context (GTK_WIDGET (priv->backward));
	style_title = gtk_widget_get_style_context (GTK_WIDGET (priv->title));
	style_step_box = gtk_widget_get_style_context (GTK_WIDGET (priv->step_box));

	gtk_style_context_remove_class (style, classname);
	gtk_style_context_remove_class (style_first, classname);
	gtk_style_context_remove_class (style_forward, classname);
	gtk_style_context_remove_class (style_backward, classname);
	gtk_style_context_remove_class (style_title, classname);
	gtk_style_context_remove_class (style_step_box, classname);
}

static void
add_style_class (GreeterAssistant *assistant, const gchar *classname)
{
	GtkStyleContext *style, *style_title, *style_step_box;
	GtkStyleContext *style_first, *style_forward, *style_backward;
	GreeterAssistantPrivate *priv = assistant->priv;

	style = gtk_widget_get_style_context (GTK_WIDGET (assistant));
	style_first = gtk_widget_get_style_context (GTK_WIDGET (priv->first));
	style_forward = gtk_widget_get_style_context (GTK_WIDGET (priv->forward));
	style_backward = gtk_widget_get_style_context (GTK_WIDGET (priv->backward));
	style_title = gtk_widget_get_style_context (GTK_WIDGET (priv->title));
	style_step_box = gtk_widget_get_style_context (GTK_WIDGET (priv->step_box));

	gtk_style_context_add_class (style, classname);
	gtk_style_context_add_class (style_first, classname);
	gtk_style_context_add_class (style_forward, classname);
	gtk_style_context_add_class (style_backward, classname);
	gtk_style_context_add_class (style_title, classname);
	gtk_style_context_add_class (style_step_box, classname);
}

static void
set_suggested_action_sensitive (GtkWidget *widget, gboolean sensitive)
{
	gtk_widget_set_sensitive (widget, sensitive);
	if (sensitive)
		gtk_style_context_add_class (gtk_widget_get_style_context (widget), "suggested-action");
	else
		gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "suggested-action");
}

//static void
//set_navigation_button (GreeterAssistant *assistant, GtkWidget *widget)
//{
//	GreeterAssistantPrivate *priv = assistant->priv;
//
//	gtk_widget_set_visible (priv->forward, (widget == priv->forward));
//}

static void
update_titlebar (GreeterAssistant *assistant)
{
	const gchar *title;
	const gchar *page_id;
	GreeterAssistantPrivate *priv = assistant->priv;

	title = greeter_assistant_get_title (assistant);
	if (title)
		gtk_label_set_text (GTK_LABEL (priv->title), title);

	page_id = GREETER_PAGE_GET_CLASS (priv->current_page)->page_id;
	if (page_id) {
		gtk_label_set_text (GTK_LABEL (priv->step), page_id);
	}
}

static void
update_navigation_buttons (GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;
	gboolean is_last_page;

	if (priv->current_page == NULL)
		return;

	gtk_widget_hide (priv->first);

	is_last_page = (find_next_page (assistant) == NULL);

	if (is_last_page) {
		gtk_widget_hide (priv->forward);
		gtk_widget_hide (priv->backward);
		gtk_widget_show (priv->first);
	} else {
		gboolean is_first_page;

		is_first_page = (find_prev_page (assistant) == NULL);

		gtk_widget_set_visible (priv->forward, !is_first_page);
		gtk_widget_set_visible (priv->backward, !is_first_page);

		if (greeter_page_get_complete (priv->current_page)) {
			set_suggested_action_sensitive (priv->forward, TRUE);
		} else {
			set_suggested_action_sensitive (priv->forward, FALSE);
		}
//		set_navigation_button (assistant, priv->forward);
	}
}

static void
update_current_page (GreeterAssistant *assistant,
                     GreeterPage      *page)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (priv->current_page == page)
		return;

	priv->current_page = page;

	if (GREETER_IS_LOGIN_PAGE (assistant->priv->current_page)) {
		if (greeter_page_manager_get_mode (priv->manager) == MODE_INTERNAL) {
			remove_style_class (assistant, "external");
			add_style_class (assistant, "internal");
		} else {
			remove_style_class (assistant, "internal");
			add_style_class (assistant, "external");
		}
	} else {
		remove_style_class (assistant, "internal");
		remove_style_class (assistant, "external");
	}

	gtk_widget_queue_draw (GTK_WIDGET (assistant));

	update_titlebar (assistant);
	update_navigation_buttons (assistant);
	gtk_widget_grab_focus (priv->forward);

	if (page)
		greeter_page_shown (page);
}

static void
vpn_service_reload_done_cb (GPid pid, gint status, gpointer user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
	GreeterPageManager *manager = assistant->priv->manager;

    g_spawn_close_pid (pid);

	greeter_page_manager_hide_splash (manager);
}

static gboolean
reload_vpn_service (gpointer user_data)
{
    GPid pid;
    gchar **argv;
    const gchar *cmd;
	GtkWidget *toplevel;
	const gchar *message;
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
	GreeterPageManager *manager = assistant->priv->manager;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (assistant));
	message = _("Initializing Settings for VPN.\nPlease wait...");

	greeter_page_manager_show_splash (manager, toplevel, message, NULL);

	cmd = "/bin/systemctl restart gooroom-vpn-daemon.service";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	if (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL))
		g_child_watch_add (pid, (GChildWatchFunc) vpn_service_reload_done_cb, assistant);

	g_strfreev (argv);

	return FALSE;
}

static void
go_first_button_cb (GtkWidget *button,
                    gpointer   user_data)
{
	int res;
	const gchar *title;
	const gchar *message;
	GtkWidget *dialog, *toplevel;
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	if (greeter_page_manager_get_is_vpn_logined (assistant->priv->manager)) {
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (assistant));

		title = _("VPN Connection Termination Warning");
		message = _("If you return to the first step, the VPN connection is terminated and "
                    "you need to proceed with the authentication process again. "
                    "Would you like to continue anyway?");

		dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
                                             "network-vpn-symbolic",
                                             title,
                                             message);

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                _("_Ok"), GTK_RESPONSE_OK,
                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                NULL);

		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

		res = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (res == GTK_RESPONSE_CANCEL)
			return;
	}

	greeter_assistant_first_page (assistant);

	g_idle_add ((GSourceFunc)reload_vpn_service, assistant);
}

static void
go_forward_button_cb (GtkWidget *button,
                      gpointer   user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	greeter_assistant_next_page (assistant);
}

static void
go_backward_button_cb (GtkWidget *button,
                       gpointer   user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	greeter_assistant_prev_page (assistant);
}

static void
help_button_clicked_cb (GtkWidget *button,
                        gpointer   user_data)
{
	launch_help ();
}

static gboolean
help_button_activated_cb (GtkLinkButton *button,
                          gpointer       user_data)
{
	launch_help ();

	return FALSE;
}

static gboolean
draw_logo_cb (GtkWidget *da,
              cairo_t   *cr,
              gpointer   user_data)
{
	gint w, h;
	double x = 0, y = 0;
	double radius = 0;
	double degrees = M_PI / 180.0;
	GdkPixbuf *new = NULL;
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
	GreeterAssistantPrivate *priv = assistant->priv;

	w = gtk_widget_get_allocated_width (da);
	h = gtk_widget_get_allocated_height (da);

	if (GREETER_IS_LOGIN_PAGE (priv->current_page)) {
		if (greeter_page_manager_get_mode (priv->manager) == MODE_INTERNAL) {
			new = gdk_pixbuf_scale_simple (priv->internal_login_bg, w, h, GDK_INTERP_BILINEAR);
		} else {
			new = gdk_pixbuf_scale_simple (priv->external_login_bg, w, h, GDK_INTERP_BILINEAR);
		}
	} else {
		new = gdk_pixbuf_scale_simple (assistant->priv->logo_bg_pixbuf, w, h, GDK_INTERP_BILINEAR);
	}

	gdk_cairo_set_source_pixbuf (cr, new, 0, 0);

	cairo_arc (cr, x + w - radius, y + radius, radius, -90 * degrees, 0 * degrees);
	cairo_arc (cr, x + w - radius, y + h - radius, radius, 0 * degrees, 90 * degrees);
	radius = 32;
	cairo_arc (cr, x + radius, y + h - radius, radius, 90 * degrees, 180 * degrees);
	cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
	cairo_clip (cr);

	cairo_paint (cr);

	g_object_unref (new);

	return FALSE;
}

static void
page_notify_cb (GreeterPage      *page,
                GParamSpec       *pspec,
                GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (page != priv->current_page)
		return;

	if (strcmp (pspec->name, "title") == 0) {
		update_titlebar (assistant);
	} else {
		update_navigation_buttons (assistant);
	}
}

static void
current_page_changed_cb (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);
	GtkWidget *new_page = gtk_stack_get_visible_child (GTK_STACK (gobject));

	update_current_page (assistant, GREETER_PAGE (new_page));
}

static gboolean
change_current_page_idle (gpointer user_data)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (user_data);

	current_page_changed_cb (G_OBJECT (assistant->priv->stack), NULL, assistant);

	return FALSE;
}

static void
go_next_page_cb (GreeterPageManager *manager,
                 gpointer            user_data)
{
	greeter_assistant_next_page (GREETER_ASSISTANT (user_data));
}

static void
go_first_page_cb (GreeterPageManager *manager,
                  gpointer            user_data)
{
	greeter_assistant_first_page (GREETER_ASSISTANT (user_data));
}

static void
greeter_assistant_ui_setup (GreeterAssistant *assistant)
{
	gboolean show_help = TRUE, show_help_auto = FALSE;
	gchar *iti = NULL, *eti = NULL;
	gchar *help_pos_type = NULL;
	gchar *bg_path, *filename;
	GError *error = NULL;
	GreeterAssistantPrivate *priv = assistant->priv;

	if (g_file_test (KEPCO_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
		GKeyFile *keyfile;

		keyfile = g_key_file_new ();
		g_key_file_load_from_file (keyfile, KEPCO_CONFIG_FILE, G_KEY_FILE_NONE, NULL);
		iti = g_key_file_get_string (keyfile, "Settings", "ITI", NULL);
		eti = g_key_file_get_string (keyfile, "Settings", "ETI", NULL);
		show_help = g_key_file_get_boolean (keyfile, "Settings", "SHOW-HELP", NULL);
		show_help_auto = g_key_file_get_boolean (keyfile, "Settings", "SHOW-HELP-AUTOMATIC", NULL);
		help_pos_type = g_key_file_get_string (keyfile, "Settings", "HELP-POSITION-TYPE", NULL);
		g_key_file_unref (keyfile);
	} else {
		g_warning("Failed to load kepco.conf file: %s", KEPCO_CONFIG_FILE);
	}

	if (!iti) iti = g_strdup ("1166");
	if (!eti) eti = g_strdup ("061-345-1166");
	if (!help_pos_type) help_pos_type = g_strdup ("linkbutton");

	gtk_label_set_text (GTK_LABEL (priv->iti_label), iti);
	gtk_label_set_text (GTK_LABEL (priv->eti_label), eti);

	if (show_help) {
		gtk_button_set_label (GTK_BUTTON (priv->help_button_type2), _("Show Help"));

		if (g_str_equal (help_pos_type, "button")) {
			gtk_widget_show (priv->help_button_type1);
			gtk_widget_hide (priv->help_button_type2);
		} else if (g_str_equal (help_pos_type, "linkbutton")) {
			gtk_widget_show (priv->help_button_type2);
			gtk_widget_hide (priv->help_button_type1);
		} else {
			gtk_widget_hide (priv->help_button_type1);
			gtk_widget_hide (priv->help_button_type2);
		}
		if (show_help_auto) {
			launch_help ();
		}
	} else {
		gtk_widget_hide (priv->help_button_type1);
		gtk_widget_hide (priv->help_button_type2);
	}

	g_free (iti);
	g_free (eti);
	g_free (help_pos_type);

	bg_path = g_build_filename (PKGDATA_DIR, "backgrounds", NULL);

	filename = g_build_filename (bg_path, INTERNAL_LOGIN_BG_FILENAME, NULL);
	priv->internal_login_bg = gdk_pixbuf_new_from_file (filename, &error);
	if (error) {   
		g_warning("Failed to load background: %s", error->message);
		g_clear_error(&error);
	}
	g_free (filename);

	filename = g_build_filename (bg_path, EXTERNAL_LOGIN_BG_FILENAME, NULL);
	priv->external_login_bg = gdk_pixbuf_new_from_file (filename, &error);
	if (error) {   
		g_warning("Failed to load background: %s", error->message);
		g_clear_error(&error);
	}

	do {
		g_free (filename);
		gint c = g_random_int_range (1, 100);
		filename = g_strdup_printf ("%s/logo-bg-%03d.jpg", bg_path, c);
	} while (!g_file_test (filename, G_FILE_TEST_EXISTS));

	priv->logo_bg_pixbuf = gdk_pixbuf_new_from_file (filename, &error);
	if (error) {   
		g_warning("Failed to load background: %s", error->message);
		g_clear_error(&error);
	}

	g_free (bg_path);
	g_free (filename);
}

//static void
//greeter_assistant_realize (GtkWidget *widget)
//{
//	gint pref_w, pref_h;
//
//	if (GTK_WIDGET_CLASS (greeter_assistant_parent_class)->realize)
//		GTK_WIDGET_CLASS (greeter_assistant_parent_class)->realize (widget);
//
//	gtk_widget_get_preferred_width (widget, NULL, &pref_w);
//	gtk_widget_get_preferred_height (widget, NULL, &pref_h);
//}


static void
greeter_assistant_finalize (GObject *object)
{
	GreeterAssistant *assistant = GREETER_ASSISTANT (object);
	GreeterAssistantPrivate *priv = assistant->priv;

	g_clear_object (&priv->manager);
	g_clear_object (&priv->internal_login_bg);
	g_clear_object (&priv->external_login_bg);

	if (priv->pages) {
		g_list_free (priv->pages);
		priv->pages = NULL;
	}

	G_OBJECT_CLASS (greeter_assistant_parent_class)->finalize (object);
}

static void
greeter_assistant_init (GreeterAssistant *assistant)
{
	PageData *page_data;
	GreeterAssistantPrivate *priv;

	priv = assistant->priv = greeter_assistant_get_instance_private (assistant);

	priv->manager = greeter_page_manager_new ();

	gtk_widget_init_template (GTK_WIDGET (assistant));

	greeter_assistant_ui_setup (assistant);

	page_data = page_table;
	for (; page_data->page_id != NULL; ++page_data) {
		GreeterPage *page = page_data->prepare_page_func (priv->manager);
		if (!page)
			continue;

		greeter_assistant_add_page (assistant, page);
	}

	g_idle_add ((GSourceFunc) change_current_page_idle, assistant);

	g_signal_connect (priv->manager, "go-next", G_CALLBACK (go_next_page_cb), assistant);
	g_signal_connect (priv->manager, "go-first", G_CALLBACK (go_first_page_cb), assistant);

	g_signal_connect (priv->stack, "notify::visible-child",
                      G_CALLBACK (current_page_changed_cb), assistant);

	g_signal_connect (priv->first, "clicked", G_CALLBACK (go_first_button_cb), assistant);
	g_signal_connect (priv->forward, "clicked", G_CALLBACK (go_forward_button_cb), assistant);
	g_signal_connect (priv->backward, "clicked", G_CALLBACK (go_backward_button_cb), assistant);
	g_signal_connect (priv->help_button_type1, "clicked",
                      G_CALLBACK (help_button_clicked_cb), assistant);
	g_signal_connect (priv->help_button_type2, "activate-link",
                      G_CALLBACK (help_button_activated_cb), assistant);

	g_signal_connect (priv->logo_box, "draw", G_CALLBACK (draw_logo_cb), assistant);
}

static void
greeter_assistant_class_init (GreeterAssistantClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
//	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->finalize = greeter_assistant_finalize;
//	widget_class->realize = greeter_assistant_realize;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-assistant.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, stack);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, first);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, forward);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, backward);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, logo_box);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, brand_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, help_button_type1);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, help_button_type2);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, step_box);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, step);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, title);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, iti_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, eti_label);
}

GtkWidget *
greeter_assistant_new (void)
{
	return g_object_new (GREETER_TYPE_ASSISTANT, NULL);
}

void
greeter_assistant_add_page (GreeterAssistant *assistant,
                            GreeterPage      *page)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	priv->pages = g_list_append (priv->pages, page);

	g_signal_connect (page, "notify", G_CALLBACK (page_notify_cb), assistant);

	gtk_widget_set_halign (GTK_WIDGET (page), GTK_ALIGN_FILL);
	gtk_widget_set_valign (GTK_WIDGET (page), GTK_ALIGN_FILL);

	gtk_container_add (GTK_CONTAINER (priv->stack), GTK_WIDGET (page));
}

GreeterPage *
greeter_assistant_get_current_page (GreeterAssistant *assistant)
{
	return assistant->priv->current_page;
}

const gchar *
greeter_assistant_get_title (GreeterAssistant *assistant)
{
	GreeterAssistantPrivate *priv = assistant->priv;

	if (priv->current_page != NULL)
		return greeter_page_get_title (priv->current_page);

	return NULL;
}

void
greeter_assistant_first_page (GreeterAssistant *assistant)
{
	GreeterPage *first_page;
	GreeterAssistantPrivate *priv = assistant->priv;

	first_page = find_first_page (assistant);

	if (first_page && priv->current_page &&
        (priv->current_page != first_page)) {

		GList *l = g_list_first (priv->pages);
		for (l = l->next; l; l = l->next) {
			GreeterPage *page = GREETER_PAGE (l->data);
			if (greeter_page_should_show (page))
				greeter_page_out (page, FALSE);
		}
	}

	switch_to (assistant, first_page);
}

void
greeter_assistant_next_page (GreeterAssistant *assistant)
{
	GreeterPage *next_page;
	GreeterAssistantPrivate *priv = assistant->priv;

	next_page = find_next_page (assistant);

	if (next_page && priv->current_page &&
        (priv->current_page != next_page)) {
		greeter_page_out (priv->current_page, TRUE);
	}

	switch_to (assistant, next_page);
}

void
greeter_assistant_prev_page (GreeterAssistant *assistant)
{
	GreeterPage *prev_page;
	GreeterAssistantPrivate *priv = assistant->priv;

	prev_page = find_prev_page (assistant);

	if (prev_page && priv->current_page &&
        (priv->current_page != prev_page)) {
		greeter_page_out (priv->current_page, FALSE);
	}

	switch_to (assistant, prev_page);
}
