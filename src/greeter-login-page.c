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

#define PAGE_ID "STEP 3"

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
	VPN_SERVICE_LOGIN_REQUEST_ERROR = 1015,
	VPN_SERVICE_INFO_ERROR          = 1016,
	VPN_SERVICE_TIMEOUT_ERROR       = 1017
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
	GtkWidget *msg_label;
	GtkWidget *infobar;

	GtkWidget *pw_dialog;

	gboolean prompted;
	gboolean prompt_active;
	gboolean changing_password;

	gchar *id;
	gchar *pw;
	gchar *current_session;
	gchar *current_language;

	/* Pending questions */
	GSList *pending_questions;

	LightDMGreeter *greeter;

// for vpn
	guint  vpn_dbus_watch_id;
	guint  vpn_dbus_signal_id;
	guint  splash_timeout_id;

	gboolean  vpn_service_enabled;

    GDBusProxy *vpn_dbus_proxy;
};


static void process_prompts      (GreeterLoginPage *page);
static void start_authentication (GreeterLoginPage *page, const gchar *username);
static void login_button_clicked_cb (GtkWidget *widget, gpointer user_data);
static void handle_vpn_login_result (GreeterLoginPage *page, int result);
static void try_to_login_system (GreeterLoginPage *page);


G_DEFINE_TYPE_WITH_PRIVATE (GreeterLoginPage, greeter_login_page, GREETER_TYPE_PAGE);



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
show_vpn_error_dialog (GreeterLoginPage *page)
{
	const gchar *title;
	const gchar *message;
	GtkWidget   *dialog, *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

	title = _("VPN Service Error");
	message = _("VPN service is down. Please enable VPN service and try again.");

	dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
                                         "dialog-warning-symbolic.symbolic",
                                         title,
                                         message);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Ok"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
update_message_label (GreeterLoginPage *page, LightDMMessageType type, const gchar *text)
{
	GreeterLoginPagePrivate *priv = page->priv;

	const gchar *str = (text != NULL) ? text : "";

	if (type == LIGHTDM_MESSAGE_TYPE_INFO)
		gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), GTK_MESSAGE_INFO);
	else
		gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), GTK_MESSAGE_ERROR);

	gtk_label_set_text (GTK_LABEL (priv->msg_label), str);
}

static void
update_vpn_login_error_message_label (GreeterLoginPage *page, int result)
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
		update_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR, message);
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
	message = _("Authentication is in progress.\nPlease wait...");
//	theme = (greeter_page_manager_get_mode (manager) == MODE_INTERNAL) ? "internal" : "external";

	greeter_page_manager_show_splash (manager, toplevel, message, NULL);

	g_signal_handlers_block_by_func (priv->login_button, login_button_clicked_cb, page);

	gtk_widget_set_sensitive (priv->id_entry, FALSE);
	gtk_widget_set_sensitive (priv->pw_entry, FALSE);
	gtk_widget_set_sensitive (priv->login_button, FALSE);

	update_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, NULL);

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
		update_vpn_login_error_message_label (page, result);
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
//
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
//	message = _("Initializing Settings for VPN.\nPlease wait...");
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

//	try_to_vpn_logout (page);
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

static void
display_warning_message (GreeterLoginPage *page, LightDMMessageType type, const gchar *msg)
{
	update_message_label (page, type, msg);

	start_authentication (page, lightdm_greeter_get_authentication_user (page->priv->greeter));
}

