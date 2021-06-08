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
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <pwd.h>
#include <ctype.h>

#include <lightdm.h>

#include "greeter-login-page.h"
#include "greeterconfiguration.h"
#include "greeter-message-dialog.h"
#include "greeter-password-settings-dialog.h"

#define PAGE_ID "STEP 1"

#define VPN_LOGIN_TIMEOUT_SECS 60

#define VPN_SERVICE_NAME      "kr.gooroom.VPN"
#define VPN_SERVICE_PATH      "/kr/gooroom/VPN"
#define VPN_SERVICE_INTERFACE "kr.gooroom.VPN"

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
//	VPN_ACCOUNT_CHPW_REQUEST           = 2004,
//	VPN_ACCOUNT_CHPW_SUCCESS           = 2005,
//	VPN_ACCOUNT_CHPW_FAILURE           = 2006,
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
//  VPN_SERVICE_USERINFO_REQUEST_ERROR = 9105
};

typedef struct
{
    gboolean is_prompt;
    union
    {
        LightDMMessageType message;
        LightDMPromptType prompt;
    } type;
    gchar *text;
} PAMConversationMessage;

struct _GreeterLoginPagePrivate {
	GtkWidget *id_entry;
	GtkWidget *pw_entry;
	GtkWidget *login_button;
	GtkWidget *mode_switch_button;
	GtkWidget *remember_id_checkbutton;
	GtkWidget *network_settings_button;

	GtkWidget *pw_dialog;

	gboolean prompted;
	gboolean prompt_active;
	gboolean have_pam_error;
	gboolean changing_password;

	gchar *id;
	gchar *pw;
	gchar *current_session;
	gchar *current_language;
	gchar *internal_last_user;
	gchar *external_last_user;

	/* Pending questions */
	GSList *pending_questions;

	LightDMGreeter *greeter;

// for vpn
	guint  vpn_dbus_watch_id;
	guint  vpn_dbus_signal_id;
	guint  splash_timeout_id;
	gint changing_password_step;

	gboolean  vpn_service_enabled;

    GDBusProxy *vpn_dbus_proxy;
};


static void process_prompts      (GreeterLoginPage *page);
static void start_authentication (GreeterLoginPage *page, const gchar *username);
static void login_button_clicked_cb (GtkButton *widget, gpointer user_data);
static void handle_vpn_login_result (GreeterLoginPage *page, int result);
static void try_to_login_system (GreeterLoginPage *page);


G_DEFINE_TYPE_WITH_PRIVATE (GreeterLoginPage, greeter_login_page, GREETER_TYPE_PAGE);



static gboolean
grab_focus_idle (gpointer user_data)
{
	gtk_widget_grab_focus (GTK_WIDGET (user_data));

	return FALSE;
}

static void
add_style_class (GreeterLoginPage *page, const gchar *classname)
{
	GtkStyleContext *style, *style_id_entry, *style_pw_entry;
	GreeterLoginPagePrivate *priv = page->priv;

	style = gtk_widget_get_style_context (GTK_WIDGET (priv->login_button));
	style_id_entry = gtk_widget_get_style_context (GTK_WIDGET (priv->id_entry));
	style_pw_entry = gtk_widget_get_style_context (GTK_WIDGET (priv->pw_entry));

	gtk_style_context_add_class (style, classname);
	gtk_style_context_add_class (style_id_entry, classname);
	gtk_style_context_add_class (style_pw_entry, classname);
}

static void
remove_style_class (GreeterLoginPage *page, const gchar *classname)
{
	GtkStyleContext *style, *style_id_entry, *style_pw_entry;
	GreeterLoginPagePrivate *priv = page->priv;

	style = gtk_widget_get_style_context (GTK_WIDGET (priv->login_button));
	style_id_entry = gtk_widget_get_style_context (GTK_WIDGET (priv->id_entry));
	style_pw_entry = gtk_widget_get_style_context (GTK_WIDGET (priv->pw_entry));

	gtk_style_context_remove_class (style, classname);
	gtk_style_context_remove_class (style_id_entry, classname);
	gtk_style_context_remove_class (style_pw_entry, classname);
}

static gboolean
check_networking (void)
{
	gboolean check_networking = FALSE;

	if (g_file_test (KEPCO_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
		GKeyFile *keyfile = g_key_file_new ();
		g_key_file_load_from_file (keyfile, KEPCO_CONFIG_FILE, G_KEY_FILE_NONE, NULL);
		check_networking = g_key_file_get_boolean (keyfile, "Settings", "CHECK-NETWORK", NULL);
		g_key_file_unref (keyfile);
	}

	return check_networking;
}

static gchar *
get_id (GtkWidget *id_entry)
{
	int i = 0;
	const gchar *text;

	text = gtk_entry_get_text (GTK_ENTRY (id_entry));
	if (strlen (text) == 0)
		return g_strdup ("");

	for (i = 0; text[i] != '\0'; i++)
		if (!isdigit (text[i]))
			return g_strdup (text);

	return g_strdup_printf ("kepco-%s", text); 
}

static gchar *
get_activated_vpn_service (void)
{
	GKeyFile *keyfile;
	gchar *active_vpn = NULL;

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
vpn_service_reload_done_cb (GPid pid, gint status, gpointer user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

    g_spawn_close_pid (pid);

	greeter_page_manager_hide_splash (manager);
	greeter_page_manager_set_is_vpn_logined (manager, FALSE);

	gtk_widget_set_sensitive (page->priv->mode_switch_button, TRUE);
}

static gboolean
reload_vpn_service (gpointer user_data)
{
    GPid pid;
    gchar **argv;
    const gchar *cmd;
	GtkWidget *toplevel;
	const gchar *message;
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	gtk_widget_set_sensitive (page->priv->mode_switch_button, FALSE);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	message = _("Initializing Settings for VPN.\nPlease wait.");

	greeter_page_manager_show_splash (manager, toplevel, message, NULL);

	cmd = "/bin/systemctl restart gooroom-vpn-daemon.service";

	g_shell_parse_argv (cmd, NULL, &argv, NULL);

	if (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL)) {
		g_child_watch_add (pid, (GChildWatchFunc) vpn_service_reload_done_cb, page);
	} else {
		gtk_widget_set_sensitive (page->priv->mode_switch_button, TRUE);
	}

	g_strfreev (argv);

	return FALSE;
}

static void
password_settings_dialog_response_cb (GtkDialog *dialog,
                                      gint       response,
                                      gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	if (response == GTK_RESPONSE_OK) {
		priv->prompt_active = FALSE;

		if (lightdm_greeter_get_in_authentication (priv->greeter)) {
			const gchar *entry_text = greeter_password_settings_dialog_get_entry_text (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog));
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
			lightdm_greeter_respond (priv->greeter, entry_text, NULL);
#else
			lightdm_greeter_respond (priv->greeter, entry_text);
#endif
			/* If we have questions pending, then we continue processing
			 * those, until we are done. (Otherwise, authentication will
			 * not complete.) */
			if (priv->pending_questions)
				process_prompts (page);
		}
		return;
	}

	gtk_widget_destroy (priv->pw_dialog);
	priv->pw_dialog = NULL;
	priv->changing_password = FALSE;
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_widget_grab_focus (priv->pw_entry);
	start_authentication (page, lightdm_greeter_get_authentication_user (priv->greeter));
}

