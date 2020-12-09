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
#include "greeter-vpn-passwd-dialog.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>


#define VPN_LOGIN_TIMEOUT_SECS 60

#define VPN_SERVICE_NAME      "kr.gooroom.VPN"
#define VPN_SERVICE_PATH      "/kr/gooroom/VPN"
#define VPN_SERVICE_INTERFACE "kr.gooroom.VPN"

#define CHPASSWD_FORCE_FILE   "/var/lib/lightdm/chpasswd-force"

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
    VPN_LOGIN_REQUEST_CHPASSWD      = 1010,
	VPN_LOGIN_CHPASSWD_SUCCESS      = 1011,
	VPN_LOGIN_CHPASSWD_FAILURE      = 1012,
	VPN_SERVER_CONNECTION_ERROR     = 1013,
	VPN_SERVER_RESPONSE_ERROR       = 1014,
	VPN_SERVER_DISCONNECTED         = 1015,
	VPN_UNKNOWN_ERROR               = 1016,
	VPN_SERVICE_DAEMON_ERROR        = 1017,
	VPN_SERVICE_LOGIN_REQUEST_ERROR = 1018,
	VPN_SERVICE_INFO_ERROR          = 1019,
	VPN_SERVICE_TIMEOUT_ERROR       = 1020
};


static gboolean try_to_logout (GreeterVPNPage *page);
static void login_button_clicked_cb (GtkButton *button, gpointer user_data);
static gboolean show_password_change_dialog (gpointer user_data);


struct _GreeterVPNPagePrivate {
	GtkWidget  *id_entry;
	GtkWidget  *pw_entry;
	GtkWidget  *message_label;
	GtkWidget  *login_button;
	GtkWidget  *login_button_icon;

	guint       dbus_watch_id;
	guint       dbus_signal_id;
	guint       splash_timeout_id;
	guint       check_vpn_service_timeout_id;

	gboolean    login_success;
	gboolean    service_enabled;

	gchar      *id;
	gchar      *pw;
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

	config_path = g_strdup_printf ("%s/%s.conf", VPN_SERVICE_CONFIG_DIR, active_vpn);

	if (!g_file_test (config_path, G_FILE_TEST_EXISTS)) {
		g_warning (G_STRLOC ": No VPN config file: %s", config_path);
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
show_message_dialog (GreeterVPNPage *page,
                     const gchar    *icon,
                     const gchar    *title,
                     const gchar    *message)
{
	GtkWidget *dialog, *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

	dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel), icon, title, message);

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

		case VPN_SERVICE_DAEMON_ERROR:
			message = _("An error has occurred in the system's VPN service.\n"
                        "Reboot your system and try again.");
		break;

		case VPN_SERVICE_LOGIN_REQUEST_ERROR:
			message = _("VPN login request failed.\nReboot your system and try again.");
		break;

		case VPN_SERVICE_TIMEOUT_ERROR:
			message = _("VPN login request failed.\nThere is no reponse from server.");
		break;

		case VPN_SERVICE_INFO_ERROR:
			message = _("VPN connection information is incorrect.\n"
                        "Check the configuration file and try again.");
		break;

		case VPN_AUTH_FAILURE:
			message = _("User authentication failed.\n"
                        "If authentication fails more than 5 times, "
                        "you can no longer log in.");
		break;

		case VPN_ACCOUNT_LOCKED:
			message = _("Login is not possible because user "
                        "authentication has failed more than 5 times.\n"
                        "Please contact the administrator.");
		break;

		case VPN_SERVER_CONNECTION_ERROR:
			message = _("The server cannot be reached due to network problems.\n"
                        "Please check the network status and try again.");
		break;

		case VPN_SERVER_DISCONNECTED:
			message = _("The connection to the server has been lost. "
                        "Check the network status and connect again.");
		break;

		default:
			message = _("Login failed due to an unknown error.\n"
                        "Please try again later or contact the administrator.");
		break;
	}

	if (message)
		gtk_label_set_text (GTK_LABEL (label), message);
}

static void
post_login (GreeterVPNPage *page)
{
	GreeterVPNPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

    g_clear_handle_id (&priv->splash_timeout_id, g_source_remove);
	priv->splash_timeout_id = 0;

	greeter_page_manager_hide_splash (manager);

	g_signal_handlers_unblock_by_func (page->priv->login_button,
                                       login_button_clicked_cb, page);

	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);

	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
}

