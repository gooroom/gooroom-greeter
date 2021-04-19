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
#include "greeter-login-page.h"
#include "greeter-network-page.h"
#include "greeter-message-dialog.h"

#define INTERNAL_LOGIN_BG_FILENAME "internal-login-bg.svg"
#define EXTERNAL_LOGIN_BG_FILENAME "external-login-bg.svg"


struct _GreeterAssistantPrivate {
	GtkWidget *stack;
	GtkWidget *logo_box;
	GtkWidget *brand_image;
	GtkWidget *usage_box;
	GtkWidget *brand_label;
	GtkWidget *help_button;
	GtkWidget *title;
	GtkWidget *title_image;
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
	{ "login",    greeter_prepare_login_page   },
	{ "network",    greeter_prepare_network_page   },
	{ NULL, NULL }
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterAssistant, greeter_assistant, GTK_TYPE_BOX)



static gboolean
launch_help (GtkWidget *assistant)
{
	gchar **argv = NULL;
	gchar *program = NULL;

	program = g_find_program_in_path ("gooroom-greeter-guide");
	if (!program) {
		GtkWidget *dialog;

		dialog = greeter_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (assistant)),
                                             "dialog-warning-symbolic.symbolic",
                                             NULL,
                                             _("Help cannot be launched.\nInstall the help package."));
		gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("Ok"), GTK_RESPONSE_OK, NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return FALSE;
	}

	g_shell_parse_argv (program, NULL, &argv, NULL);
	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
	g_strfreev (argv);
	g_free (program);

	return FALSE;
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
	GtkStyleContext *style, *style_title, *style_brand_label;
	GreeterAssistantPrivate *priv = assistant->priv;

	style = gtk_widget_get_style_context (GTK_WIDGET (assistant));
	style_title = gtk_widget_get_style_context (priv->title);
	style_brand_label = gtk_widget_get_style_context (priv->brand_label);

	gtk_style_context_remove_class (style, classname);
	gtk_style_context_remove_class (style_title, classname);
	gtk_style_context_remove_class (style_brand_label, classname);
}

static void
add_style_class (GreeterAssistant *assistant, const gchar *classname)
{
	GtkStyleContext *style, *style_title, *style_brand_label;
	GreeterAssistantPrivate *priv = assistant->priv;

	style = gtk_widget_get_style_context (GTK_WIDGET (assistant));
	style_title = gtk_widget_get_style_context (priv->title);
	style_brand_label = gtk_widget_get_style_context (priv->brand_label);

	gtk_style_context_add_class (style, classname);
	gtk_style_context_add_class (style_title, classname);
	gtk_style_context_add_class (style_brand_label, classname);
}

static void
update_titlebar (GreeterAssistant *assistant)
{
	const gchar *title;
	GreeterAssistantPrivate *priv = assistant->priv;

	title = greeter_assistant_get_title (assistant);
	if (title)
		gtk_label_set_text (GTK_LABEL (priv->title), title);
}

static void
update_current_page (GreeterAssistant *assistant,
                     GreeterPage      *page)
{
	const gchar *title_img_res, *brand_img_res;
	GreeterAssistantPrivate *priv = assistant->priv;

	priv->current_page = page;

	if (GREETER_IS_LOGIN_PAGE (assistant->priv->current_page)) {
		if (greeter_page_manager_get_mode (priv->manager) == MODE_INTERNAL) {
			remove_style_class (assistant, "external");
			add_style_class (assistant, "internal");
			title_img_res = "/kr/gooroom/greeter/mode-internal";
			brand_img_res = "/kr/gooroom/greeter/brand-internal";
			gtk_widget_hide (priv->usage_box);
		} else {
			remove_style_class (assistant, "internal");
			add_style_class (assistant, "external");
			title_img_res = "/kr/gooroom/greeter/mode-external";
			brand_img_res = "/kr/gooroom/greeter/brand-external";
			gtk_widget_show (priv->usage_box);
		}
		gtk_image_set_from_resource (GTK_IMAGE (priv->title_image), title_img_res);
		gtk_widget_show (priv->title_image);
	} else {
		remove_style_class (assistant, "internal");
		remove_style_class (assistant, "external");
		brand_img_res = "/kr/gooroom/greeter/brand-normal";
		gtk_widget_hide (priv->usage_box);
		gtk_widget_hide (priv->title_image);
	}

	gtk_image_set_from_resource (GTK_IMAGE (priv->brand_image), brand_img_res);

	gtk_widget_queue_draw (GTK_WIDGET (assistant));

	update_titlebar (assistant);

	if (page)
		greeter_page_shown (page);
}

static void
help_button_clicked_cb (GtkWidget *button,
                        gpointer   user_data)
{
	launch_help (GTK_WIDGET (user_data));
}

static gboolean
help_button_enter_notify_event_cb (GtkWidget *widget,
                                   GdkEvent  *event,
                                   gpointer   user_data)
{
	GdkDisplay *display;
	GdkCursor *cursor;

	display = gtk_widget_get_display (widget);
	cursor = gdk_cursor_new_from_name (display, "pointer");
	gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
	g_object_unref (cursor);

	return FALSE;
}