static void
login_error_dialog_response_cb (GtkDialog *dialog,
                                gint       response,
                                gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_widget_grab_focus (priv->pw_entry);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (greeter_page_manager_get_is_vpn_logined (GREETER_PAGE (page)->manager))
		g_idle_add ((GSourceFunc)reload_vpn_service, page);
}

static void
show_login_error_dialog (GreeterLoginPage *page,
                         const gchar      *title,
                         const gchar      *message)
{
	GtkWidget *dialog;

	dialog = greeter_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                         "dialog-warning-symbolic.symbolic",
                                         title,
                                         message ? message : "");

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("Ok"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (login_error_dialog_response_cb), page);

	gtk_widget_show (dialog);

	page->priv->have_pam_error = TRUE;
}

static void
run_warning_dialog (GreeterLoginPage *page,
                    const gchar      *title,
                    const gchar      *message,
                    const gchar      *data)
{
	GtkWidget *dialog;
	gchar *response = NULL;
	GreeterLoginPagePrivate *priv = page->priv;

	dialog = greeter_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                         "dialog-warning-symbolic.symbolic",
                                         title,
                                         message);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("Ok"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (data) {
		if (g_str_equal (data, "CHPASSWD_FAILURE_OK")) {
			priv->changing_password = FALSE;
			gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
			gtk_widget_grab_focus (priv->pw_entry);
			start_authentication (page, lightdm_greeter_get_authentication_user (priv->greeter));
			if (greeter_page_manager_get_is_vpn_logined (GREETER_PAGE (page)->manager))
				g_idle_add ((GSourceFunc)reload_vpn_service, page);
		} else if (g_str_equal (data, "ACCT_EXP_OK")) {
			response = "acct_exp_ok";
		} else if (g_str_equal (data, "DEPT_EXP_OK")) {
			response = "dept_exp_ok";
		} else if (g_str_equal (data, "PASS_EXP_OK")) {
			response = "pass_exp_ok";
		} else if (g_str_equal (data, "DUPLICATE_LOGIN_OK")) {
			response = "duplicate_login_ok";
		} else if (g_str_equal (data, "TRIAL_LOGIN_OK")) {
			response = "trial_login_ok";
		}
	}

	if (response) {
		if (lightdm_greeter_get_in_authentication (priv->greeter)) {
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
			lightdm_greeter_respond (priv->greeter, response, NULL);
#else
			lightdm_greeter_respond (priv->greeter, response);
#endif
		}
	}

	priv->have_pam_error = TRUE;
}

static gboolean
show_password_settings_dialog (GreeterLoginPage *page)
{
	GtkWidget *dialog, *toplevel;

	if (page->priv->pw_dialog)
		return FALSE;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	dialog = page->priv->pw_dialog = greeter_password_settings_dialog_new (GTK_WINDOW (toplevel));

	g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (password_settings_dialog_response_cb), page);

	gtk_widget_show (dialog);

	return TRUE;
}

static void
run_password_changing_dialog (GreeterLoginPage *page,
                              const gchar      *title,
                              const gchar      *message,
                              const gchar      *yes,
                              const gchar      *no,
                              const gchar      *data)
{
	gint res;
	GtkWidget *dialog;
	const gchar *yes_text, *no_text;
	GtkWidget *suggested_button;
	GtkStyleContext *style = NULL;
	GreeterLoginPagePrivate *priv = page->priv;

	dialog = greeter_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))),
                                         "dialog-password-symbolic",
                                         title,
                                         message);

	yes_text = (yes) ? yes : _("Ok");
	no_text = (no) ? no : _("Cancel");

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            yes_text, GTK_RESPONSE_OK,
                            no_text, GTK_RESPONSE_CANCEL,
                            NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show (dialog);

	suggested_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	style = gtk_widget_get_style_context (suggested_button);
	gtk_style_context_add_class (style, "suggested-action");
	gtk_widget_queue_draw (dialog);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (res == GTK_RESPONSE_OK) {
		priv->changing_password = TRUE;

		if (g_strcmp0 (data, "req_response") == 0) {
			if (!show_password_settings_dialog (page))
				goto out;

#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
			lightdm_greeter_respond (priv->greeter, "chpasswd_yes", NULL);
#else
			lightdm_greeter_respond (priv->greeter, "chpasswd_yes");
#endif
        } else {
			if (!show_password_settings_dialog (page))
				goto out;
		}

		return;
	}

	if (g_strcmp0 (data, "req_response") == 0) {
		if (lightdm_greeter_get_in_authentication (priv->greeter)) {
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
			lightdm_greeter_respond (priv->greeter, "chpasswd_no", NULL);
#else
			lightdm_greeter_respond (priv->greeter, "chpasswd_no");
#endif
		}
	}

out:
	priv->changing_password = FALSE;
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_widget_grab_focus (priv->pw_entry);
	start_authentication (page, lightdm_greeter_get_authentication_user (priv->greeter));
}

