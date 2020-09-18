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


#define PAGE_ID "Login"

#include "config.h"
#include "greeter-login-page.h"
#include "greeterconfiguration.h"
#include "greeter-message-dialog.h"
#include "greeter-password-settings-dialog.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <pwd.h>
#include <lightdm.h>

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
	GtkWidget *pw_entry;
	GtkWidget *id_entry;
	GtkWidget *login_button;
	GtkWidget *login_button_icon;
	GtkWidget *msg_label;
	GtkWidget *infobar;

	GtkWidget *pw_dialog;

	gboolean prompted;
	gboolean prompt_active;
	gboolean changing_password;
	gboolean password_prompted;
	gboolean id_entry_focus_out;

	gchar *current_session;
	gchar *current_language;

	/* Pending questions */
	GSList *pending_questions;

	LightDMGreeter *greeter;
};


static void process_prompts      (GreeterLoginPage *page);
static void start_authentication (GreeterLoginPage *page, const gchar *username);


G_DEFINE_TYPE_WITH_PRIVATE (GreeterLoginPage, greeter_login_page, GREETER_TYPE_PAGE);


static gboolean
grab_focus_idle (gpointer data)
{
	gtk_widget_grab_focus (GTK_WIDGET (data));

	return FALSE;
}

static void
set_message_label (GreeterLoginPage *page, LightDMMessageType type, const gchar *text)
{
	GreeterLoginPagePrivate *priv = page->priv;

	if (type == LIGHTDM_MESSAGE_TYPE_INFO)
		gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), GTK_MESSAGE_INFO);
	else
		gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), GTK_MESSAGE_ERROR);

	const gchar *str = (text != NULL) ? text : "";
	gtk_label_set_text (GTK_LABEL (priv->msg_label), str);
}