static void
handle_result (GreeterVPNPage *page,
               int             result)
{
	GreeterVPNPagePrivate *priv = page->priv;

	// 0은 성공 or 실패(에러)코드가 아닌 상태를 알리기위한 코드
	if (result == 0)
		return;

	priv->login_success = FALSE;

	post_login (page);

	switch (result)
	{
		case VPN_LOGIN_SUCCESS:
		{
			// 패스워드 변경창 강제로 팝업
			if (g_file_test (CHPASSWD_FORCE_FILE, G_FILE_TEST_EXISTS)) {
				g_idle_add ((GSourceFunc)show_password_change_dialog, page);
				break;
			}

			priv->login_success = TRUE;
			show_message_dialog (page,
                             "dialog-information-symbolic",
                             _("VPN Login Success"),
                             _("You have successfully login to the VPN.\n"
                               "Click the 'OK' button and proceed with system login."));
			greeter_page_set_complete (GREETER_PAGE (page), TRUE);
			greeter_page_manager_go_next (GREETER_PAGE (page)->manager);
		}
		break;

		// 최초 접속 시 비밀번호 변경 요구
		case VPN_LOGIN_REQUEST_CHPASSWD:
			g_idle_add ((GSourceFunc)show_password_change_dialog, page);
		break;

		// 비밀번호 변경 성공
		case VPN_LOGIN_CHPASSWD_SUCCESS:
		{
			show_message_dialog (page,
                                 "dialog-information-symbolic",
                                 _("Success Of Changing VPN Password"),
                                 _("You have completed the password change.\n"
                                   "Please login again after clicking the 'OK' button."));

			if (g_file_test (CHPASSWD_FORCE_FILE, G_FILE_TEST_EXISTS)) {
				// 패스워드 변경 완료 시 패스워드 변경창 강제 팝업용 파일 삭제
				g_remove (CHPASSWD_FORCE_FILE);

				try_to_logout (page);
			}

			gtk_widget_grab_focus (priv->id_entry);
		}
		break;

		// 비밀번호 변경 실패
		case VPN_LOGIN_CHPASSWD_FAILURE:
		{
			show_message_dialog (page,
                             "dialog-information-symbolic",
                             _("Failure Of Changing VPN Password"),
                             _("Password change failed.\n"
                               "You must change your password the first time you connect to the VPN."));
			gtk_widget_grab_focus (priv->id_entry);
		}
		break;

		case VPN_SERVER_DISCONNECTED:
			// TODO: 연결이후 접속이 끊어진 경우 팝업 메세지 처리
		break;

		default:
			update_error_message_label (priv->message_label, result);
			gtk_widget_grab_focus (priv->id_entry);
		break;
	}
}

static gboolean
start_timeout_cb (gpointer user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);

	handle_result (page, VPN_SERVICE_TIMEOUT_ERROR);

	return FALSE;
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

	gtk_label_set_text (GTK_LABEL (priv->message_label), "");

	priv->splash_timeout_id = g_timeout_add (VPN_LOGIN_TIMEOUT_SECS * 1000,
                                             start_timeout_cb, page);
}

static void
try_to_chpasswd_done_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
	gint32 result = -1;
	GVariant *variant;
	GError *error = NULL;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);

	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (error) {
		g_warning ("Failed to chpasswd VPN: %s", error->message);
		g_error_free (error);
		handle_result (page, VPN_LOGIN_CHPASSWD_FAILURE);
		return;
	}

	if (variant) {
		g_variant_get (variant, "(i)", &result);
		g_variant_unref (variant);
	}

	if (result == 0) {
		g_debug ("VPN Password Changing Success");
	} else {
		g_debug ("VPN Password Changing Failure");
	}
}