static void
handle_vpn_login_error (GreeterLoginPage *page, int result)
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
			message = _("The VPN connection cannot proceed because the login period has been exceeded.\n"
                        "Please try again later or contact the administrator.");
		break;

		case VPN_ACCOUNT_LOCKED:
			message = _("Login is not possible because user "
                        "authentication has failed more than 5 times.\n"
                        "Please contact the administrator.");
		break;

		case VPN_ACCOUNT_ID_EXPIRED:
			message = _("The VPN connection cannot be proceeded due to the ID usage period exceeded.\n"
                        "Please try again later or contact the administrator.");
		break;

		case VPN_ACCOUNT_PW_EXPIRED:
			message = _("VPN connection cannot be proceeded because the password has expired.\n"
                        "Please try again later or contact the administrator.");
		break;

		case VPN_SERVER_CONNECTION_ERROR:
			message = _("The server cannot be reached due to network problems.\n"
                        "Please check the network status and try again.");
		break;

		case VPN_SERVER_RESPONSE_ERROR:
			message = _("VPN connection cannot proceed due to a VPN server response error.\n"
                        "Please try again later or contact the administrator.");
		break;

		case VPN_SERVER_DISCONNECTED:
			message = _("The connection to the server has been lost.\n"
                        "Check the network status and connect again.");
		break;

		case VPN_EXECUTION_ERROR:
			message = _("VPN connection cannot proceed due to VPN execution failure.\n"
                        "Please try again later or contact the administrator.");
		break;

		case VPN_CONFIGURATION_ERROR:
			message = _("VPN connection cannot proceed due to a VPN setting error.\n"
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

//		case VPN_SERVICE_USERINFO_REQUEST_ERROR:
//			message = _("VPN login request failed.\nReboot your system and try again.");
//		break;

		default:
		{
			gchar *msg = NULL;
			msg = g_strdup_printf ("%s [%s: VPNE-%d]\n%s",
                                   _("Authentication Failure"),
                                   _("Error Code"),
                                   result,
                                   _("Please try again later or contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
            g_free (msg);

			return;
		}
	}

	if (message)
		show_login_error_dialog (page, NULL, message);
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

static gboolean
start_splash_timeout_cb (gpointer user_data)
{   
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);

	handle_vpn_login_result (page, VPN_SERVICE_TIMEOUT_ERROR);

	return FALSE;
}

static void
post_login (GreeterLoginPage *page)
{
	GreeterLoginPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	greeter_page_manager_hide_splash (manager);

	g_clear_handle_id (&priv->splash_timeout_id, g_source_remove);
	priv->splash_timeout_id = 0;

	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);
	gtk_widget_set_sensitive (priv->login_button, TRUE);
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_widget_grab_focus (priv->pw_entry);

	g_signal_handlers_unblock_by_func (page->priv->login_button, login_button_clicked_cb, page);

}

static void
pre_login (GreeterLoginPage *page)
{
	GtkWidget *toplevel;
	const gchar *message;
	GreeterLoginPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
	message = _("Authentication is in progress.\nPlease wait.");

	greeter_page_manager_show_splash (manager, toplevel, message, NULL);

	g_signal_handlers_block_by_func (priv->login_button, login_button_clicked_cb, page);

	gtk_widget_set_sensitive (priv->id_entry, FALSE);
	gtk_widget_set_sensitive (priv->pw_entry, FALSE);
	gtk_widget_set_sensitive (priv->login_button, FALSE);

	priv->splash_timeout_id = g_timeout_add (VPN_LOGIN_TIMEOUT_SECS * 1000,
                                             start_splash_timeout_cb, page);
}

static void
handle_vpn_login_result (GreeterLoginPage *page,
                         int               result)
{
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

    // 0은 성공 or 실패(에러)코드가 아닌 상태를 알리기위한 코드
	if (result == 0)
		return;

	if (result == VPN_LOGIN_SUCCESS) {
		greeter_page_manager_set_is_vpn_logined (manager, TRUE);
		try_to_login_system (page);
		return;
	}

	post_login (page);

	greeter_page_manager_set_is_vpn_logined (manager, FALSE);

	if (result == VPN_SERVER_DISCONNECTED) {
		// TODO: 연결이후 접속이 끊어진 경우 이므로 팝업 메세지 처리
	} else {
		handle_vpn_login_error (page, result);
	}
}

//static void
//try_to_vpn_logout_done_cb (GObject      *source_object,
//                           GAsyncResult *res,
//                           gpointer      user_data)
//{
//	gint32 result = -1;
//	GVariant *variant; 
//	GreeterPageManager *manager = GREETER_PAGE (user_data)->manager;
//
//	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, NULL);
//	if (variant) {
//		g_variant_get (variant, "(i)", &result);
//		g_variant_unref (variant);
//	}
//
//	greeter_page_manager_hide_splash (manager);
//}

//static gboolean
//try_to_vpn_logout (GreeterLoginPage *page)
//{
//	GtkWidget *toplevel;
//	const gchar *message;
//	GreeterLoginPagePrivate *priv = page->priv;
//	GreeterPageManager *manager = GREETER_PAGE (page)->manager;
//
//	if (!priv->vpn_dbus_proxy)
//		return FALSE;
//
//	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
//	message = _("Initializing Settings for VPN.\nPlease wait.");
//
//	greeter_page_manager_show_splash (manager, toplevel, message);
//
//	g_dbus_proxy_call (priv->vpn_dbus_proxy,
//                       "Logout",
//                       NULL,
//                       G_DBUS_CALL_FLAGS_NONE,
//                       -1,
//                       NULL,
//                       try_to_vpn_logout_done_cb,
//                       page);
//
//	return TRUE;
//}

static void
try_to_vpn_login_done_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
	gint32 result = -1;
	GVariant *variant;
	GError *error = NULL;
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);

	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
	if (error) {
		g_warning ("Failed to login VPN: %s", error->message);
		g_error_free (error);
		handle_vpn_login_result (page, VPN_SERVICE_LOGIN_REQUEST_ERROR);
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
try_to_login_vpn (GreeterLoginPage *page,
                  const gchar    *ip,
                  const gchar    *port,
                  const gchar    *id,
                  const gchar    *pw)
{
	GreeterLoginPagePrivate *priv = page->priv;

	if (!priv->vpn_dbus_proxy)
		return FALSE;

	g_dbus_proxy_call (priv->vpn_dbus_proxy,
                       "Login",
                       g_variant_new ("(ssss)", ip, port, id, pw),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       NULL,
                       try_to_vpn_login_done_cb,
                       page);

	return TRUE;
}

static void
vpn_dbus_signal_handler (GDBusProxy *proxy,
                         gchar      *sender_name,
                         gchar      *signal_name,
                         GVariant   *parameters,
                         gpointer    user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);

	if (g_str_equal (signal_name, "ConnectionStateChanged")) {
		gint32 result = -1;
		if (parameters)
			g_variant_get (parameters, "(i)", &result);

		handle_vpn_login_result (page, result);
	}
}

static void
vpn_dbus_name_appeared_cb (GDBusConnection *connection,
                           const gchar     *name,
                           const gchar     *name_owner,
                           gpointer         user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	priv->vpn_service_enabled = TRUE;

	if (!priv->vpn_dbus_proxy)
		priv->vpn_dbus_proxy = get_vpn_dbus_proxy ();

	if (!priv->vpn_dbus_signal_id)
		priv->vpn_dbus_signal_id = g_signal_connect (G_OBJECT (priv->vpn_dbus_proxy), "g-signal",
                                                     G_CALLBACK (vpn_dbus_signal_handler), page);
}

static void
vpn_dbus_name_vanished_cb (GDBusConnection *connection,
                           const gchar     *name,
                           gpointer         user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	priv->vpn_service_enabled = FALSE;
}

static gboolean
is_valid_session (GList       *items,
                  const gchar *session)
{
	for (; items; items = g_list_next (items))
		if (g_strcmp0 (session, lightdm_session_get_key (items->data)) == 0)
			return TRUE;

	return FALSE;
}