static void
display_warning_message (GreeterLoginPage *page, LightDMMessageType type, const gchar *msg)
{
	set_message_label (page, type, msg);

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

static gboolean
is_valid_user (GreeterLoginPage  *page,
               const char        *user,
               char             **error)
{
	char *error_msg = NULL;
	gboolean ret = TRUE;
	gboolean is_local_user = TRUE;
	GreeterPageManager *manager = GREETER_PAGE (page)->manager;

	struct passwd *user_entry = getpwnam (user);
	if (user_entry) {
		char **tokens = g_strsplit (user_entry->pw_gecos, ",", -1);
		if (tokens && (g_strv_length (tokens) > 4)) {
			if (tokens[4]) {
				if (g_str_equal (tokens[4], "gooroom-account")) {
					is_local_user = FALSE;
				}
			}
		}
		g_strfreev (tokens);
	} else {
		is_local_user = FALSE;
	}

	if (greeter_page_manager_get_mode (manager) == MODE_OFFLINE) {
		if (!is_local_user) {
			error_msg = _("In offline mode, only system (local) users can log in. Please try again.");
			ret = FALSE;
		}
	} else {
		if (is_local_user) {
			error_msg = _("In online mode, only online users can log in. Please try again.");
			ret = FALSE;
		}
	}

	if (error && error_msg)
		*error = g_strdup (error_msg);

	return ret;
}

static void
login_window_reset (GreeterLoginPage *page, GtkWidget *focus)
{
	gtk_entry_set_text (GTK_ENTRY (page->priv->pw_entry), "");
	set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, NULL);
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
			const char *entry_text = greeter_password_settings_dialog_get_entry_text (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog));
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
	const char *yes_text, *no_text;
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
	const char *id;
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
			ask_to_change_password (page, _("Password Expiration"),
					_("Your password has expired.\nPlease change your password immediately."),
					_("Changing Password"), _("Cancel"), "req_no_response");
			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_030)) {
			ask_to_change_password (page, _("Temporary Password Warning"),
					_("Your password has been issued temporarily.\nFor security reasons, please change your password immediately."),
					_("Changing Password"), _("Cancel"), "req_no_response");
			continue;
		} else if (g_str_has_prefix (message->text, filter_msg_040)) {
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
			gchar *msg = g_strdup_printf (_("Your account has been locked because you have exceeded the number of login attempts.\n"
						"Please try again in a moment."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_090)) {
			gchar *msg = g_strdup_printf (_("This account has expired and is no longer available.\n"
						"Please contact the administrator."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_100)) {
			gchar *msg = g_strdup_printf (_("The password for your account has expired.\n"
						"Please contact the administrator."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_110)) {
			gchar *msg = g_strdup_printf (_("Login Failure (Duplicate Login)"));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_120)) {
			gchar *msg = g_strdup_printf (_("Due to the expiration of your organization, this account is no longer available.\nPlease contact the administrator."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_130)) {
			gchar *msg = g_strdup_printf (_("Login attempts exceeded the number of times, so you cannot login for a certain period of time.\nPlease try again in a moment."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_140)) {
			gchar *msg = g_strdup_printf (_("Trial period has expired."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_150)) {
			gchar *msg = g_strdup_printf (_("Time error occurred."));
			display_warning_message (page, LIGHTDM_MESSAGE_TYPE_ERROR, msg);
			g_free (msg);
			break;
		} else if (g_str_has_prefix (message->text, filter_msg_160)) {
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
			/* FIXME: this doesn't show multiple messages, but that was
			 * already the case before. */
			if (priv->changing_password) {
				if (priv->pw_dialog) {
					greeter_password_settings_dialog_set_message_label (GREETER_PASSWORD_SETTINGS_DIALOG (priv->pw_dialog), message->text);
				}
			} else {
//				set_message_label (page, message->type.message, message->text);
				set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, message->text);
			}
			continue;
        }

        if (priv->changing_password) {
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
			gtk_widget_grab_focus (priv->pw_entry);
			gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
#if 0
            if (message_label_is_empty (priv->msg_label) && priv->password_prompted)
            {
                /* No message was provided beforehand and this is not the
                 * first password prompt, so use the prompt as label,
                 * otherwise the user will be completely unclear of what
                 * is going on. Actually, the fact that prompt messages are
                 * not shown is problematic in general, especially if
                 * somebody uses a custom PAM module that wants to ask
                 * something different. */
                gchar *str = message->text;
                if (g_str_has_suffix (str, ": "))
                    str = g_strndup (str, strlen (str) - 2);
                else if (g_str_has_suffix (str, ":"))
                    str = g_strndup (str, strlen (str) - 1);
                set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, str);
                if (str != message->text)
                    g_free (str);
            }
#endif
        }

        priv->prompted = TRUE;
        priv->password_prompted = TRUE;
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
	priv->password_prompted = FALSE;
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
set_networking_enable (gboolean enabled)
{
	gchar *cmd = NULL;
	const char *on_off;

	on_off = enabled ? "on" : "off";
	cmd = g_strdup_printf ("%s --action %s", GREETER_NETWORK_CONTROL_HELPER, on_off);
	g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
	g_free (cmd);
}

static void
start_session (GreeterLoginPage *page)
{
	GreeterLoginPagePrivate *priv = page->priv;
	LightDMGreeter *greeter = priv->greeter;

	if (priv->current_language)
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_set_language (greeter, priv->current_language, NULL);
#else
		lightdm_greeter_set_language (greeter, priv->current_language);
#endif

	/* Remember last choice */
	config_set_string (STATE_SECTION_GREETER, STATE_KEY_LAST_SESSION, priv->current_session);

	//	greeter_background_save_xroot (greeter_background);

	GreeterPageManager *manager = GREETER_PAGE (page)->manager;
	set_networking_enable (greeter_page_manager_get_mode (manager) == MODE_ONLINE);

	if (!lightdm_greeter_start_session_sync (greeter, priv->current_session, NULL))
	{
		set_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR, _("Failed to start session"));
		start_authentication (page, lightdm_greeter_get_authentication_user (greeter));
		set_networking_enable (TRUE);
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

	priv->prompt_active = FALSE;
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");

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
			start_session (page);
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
				set_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR,
                                   _("Login Failure (Authentication Failure)"));
			start_authentication (page, lightdm_greeter_get_authentication_user (greeter));
		}
		else
		{
			g_warning ("Failed to authenticate");
			if (!have_pam_error)
				set_message_label (page, LIGHTDM_MESSAGE_TYPE_ERROR, _("Failed to authenticate"));
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


#if 0
static void
timed_autologin_cb (LightDMGreeter *greeter)
{
    /* Don't trigger autologin if user locks screen with light-locker (thanks to Andrew P.). */
    if (!lightdm_greeter_get_lock_hint (greeter))
    {
        if (lightdm_greeter_get_is_authenticated (greeter))
        {
            /* Configured autologin user may be already selected in user list. */
            if (lightdm_greeter_get_authentication_user (greeter))
                /* Selected user matches configured autologin-user option. */
                start_session ();
            else if (lightdm_greeter_get_autologin_guest_hint (greeter))
                /* "Guest session" is selected and autologin-guest is enabled. */
                start_session ();
            else if (lightdm_greeter_get_autologin_user_hint (greeter))
            {
                /* "Guest session" is selected, but autologin-user is configured. */
                start_authentication (lightdm_greeter_get_autologin_user_hint (greeter));
                prompted = TRUE;
            }
        }
        else
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
            lightdm_greeter_authenticate_autologin (greeter, NULL);
#else
            lightdm_greeter_authenticate_autologin (greeter);
#endif
    }
}
#endif

static void
login_button_clicked_cb (GtkWidget *widget,
                         gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;
	LightDMGreeter *greeter = priv->greeter;

	/* Reset to default screensaver values */
//	if (lightdm_greeter_get_lock_hint (greeter))
//		XSetScreenSaver (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), timeout, interval, prefer_blanking, allow_exposures);

	gtk_widget_set_sensitive (priv->id_entry, FALSE);
	gtk_widget_set_sensitive (priv->pw_entry, FALSE);
	gtk_widget_set_sensitive (priv->login_button, FALSE);
	set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, NULL);

	priv->prompt_active = FALSE;

	if (lightdm_greeter_get_is_authenticated (greeter)) {
		start_session (page);
	} else if (lightdm_greeter_get_in_authentication (greeter)) {
#ifdef HAVE_LIBLIGHTDMGOBJECT_1_19_2
		lightdm_greeter_respond (greeter, gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)), NULL);
#else
		lightdm_greeter_respond (greeter, gtk_entry_get_text (GTK_ENTRY (priv->pw_entry)));
#endif
		/* If we have questions pending, then we continue processing
		 * those, until we are done. (Otherwise, authentication will
		 * not complete.) */
		if (priv->pending_questions) {
			process_prompts (page);
		}
	} else {
		start_authentication (page, lightdm_greeter_get_authentication_user (greeter));
	}
}

