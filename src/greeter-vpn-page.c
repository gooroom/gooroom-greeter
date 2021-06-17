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
#include "greeter-ars-dialog.h"
#include "greeter-message-dialog.h"
#include "greeter-vpn-passwd-dialog.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>


#define VPN_LOGIN_TIMEOUT_SECS 60*3   //3 minutes

#define VPN_SERVICE_NAME      "kr.gooroom.VPN"
#define VPN_SERVICE_PATH      "/kr/gooroom/VPN"
#define VPN_SERVICE_INTERFACE "kr.gooroom.VPN"

#define CHPASSWD_FORCE_FILE   "/var/lib/lightdm/chpasswd-force"

enum {
	VPN_LOGIN_SUCCESS                  = 1001,
	VPN_LOGIN_FAILURE                  = 1002,
	VPN_LOGIN_ALREADY                  = 1003,
	VPN_LOGIN_AUTH_FAILURE             = 1004,
	VPN_LOGIN_PERIOD_EXPIRED           = 1005,
	VPN_LOGIN_TIME_BLOCKED             = 1006,
	VPN_LOGIN_WEEK_BLOCKED             = 1007,
	VPN_LOGIN_PROGRESS_ERROR           = 1008,
	VPN_ACCOUNT_LOCKED                 = 2001,
	VPN_ACCOUNT_ID_EXPIRED             = 2002,
	VPN_ACCOUNT_PW_EXPIRED             = 2003,
	VPN_ACCOUNT_CHPW_REQUEST           = 2004,
	VPN_ACCOUNT_CHPW_SUCCESS           = 2005,
	VPN_ACCOUNT_CHPW_FAILURE           = 2006,
	VPN_SERVER_CONNECTION_ERROR        = 3001,
	VPN_SERVER_RESPONSE_ERROR          = 3002,
	VPN_SERVER_DISCONNECTED            = 3003,
	VPN_EXECUTION_ERROR                = 4001,
	VPN_CONFIGURATION_ERROR            = 4002,
	VPN_AUTH_FAILURE_001               = 5001, // VPNE-5001
	VPN_AUTH_FAILURE_002               = 5002, // VPNE-5002
	VPN_AUTH_FAILURE_003               = 5003, // VPNE-5003
	VPN_AUTH_FAILURE_004               = 5004, // VPNE-5004
	VPN_AUTH_FAILURE_005               = 5005, // VPNE-5005
	VPN_AUTH_FAILURE_006               = 5006, // VPNE-5006
	VPN_AUTH_FAILURE_007               = 5007, // VPNE-5007
	VPN_AUTH_FAILURE_008               = 5008, // VPNE-5008
	VPN_AUTH_FAILURE_009               = 5009, // VPNE-5009
	VPN_AUTH_FAILURE_010               = 5010, // VPNE-5010
	VPN_AUTH_FAILURE_011               = 5011, // VPNE-5011
	VPN_AUTH_FAILURE_012               = 5012, // VPNE-5012
	VPN_AUTH_FAILURE_013               = 5013, // VPNE-5013
	VPN_UNKNOWN_ERROR                  = 9001,
	VPN_SERVICE_INFO_ERROR             = 9101,
	VPN_SERVICE_DAEMON_ERROR           = 9102,
	VPN_SERVICE_TIMEOUT_ERROR          = 9103,
	VPN_SERVICE_LOGIN_REQUEST_ERROR    = 9104,
	VPN_SERVICE_USERINFO_REQUEST_ERROR = 9105
};


static gboolean try_to_logout (GreeterVPNPage *page);
static gboolean show_password_change_dialog (gpointer user_data);
static void login_button_clicked_cb (GtkButton *button, gpointer user_data);


struct _GreeterVPNPagePrivate {
	GtkWidget  *id_entry;
	GtkWidget  *pw_entry;
	GtkWidget  *message_label;
	GtkWidget  *login_button;
	GtkWidget  *login_button_icon;
	GtkWidget  *ars_dialog;

	guint       dbus_watch_id;
	guint       dbus_signal_id;
	guint       login_timeout_id;
	guint       check_vpn_service_timeout_id;

	gboolean    login_success;
	gboolean    about_to_login;
	gboolean    service_enabled;

	gchar      *id;
	gchar      *pw;
	gchar      *ip;
	gchar      *port;
	gchar      *phone_num;
	gchar      *auth_num;