static void
set_session (GreeterLoginPage *page, const gchar *session)
{
    gchar *last_session = NULL;
    GList *sessions = lightdm_get_sessions ();
	GreeterLoginPagePrivate *priv = page->priv;

    /* Validation */
    if (!session || !is_valid_session (sessions, session))
    {
        /* previous session */
        last_session = config_get_string (STATE_SECTION_GREETER, STATE_KEY_LAST_SESSION, NULL);
        if (last_session && g_strcmp0 (session, last_session) != 0 &&
            is_valid_session (sessions, last_session))
            session = last_session;
        else
        {
            /* default */
            const gchar* default_session = lightdm_greeter_get_default_session_hint (priv->greeter);
            if (g_strcmp0 (session, default_session) != 0 &&
                is_valid_session (sessions, default_session))
                session = default_session;
            /* first in the sessions list */
            else if (sessions)
                session = lightdm_session_get_key (sessions->data);
            /* give up */
            else
                session = NULL;
        }
    }

    g_free (priv->current_session);
    priv->current_session = g_strdup (session);
    g_free (last_session);
}

static void
set_language (GreeterLoginPage *page, const gchar *language)
{
	GreeterLoginPagePrivate *priv = page->priv;

	g_free (priv->current_language);
	priv->current_language = g_strdup (language);
}

/* Pending questions */
static void
pam_message_finalize (PAMConversationMessage *message)
{
	g_free (message->text);
	g_free (message);
}

