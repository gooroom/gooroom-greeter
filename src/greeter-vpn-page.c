/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */


#define PAGE_ID "vpn"

#include "config.h"
#include "greeter-vpn-page.h"
#include "greeter-message-dialog.h"

#include <glib/gi18n.h>
#include <gio/gio.h>


#define VPN_LOGIN_TIMEOUT_SECS 60

#define VPN_SERVICE_NAME      "kr.gooroom.VPN"
#define VPN_SERVICE_PATH      "/kr/gooroom/VPN"
#define VPN_SERVICE_INTERFACE "kr.gooroom.VPN"

enum {
	VPN_LOGIN_FAILURE               = 1001,
	VPN_LOGIN_SUCCESS               = 1002,
	VPN_AUTH_FAILURE                = 1003,
	VPN_ACCOUNT_LOCKED              = 1004,
	VPN_ID_EXPIRED                  = 1005,
	VPN_PW_EXPIRED                  = 1006,
	VPN_LOGIN_EXPIRED               = 1007,
	VPN_LOGIN_TIME_BLOCKED          = 1008,
	VPN_LOGIN_WEEK_BLOCKED          = 1009,
	VPN_SERVER_CONNECTION_ERROR     = 1010,
	VPN_SERVER_RESPONSE_ERROR       = 1011,
	VPN_SERVER_DISCONNECTED         = 1012,
	VPN_UNKNOWN_ERROR               = 1013,
	VPN_SERVICE_DAEMON_ERROR        = 1014,
	VPN_SERVICE_LOGIN_REQUEST_ERROR = 1015
};


static void login_button_clicked_cb (GtkButton *button, gpointer user_data);


struct _GreeterVPNPagePrivate {
	GtkWidget  *id_entry;
	GtkWidget  *pw_entry;
	GtkWidget  *message_label;
	GtkWidget  *login_button;
	GtkWidget  *login_button_icon;

	gchar      *ip;
	gchar      *port;

	GDBusProxy *proxy;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterVPNPage, greeter_vpn_page, GREETER_TYPE_PAGE);



static gboolean
grab_focus_idle (gpointer data)
{
	gtk_widget_grab_focus (GTK_WIDGET (data));
	
	return FALSE;
}

static GDBusProxy *
get_vpn_dbus_proxy (void)
{
	GDBusProxy *proxy;
	GError *error = NULL;

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_CALL_FLAGS_NONE,
                                          NULL,
                                          VPN_SERVICE_NAME,
                                          VPN_SERVICE_PATH,
                                          VPN_SERVICE_INTERFACE,
                                          NULL,
                                          &error);

	if (!proxy || error) {
		if (error) {
			g_error_free (error);
		}
		if (proxy)
			g_clear_object (&proxy);

		return NULL;
	}

	g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (proxy),
                                      VPN_LOGIN_TIMEOUT_SECS * 1000);

	return proxy;
}

static gchar *
get_activated_vpn_service (void)
{
    GKeyFile *keyfile; 
    char *active_vpn = NULL;

    if (!g_file_test (GOOROOM_VPN_SERVICE_CONFIG_FILE, G_FILE_TEST_EXISTS))
        return NULL;

    keyfile = g_key_file_new ();
    g_key_file_load_from_file (keyfile, GOOROOM_VPN_SERVICE_CONFIG_FILE, G_KEY_FILE_NONE, NULL);
    active_vpn = g_key_file_get_string (keyfile, "VPN", "Activate", NULL);
    g_key_file_unref (keyfile);

    return active_vpn;
}

static void
get_vpn_connection_info (gchar **ip,
                         gchar **port)
{
	GKeyFile *keyfile;
	gchar    *active_vpn;
    gchar    *config_path;

	active_vpn = get_activated_vpn_service ();
	if (!active_vpn)
		return;

	config_path = g_strdup_printf ("%s/%s.conf",
                                        VPN_SERVICE_CONFIG_DIR,
                                        active_vpn);

	if (!g_file_test (config_path, G_FILE_TEST_EXISTS)) {
//		g_warning (G_STRLOC ": No secuwiz vpn config file: %s", SECUWIZ_VPN_CONFIG_FILE);
		goto done;
	}

	keyfile = g_key_file_new ();
	g_key_file_load_from_file (keyfile, config_path, G_KEY_FILE_NONE, NULL);

	if (ip)
		*ip = g_key_file_get_string (keyfile, "Settings", "Ip", NULL);

	if (port)
		*port = g_key_file_get_string (keyfile, "Settings", "Port", NULL);

	g_key_file_unref (keyfile);

done:
	g_free (active_vpn);
	g_free (config_path);
}