/* Message label */
static gboolean
message_label_is_empty (GtkWidget *label)
{
	return gtk_label_get_text (GTK_LABEL (label))[0] == '\0';
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
login_window_reset (GreeterLoginPage *page, GtkWidget *focus)
{
	gtk_entry_set_text (GTK_ENTRY (page->priv->pw_entry), "");
	update_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, NULL);
	gtk_widget_grab_focus (focus);
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

static void
show_message_dialog (GreeterLoginPage *page,
                     const gchar      *title,
                     const gchar      *message,
                     const gchar      *ok,
                     const gchar      *data)
{
	gint res;
	const gchar *ok_text;
	const gchar *response;
	GtkWidget *dialog, *toplevel;
	GreeterLoginPagePrivate *priv = page->priv;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

	dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
                                         "dialog-warning-symbolic.symbolic",
                                         title,
                                         message);

	ok_text = (ok) ? ok : _("_Ok");

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), ok_text, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show (dialog);
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (res == GTK_RESPONSE_OK) {
		if (g_str_equal (data, "CHPASSWD_FAILURE_OK")) {
			response = NULL;
			priv->changing_password = FALSE;
			login_window_reset (page, priv->pw_entry);
			start_authentication (page, lightdm_greeter_get_authentication_user (priv->greeter));
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
		} else {
			response = NULL;
		}
	} else {
		response = NULL;
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
	login_window_reset (page, priv->pw_entry);
	start_authentication (page, lightdm_greeter_get_authentication_user (priv->greeter));
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
ask_to_change_password (GreeterLoginPage *page,
                        const gchar      *title,
                        const gchar      *message,
                        const gchar      *yes,
                        const gchar      *no,
                        const gchar      *data)
{
	gint res;
	GtkWidget *dialog, *toplevel;
	const gchar *yes_text, *no_text;
	GreeterLoginPagePrivate *priv = page->priv;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

	dialog = greeter_message_dialog_new (GTK_WINDOW (toplevel),
                                         "dialog-password-symbolic",
                                         title,
                                         message);

	yes_text = (yes) ? yes : _("_Ok");
	no_text = (no) ? no : _("_Cancel");

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            yes_text, GTK_RESPONSE_OK,
                            no_text, GTK_RESPONSE_CANCEL,
                            NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show (dialog);
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

	priv->changing_password = FALSE;
	if (g_strcmp0 (data, "req_response") == 0) {
		if (lightdm_greeter_get_in_authentication (priv->greeter)) {
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
			lightdm_greeter_respond (priv->greeter, "chpasswd_no", NULL);
#else
			lightdm_greeter_respond (priv->greeter, "chpasswd_no");
#endif
			return;
		}
	}

out:
	priv->changing_password = FALSE;
	login_window_reset (page, priv->pw_entry);
	start_authentication (page, lightdm_greeter_get_authentication_user (priv->greeter));
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
		gtk_widget_show (priv->pw_entry);
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
		const gchar *filter_msg_060 = "Duplicate Login Notification";
		const gchar *filter_msg_070 = "Authentication Failure";
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
			ask_to_change_password (page, _("Password Expiration"),
					_("Your password has expired.\nPlease change your password immediately."),
					_("Changing Password"), _("Cancel"), "req_no_response");
			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_030)) {
			post_login (page);
			ask_to_change_password (page, _("Temporary Password Warning"),
					_("Your password has been issued temporarily.\nFor security reasons, please change your password immediately."),
					_("Changing Password"), _("Cancel"), "req_no_response");
			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_040)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 1) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Please change your password for security.\n"
                                           "If you do not change your password within %s day, "
                                           "your password expires.You can no longer log in.\n"
                                           "Do you want to change password now?"), tokens[1]);
				} else {
					msg = g_strdup_printf (_("Please change your password for security.\n"
                                           "If you do not change your password within %s days, "
                                           "your password expires.You can no longer log in.\n"
                                           "Do you want to change password now?"), tokens[1]);
				}
			} else {
				msg = g_strdup (_("Please change your password for security.\n"
                                "If you do not change your password within a few days, "
                                "your password expires.You can no longer log in.\n"
                                "Do you want to change password now?"));
			}
			g_strfreev (tokens);

			ask_to_change_password (page, _("Password Maxday Warning"), msg,
                                    _("Change now"), _("Later"), "req_response");
			g_free (msg);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_050)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Your account will not be available after %s.\n"
								"Your account will expire in %s day"),
							tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("Your account will not be available after %s.\n"
								"Your account will expire in %s days"),
							tokens[1], tokens[2]);
				}
			}
			g_strfreev (tokens);

			show_message_dialog (page, _("Account Expiration Warning"), msg, _("_Ok"), "ACCT_EXP_OK");
			g_free (msg);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_051)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Your organization will not be available after %s.\n"
								"Your organization will expire in %s day"),
							tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("Your organization will not be available after %s.\n"
								"Your organization will expire in %s days"),
							tokens[1], tokens[2]);
				}
			}
			g_strfreev (tokens);

			show_message_dialog (page, _("Division Expiration Warning"), msg, _("_Ok"), "DEPT_EXP_OK");
			g_free (msg);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_052)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 2) {
				if (g_str_equal (tokens[1], "1")) {
					msg = g_strdup_printf (_("Your password will not be available after %s.\n"
								"Your password will expire in %s day"),
							tokens[1], tokens[2]);
				} else {
					msg = g_strdup_printf (_("Your password will not be available after %s.\n"
								"Your password will expire in %s days"),
							tokens[1], tokens[2]);
				}
			}
			g_strfreev (tokens);

			show_message_dialog (page, _("Passowrd Expiration Warning"), msg, _("_Ok"), "PASS_EXP_OK");
			g_free (msg);

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

			show_message_dialog (page, _("Duplicate Login Notification"),
                                 msg->str, _("_Ok"), "DUPLICATE_LOGIN_OK");
			g_string_free (msg, TRUE);

			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_070)) {
			post_login (page);

			gchar *msg = NULL;
			gchar **tokens = g_strsplit (message->text, ":", -1);
			if (g_strv_length (tokens) > 1) {
				msg = g_strdup_printf (_("Authentication Failure\n\nYou have %s login attempts remaining.\n"
							"You can no longer log in when the maximum number of login attempts is exceeded."), tokens[1]);
			} else {
				msg = g_strdup_printf (_("Login Failure (Authentication Failure)"));
			}
			g_strfreev (tokens);
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_080)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("Your account has been locked because you have exceeded the number of login attempts.\n"
						"Please try again in a moment."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_090)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("This account has expired and is no longer available.\n"
						"Please contact the administrator."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_100)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("The password for your account has expired.\n"
						"Please contact the administrator."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_110)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("Login Failure (Duplicate Login)"));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_120)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("Due to the expiration of your organization, this account is no longer available.\nPlease contact the administrator."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_130)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("Login attempts exceeded the number of times, so you cannot login for a certain period of time.\nPlease try again in a moment."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_140)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("Trial period has expired."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_150)) {
			post_login (page);

			gchar *msg = g_strdup_printf (_("Time error occurred."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
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

			show_message_dialog (page, _("Trial Period Notification"), msg, _("_Ok"), "TRIAL_LOGIN_OK");
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
				update_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, message->text);
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
				title = _("Changing Password - [Step 1]");
				prompt_label = _("Enter current password :");
			} else if ((strstr (message->text, "New password: ") != NULL) ||
					(strstr (message->text, _("New password: ")) != NULL)) {
				title = _("Changing Password - [Step 2]");
				prompt_label = _("Enter new password :");
			} else if ((strstr (message->text, "Retype new password: ") != NULL) ||
					(strstr (message->text, _("Retype new password: ")) != NULL)) {
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
		} else {
			gtk_widget_show (priv->pw_entry);
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

	if (priv->pending_questions)
	{
		g_slist_free_full (priv->pending_questions, (GDestroyNotify) pam_message_finalize);
		priv->pending_questions = NULL;
	}

	config_set_string (STATE_SECTION_GREETER, STATE_KEY_LAST_USER, username);

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
	GreeterLoginPagePrivate *priv = page->priv;
	LightDMGreeter *greeter = priv->greeter;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	if (priv->current_language)
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_set_language (greeter, priv->current_language, NULL);
#else
		lightdm_greeter_set_language (greeter, priv->current_language);
#endif

	/* Remember last choice */
	config_set_string (STATE_SECTION_GREETER, STATE_KEY_LAST_SESSION, priv->current_session);

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
		update_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR, _("Failed to start session"));
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

	if (priv->pending_questions)
	{
		g_slist_free_full (priv->pending_questions, (GDestroyNotify) pam_message_finalize);
		priv->pending_questions = NULL;
	}

	if (lightdm_greeter_get_is_authenticated (greeter))
	{
		if (priv->pw_dialog)
		{
			gtk_widget_destroy (priv->pw_dialog);
			priv->pw_dialog = NULL;
		}

		if (priv->prompted)
		{
			start_session (page);
		}
		else
		{
			gtk_widget_hide (priv->pw_entry);
		}
	}
	else
	{
		/* If an error message is already printed we do not print it this statement
		 * The error message probably comes from the PAM module that has a better knowledge
		 * of the failure. */
		gboolean have_pam_error = !message_label_is_empty (priv->msg_label) &&
				gtk_info_bar_get_message_type (GTK_INFO_BAR (priv->infobar)) != GTK_MESSAGE_ERROR;
		if (priv->prompted)
		{
			if (!have_pam_error)
				update_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR,
                                   _("Login Failure (Authentication Failure)"));
			start_authentication (page, lightdm_greeter_get_authentication_user (greeter));
		}
		else
		{
			g_warning ("Failed to authenticate");
			if (!have_pam_error)
				update_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR, _("Failed to authenticate"));
		}

		if (priv->changing_password) {
			// remove password settings dialog
			if (priv->pw_dialog) {
				gtk_widget_destroy (priv->pw_dialog);
				priv->pw_dialog = NULL;
			}
			show_message_dialog (page, _("Failure Of Changing Password"),
                                 _("Failed to change password.\nPlease try again."),
                                 _("_Ok"),
                                 "CHPASSWD_FAILURE_OK");
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
login_button_clicked_cb (GtkWidget *widget,
                         gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	pre_login (page);

	if (greeter_page_manager_get_mode (manager) == MODE_EXTERNAL) {
		if (greeter_page_manager_get_is_vpn_logined (manager)) {
			try_to_login_system (page);
			return;
		}

		const gchar *id, *pw;
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

		id = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));
		pw = gtk_entry_get_text (GTK_ENTRY (priv->pw_entry));

		if (!try_to_login_vpn (page, ip, port, id, pw))
			handle_vpn_login_result (page, VPN_SERVICE_DAEMON_ERROR);

out:
		g_free (ip);
		g_free (port);

		return;
	}

	g_clear_pointer (&priv->id, g_free);
	g_clear_pointer (&priv->pw, g_free);

	priv->id = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->id_entry)));
	priv->pw = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)));

	try_to_login_system (page);
}