static void
process_prompts (GreeterLoginPage *page)
{
	const gchar *id;
	GreeterLoginPagePrivate *priv = page->priv;
	LightDMGreeter *greeter = priv->greeter;

	if (!priv->pending_questions)
		return;

	/* always allow the user to change username again */
	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);
	id = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));
	gtk_widget_set_sensitive (priv->login_button, strlen (id) > 0);

	/* Special case: no user selected from list, so PAM asks us for the user
	 * via a prompt. For that case, use the username field */
	if (!priv->prompted && priv->pending_questions && !priv->pending_questions->next &&
        ((PAMConversationMessage *) priv->pending_questions->data)->is_prompt &&
        ((PAMConversationMessage *) priv->pending_questions->data)->type.prompt != LIGHTDM_PROMPT_TYPE_SECRET &&
        gtk_widget_get_visible (priv->id_entry) &&
        lightdm_greeter_get_authentication_user (greeter) == NULL)
	{
		priv->prompted = TRUE;
		priv->prompt_active = TRUE;
		gtk_widget_grab_focus (priv->id_entry);
		return;
	}

	while (priv->pending_questions)
	{
		PAMConversationMessage *message = (PAMConversationMessage *) priv->pending_questions->data;
		priv->pending_questions = g_slist_remove (priv->pending_questions, (gconstpointer) message);

		const gchar *filter_msg_000 = "You are required to change your password immediately";
		const gchar *filter_msg_010 = g_dgettext("Linux-PAM", "You are required to change your password immediately (administrator enforced)");
		const gchar *filter_msg_020 = g_dgettext("Linux-PAM", "You are required to change your password immediately (password expired)");
		const gchar *filter_msg_030 = "Temporary Password";
		const gchar *filter_msg_040 = "Password Maxday Warning";
		const gchar *filter_msg_050 = "Account Expiration Warning";
		const gchar *filter_msg_051 = "Division Expiration Warning";
		const gchar *filter_msg_052 = "Password Expiration Warning";
		const gchar *filter_msg_053 = _("your password will expire in");
		const gchar *filter_msg_060 = "Duplicate Login Notification";
		const gchar *filter_msg_070 = "Authentication Failure";
        const gchar *filter_msg_071 = "Deleted Account";
        const gchar *filter_msg_072 = "Invalid Account";
        const gchar *filter_msg_073 = "No Exist Account";
        const gchar *filter_msg_074 = "Policy Violation Account";
        const gchar *filter_msg_075 = "Not Allowed IP";
		const gchar *filter_msg_080 = "Account Locking";
		const gchar *filter_msg_090 = "Account Expiration";
		const gchar *filter_msg_100 = "Password Expiration";
		const gchar *filter_msg_110 = "Duplicate Login";
		const gchar *filter_msg_120 = "Division Expiration";
		const gchar *filter_msg_130 = "Login Trial Exceed";
		const gchar *filter_msg_140 = "Trial Period Expired";
		const gchar *filter_msg_150 = "DateTime Error";
		const gchar *filter_msg_160 = "Trial Period Warning";

		if ((strstr (message->text, filter_msg_000) != NULL) ||
		    (strstr (message->text, filter_msg_010) != NULL) ||
            (strstr (message->text, filter_msg_020) != NULL)) {
			post_login (page);
			run_password_changing_dialog (page,
                                          NULL,
                                          _("Your password has expired.\n"
                                            "Please change your password immediately."),
                                          _("Changing Password"),
                                          _("Cancel"),
                                          "req_no_response");
			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_030)) {
			post_login (page);
			run_password_changing_dialog (page,
                                          NULL,
                                          _("Your password has been issued temporarily.\n"
                                            "For security reasons, please change your password immediately."),
                                          _("Changing Password"),
                                          _("Cancel"),
                                          "req_no_response");
			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_040)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 1) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Please change your password for security.\n"
                                             "If you do not change your password within %s day, "
                                             "your password expires.\n"
                                             "You can no longer log in.\n"
                                             "Do you want to change password now?"), tokens[1]);
				} else {
					msg = g_strdup_printf (_("Please change your password for security.\n"
                                             "If you do not change your password within %s days, "
                                             "your password expires.\n"
                                             "You can no longer log in.\n"
                                             "Do you want to change password now?"), tokens[1]);
				}
			} else {
				msg = g_strdup (_("Please change your password for security.\n"
                                  "If you do not change your password within a few days, "
                                  "your password expires.\n"
                                  "You can no longer log in.\n"
                                  "Do you want to change password now?"));
			}
			g_strfreev (tokens);

			run_password_changing_dialog (page, NULL, msg, _("Change now"), _("Later"), "req_response");
			g_free (msg);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_050)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Your account will not be available after %s.\n"
                                             "Your account will expire in %s day."),
                                           tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("Your account will not be available after %s.\n"
                                             "Your account will expire in %s days."),
                                           tokens[1], tokens[2]);
				}
			}
			g_strfreev (tokens);

			run_warning_dialog (page, NULL, msg, "ACCT_EXP_OK");
			g_free (msg);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_051)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Your organization will not be available after %s.\n"
                                             "Your organization will expire in %s day."),
                                           tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("Your organization will not be available after %s.\n"
                                             "Your organization will expire in %s days."),
                                           tokens[1], tokens[2]);
				}
			}
			g_strfreev (tokens);

			run_warning_dialog (page, NULL, msg, "DEPT_EXP_OK");
			g_free (msg);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_052)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Your password will not be available after %s.\n"
                                             "Your password will expire in %s day."),
                                           tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("Your password will not be available after %s.\n"
                                             "Your password will expire in %s days."),
                                           tokens[1], tokens[2]);
				}
			}
			g_strfreev (tokens);

			run_warning_dialog (page, NULL, msg, "PASS_EXP_OK");
			g_free (msg);

			continue;
		} else if ((strstr (message->text, filter_msg_053) != NULL)) {
			run_warning_dialog (page, NULL, message->text, NULL);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_060)) {
			post_login (page);

			GString *msg = g_string_new (_("Duplicate logins detected with the same ID."));
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (tokens[1]) {
				gchar *text = g_strdup_printf ("%s : %s", _("Client ID"), tokens[1]);
				g_string_append_printf (msg, "\n\n%s", text);
				g_free (text);
			}
			if (tokens[2]) {
				gchar *text = g_strdup_printf ("%s : %s", _("Client Name"), tokens[2]);
				g_string_append_printf (msg, "\n%s", text);
				g_free (text);
			}
			if (tokens[3]) {
				gchar *text = g_strdup_printf ("%s : %s", _("IP"), tokens[3]);
				g_string_append_printf (msg, "\n%s", text);
				g_free (text);
			}
			if (tokens[4]) {
				gchar *text = g_strdup_printf ("%s : %s", _("Local IP"), tokens[4]);
				g_string_append_printf (msg, "\n%s", text);
				g_free (text);
			}

			run_warning_dialog (page, NULL, msg->str, "DUPLICATE_LOGIN_OK");
			g_string_free (msg, TRUE);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_070)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 1) {
				msg = g_strdup_printf (_("Authentication Failure\n"
                                         "You have %s login attempts remaining.\n"
                                         "You can no longer log in when the maximum number of login "
                                         "attempts is exceeded."), tokens[1]);
			} else {
				msg = g_strdup (_("The user could not be authenticated due to an unknown error.\n"
                                  "Please contact the administrator."));
			}
			g_strfreev (tokens);
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_071)) {
			gchar *msg = g_strdup (_("This account has deleted and is no longer available.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_072)) {
			gchar *msg = g_strdup (_("You attempted to log in from an unregistered device.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_073)) {
			gchar *msg = g_strdup ( _("Authentication Failure\n"
                                      "Please check the username and password and try again."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_074)) {
			gchar *msg = g_strdup (_("Login was denied because "
                                     "it violated the policy set by the GPMS.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_075)) {
			gchar *msg = g_strdup (_("Login was denied because "
                                     "it violated the policy(Allowed IP) set by the GPMS.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_080)) {
			post_login (page);

			gchar *msg = g_strdup (_("Your account has been locked because\n"
                                     "you have exceeded the number of login attempts.\n"
                                     "Please try again in a moment."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_090)) {
			post_login (page);

			gchar *msg = g_strdup (_("This account has expired and is no longer available.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_100)) {
			post_login (page);

			gchar *msg = g_strdup (_("The password for your account has expired.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_110)) {
			post_login (page);

			gchar *msg = g_strdup (_("You are already logged in.\n"
                                     "Log out of the other device and try again.\n"
                                     "If the problem persists, please contact your administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_120)) {
			post_login (page);

			gchar *msg = g_strdup (_("Due to the expiration of your organization, "
                                     "this account is no longer available.\n"
                                     "Please contact the administrator."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_130)) {
			post_login (page);

			gchar *msg = g_strdup (_("Login attempts exceeded the number of times,\n"
                                     "so you cannot login for a certain period of time.\n"
                                     "Please try again in a moment."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_140)) {
			post_login (page);

			gchar *msg = g_strdup (_("Trial period has expired."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_150)) {
			post_login (page);

			gchar *msg = g_strdup (_("Time error occurred."));
			show_login_error_dialog (page, NULL, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_160)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[2], "0")) {
					msg = g_strdup_printf (_("The trial period is up to %s days.\n"
                                             "The trial period expires today."), tokens[1]);
				} else if (g_str_equal (tokens[2], "1")){
					msg = g_strdup_printf (_("The trial period is up to %s days.\n"
                                             "%s day left to expire."), tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("The trial period is up to %s days.\n"
                                             "%s days left to expire."), tokens[1], tokens[2]);
				}
			} else {
				msg = g_strdup (_("The trial period is unknown."));
			}
			g_strfreev (tokens);

			run_warning_dialog (page, NULL, msg, "TRIAL_LOGIN_OK");
			g_free (msg);
			continue;
		}

        if (!message->is_prompt)
        {
			post_login (page);

			/* FIXME: this doesn't show multiple messages, but that was
			 * already the case before. */
			if (priv->changing_password) {
				if (priv->pw_dialog) {
					greeter_password_settings_dialog_set_message_label (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog), message->text);
				}
			} else {
				show_login_error_dialog (page, NULL, message->text);
			}
			continue;
        }

        if (priv->changing_password) {
			post_login (page);

			const gchar *title;
			const gchar *prompt_label;

			/* for pam-gooroom and Linux-PAM, libpwquality */
			if ((strstr (message->text, "Current password: ") != NULL) ||
					(strstr (message->text, _("Current password: ")) != NULL)) {
				priv->changing_password_step = 1;
				title = _("Changing Password - [Step 1]");
				prompt_label = _("Enter current password :");
			} else if ((strstr (message->text, "New password: ") != NULL) ||
					(strstr (message->text, _("New password: ")) != NULL)) {
				priv->changing_password_step = 2;
				title = _("Changing Password - [Step 2]");
				prompt_label = _("Enter new password :");
			} else if ((strstr (message->text, "Retype new password: ") != NULL) ||
					(strstr (message->text, _("Retype new password: ")) != NULL)) {
				priv->changing_password_step = 3;
				title = _("Changing Password - [Step 3]");
				prompt_label = _("Retype new password :");
			} else {
				title = NULL;
				prompt_label = NULL;
			}

			greeter_password_settings_dialog_set_title (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog), title);
			greeter_password_settings_dialog_set_prompt_label (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog), prompt_label);
			greeter_password_settings_dialog_set_entry_text (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog), "");
			greeter_password_settings_dialog_grab_entry_focus (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog));
		}

		priv->prompted = TRUE;
		priv->prompt_active = TRUE;

        /* If we have more stuff after a prompt, assume that other prompts are pending,
         * so stop here. */
        break;
    }
}

static void
start_authentication (GreeterLoginPage *page, const gchar *username)
{
	GreeterLoginPagePrivate *priv = page->priv;
	LightDMGreeter *greeter = priv->greeter;

	priv->prompted = FALSE;
	priv->prompt_active = FALSE;
	priv->have_pam_error = FALSE;

	if (priv->pending_questions)
	{
		g_slist_free_full (priv->pending_questions, (GDestroyNotify) pam_message_finalize);
		priv->pending_questions = NULL;
	}

	if (g_strcmp0 (username, "*other") == 0)
	{
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_authenticate (greeter, NULL, NULL);
#else
		lightdm_greeter_authenticate (greeter, NULL);
#endif
	}
	else if (g_strcmp0 (username, "*guest") == 0)
	{
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_authenticate_as_guest (greeter, NULL);
#else
		lightdm_greeter_authenticate_as_guest (greeter);
#endif
	}
	else
	{
		LightDMUser *user;

		user = lightdm_user_list_get_user_by_name (lightdm_user_list_get_instance (), username);
		if (user)
		{
			if (!priv->current_session)
				set_session (page, lightdm_user_get_session (user));
			if (!priv->current_language)
				set_language (page, lightdm_user_get_language (user));
		}
		else
		{
			set_session (page, NULL);
			set_language (page, NULL);
		}
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_authenticate (greeter, username, NULL);
#else
		lightdm_greeter_authenticate (greeter, username);
#endif
	}
}

static void
start_session (GreeterLoginPage *page)
{
	const gchar *last_user_key;
	GreeterLoginPagePrivate *priv = page->priv;
	LightDMGreeter *greeter = priv->greeter;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	if (priv->current_language)
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_set_language (greeter, priv->current_language, NULL);
#else
		lightdm_greeter_set_language (greeter, priv->current_language);
#endif

	/* Remember last session */
	config_set_string (STATE_SECTION_GREETER, STATE_KEY_LAST_SESSION, priv->current_session);

	if (greeter_page_manager_get_mode (manager) == MODE_INTERNAL) {
		last_user_key = STATE_KEY_INTERNAL_LAST_USER;
	} else {
		last_user_key = STATE_KEY_EXTERNAL_LAST_USER;
	}

	if (config_get_bool (STATE_SECTION_GREETER, STATE_KEY_REMEMBER_USER, FALSE)) {
		/* save last user */
		config_set_string (STATE_SECTION_GREETER, last_user_key, priv->id);
	} else {
		/* delete last user */
		config_set_string (STATE_SECTION_GREETER, last_user_key, "");
	}

	//	greeter_background_save_xroot (greeter_background);

	if (greeter_page_manager_get_mode (manager) == MODE_INTERNAL) {
		const gchar *json;
		gchar *arg = NULL;
		GVariant *variant = NULL;
		GDBusProxy *proxy = NULL;

		proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_CALL_FLAGS_NONE,
                                               NULL,
                                               "kr.gooroom.ssohelper",
                                               "/kr/gooroom/ssohelper",
                                               "kr.gooroom.ssohelper",
                                               NULL,
                                               NULL);

		json = "{\"task\":\"setpw\",\"id\":\"%s\",\"pw\":\"%s\"}";

		arg = g_strdup_printf (json, priv->id, priv->pw);

		variant = g_dbus_proxy_call_sync (proxy,
                                          "do_task",
                                          g_variant_new ("(s)", arg),
                                          G_DBUS_CALL_FLAGS_NONE, -1,
                                          NULL, NULL);
		if (variant)
			g_variant_unref (variant);

		g_free (arg);

		g_clear_object (&proxy);
	}

	if (!lightdm_greeter_start_session_sync (greeter, priv->current_session, NULL)) {
		run_warning_dialog (page, NULL, _("Failed to start session"), NULL);
		start_authentication (page, lightdm_greeter_get_authentication_user (greeter));
	}
}

static void
show_prompt_cb (LightDMGreeter    *greeter,
                const gchar       *text,
                LightDMPromptType  type,
                gpointer           user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	PAMConversationMessage *message_obj = g_new (PAMConversationMessage, 1);
	if (message_obj)
	{
		message_obj->is_prompt = TRUE;
		message_obj->type.prompt = type;
		message_obj->text = g_strdup (text);
		priv->pending_questions = g_slist_append (priv->pending_questions, message_obj);
	}

	if (!priv->prompt_active)
		process_prompts (page);
}

static void
show_message_cb (LightDMGreeter     *greeter,
                 const gchar        *text,
                 LightDMMessageType  type,
                 gpointer            user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

    PAMConversationMessage *message_obj = g_new (PAMConversationMessage, 1);
    if (message_obj)
    {
        message_obj->is_prompt = FALSE;
        message_obj->type.message = type;
        message_obj->text = g_strdup (text);
        priv->pending_questions = g_slist_append (priv->pending_questions, message_obj);
    }

    if (!priv->prompt_active)
        process_prompts (page);
}

static void
authentication_complete_cb (LightDMGreeter *greeter,
                            gpointer        user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	post_login (page);

	priv->prompt_active = FALSE;

	if (priv->pending_questions) {
		g_slist_free_full (priv->pending_questions, (GDestroyNotify) pam_message_finalize);
		priv->pending_questions = NULL;
	}

	if (lightdm_greeter_get_is_authenticated (greeter)) {
		if (priv->pw_dialog) {
			gtk_widget_destroy (priv->pw_dialog);
			priv->pw_dialog = NULL;
		}
		start_session (page);
	} else {
		if (priv->changing_password) {
			gchar *msg = NULL;

			// remove password settings dialog
			if (priv->pw_dialog) {
				gtk_widget_destroy (priv->pw_dialog);
				priv->pw_dialog = NULL;
			}

			if (priv->changing_password_step == 1) {
				msg = _("Changing password is terminated because\n"
                        "the current password does not match.\n"
                        "Please try again later.");
			} else if (priv->changing_password_step == 2) {
				msg = _("New password violates the security conformity,\n"
                        "so the change of the password is terminated.\n"
                        "Please try again later.");
			} else if (priv->changing_password_step == 3) {
				msg = _("In Confirm New Password, the password did not match,\n"
                        "so the change of password is terminated.\n"
                        "Please try again later.");
			}
			run_warning_dialog (page, NULL, msg, "CHPASSWD_FAILURE_OK");
			return;
		}

		/* If an error message is already printed we do not print it this statement
		 * The error message probably comes from the PAM module that has a better knowledge
		 * of the failure. */
		if (!priv->have_pam_error) {
			show_login_error_dialog (page,
                                     NULL,
                                     _("Authentication Failure\n"
                                       "Please check the username and password and try again."));
		}
	}
}

//static void
//timed_autologin_cb (LightDMGreeter *greeter)
//{
//    /* Don't trigger autologin if user locks screen with light-locker (thanks to Andrew P.). */
//    if (!lightdm_greeter_get_lock_hint (greeter))
//    {
//        if (lightdm_greeter_get_is_authenticated (greeter))
//        {
//            /* Configured autologin user may be already selected in user list. */
//            if (lightdm_greeter_get_authentication_user (greeter))
//                /* Selected user matches configured autologin-user option. */
//                start_session ();
//            else if (lightdm_greeter_get_autologin_guest_hint (greeter))
//                /* "Guest session" is selected and autologin-guest is enabled. */
//                start_session ();
//            else if (lightdm_greeter_get_autologin_user_hint (greeter))
//            {
//                /* "Guest session" is selected, but autologin-user is configured. */
//                start_authentication (lightdm_greeter_get_autologin_user_hint (greeter));
//                prompted = TRUE;
//            }
//        }
//        else
//#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
//            lightdm_greeter_authenticate_autologin (greeter, NULL);
//#else
//            lightdm_greeter_authenticate_autologin (greeter);
//#endif
//    }
//}

static void
try_to_login_system (GreeterLoginPage *page)
{
	gchar *id = NULL, *pw = NULL;
	GreeterLoginPagePrivate *priv = page->priv;

	id = get_id (priv->id_entry);
	pw = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)));

	if (strlen (id) == 0)
		goto out;

	start_authentication (page, id);

	while (!priv->prompted)
		gtk_main_iteration ();

	priv->prompt_active = FALSE;


	if (lightdm_greeter_get_in_authentication (priv->greeter)) {
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_respond (priv->greeter, pw, NULL);
#else
		lightdm_greeter_respond (priv->greeter, pw);
#endif
        /* If we have questions pending, then we continue processing
         * those, until we are done. (Otherwise, authentication will
         * not complete.) */
		if (priv->pending_questions) {
			process_prompts (page);
		}
	}

out:
	g_free (id);
	g_free (pw);
}

static void
remember_id_checkbutton_toggled_cb (GtkToggleButton *button,
                                    gpointer         user_data)
{
	gboolean active = FALSE;

	active = gtk_toggle_button_get_active (button);

	config_set_bool (STATE_SECTION_GREETER, STATE_KEY_REMEMBER_USER, active);
}

static gboolean
button_enter_notify_event_cb (GtkWidget *widget,
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
button_leave_notify_event_cb (GtkWidget *widget,
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

static void
mode_switch_button_clicked_cb (GtkButton *widget,
                               gpointer   user_data)
{
	GreeterPage *page = GREETER_PAGE (user_data);
	GreeterPageManager *manager = page->manager;

	if (greeter_page_manager_get_mode (manager) == MODE_INTERNAL) {
		greeter_page_manager_set_mode (manager, MODE_EXTERNAL);
	} else {
		greeter_page_manager_set_mode (manager, MODE_INTERNAL);
	}
	greeter_page_manager_reload (manager);
}

static void
network_settings_button_clicked_cb (GtkButton *widget,
                                    gpointer   user_data)
{
	greeter_page_manager_go_next (GREETER_PAGE (user_data)->manager);
}

static void
login_button_clicked_cb (GtkButton *widget,
                         gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	/* Is network online? */
	if (check_networking ()) {
		if (!greeter_page_manager_get_network_available (manager)) {
			run_warning_dialog (page, NULL,
                               _("You are not currently connected to the network.\n"
                                 "Click the 'Network Settings' button to set up the network first."),
                                NULL);
			return;
		}
	}

	pre_login (page);

	g_clear_pointer (&priv->id, g_free);
	g_clear_pointer (&priv->pw, g_free);

	priv->id = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->id_entry)));
	priv->pw = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)));

	if (greeter_page_manager_get_mode (manager) == MODE_EXTERNAL) {
		if (greeter_page_manager_get_is_vpn_logined (manager)) {
			try_to_login_system (page);
			return;
		}

		gchar *ip = NULL, *port = NULL;

		greeter_page_manager_set_is_vpn_logined (manager, FALSE);

		get_vpn_connection_info (&ip, &port);

		if (!ip || g_str_equal (ip, "")) {
			handle_vpn_login_result (page, VPN_SERVICE_INFO_ERROR);
			goto out;
		}

		if (!port || g_str_equal (port, "")) {
			handle_vpn_login_result (page, VPN_SERVICE_INFO_ERROR);
			goto out;
		}

		if (!try_to_login_vpn (page, ip, port, priv->id, priv->pw))
			handle_vpn_login_result (page, VPN_SERVICE_DAEMON_ERROR);

out:
		g_free (ip);
		g_free (port);

		return;
	}

	try_to_login_system (page);
}

static void
pw_entry_activate_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);

	if (gtk_widget_get_sensitive (page->priv->login_button))
		login_button_clicked_cb (GTK_BUTTON (page->priv->login_button), page);
}