	GDBusProxy *proxy;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterVPNPage, greeter_vpn_page, GREETER_TYPE_PAGE);




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

	// 60 secs
	g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (proxy), VPN_LOGIN_TIMEOUT_SECS * 1000);

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
	GtkWidget *dialog;

	dialog = greeter_message_dialog_new (GREETER_PAGE (page)->parent, icon, title, message);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("Ok"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

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

		case VPN_LOGIN_AUTH_FAILURE:
			message = _("User authentication failed.\n"
                        "If authentication fails more than 5 times, "
                        "you can no longer log in.");
		break;

		case VPN_LOGIN_ALREADY:
			message = _("You are already logged in.\n"
                        "Log out of the other device and try again.\n"
                        "If the problem persists, please contact your administrator.");
		break;

		case VPN_LOGIN_TIME_BLOCKED:
			message = _("It is not the VPN connection time.\n"
                        "Please contact the administrator to check the access time and try again.");
		break;

		case VPN_LOGIN_WEEK_BLOCKED:
			message = _("It is not the day of the week for VPN access.\n"
                        "Please contact the administrator to check the access time and try again.");
		break;

		case VPN_LOGIN_PERIOD_EXPIRED:
			message = _("The VPN connection cannot proceed because the login period has been exceeded. "
                        "Please try again later or contact the administrator.");
		break;

		case VPN_ACCOUNT_LOCKED:
			message = _("Login is not possible because user "
                        "authentication has failed more than 5 times. "
                        "Please contact the administrator.");
		break;

		case VPN_ACCOUNT_ID_EXPIRED:
			message = _("The VPN connection cannot be proceeded due to the ID usage period exceeded. "
                        "Please try again later or contact the administrator.");
		break;

		case VPN_ACCOUNT_PW_EXPIRED:
			message = _("VPN connection cannot be proceeded because the password has expired. "
                        "Please try again later or contact the administrator.");
		break;

		case VPN_SERVER_CONNECTION_ERROR:
			message = _("The server cannot be reached due to network problems.\n"
                        "Please check the network status and try again.");
		break;

		case VPN_SERVER_RESPONSE_ERROR:
			message = _("VPN connection cannot proceed due to a VPN server response error. "
                        "Please try again later or contact the administrator.");
		break;

		case VPN_SERVER_DISCONNECTED:
			message = _("The connection to the server has been lost.\n"
                        "Check the network status and connect again.");
		break;

		case VPN_EXECUTION_ERROR:
			message = _("VPN connection cannot proceed due to VPN execution failure. "
                        "Please try again later or contact the administrator.");
		break;

		case VPN_CONFIGURATION_ERROR:
			message = _("VPN connection cannot proceed due to a VPN setting error. "
                        "Please try again later or contact the administrator.");
		break;

		case VPN_SERVICE_DAEMON_ERROR:
			message = _("An error has occurred in the system's VPN service.\n"
                        "Reboot your system and try again.");
		break;

		case VPN_SERVICE_TIMEOUT_ERROR:
			message = _("VPN login request failed.\nThere is no reponse from server.");
		break;

		case VPN_SERVICE_INFO_ERROR:
			message = _("VPN connection information is incorrect.\n"
                        "Check the configuration file and try again.");
		break;

		case VPN_SERVICE_LOGIN_REQUEST_ERROR:
			message = _("VPN login request failed.\nReboot your system and try again.");
		break;

		case VPN_SERVICE_USERINFO_REQUEST_ERROR:
			message = _("Failed to get user information from the VPN server.\n"
                        "Please try again.");
		break;

		default:
		{
			gchar *msg = NULL;
			msg = g_strdup_printf ("%s [%s: VPNE-%d]\n%s",
                                   _("Authentication Failure"),
                                   _("Error Code"),
                                   result,
                                   _("Please try again later or contact the administrator."));
			gtk_label_set_text (GTK_LABEL (label), msg);
			g_free (msg);
		}
		return;
	}

	if (message)
		gtk_label_set_text (GTK_LABEL (label), message);
}