static void
post_login (GreeterVPNPage *page)
{
	GreeterVPNPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	greeter_page_manager_hide_splash (manager);

	g_signal_handlers_unblock_by_func (page->priv->login_button,
                                       login_button_clicked_cb, page);

	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);

	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
}

static void
pre_login (GreeterVPNPage *page)
{
	GtkWidget *toplevel;
	const char *message;

	GreeterVPNPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	message = _("Authentication is in progress.\nPlease wait...");

	greeter_page_manager_show_splash (manager, toplevel, message);

	g_signal_handlers_block_by_func (priv->login_button,
                                     login_button_clicked_cb, page);

	gtk_widget_set_sensitive (priv->id_entry, FALSE);
	gtk_widget_set_sensitive (priv->pw_entry, FALSE);

	gtk_label_set_text (GTK_LABEL (priv->message_label), "");
}

static void
show_login_success_dialog (GreeterVPNPage *page)
{
	const gchar *title;
	const gchar *message;
	GtkWidget   *dialog, *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

	title = _("VPN Login Success");
	message = _("You have successfully login to the VPN.\n"
                "Click the 'OK' button and proceed with system login.");

	dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
                                         "dialog-information",
                                         title,
                                         message);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Ok"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
update_error_message_label (GtkWidget *label, int result)
{
	const gchar *message;

	switch (result)
	{
		case VPN_LOGIN_SUCCESS:
			message = NULL;
		break;

		//시스템의 VPN 서비스에서 오류가 발생하였습니다.
		//시스템 재부팅 후 다시 시도하십시오.
		case VPN_SERVICE_DAEMON_ERROR:
			message = _("An error has occurred in the system's VPN service.\n"
                        "Reboot your system and try again.");
		break;

		//VPN 로그인 요청에 실패하였습니다.
		//시스템 재부팅 후 다시 시도하십시오.
		case VPN_SERVICE_LOGIN_REQUEST_ERROR:
			message = _("VPN login request failed.\nReboot your system and try again.");
		break;

		// 사용자 인증에 실패하였습니다.
		// 5회 이상 인증 실패 시 로그인이 제한 됩니다.
		case VPN_AUTH_FAILURE:
			message = _("User authentication failed.\n"
                        "If authentication fails more than 5 times, "
                        "you can no longer log in.");
		break;

		// 사용자 인증이 5회이상 실패하여 더이상 로그인 할 수 없습니다.
        // 관리자에게 문의하십시오.
		case VPN_ACCOUNT_LOCKED:
			message = _("Login is not possible because user "
                        "authentication has failed more than 5 times.\n"
                        "Please contact the administrator.");
		break;

		// 네트워크 문제로 서버에 접속 할 수 없습니다.
        // 네트워크 상태 확인 후 다시 시도해 주십시오.
		case VPN_SERVER_CONNECTION_ERROR:
			message = _("The server cannot be reached due to network problems.\n"
                        "Please check the network status and try again.");
		break;

		//서버와의 연결이 끊겼습니다.
		//네트워크 상태 확인 후 다시 연결하십시오.
		case VPN_SERVER_DISCONNECTED:
			message = _("The connection to the server has been lost. "
                        "Check the network status and connect again.");
		break;

		// 알 수 없는 오류로 인해 로그인에 실패하였습니다.
		// 잠시 후 다시 시도 하거나 관리자에게 문의하십시오.
		default:
			message = _("Login failed due to an unknown error.\n"
                        "Please try again later or contact the administrator.");
		break;
	}

	if (message) {
		gchar *markup;
		markup = g_markup_printf_escaped ("<span foreground='red'>%s</span>", message);
		gtk_label_set_markup (GTK_LABEL (label), markup);
		g_free (markup);
	}
}

static void
handle_login_result (GreeterVPNPage *page,
                     int             result)
{
	GreeterVPNPagePrivate *priv = page->priv;

	// 성공 or 실패(에러)코드가 아닌 상태를 알리기위한 코드
	if (result == 0)
		return;

	post_login (page);

	if (result == VPN_LOGIN_SUCCESS) {
		greeter_page_set_complete (GREETER_PAGE (page), TRUE);
		show_login_success_dialog (page);
		greeter_page_manager_go_next (GREETER_PAGE (page)->manager);
	} else if (result == VPN_SERVER_DISCONNECTED) {
//		GreeterPageManager *manager = GREETER_PAGE (page)->manager;
g_print ("VVVVVVPN_SERVER_DISCONNECTED\n");
	} else {
		update_error_message_label (priv->message_label, result);
		g_idle_add ((GSourceFunc) grab_focus_idle, priv->id_entry);
	}
}