static gboolean
id_entry_key_press_cb (GtkWidget   *widget,
                       GdkEventKey *event,
                       gpointer     user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

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
	const gchar *text;
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	text = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));

	gtk_widget_set_sensitive (priv->login_button, strlen (text) > 0);
}

static void
set_last_user (GreeterLoginPage *page)
{
	gboolean remember_id = FALSE;
	GreeterLoginPagePrivate *priv = page->priv;

	remember_id = config_get_bool (STATE_SECTION_GREETER, STATE_KEY_REMEMBER_USER, FALSE);
	if (remember_id) {
		priv->internal_last_user = config_get_string (STATE_SECTION_GREETER, STATE_KEY_INTERNAL_LAST_USER, NULL);
		priv->external_last_user = config_get_string (STATE_SECTION_GREETER, STATE_KEY_EXTERNAL_LAST_USER, NULL);
	}

	g_signal_handlers_block_by_func (priv->remember_id_checkbutton,
                                     remember_id_checkbutton_toggled_cb, page);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->remember_id_checkbutton), remember_id);
	g_signal_handlers_unblock_by_func (priv->remember_id_checkbutton,
                                       remember_id_checkbutton_toggled_cb, page);
}

static void
lightdm_greeter_init (GreeterLoginPage *page)
{
	GreeterLoginPagePrivate *priv = page->priv;

	priv->greeter = lightdm_greeter_new ();

	g_signal_connect (priv->greeter, "show-prompt", G_CALLBACK (show_prompt_cb), page);
	g_signal_connect (priv->greeter, "show-message", G_CALLBACK (show_message_cb), page);
	g_signal_connect (priv->greeter, "authentication-complete",
                      G_CALLBACK (authentication_complete_cb), page);
//	g_signal_connect (greeter, "autologin-timer-expired", G_CALLBACK (timed_autologin_cb), page);

	/* set default session */
	set_session (page, lightdm_greeter_get_default_session_hint (priv->greeter));

	lightdm_greeter_connect_sync (priv->greeter, NULL);
}