static void
pw_entry_activate_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);

	if (gtk_widget_get_sensitive (page->priv->login_button))
		login_button_clicked_cb (page->priv->login_button, page);
}

//static gboolean
//pw_entry_key_press_cb (GtkWidget   *widget,
//                       GdkEventKey *event,
//                       gpointer     user_data)
//{
//	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
//	GreeterLoginPagePrivate *priv = page->priv;
//
//	if ((event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down))
//	{
//		/* Back to login_win_username_entry if it is available */
//		if (event->keyval == GDK_KEY_Up &&
//            gtk_widget_get_visible (priv->id_entry) &&
//            widget == priv->pw_entry)
//		{
//			gtk_widget_grab_focus (priv->id_entry);
//			return TRUE;
//		}
//
//		return TRUE;
//	}
//
//	return FALSE;
//}
//static gboolean
//id_entry_focus_out_cb (GtkWidget *widget,
//                       GdkEvent  *event,
//                       gpointer   user_data)
//{
//	return FALSE;
//}
//static gboolean
//pw_entry_focus_in_cb (GtkWidget *widget,
//                      GdkEvent  *event,
//                      gpointer   user_data)
//{
//	return FALSE;
//}

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
	gchar *title;
	GreeterLoginPage *self = GREETER_LOGIN_PAGE (page);
	GreeterLoginPagePrivate *priv = self->priv;
	GreeterPageManager *manager = page->manager;

	priv->prompted = FALSE;
	priv->prompt_active = FALSE;
	priv->changing_password = FALSE;
	priv->pending_questions = NULL;
	priv->current_session = NULL;
	priv->current_language = NULL;

	if (greeter_page_manager_get_mode (manager) == MODE_INTERNAL) {
		title = _("Connecting from internal");
		remove_style_class (self, "external");
		add_style_class (self, "internal");
	} else {
		title = _("Connecting from external");
		remove_style_class (self, "internal");
		add_style_class (self, "external");
	}

	greeter_page_set_title (GREETER_PAGE (page), title);

	gtk_widget_set_visible (priv->id_entry, TRUE);
	gtk_widget_set_visible (priv->pw_entry, TRUE);
	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);
	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_widget_set_sensitive (priv->login_button, FALSE);
	gtk_widget_grab_focus (GTK_WIDGET (priv->id_entry));
	update_message_label (self, LIGHTDM_MESSAGE_TYPE_INFO, NULL);

	greeter_page_manager_set_is_vpn_logined (manager, FALSE);

	if (greeter_page_manager_get_mode (manager) == MODE_EXTERNAL && !priv->vpn_service_enabled) {
		show_vpn_error_dialog (self);
		greeter_page_manager_go_first (manager);
		return;
	}

	start_authentication (GREETER_LOGIN_PAGE (page), "*other");
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
	priv->changing_password = FALSE;
	priv->pending_questions = NULL;
	priv->current_session = NULL;
	priv->current_language = NULL;
	priv->id = NULL;
	priv->pw = NULL;

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

	g_signal_connect (priv->id_entry, "changed", G_CALLBACK (id_entry_changed_cb), page);
	g_signal_connect (priv->id_entry, "key-press-event", G_CALLBACK (id_entry_key_press_cb), page);
//	g_signal_connect (priv->id_entry, "focus-out-event", G_CALLBACK (id_entry_focus_out_cb), page);
//	g_signal_connect (priv->pw_entry, "key-press-event", G_CALLBACK (pw_entry_key_press_cb), page);
//	g_signal_connect (priv->pw_entry, "focus-in-event", G_CALLBACK (pw_entry_focus_in_cb), page);
	g_signal_connect (priv->pw_entry, "activate", G_CALLBACK (pw_entry_activate_cb), page);
	g_signal_connect (priv->login_button, "clicked", G_CALLBACK (login_button_clicked_cb), page);

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
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, msg_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, infobar);

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