static gboolean
try_to_chpasswd (GreeterVPNPage *page,
                 const gchar    *ip,
                 const gchar    *port,
                 const gchar    *id,
                 const gchar    *old_pw,
                 const gchar    *new_pw)
{
	GreeterVPNPagePrivate *priv = page->priv;

	if (!priv->proxy)
		priv->proxy = get_vpn_dbus_proxy ();

	if (priv->proxy) {
		g_dbus_proxy_call (priv->proxy,
                           "Chpasswd",
                           g_variant_new ("(sssss)", ip, port, id, old_pw, new_pw),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           try_to_chpasswd_done_cb,
                           page);

		return TRUE;
	}

	return FALSE;
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
		handle_result (page, VPN_SERVICE_LOGIN_REQUEST_ERROR);
		return;
	}

	// 로그인시도 후 결과는 0 (성공) or -1(실패)
	// 성공의 의미는 로그인의 성공을 의미하는 것이 아니라 로그인시도의 성공을 의미함
	// 로그인시도 성공 후 로그인의 성공여부는 등록한 시그널 (vpn_dbus_signal_handler)에서 처리됨.
	if (variant) {
		g_variant_get (variant, "(i)", &result);
		g_variant_unref (variant);
	}

	if (result == 0) {
		g_debug ("Successful VPN login attempts");
	} else {
		g_debug ("Failed to login VPN");
	}
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

static gboolean
show_password_change_dialog (gpointer user_data)
{
	int res;
	gchar *new_pw = NULL;
	GreeterVPNPasswdDialog *dialog;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	dialog = greeter_vpn_passwd_dialog_new (priv->pw);
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	new_pw = g_strdup (greeter_vpn_passwd_dialog_get_new_password (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (res != GTK_RESPONSE_OK) {
		show_message_dialog (page,
                             "dialog-information-symbolic",
                             _("Cancelling VPN Password Change"),
                             _("The password change has been cancelled.\n"
                               "Please be sure to change your password for VPN access."));

		if (g_file_test (CHPASSWD_FORCE_FILE, G_FILE_TEST_EXISTS))
			try_to_logout (page);

		gtk_widget_grab_focus (priv->id_entry);
		goto fail;
	}

	if (new_pw) {
		if (!try_to_chpasswd (page, priv->ip, priv->port, priv->id, priv->pw, new_pw))
			handle_result (page, VPN_LOGIN_CHPASSWD_FAILURE);
	}

fail:
	g_free (new_pw);

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

		handle_result (page, result);
	}
}

static gboolean
check_vpn_service (gpointer user_data)
{
	int mode;
	GreeterVPNPage *self = GREETER_VPN_PAGE (user_data);
	GreeterPage *page = GREETER_PAGE (self);
	GreeterVPNPagePrivate *priv = self->priv;

	mode = greeter_page_manager_get_mode (page->manager);
	if (mode == MODE_ONLINE && priv->login_success) {
		show_message_dialog (self,
                             "dialog-information-symbolic",
                             _("VPN Service Error"),
                             _("VPN service is down. Please enable VPN service and try again."));
		greeter_page_manager_go_first (page->manager);
	}

	if (priv->check_vpn_service_timeout_id > 0) {
		g_source_remove (priv->check_vpn_service_timeout_id);
		priv->check_vpn_service_timeout_id  = 0;
	}

	return FALSE;
}

static void
name_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	priv->service_enabled = TRUE;

	priv->proxy = get_vpn_dbus_proxy ();

	if (!priv->dbus_signal_id)
		priv->dbus_signal_id = g_signal_connect (G_OBJECT (priv->proxy), "g-signal",
                                                 G_CALLBACK (vpn_dbus_signal_handler), page);

	try_to_logout (page);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	priv->service_enabled = FALSE;

	if (!priv->check_vpn_service_timeout_id)
		priv->check_vpn_service_timeout_id = g_timeout_add (1000, check_vpn_service, page);
}

static void
greeter_vpn_page_shown (GreeterPage *page)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (page);
	GreeterVPNPagePrivate *priv = self->priv;

	priv->login_success = FALSE;

	if (!priv->service_enabled) {
		show_message_dialog (self,
                             "dialog-information-symbolic",
                             _("VPN Service Error"),
                             _("VPN service is down. Please enable VPN service and try again."));
		greeter_page_manager_go_first (GREETER_PAGE (page)->manager);
		return;
	}

	if (!priv->ip || !priv->port) {
		show_message_dialog (self,
                             "dialog-information-symbolic",
                             _("VPN Service Error"),
			                 _("VPN connection information is incorrect.\n"
                               "Check the configuration file and try again."));
		greeter_page_manager_go_first (GREETER_PAGE (page)->manager);
		return;
	}

	try_to_logout (self);

	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_label_set_text (GTK_LABEL (priv->message_label), "");

	gtk_widget_set_sensitive (priv->login_button, FALSE);

	greeter_page_set_complete (GREETER_PAGE (page), FALSE);

	g_idle_add ((GSourceFunc) grab_focus_idle, priv->id_entry);
}