static gboolean
greeter_login_page_should_show (GreeterPage *page)
{
	return TRUE;
}

static void
greeter_login_page_shown (GreeterPage *page)
{
	gchar *title, *label;
	gchar *last_user = NULL;
	GreeterLoginPage *self = GREETER_LOGIN_PAGE (page);
	GreeterLoginPagePrivate *priv = self->priv;
	GreeterPageManager *manager = page->manager;

	priv->prompted = FALSE;
	priv->prompt_active = FALSE;
	priv->have_pam_error = FALSE;
	priv->changing_password = FALSE;

	g_clear_handle_id (&priv->splash_timeout_id, g_source_remove);
	priv->splash_timeout_id = 0;

	g_clear_pointer (&priv->id, g_free);
	g_clear_pointer (&priv->pw, g_free);
	g_clear_pointer (&priv->current_session, g_free);
	g_clear_pointer (&priv->current_language, g_free);

	if (priv->pending_questions) {
		g_slist_free_full (priv->pending_questions, (GDestroyNotify) pam_message_finalize);
		priv->pending_questions = NULL;
	}

	if (greeter_page_manager_get_mode (manager) == MODE_INTERNAL) {
		label = _("Connect from external");
		title = _("Connecting from internal");
		last_user = priv->internal_last_user;
		remove_style_class (self, "external");
		add_style_class (self, "internal");
	} else {
		label = _("Connect from internal");
		title = _("Connecting from external");
		last_user = priv->external_last_user;
		remove_style_class (self, "internal");
		add_style_class (self, "external");
	}

	greeter_page_set_title (page, title);

	gtk_widget_set_visible (priv->id_entry, TRUE);
	gtk_widget_set_visible (priv->pw_entry, TRUE);
	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);
	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_button_set_label (GTK_BUTTON (priv->mode_switch_button), label);
	gtk_widget_set_sensitive (priv->login_button, FALSE);

	if (last_user && strlen (last_user) > 0) {
		gtk_entry_set_text (GTK_ENTRY (priv->id_entry), last_user);
		g_idle_add ((GSourceFunc)grab_focus_idle, priv->pw_entry);
	} else {
		g_idle_add ((GSourceFunc)grab_focus_idle, priv->id_entry);
	}

	if (greeter_page_manager_get_mode (manager) == MODE_EXTERNAL && !priv->vpn_service_enabled) {
		run_warning_dialog (self,
                            NULL,
                            _("VPN service is down.\nPlease enable VPN service and try again."),
                            NULL);
		greeter_page_manager_set_mode (manager, MODE_INTERNAL);
		greeter_page_manager_reload (manager);
		return;
	}

	g_idle_add ((GSourceFunc)reload_vpn_service, page);
}