static void
pw_entry_activate_cb (GtkWidget *widget,
                      gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);

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

static gboolean
id_entry_focus_out_cb (GtkWidget *widget,
                       GdkEvent  *event,
                       gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	priv->id_entry_focus_out = TRUE;

	return FALSE;
}

static gboolean
pw_entry_focus_in_cb (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   user_data)
{
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	if (priv->id_entry_focus_out) {
		priv->id_entry_focus_out = FALSE;

		const char *user;
		gchar *error_message = NULL;
		GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
		GreeterLoginPagePrivate *priv = page->priv;

		user = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));

		if (strlen (user) == 0)
			return FALSE;

		if (is_valid_user (page, user, &error_message)) {
			set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, NULL);
			start_authentication (page, gtk_entry_get_text (GTK_ENTRY (priv->id_entry)));
		} else {
			if (error_message) {
				set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, error_message);
				g_free (error_message);
			}
			start_authentication (GREETER_LOGIN_PAGE (page), "*other");
		}
	}

	return FALSE;
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
	const char *text;
	GreeterLoginPage *page = GREETER_LOGIN_PAGE (user_data);
	GreeterLoginPagePrivate *priv = page->priv;

	text = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));

	gtk_widget_set_sensitive (priv->login_button, strlen (text) > 0);
}