static gboolean
greeter_vpn_page_should_show (GreeterPage *page)
{
	return (greeter_page_manager_get_mode (page->manager) == MODE_ONLINE);
}

static void
greeter_vpn_page_out (GreeterPage *page,
                      gboolean     next)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (page);

	if (!next)
		try_to_logout (self);
}

static void
login_button_clicked_cb (GtkButton *button,
                         gpointer   user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	pre_login (page);

	priv->login_success = FALSE;

	g_clear_pointer (&priv->id, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->pw, (GDestroyNotify) g_free);

	priv->id = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->id_entry)));
	priv->pw = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)));

	if (!try_to_login (page, priv->ip, priv->port, priv->id, priv->pw))
		handle_result (page, VPN_SERVICE_DAEMON_ERROR);
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
id_entry_key_press_cb (GtkWidget   *widget,
                       GdkEventKey *event,
                       gpointer     user_data)
{   
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data); 
	GreeterVPNPagePrivate *priv = page->priv;

	/* Enter activates the password entry */
	if ((event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_Tab) &&
         gtk_widget_get_visible (priv->pw_entry))
	{
		gtk_widget_grab_focus (priv->pw_entry);
		return TRUE;
	}
	else
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
	GreeterVPNPagePrivate *priv = self->priv;

	g_signal_handler_disconnect (priv->proxy, priv->dbus_signal_id);

	if (priv->dbus_watch_id) {
		g_bus_unwatch_name (priv->dbus_watch_id);
		priv->dbus_watch_id = 0;
	}

    g_clear_handle_id (&priv->check_vpn_service_timeout_id, g_source_remove);
	priv->check_vpn_service_timeout_id  = 0;

    g_clear_handle_id (&priv->splash_timeout_id, g_source_remove);
	priv->splash_timeout_id = 0;

	g_clear_object (&priv->proxy);

	G_OBJECT_CLASS (greeter_vpn_page_parent_class)->dispose (object);
}

static void
greeter_vpn_page_finalize (GObject *object)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (object);
	GreeterVPNPagePrivate *priv = self->priv;

	g_clear_pointer (&priv->ip, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->port, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->id, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->pw, (GDestroyNotify) g_free);

	G_OBJECT_CLASS (greeter_vpn_page_parent_class)->finalize (object);
}

static void
greeter_vpn_page_init (GreeterVPNPage *page)
{
	GreeterVPNPagePrivate *priv;
	page->priv = priv = greeter_vpn_page_get_instance_private (page);

	priv->ip = NULL;
	priv->port = NULL;
	priv->id = NULL;
	priv->pw = NULL;
	priv->dbus_watch_id = 0;
	priv->dbus_signal_id = 0;
	priv->splash_timeout_id = 0;
	priv->check_vpn_service_timeout_id = 0;
	priv->service_enabled = FALSE;;

	get_vpn_connection_info (&priv->ip, &priv->port);

	priv->dbus_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                            VPN_SERVICE_NAME,
                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                            name_appeared_cb,
                                            name_vanished_cb,
                                            page, NULL);

	gtk_widget_init_template (GTK_WIDGET (page));

	greeter_page_set_title (GREETER_PAGE (page), _("Connecting VPN"));

	setup_page_ui (page);

	g_signal_connect (G_OBJECT (priv->id_entry), "key-press-event",
                      G_CALLBACK (id_entry_key_press_cb), page);
	g_signal_connect (G_OBJECT (priv->id_entry), "changed",
                      G_CALLBACK (id_entry_changed_cb), page);
	g_signal_connect (G_OBJECT (priv->pw_entry), "activate",
                      G_CALLBACK (pw_entry_activate_cb), page);
	g_signal_connect (G_OBJECT (priv->login_button), "clicked",
                      G_CALLBACK (login_button_clicked_cb), page);

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