static void
try_to_logout_done_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
	gint32 result = -1;
	GVariant *variant;
	GreeterPageManager *manager = GREETER_PAGE (user_data)->manager;
	
	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, NULL);
	if (variant) {
        g_variant_get (variant, "(i)", &result);
		g_variant_unref (variant);
	}

	greeter_page_set_complete (GREETER_PAGE (user_data), FALSE);

	greeter_page_manager_hide_splash (manager);
}

static gboolean
try_to_logout (GreeterVPNPage *page)
{
	GtkWidget *toplevel;
	const char *message;
	GreeterVPNPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	message = _("Initializing Settings for VPN.\nPlease wait...");

	greeter_page_manager_show_splash (manager, toplevel, message);

	if (!priv->proxy)
		priv->proxy = get_vpn_dbus_proxy ();

	if (priv->proxy) {
		g_dbus_proxy_call (priv->proxy,
                           "Logout",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           try_to_logout_done_cb,
                           page);

		return TRUE;
	}

	return FALSE;
}

static void
try_to_login_done_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	gint32 result = -1;
	GVariant *variant;
	GError *error = NULL;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);

	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (error) {
		g_warning ("Failed to login VPN: %s", error->message);
		g_error_free (error);
		handle_login_result (page, VPN_SERVICE_LOGIN_REQUEST_ERROR);
		return;
	}

	if (variant) {
		g_variant_get (variant, "(i)", &result);
		g_variant_unref (variant);
	}

	// 로그인시도 후 결과는 0 (성공) or -1(실패)
	// 성공의 의미는 로그인의 성공을 의미하는 것이 아니라 로그인시도의 성공을 의미함
	// 로그인시도 성공 후 로그인의 성공여부는 vpn_dbus_signal_handler에서 처리함.
//	if (result == -1) {
//		handle_login_result (page, VPN_SERVICE_LOGIN_REQUEST_ERROR);
//	}
}

static gboolean
try_to_login (GreeterVPNPage *page,
               const gchar    *ip,
               const gchar    *port,
               const gchar    *id,
               const gchar    *pw)
{
	GreeterVPNPagePrivate *priv = page->priv;

	if (!priv->proxy)
		priv->proxy = get_vpn_dbus_proxy ();

	if (priv->proxy) {
		g_dbus_proxy_call (priv->proxy,
                           "Login",
                           g_variant_new ("(ssss)", ip, port, id, pw),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           try_to_login_done_cb,
                           page);

		return TRUE;
	}

	return FALSE;
}

static void
vpn_dbus_signal_handler (GDBusProxy *proxy,
                         gchar      *sender_name,
                         gchar      *signal_name,
                         GVariant   *parameters,
                         gpointer    user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);

	if (g_str_equal (signal_name, "ConnectionStateChanged")) {
		gint32 result = -1;
		if (parameters)
			g_variant_get (parameters, "(i)", &result);

		handle_login_result (page, result);
	}
}

static void
greeter_vpn_page_shown (GreeterPage *page)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (page);
	GreeterVPNPagePrivate *priv = self->priv;

	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_label_set_text (GTK_LABEL (priv->message_label), "");

	gtk_widget_set_sensitive (priv->login_button, FALSE);

	greeter_page_set_complete (GREETER_PAGE (page), FALSE);

	g_idle_add ((GSourceFunc) grab_focus_idle, priv->id_entry);

	try_to_logout (self);
}

static gboolean
greeter_vpn_page_should_show (GreeterPage *page)
{

#if 0
	FILE *fp = fopen ("/var/tmp/greeter.debug", "a+");
	fprintf (fp, "vpn page mode = %d\n", greeter_page_manager_get_mode (page->manager));
	fclose (fp);

#endif
	return (greeter_page_manager_get_mode (page->manager) == MODE_ONLINE);
}

static void
greeter_vpn_page_out (GreeterPage *page,
                      gboolean     next)
{
g_print ("greeter_vpn_page_out : %d\n", next);

	if (!next)
		try_to_logout (GREETER_VPN_PAGE (page));
}

static void
login_button_clicked_cb (GtkButton *button,
                         gpointer   user_data)
{
	const char *id, *pw;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	pre_login (page);

	id = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));
	pw = gtk_entry_get_text (GTK_ENTRY (priv->pw_entry));

	if (!try_to_login (page, priv->ip, priv->port, id, pw)) {
		handle_login_result (page, VPN_SERVICE_DAEMON_ERROR);
		return;
	}
}

static void
pw_entry_activate_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);

	if (gtk_widget_get_sensitive (page->priv->login_button))
		login_button_clicked_cb (GTK_BUTTON (page->priv->login_button), page);
}