static void
greeter_login_page_dispose (GObject *object)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (object);
	GreeterLoginPagePrivate *priv = page->priv;

	g_signal_handler_disconnect (priv->vpn_dbus_proxy, priv->vpn_dbus_signal_id);

	if (priv->vpn_dbus_watch_id) {
		g_bus_unwatch_name (priv->vpn_dbus_watch_id);
		priv->vpn_dbus_watch_id = 0;
	}

	g_clear_handle_id (&priv->splash_timeout_id, g_source_remove);
	priv->splash_timeout_id = 0;

	g_clear_object (&priv->vpn_dbus_proxy);

	g_clear_pointer (&priv->id, g_free);
	g_clear_pointer (&priv->pw, g_free);
	g_clear_pointer (&priv->current_session, g_free);
	g_clear_pointer (&priv->current_language, g_free);
	g_clear_pointer (&priv->internal_last_user, g_free);
	g_clear_pointer (&priv->external_last_user, g_free);

	if (priv->pending_questions) {
		g_slist_free_full (priv->pending_questions, (GDestroyNotify) pam_message_finalize);
		priv->pending_questions = NULL;
	}

	G_OBJECT_CLASS (greeter_login_page_parent_class)->dispose (object);
}

static void
greeter_login_page_init (GreeterLoginPage *page)
{
	GreeterLoginPagePrivate *priv;
	priv = page->priv = greeter_login_page_get_instance_private (page);

	gtk_widget_init_template (GTK_WIDGET (page));

	priv->prompted = FALSE;
	priv->prompt_active = FALSE;
	priv->have_pam_error = FALSE;
	priv->changing_password = FALSE;
	priv->pending_questions = NULL;
	priv->current_session = NULL;
	priv->current_language = NULL;
	priv->id = NULL;
	priv->pw = NULL;
	priv->internal_last_user = NULL;
	priv->external_last_user = NULL;

	priv->vpn_dbus_watch_id = 0;
	priv->vpn_dbus_signal_id = 0; 
	priv->splash_timeout_id = 0;
	priv->vpn_service_enabled = FALSE;
    priv->vpn_dbus_proxy = NULL;

	priv->vpn_dbus_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                                VPN_SERVICE_NAME,
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                vpn_dbus_name_appeared_cb,
                                                vpn_dbus_name_vanished_cb,
                                                page, NULL);

	lightdm_greeter_init (page);

	set_last_user (page);

	g_signal_connect (priv->id_entry, "changed", G_CALLBACK (id_entry_changed_cb), page);
	g_signal_connect (priv->id_entry, "key-press-event", G_CALLBACK (id_entry_key_press_cb), page);
	g_signal_connect (priv->pw_entry, "activate", G_CALLBACK (pw_entry_activate_cb), page);
	g_signal_connect (priv->login_button, "clicked", G_CALLBACK (login_button_clicked_cb), page);
	g_signal_connect (priv->remember_id_checkbutton, "toggled",
                      G_CALLBACK (remember_id_checkbutton_toggled_cb), page);
	g_signal_connect (priv->mode_switch_button, "clicked",
                      G_CALLBACK (mode_switch_button_clicked_cb), page);
	g_signal_connect (priv->mode_switch_button, "enter-notify-event",
                      G_CALLBACK (button_enter_notify_event_cb), page);
	g_signal_connect (priv->mode_switch_button, "leave-notify-event",
                      G_CALLBACK (button_leave_notify_event_cb), page);
	g_signal_connect (priv->network_settings_button, "clicked",
                      G_CALLBACK (network_settings_button_clicked_cb), page);
	g_signal_connect (priv->network_settings_button, "enter-notify-event",
                      G_CALLBACK (button_enter_notify_event_cb), page);
	g_signal_connect (priv->network_settings_button, "leave-notify-event",
                      G_CALLBACK (button_leave_notify_event_cb), page);

	gtk_widget_show (GTK_WIDGET (page));
}

static void
greeter_login_page_class_init (GreeterLoginPageClass *klass)
{
	GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/kr/gooroom/greeter/greeter-login-page.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, pw_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, id_entry);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, login_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, remember_id_checkbutton);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, mode_switch_button);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, network_settings_button);

	page_class->page_id = PAGE_ID;
	page_class->shown = greeter_login_page_shown;
	page_class->should_show = greeter_login_page_should_show;

	object_class->dispose = greeter_login_page_dispose;
}

GreeterPage *
greeter_prepare_login_page (GreeterPageManager *manager)
{
	return g_object_new (GREETER_TYPE_LOGIN_PAGE,
                         "manager", manager,
                         NULL);
}