static void
post_login (GreeterVPNPage *page)
{
	GreeterVPNPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

    g_clear_handle_id (&priv->login_timeout_id, g_source_remove);
	priv->login_timeout_id = 0;

	if (priv->ars_dialog) {
		gtk_widget_destroy (priv->ars_dialog);
		priv->ars_dialog = NULL;
	}

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
		case VPN_ACCOUNT_CHPW_REQUEST:
			g_idle_add ((GSourceFunc)show_password_change_dialog, page);
		break;

		// 비밀번호 변경 성공
		case VPN_ACCOUNT_CHPW_SUCCESS:
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
		case VPN_ACCOUNT_CHPW_FAILURE:
		{
			show_message_dialog (page,
                                 "dialog-warning-symbolic.symbolic",
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
	message = _("Authentication is in progress.\nPlease wait.");

	greeter_page_manager_show_splash (manager, toplevel, message);

	g_signal_handlers_block_by_func (priv->login_button,
                                     login_button_clicked_cb, page);

	gtk_label_set_text (GTK_LABEL (priv->message_label), "");

	priv->login_timeout_id = g_timeout_add (VPN_LOGIN_TIMEOUT_SECS * 1000,
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
		handle_result (page, VPN_ACCOUNT_CHPW_FAILURE);
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

static void
try_to_get_userinfo_done_cb (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
	gint32 result = -1;
	GVariant *variant;
	GError *error = NULL;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);

	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (error) {
		g_warning ("Failed to get userinfo: %s", error->message);
		g_error_free (error);
		handle_result (page, VPN_SERVICE_USERINFO_REQUEST_ERROR);
		return;
	}

	// 사용자 정보(전화번호) 획득은 등록한 시그널 핸들러(vpn_dbus_signal_handler)에서 처리됨.
	if (variant) {
		g_variant_get (variant, "(i)", &result);
		g_variant_unref (variant);
	}

	if (result == 0) {
		g_debug ("succeeded to get userinfo");
	} else {
		g_debug ("Failed to get userinfo");
		handle_result (page, VPN_SERVICE_USERINFO_REQUEST_ERROR);
	}
}

static gboolean
try_to_logout (GreeterVPNPage *page)
{
	GtkWidget *toplevel;
	const char *message;
	GreeterVPNPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	message = _("Initializing Settings for VPN.\nPlease wait.");

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

static gboolean
try_to_login (GreeterVPNPage *page,
               const gchar    *ip,
               const gchar    *port,
               const gchar    *id,
               const gchar    *pw,
               const gchar    *auth_num)
{
	GreeterVPNPagePrivate *priv = page->priv;

	if (!priv->proxy)
		priv->proxy = get_vpn_dbus_proxy ();

	if (priv->proxy) {
		g_dbus_proxy_call (priv->proxy,
                           "Login",
                           g_variant_new ("(sssss)", ip, port, id, pw, auth_num),
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
try_to_get_userinfo (GreeterVPNPage *page,
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
                           "Userinfo",
                           g_variant_new ("(ssss)", ip, port, id, pw),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           try_to_get_userinfo_done_cb,
                           page);

		return TRUE;
	}

	return FALSE;
}

static gboolean
show_password_change_dialog (gpointer user_data)
{
	gint res;
	GtkWidget *dialog;
	gchar *new_pw = NULL;
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	dialog = greeter_vpn_passwd_dialog_new (GREETER_PAGE (page)->parent, priv->pw);
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	new_pw = g_strdup (greeter_vpn_passwd_dialog_get_new_password (GREETER_VPN_PASSWD_DIALOG (dialog)));
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (res != GTK_RESPONSE_OK) {
		show_message_dialog (page,
                             "dialog-warning-symbolic.symbolic",
                             _("Cancelling VPN Password Change"),
                             _("The password change has been cancelled.\n"
                               "Please be sure to change your password for VPN access."));

		if (g_file_test (CHPASSWD_FORCE_FILE, G_FILE_TEST_EXISTS))
			try_to_logout (page);

		gtk_widget_grab_focus (priv->id_entry);
	} else {
		if (new_pw) {
			if (!try_to_chpasswd (page, priv->ip, priv->port, priv->id, priv->pw, new_pw))
				handle_result (page, VPN_ACCOUNT_CHPW_FAILURE);
		}
	}

	g_clear_pointer (&new_pw, (GDestroyNotify) g_free);

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
	GreeterVPNPagePrivate *priv = page->priv;

	if (g_str_equal (signal_name, "ConnectionStateChanged")) {
		gint32 result = -1;
		if (parameters)
			g_variant_get (parameters, "(i)", &result);

		handle_result (page, result);
	}

	if (g_str_equal (signal_name, "UserinfoObtained")) {
		if (priv->about_to_login) {
			g_clear_pointer (&priv->phone_num, (GDestroyNotify) g_free);
			g_clear_pointer (&priv->auth_num, (GDestroyNotify) g_free);

			if (parameters)
				g_variant_get (parameters, "(ss)", &priv->phone_num, &priv->auth_num);

			if (!priv->phone_num || !priv->auth_num) {
				post_login (page);

				gtk_label_set_text (GTK_LABEL (priv->message_label),
                                _("Failed to get user information from the VPN server.\n"
                                  "Please try again."));

				gtk_widget_grab_focus (priv->id_entry);
			} else {
				priv->ars_dialog = greeter_ars_dialog_new (GREETER_PAGE (page)->parent,
                                                           priv->phone_num,
                                                           priv->auth_num);
				gtk_widget_show (priv->ars_dialog);

				if (!try_to_login (page, priv->ip, priv->port, priv->id, priv->pw, priv->auth_num))
					handle_result (page, VPN_SERVICE_DAEMON_ERROR);
			}
			priv->about_to_login = FALSE;
		}
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
	if (mode == MODE_ONLINE && priv->login_success && !priv->service_enabled) {
		show_message_dialog (self,
                             "dialog-warning-symbolic.symbolic",
                             _("VPN Service Error"),
                             _("VPN service is down.\nPlease enable VPN service and try again."));
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

	if (priv->check_vpn_service_timeout_id > 0) {
		g_source_remove (priv->check_vpn_service_timeout_id);
		priv->check_vpn_service_timeout_id  = 0;
	}

	priv->service_enabled = TRUE;

	priv->proxy = get_vpn_dbus_proxy ();

	if (!priv->dbus_signal_id)
		priv->dbus_signal_id = g_signal_connect (G_OBJECT (priv->proxy), "g-signal",
                                                 G_CALLBACK (vpn_dbus_signal_handler), page);
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
		priv->check_vpn_service_timeout_id = g_timeout_add (3000, check_vpn_service, page);
}

static void
greeter_vpn_page_shown (GreeterPage *page)
{
	GreeterVPNPage *self = GREETER_VPN_PAGE (page);
	GreeterVPNPagePrivate *priv = self->priv;

	priv->login_success = FALSE;
	priv->about_to_login = FALSE;

	if (!priv->service_enabled) {
		show_message_dialog (self,
                             "dialog-warning-symbolic.symbolic",
                             _("VPN Service Error"),
                             _("VPN service is down.\nPlease enable VPN service and try again."));
		greeter_page_manager_go_first (GREETER_PAGE (page)->manager);
		return;
	}

	if (!priv->ip || !priv->port) {
		show_message_dialog (self,
                             "dialog-warning-symbolic.symbolic",
                             _("VPN Service Error"),
			                 _("VPN connection information is incorrect.\n"
                               "Check the configuration file and try again."));
		greeter_page_manager_go_first (GREETER_PAGE (page)->manager);
		return;
	}

	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_label_set_text (GTK_LABEL (priv->message_label), "");

	gtk_widget_set_sensitive (priv->login_button, FALSE);

	greeter_page_set_complete (GREETER_PAGE (page), FALSE);

	gtk_widget_grab_focus (GTK_WIDGET (priv->id_entry));
}

static gboolean
greeter_vpn_page_should_show (GreeterPage *page)
{
	return (greeter_page_manager_get_mode (page->manager) == MODE_ONLINE);
}

//static void
//greeter_vpn_page_out (GreeterPage *page,
//                      gboolean     next)
//{
//	if (!next)
//		g_idle_add ((GSourceFunc)reload_vpn_service, page);
//}

static void
login_button_clicked_cb (GtkButton *button,
                         gpointer   user_data)
{
	GreeterVPNPage *page = GREETER_VPN_PAGE (user_data);
	GreeterVPNPagePrivate *priv = page->priv;

	priv->login_success = FALSE;
	priv->about_to_login = TRUE;

	pre_login (page);

	g_clear_pointer (&priv->id, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->pw, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->phone_num, (GDestroyNotify) g_free);
	g_clear_pointer (&priv->auth_num, (GDestroyNotify) g_free);

	priv->id = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->id_entry)));
	priv->pw = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)));

	if (!try_to_get_userinfo (page, priv->ip, priv->port, priv->id, priv->pw)) {
		handle_result (page, VPN_SERVICE_DAEMON_ERROR);
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
	PangoAttrList *attrs;
	PangoAttribute *attr;
	GdkPixbuf *pixbuf = NULL;
	GreeterVPNPagePrivate *priv = page->priv;

	attrs = pango_attr_list_new ();
	attr = pango_attr_rise_new (8000);
	pango_attr_list_insert (attrs, attr);
	gtk_label_set_attributes (GTK_LABEL (priv->message_label), attrs);

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

    g_clear_handle_id (&priv->login_timeout_id, g_source_remove);
	priv->login_timeout_id = 0;

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
	g_clear_pointer (&priv->phone_num, (GDestroyNotify) g_free);

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
	priv->phone_num = NULL;
	priv->auth_num = NULL;
	priv->dbus_watch_id = 0;
	priv->dbus_signal_id = 0;
	priv->login_timeout_id = 0;
	priv->check_vpn_service_timeout_id = 0;
	priv->service_enabled = FALSE;;
	priv->ars_dialog = NULL;

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
//	page_class->out = greeter_vpn_page_out;
	page_class->shown = greeter_vpn_page_shown;
	page_class->should_show = greeter_vpn_page_should_show;

	object_class->dispose= greeter_vpn_page_dispose;
	object_class->finalize = greeter_vpn_page_finalize;
}

GreeterPage *
greeter_prepare_vpn_page (GreeterPageManager *manager,
                          GtkWidget          *parent)
{
	return g_object_new (GREETER_TYPE_VPN_PAGE,
                         "manager", manager,
                         "parent", parent,
                         NULL);
}