static gboolean
pw_entry_key_press_cb (GtkWidget   *widget,
                       GdkEventKey *event,
                       gpointer     user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	if ((event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down)) {
		/* Back to id_entry if it is available */
		if (event->keyval == GDK_KEY_Up &&
            gtk_widget_get_visible (priv->id_entry) &&
            widget == priv->pw_entry) {
			gtk_widget_grab_focus (priv->id_entry);
			return TRUE;
		}
		return TRUE;
	}
	return FALSE;
}

static gboolean
id_entry_key_press_cb (GtkWidget   *widget,
                       GdkEventKey *event,
                       gpointer     user_data)
{   
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data); 
	GreeterVPNPagePrivate *priv = page->priv;

	if (event->keyval == GDK_KEY_Up) {
		return pw_entry_key_press_cb (widget, event, user_data);
	} else if (event->keyval == GDK_KEY_Return && gtk_widget_get_visible (priv->pw_entry)) {
		gtk_widget_grab_focus (priv->pw_entry);
		return TRUE;
	}
	return FALSE;
}

static void
id_entry_changed_cb (GtkWidget *widget,
                     gpointer   user_data)
{
	const char *text;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	text = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));

	gtk_widget_set_sensitive (priv->login_button, strlen (text) > 0);
}

static void
setup_page_ui (GreeterVPNPage *page)
{
	GdkPixbuf *pixbuf = NULL;
	GreeterVPNPagePrivate *priv = page->priv;

	pixbuf = gdk_pixbuf_new_from_resource ("/kr/gooroom/greeter/username.png", NULL);
	if (pixbuf) {
		gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (priv->id_entry), GTK_ENTRY_ICON_PRIMARY, pixbuf);
		g_object_unref (pixbuf);
	}

	pixbuf = gdk_pixbuf_new_from_resource ("/kr/gooroom/greeter/password.png", NULL);
	if (pixbuf) {
		gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (priv->pw_entry), GTK_ENTRY_ICON_PRIMARY, pixbuf);
		g_object_unref (pixbuf);
	}

	gtk_image_set_from_resource (GTK_IMAGE (priv->login_button_icon), "/kr/gooroom/greeter/arrow.png");
}

static void
greeter_vpn_page_dispose (GObject *object)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (object);

	g_clear_object (&self->priv->proxy);

	G_OBJECT_CLASS (greeter_vpn_page_parent_class)->dispose (object);
}

static void
greeter_vpn_page_finalize (GObject *object)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (object);

	g_free (self->priv->ip);
	g_free (self->priv->port);

	G_OBJECT_CLASS (greeter_vpn_page_parent_class)->finalize (object);
}

static void
greeter_vpn_page_init (GreeterVPNPage *page)
{
	GreeterVPNPagePrivate *priv;
	page->priv = priv = greeter_vpn_page_get_instance_private (page);

	priv->ip = NULL;
	priv->port = NULL;
	priv->proxy = get_vpn_dbus_proxy ();

	gtk_widget_init_template (GTK_WIDGET (page));

	greeter_page_set_title (GREETER_PAGE (page), _("Connecting VPN"));

	setup_page_ui (page);

	get_vpn_connection_info (&priv->ip, &priv->port);

	g_signal_connect (G_OBJECT (priv->id_entry), "key-press-event",
                      G_CALLBACK (id_entry_key_press_cb), page);
	g_signal_connect (G_OBJECT (priv->id_entry), "changed",
                      G_CALLBACK (id_entry_changed_cb), page);
	g_signal_connect (G_OBJECT (priv->pw_entry), "key-press-event",
                      G_CALLBACK (pw_entry_key_press_cb), page);
	g_signal_connect (G_OBJECT (priv->pw_entry), "activate",
                      G_CALLBACK (pw_entry_activate_cb), page);
	g_signal_connect (G_OBJECT (priv->login_button), "clicked",
                      G_CALLBACK (login_button_clicked_cb), page);

	if (priv->proxy)
		g_signal_connect (G_OBJECT (priv->proxy), "g-signal",
                          G_CALLBACK (vpn_dbus_signal_handler), page);

	gtk_widget_show (GTK_WIDGET (page));
}

static void
greeter_vpn_page_class_init (GreeterVPNPageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-vpn-page.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterVPNPage, id_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterVPNPage, pw_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterVPNPage, message_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterVPNPage, login_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterVPNPage, login_button_icon);

	page_class->page_id = PAGE_ID;
	page_class->out = greeter_vpn_page_out;
	page_class->shown = greeter_vpn_page_shown;
	page_class->should_show = greeter_vpn_page_should_show;

	object_class->dispose= greeter_vpn_page_dispose;
	object_class->finalize = greeter_vpn_page_finalize;
}

GreeterPage *
greeter_prepare_vpn_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_VPN_PAGE,
                         "manager", manager,
                         NULL);
}