static void
setup_page_ui (GreeterLoginPage *page)
{
	GdkPixbuf *pixbuf = NULL;
	GreeterLoginPagePrivate *priv = page->priv;

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

	set_message_label (page, LIGHTDM_MESSAGE_TYPE_INFO, NULL);
}

static gboolean
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

	return lightdm_greeter_connect_sync (priv->greeter, NULL);
}

static void
greeter_login_page_out (GreeterPage *page,
                        gboolean     next)
{
	GreeterLoginPage *self = GREETER_LOGIN_PAGE (page);
	GreeterLoginPagePrivate *priv = self->priv;

	if (!next) {
		gtk_widget_set_visible (priv->id_entry, FALSE);
		gtk_widget_set_visible (priv->pw_entry, FALSE);
	}

	start_authentication (self, "*other");
}

static gboolean
greeter_login_page_should_show (GreeterPage *page)
{
	return TRUE;
}

static void
greeter_login_page_shown (GreeterPage *page)
{
	GreeterLoginPage *self = GREETER_LOGIN_PAGE (page);
	GreeterLoginPagePrivate *priv = self->priv;

	priv->prompted = FALSE;
	priv->prompt_active = FALSE;
	priv->password_prompted = FALSE;
	priv->changing_password = FALSE;
	priv->pending_questions = NULL;
	priv->current_session = NULL;
	priv->current_language = NULL;
	priv->id_entry_focus_out = FALSE;

	gtk_widget_set_visible (priv->id_entry, TRUE);
	gtk_widget_set_visible (priv->pw_entry, TRUE);
	gtk_widget_set_sensitive (priv->id_entry, TRUE);
	gtk_widget_set_sensitive (priv->pw_entry, TRUE);
	gtk_entry_set_text (GTK_ENTRY (priv->id_entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->pw_entry), "");
	gtk_widget_set_sensitive (priv->login_button, FALSE);

	set_message_label (self, LIGHTDM_MESSAGE_TYPE_INFO, NULL);

	start_authentication (GREETER_LOGIN_PAGE (page), "*other");

	g_idle_add (grab_focus_idle, priv->id_entry);
}

static void
greeter_login_page_dispose (GObject *object)
{
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
	priv->password_prompted = FALSE;
	priv->changing_password = FALSE;
	priv->pending_questions = NULL;
	priv->current_session = NULL;
	priv->current_language = NULL;
	priv->id_entry_focus_out = FALSE;

	greeter_page_set_title (GREETER_PAGE (page), _("Login System"));

	setup_page_ui (page);

	if (!lightdm_greeter_init (page)) {
	}

	g_signal_connect (priv->id_entry, "changed", G_CALLBACK (id_entry_changed_cb), page);
	g_signal_connect (priv->id_entry, "focus-out-event", G_CALLBACK (id_entry_focus_out_cb), page);
	g_signal_connect (priv->id_entry, "key-press-event", G_CALLBACK (id_entry_key_press_cb), page);
//	g_signal_connect (priv->pw_entry, "key-press-event", G_CALLBACK (pw_entry_key_press_cb), page);
	g_signal_connect (priv->pw_entry, "focus-in-event", G_CALLBACK (pw_entry_focus_in_cb), page);
	g_signal_connect (priv->pw_entry, "activate", G_CALLBACK (pw_entry_activate_cb), page);

	g_signal_connect (priv->login_button, "clicked", G_CALLBACK (login_button_clicked_cb), page);

	gtk_widget_show (GTK_WIDGET (page));

	greeter_page_set_complete (GREETER_PAGE (page), TRUE);
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
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, login_button_icon);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, msg_label);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterLoginPage, infobar);

	page_class->page_id = PAGE_ID;
	page_class->out = greeter_login_page_out;
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