static gboolean
help_button_leave_notify_event_cb (GtkWidget *widget,
                                   GdkEvent  *event,
                                   gpointer   user_data)
{
	GdkDisplay *display;
	GdkCursor *cursor;

	display = gtk_widget_get_display (widget);
	cursor = gdk_cursor_new_from_name (display, "default");
	gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
	g_object_unref (cursor);

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

	if (strcmp (pspec->name, "title") == 0)
		update_titlebar (assistant);
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

static void
go_next_page_cb (GreeterPageManager *manager,
                 gpointer            user_data)
{
	greeter_assistant_next_page (GREETER_ASSISTANT (user_data));
}

static void
go_prev_page_cb (GreeterPageManager *manager,
                 gpointer            user_data)
{
	greeter_assistant_prev_page (GREETER_ASSISTANT (user_data));
}

static void
go_first_page_cb (GreeterPageManager *manager,
                  gpointer            user_data)
{
	greeter_assistant_first_page (GREETER_ASSISTANT (user_data));
}

static void
reload_page_cb (GreeterPageManager *manager,
                  gpointer            user_data)
{
	greeter_assistant_reload_page (GREETER_ASSISTANT (user_data));
}

static void
greeter_assistant_ui_setup (GreeterAssistant *assistant)
{
	gboolean show_help = TRUE, show_help_auto = FALSE;
	gchar *iti = NULL, *eti = NULL;
	gchar *bg_path, *filename;
	GError *error = NULL;
	PangoAttrList *attrs;
	PangoAttribute *attr;

	GreeterAssistantPrivate *priv = assistant->priv;

	if (g_file_test (KEPCO_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
		GKeyFile *keyfile;

		keyfile = g_key_file_new ();
		g_key_file_load_from_file (keyfile, KEPCO_CONFIG_FILE, G_KEY_FILE_NONE, NULL);
		iti = g_key_file_get_string (keyfile, "Settings", "ITI", NULL);
		eti = g_key_file_get_string (keyfile, "Settings", "ETI", NULL);
		show_help = g_key_file_get_boolean (keyfile, "Settings", "SHOW-HELP", NULL);
		show_help_auto = g_key_file_get_boolean (keyfile, "Settings", "SHOW-HELP-AUTOMATIC", NULL);
		g_key_file_unref (keyfile);
	} else {
		g_warning("Failed to load kepco.conf file: %s", KEPCO_CONFIG_FILE);
	}

	if (!iti) iti = g_strdup ("1166");
	if (!eti) eti = g_strdup ("061-345-1166");

	gtk_label_set_text (GTK_LABEL (priv->iti_label), iti);
	gtk_label_set_text (GTK_LABEL (priv->eti_label), eti);

	attrs = pango_attr_list_new ();
	attr = pango_attr_rise_new (7000);
	pango_attr_list_insert (attrs, attr);
	gtk_label_set_attributes (GTK_LABEL (priv->brand_label), attrs);

	if (show_help) {
		gtk_widget_show (priv->help_button);
	} else {
		gtk_widget_hide (priv->help_button);
	}

	if (show_help_auto)
		g_timeout_add (1000, (GSourceFunc) launch_help, GTK_WIDGET (assistant));

	g_free (iti);
	g_free (eti);

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
	greeter_page_manager_set_mode (priv->manager, MODE_INTERNAL);

	gtk_widget_init_template (GTK_WIDGET (assistant));

	greeter_assistant_ui_setup (assistant);

	page_data = page_table;
	for (; page_data->page_id != NULL; ++page_data) {
		GreeterPage *page = page_data->prepare_page_func (priv->manager);
		if (!page)
			continue;

		greeter_assistant_add_page (assistant, page);
	}

	current_page_changed_cb (G_OBJECT (priv->stack), NULL, assistant);

	g_signal_connect (priv->manager, "go-next", G_CALLBACK (go_next_page_cb), assistant);
	g_signal_connect (priv->manager, "go-prev", G_CALLBACK (go_prev_page_cb), assistant);
	g_signal_connect (priv->manager, "go-first", G_CALLBACK (go_first_page_cb), assistant);
	g_signal_connect (priv->manager, "reload", G_CALLBACK (reload_page_cb), assistant);

	g_signal_connect (priv->stack, "notify::visible-child",
                      G_CALLBACK (current_page_changed_cb), assistant);

	g_signal_connect (priv->help_button, "clicked",
                      G_CALLBACK (help_button_clicked_cb), assistant);
	g_signal_connect (priv->help_button, "enter-notify-event",
                      G_CALLBACK (help_button_enter_notify_event_cb), assistant);
	g_signal_connect (priv->help_button, "leave-notify-event",
                      G_CALLBACK (help_button_leave_notify_event_cb), assistant);

	g_signal_connect (priv->logo_box, "draw", G_CALLBACK (draw_logo_cb), assistant);
}

static void
greeter_assistant_class_init (GreeterAssistantClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = greeter_assistant_finalize;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-assistant.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, stack);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, logo_box);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, brand_image);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, usage_box);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, brand_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, help_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, title);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterAssistant, title_image);
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

void
greeter_assistant_reload_page (GreeterAssistant *assistant)
{
	current_page_changed_cb (G_OBJECT (assistant->priv->stack), NULL, assistant);
}
