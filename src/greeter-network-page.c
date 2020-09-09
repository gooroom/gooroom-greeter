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

/* Network page {{{1 */

#define PAGE_ID "network"

#include "config.h"
//#include "network-resources.h"
#include "network-dialogs.h"
#include "greeter-network-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>


static gboolean refresh_wireless_list (GreeterNetworkPage *page);

typedef enum {
  NM_AP_SEC_UNKNOWN,
  NM_AP_SEC_NONE,
  NM_AP_SEC_WEP,
  NM_AP_SEC_WPA,
  NM_AP_SEC_WPA2
} NMAccessPointSecurity;

struct _GreeterNetworkPagePrivate {
  GtkWidget *network_list;
  GtkWidget *scrolled_window;
  GtkWidget *no_network_box;
  GtkWidget *no_device_box;
  GtkWidget *device_turn_on_box;
//  GtkWidget *no_network_label;
//  GtkWidget *no_network_spinner;
//  GtkWidget *turn_on_label;
  GtkWidget *turn_on_switch;

  NMClient *nm_client;
  NMDevice *nm_device;
  gboolean refreshing;
//  GtkSizeGroup *icons;

  guint refresh_timeout_id;

//  GMainLoop *loop;
};

G_DEFINE_TYPE_WITH_PRIVATE (GreeterNetworkPage, greeter_network_page, GREETER_TYPE_PAGE);


#if 0
static void
get_monitor_geometry (GtkWidget    *widget,
                      GdkRectangle *geometry)
{
	GdkDisplay *d;
	GdkWindow  *w;
	GdkMonitor *m;

	d = gdk_display_get_default ();
	w = gtk_widget_get_window (widget);
	m = gdk_display_get_monitor_at_window (d, w);

	gdk_monitor_get_geometry (m, geometry);
}

static gboolean
modal_window_button_pressed_event_cb (GtkWidget      *widget,
                                      GdkEventButton *event,
                                      gpointer        user_data)
{
	g_print ("modal_window_button_pressed_event_cb\n");
	return TRUE;
}

static void
modal_window_show (GreeterNetworkPage *page)
{
	GtkWidget *window;
    GtkWidget *toplevel;
	GdkRectangle geometry;

g_print ("modal_window_showwwwwwwwwww\n");

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

	get_monitor_geometry (toplevel, &geometry);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_accept_focus (GTK_WINDOW (window), FALSE);
//	gtk_window_set_keep_above (GTK_WINDOW (window), FALSE);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (window), TRUE);
	gtk_widget_set_app_paintable (window, TRUE);
	gtk_widget_set_size_request (window, geometry.width, geometry.height);
	gtk_window_move (GTK_WINDOW (window), geometry.x, geometry.y);

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
	if(gdk_screen_is_composited (screen)) {
		GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
		if (visual == NULL)
			visual = gdk_screen_get_system_visual (screen);

		gtk_widget_set_visual (GTK_WIDGET (window), visual);
	}

	gtk_widget_show (window);

	g_signal_connect (G_OBJECT (window), "button-press-event",
                      G_CALLBACK (modal_window_button_pressed_event_cb), page);
}
#endif

static GPtrArray *
get_strongest_unique_aps (const GPtrArray *aps)
{
  GBytes *ssid;
  GBytes *ssid_tmp;
  GPtrArray *unique = NULL;
  gboolean add_ap;
  guint i;
  guint j;
  NMAccessPoint *ap;
  NMAccessPoint *ap_tmp;

  /* we will have multiple entries for typical hotspots,
   * just keep the one with the strongest signal
   */
  unique = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  if (aps == NULL)
    goto out;

  for (i = 0; i < aps->len; i++) {
    ap = NM_ACCESS_POINT (g_ptr_array_index (aps, i));
    ssid = nm_access_point_get_ssid (ap);
    add_ap = TRUE;

    if (!ssid)
      continue;

    /* get already added list */
    for (j = 0; j < unique->len; j++) {
      ap_tmp = NM_ACCESS_POINT (g_ptr_array_index (unique, j));
      ssid_tmp = nm_access_point_get_ssid (ap_tmp);

      /* is this the same type and data? */
      if (ssid_tmp &&
          nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                              g_bytes_get_data (ssid_tmp, NULL), g_bytes_get_size (ssid_tmp), TRUE)) {
        /* the new access point is stronger */
        if (nm_access_point_get_strength (ap) >
            nm_access_point_get_strength (ap_tmp)) {
          g_ptr_array_remove (unique, ap_tmp);
          add_ap = TRUE;
        } else {
          add_ap = FALSE;
        }
        break;
      }
    }
    if (add_ap) {
      g_ptr_array_add (unique, g_object_ref (ap));
    }
  }

 out:
  return unique;
}

static guint
get_access_point_security (NMAccessPoint *ap)
{
  NM80211ApFlags flags;
  NM80211ApSecurityFlags wpa_flags;
  NM80211ApSecurityFlags rsn_flags;
  guint type;

  flags = nm_access_point_get_flags (ap);
  wpa_flags = nm_access_point_get_wpa_flags (ap);
  rsn_flags = nm_access_point_get_rsn_flags (ap);

  if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
      wpa_flags == NM_802_11_AP_SEC_NONE &&
      rsn_flags == NM_802_11_AP_SEC_NONE)
    type = NM_AP_SEC_NONE;
  else if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
           wpa_flags == NM_802_11_AP_SEC_NONE &&
           rsn_flags == NM_802_11_AP_SEC_NONE)
    type = NM_AP_SEC_WEP;
  else if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
           wpa_flags != NM_802_11_AP_SEC_NONE &&
           rsn_flags != NM_802_11_AP_SEC_NONE)
    type = NM_AP_SEC_WPA;
  else
    type = NM_AP_SEC_WPA2;

  return type;
}

static gint
ap_sort (GtkListBoxRow *a,
         GtkListBoxRow *b,
         gpointer data)
{
  GtkWidget *wa, *wb;
  guint sa, sb;

  wa = gtk_bin_get_child (GTK_BIN (a));
  wb = gtk_bin_get_child (GTK_BIN (b));

  sa = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (wa), "strength"));
  sb = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (wb), "strength"));
  if (sa > sb) return -1;
  if (sb > sa) return 1;

  return 0;
}

static void
update_header_func (GtkListBoxRow *child,
                    GtkListBoxRow *before,
                    gpointer       user_data)
{
  GtkWidget *header;

  if (before == NULL)
    return;

  header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_list_box_row_set_header (child, header);
  gtk_widget_show (header);
}

static void
add_access_point (GreeterNetworkPage *page, NMAccessPoint *ap, NMAccessPoint *active)
{
  GreeterNetworkPagePrivate *priv = page->priv;
  GBytes *ssid;
  GBytes *ssid_active = NULL;
  gchar *ssid_text;
  const gchar *object_path;
  gboolean activated, activating;
  guint security;
  guint strength;
  const gchar *icon_name;
  GtkWidget *row;
  GtkWidget *widget;
  GtkWidget *grid;
  GtkWidget *state_widget = NULL;

  ssid = nm_access_point_get_ssid (ap);
  object_path = nm_object_get_path (NM_OBJECT (ap));

  if (ssid == NULL || object_path == NULL)
    return;
  ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));

//g_print ("add access point ssid = %s\n", ssid_text);
//g_print ("add access point object_path = %s\n", object_path);

  if (active)
    ssid_active = nm_access_point_get_ssid (active);
  if (ssid_active && nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                                         g_bytes_get_data (ssid_active, NULL), g_bytes_get_size (ssid_active), TRUE)) {
    switch (nm_device_get_state (priv->nm_device))
      {
      case NM_DEVICE_STATE_PREPARE:
      case NM_DEVICE_STATE_CONFIG:
      case NM_DEVICE_STATE_NEED_AUTH:
      case NM_DEVICE_STATE_IP_CONFIG:
      case NM_DEVICE_STATE_SECONDARIES:
        activated = FALSE;
        activating = TRUE;
        break;
      case NM_DEVICE_STATE_ACTIVATED:
        activated = TRUE;
        activating = FALSE;
        break;
      default:
        activated = FALSE;
        activating = FALSE;
        break;
      }
  } else {
    activated = FALSE;
    activating = FALSE;
  }

  security = get_access_point_security (ap);
  strength = nm_access_point_get_strength (ap);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (row, 12);
  gtk_widget_set_margin_end (row, 12);
  widget = gtk_label_new (ssid_text);
  gtk_widget_set_margin_top (widget, 12);
  gtk_widget_set_margin_bottom (widget, 12);
  gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);

  if (activated) {
    state_widget = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
  } else if (activating) {
    state_widget = gtk_spinner_new ();
    gtk_widget_show (state_widget);
    gtk_spinner_start (GTK_SPINNER (state_widget));
  }

  if (state_widget) {
    gtk_widget_set_halign (state_widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (state_widget, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (row), state_widget, FALSE, FALSE, 0);
  }

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
  gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
//  gtk_size_group_add_widget (priv->icons, grid);
  gtk_box_pack_end (GTK_BOX (row), grid, FALSE, FALSE, 0);

  if (security != NM_AP_SEC_UNKNOWN &&
      security != NM_AP_SEC_NONE) {
    widget = gtk_image_new_from_icon_name ("network-wireless-encrypted-symbolic", GTK_ICON_SIZE_MENU);
    gtk_grid_attach (GTK_GRID (grid), widget, 0, 0, 1, 1);
  }

  if (strength < 20)
    icon_name = "network-wireless-signal-none-symbolic";
  else if (strength < 40)
    icon_name = "network-wireless-signal-weak-symbolic";
  else if (strength < 50)
    icon_name = "network-wireless-signal-ok-symbolic";
  else if (strength < 80)
    icon_name = "network-wireless-signal-good-symbolic";
  else
    icon_name = "network-wireless-signal-excellent-symbolic";
  widget = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
  gtk_widget_set_halign (widget, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (grid), widget, 1, 0, 1, 1);

  gtk_widget_show_all (row);

  /* if this connection is the active one or is being activated, then make sure
   * it's sorted before all others */
  if (activating || activated)
    strength = G_MAXUINT;

  g_object_set_data (G_OBJECT (row), "ap", ap);
//  g_object_set_data (G_OBJECT (row), "object-path", (gpointer) object_path);
//  g_object_set_data (G_OBJECT (row), "ssid", (gpointer) ssid);
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (strength));

  widget = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (priv->network_list), widget);
}

static void
add_access_point_other (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;
  GtkWidget *row;
  GtkWidget *widget;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_start (row, 12);
  gtk_widget_set_margin_end (row, 12);
  widget = gtk_label_new (C_("Wireless access point", "Other…"));
  gtk_widget_set_margin_top (widget, 12);
  gtk_widget_set_margin_bottom (widget, 12);
  gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);
  gtk_widget_show_all (row);

//  g_object_set_data (G_OBJECT (row), "object-path", "ap-other...");
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (0));

  gtk_container_add (GTK_CONTAINER (priv->network_list), row);
}

static void
cancel_periodic_refresh (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;

  if (priv->refresh_timeout_id == 0)
    return;

  g_debug ("Stopping periodic/scheduled Wi-Fi list refresh");

  g_clear_handle_id (&priv->refresh_timeout_id, g_source_remove);
}

static gboolean
refresh_again (gpointer user_data)
{
g_print ("refresh_againnnnnnnnnnn\n");
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  refresh_wireless_list (page);
  return G_SOURCE_REMOVE;
}

static void
start_periodic_refresh (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;
  static const guint periodic_wifi_refresh_timeout_sec = 5;

  cancel_periodic_refresh (page);

  g_debug ("Starting periodic Wi-Fi list refresh (every %u secs)",
           periodic_wifi_refresh_timeout_sec);
  priv->refresh_timeout_id = g_timeout_add_seconds (periodic_wifi_refresh_timeout_sec,
                                                    refresh_again, page);
}

/* Avoid repeated calls to refreshing the wireless list by making it refresh at
 * most once per second */
static void
schedule_refresh_wireless_list (GreeterNetworkPage *page)
{
//  static const guint refresh_wireless_list_timeout_sec = 1;
  GreeterNetworkPagePrivate *priv = page->priv;

  cancel_periodic_refresh (page);

//  g_debug ("Delaying Wi-Fi list refresh (for %u sec)",
//           refresh_wireless_list_timeout_sec);

  priv->refresh_timeout_id = g_timeout_add (700,
                                            (GSourceFunc) refresh_wireless_list,
                                            page);

//  priv->refresh_timeout_id = g_idle_add_full (G_PRIORITY_DEFAULT,
//                                              (GSourceFunc) refresh_wireless_list,
//                                              page, NULL);

//  priv->refresh_timeout_id = g_timeout_add_seconds (refresh_wireless_list_timeout_sec,
//                                                    (GSourceFunc) refresh_wireless_list,
//                                                    page);
}

static gboolean
refresh_wireless_list (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;
  NMAccessPoint *active_ap = NULL;
  NMAccessPoint *ap;
  const GPtrArray *aps;
  GPtrArray *unique_aps;
  guint i;
  GList *children, *l;
  gboolean enabled;
  gboolean fast_refresh = FALSE;

  g_debug ("Refreshing Wi-Fi networks list");

  priv->refreshing = TRUE;

  g_assert (NM_IS_DEVICE_WIFI (priv->nm_device));

  cancel_periodic_refresh (page);

  active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (priv->nm_device));

  children = gtk_container_get_children (GTK_CONTAINER (priv->network_list));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (priv->network_list), l->data);
  g_list_free (children);

  aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (priv->nm_device));
  enabled = nm_client_wireless_get_enabled (priv->nm_client);

  gtk_widget_hide (priv->no_network_box);
  gtk_widget_hide (priv->no_device_box);
  gtk_widget_show (priv->scrolled_window);

  if (aps == NULL || aps->len == 0) {
    gboolean hw_enabled;

    hw_enabled = nm_client_wireless_hardware_get_enabled (priv->nm_client);

    if (!enabled || !hw_enabled) {
      gtk_widget_show_all (priv->no_device_box);

      if (!hw_enabled)
        gtk_widget_hide (priv->device_turn_on_box);

    } else {
      gtk_widget_show (priv->no_network_box);
      fast_refresh = TRUE;
    }

    gtk_widget_hide (priv->scrolled_window);
    goto out;
  }

  unique_aps = get_strongest_unique_aps (aps);
  for (i = 0; i < unique_aps->len; i++) {
    ap = NM_ACCESS_POINT (g_ptr_array_index (unique_aps, i));
    add_access_point (page, ap, active_ap);
  }
  g_ptr_array_unref (unique_aps);
  add_access_point_other (page);

 out:

  if (enabled) {
    if (fast_refresh) {
      schedule_refresh_wireless_list (page);
    } else {
      start_periodic_refresh (page);
    }
  }

  priv->refreshing = FALSE;

  return G_SOURCE_REMOVE;
}

static gboolean
sync_complete (GreeterNetworkPage *page)
{
  gboolean activated;
  GreeterNetworkPagePrivate *priv = page->priv;

//  activated = (nm_device_get_state (priv->nm_device) == NM_DEVICE_STATE_ACTIVATED);
  activated = (nm_client_get_state (priv->nm_client) > NM_STATE_CONNECTING);

  greeter_page_set_complete (GREETER_PAGE (page), activated);
  schedule_refresh_wireless_list (page);

  return FALSE;
}

static void
connection_activate_cb (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
  NMClient *client = NM_CLIENT (object);
  NMActiveConnection *connection;
  GError *error = NULL;
//  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);

  connection = nm_client_activate_connection_finish (client, result, &error);
  if (connection) {
    g_object_unref (connection);
  } else {
    /* failed to activate */
    g_warning ("Failed to activate a connection: %s", error->message);
    g_error_free (error);
  }

g_print ("connection_activate_cbbbbbbbbbbb\n");

//  g_main_loop_quit (page->priv->loop);
}

static void
connection_add_activate_cb (GObject *object,
                            GAsyncResult *result,
                            gpointer user_data)
{
  NMClient *client = NM_CLIENT (object);
  NMActiveConnection *connection;
  GError *error = NULL;

g_print ("connection_add_activate_cbbbbbbbbbbb\n");
  connection = nm_client_add_and_activate_connection_finish (client, result, &error);
  if (connection) {
    g_object_unref (connection);
  } else {
    /* failed to activate */
    g_warning ("Failed to add and activate a connection: %s", error->message);
    g_error_free (error);
  }
}

static void
connect_to_hidden_network (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;
  cc_network_panel_connect_to_hidden_network (gtk_widget_get_toplevel (GTK_WIDGET (page)),
                                              priv->nm_client);
}

static void
row_activated (GtkListBox *box,
               GtkListBoxRow *row,
               GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;
  const gchar *object_path;
  const GPtrArray *list;
  GPtrArray *filtered;
  NMConnection *connection;
  NMConnection *connection_to_activate;
  NMSettingWireless *setting;
  GBytes *ssid;
  GBytes *ssid_target;
  GtkWidget *child;
  NMAccessPoint *ap;
  int i;

  if (priv->refreshing)
    return;

  child = gtk_bin_get_child (GTK_BIN (row));
  gpointer data = g_object_get_data (G_OBJECT (child), "ap");
  if (data) {
    ap = NM_ACCESS_POINT (data);
    object_path = nm_object_get_path (NM_OBJECT (ap));
    ssid_target = nm_access_point_get_ssid (ap);
  } else {
//    object_path = g_object_get_data (G_OBJECT (child), "object-path");
//    ssid_target = g_object_get_data (G_OBJECT (child), "ssid");
    connect_to_hidden_network (page);
    goto out;
  }

  if (object_path == NULL || object_path[0] == 0)
    return;

g_print ("object_path = %s\n", object_path);

//  if (g_strcmp0 (object_path, "ap-other...") == 0) {
//    connect_to_hidden_network (page);
//    goto out;
//  }

  list = nm_client_get_connections (priv->nm_client);
  filtered = nm_device_filter_connections (priv->nm_device, list);

  connection_to_activate = NULL;

  for (i = 0; i < filtered->len; i++) {
    connection = NM_CONNECTION (filtered->pdata[i]);
    setting = nm_connection_get_setting_wireless (connection);
    if (!NM_IS_SETTING_WIRELESS (setting))
      continue;

    ssid = nm_setting_wireless_get_ssid (setting);
    if (ssid == NULL)
      continue;

    if (nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                            g_bytes_get_data (ssid_target, NULL), g_bytes_get_size (ssid_target), TRUE)) {
      connection_to_activate = connection;
      break;
    }
  }
  g_ptr_array_unref (filtered);

//  modal_window_show (page);

  if (connection_to_activate != NULL) {
g_print ("connection_to_activate is not NULLLLL\n");
    nm_client_activate_connection_async (priv->nm_client,
                                         connection_to_activate,
                                         priv->nm_device, NULL,
                                         NULL,
                                         connection_activate_cb, page);

//    g_main_loop_run (priv->loop);
    return;
  }


g_print ("connection_to_activate is NULLLLL\n");
  nm_client_add_and_activate_connection_async (priv->nm_client,
                                               NULL,
                                               priv->nm_device, object_path,
                                               NULL,
                                               connection_add_activate_cb, page);

//  g_main_loop_run (priv->loop);

 out:
  schedule_refresh_wireless_list (page);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, GreeterNetworkPage *page)
{
  g_timeout_add (100, (GSourceFunc)sync_complete, page);
}

static void
active_connections_changed (NMClient *client, GParamSpec *pspec, GreeterNetworkPage *page)
{
  const GPtrArray *connections;
  guint i;

  connections = nm_client_get_active_connections (client);
  for (i = 0; connections && (i < connections->len); i++) {
    NMActiveConnection *connection;

    connection = g_ptr_array_index (connections, i);
    if (!g_object_get_data (G_OBJECT (connection), "has-state-changed-handler")) {
      g_signal_connect (connection, "notify::state",
                        G_CALLBACK (connection_state_changed), page);
      g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (1));
    }
  }
}

static void
device_state_changed (GObject *object, GParamSpec *param, GreeterNetworkPage *page)
{
//  sync_complete (page);
//  g_idle_add ((GSourceFunc)sync_complete, page);
  g_timeout_add (100, (GSourceFunc)sync_complete, page);
}

static void
greeter_network_page_constructed (GObject *object)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (object);
  GreeterNetworkPagePrivate *priv = page->priv;
  const GPtrArray *devices;
  NMDevice *device;
  guint i;
  gboolean visible = FALSE;
  GError *error = NULL;

  G_OBJECT_CLASS (greeter_network_page_parent_class)->constructed (object);

//  priv->icons = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  priv->nm_client = nm_client_new (NULL, &error);
  if (!priv->nm_client) {
    g_warning ("Can't create NetworkManager client, hiding network page: %s",
               error->message);
    g_error_free (error);
    goto out;
  }

  g_object_bind_property (priv->nm_client, "wireless-enabled",
                          priv->turn_on_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  devices = nm_client_get_devices (priv->nm_client);
  if (devices) {
    for (i = 0; i < devices->len; i++) {
      device = g_ptr_array_index (devices, i);

      if (!nm_device_get_managed (device))
        continue;

      if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI) {
        /* FIXME deal with multiple, dynamic devices */
        priv->nm_device = g_object_ref (device);
        break;
      }
    }
  }

  if (priv->nm_device == NULL) {
    g_debug ("No network device found, hiding network page");
    goto out;
  }

#if 0
  g_print ("nm_client_get_state = %d\n", nm_client_get_state (priv->nm_client));

g_print ("NM_STATE_UNKNOWN = %d\n", NM_STATE_UNKNOWN);
g_print ("NM_STATE_ASLEEP = %d\n", NM_STATE_ASLEEP);
g_print ("NM_STATE_DISCONNECTED = %d\n", NM_STATE_DISCONNECTED);
g_print ("NM_STATE_DISCONNECTING = %d\n", NM_STATE_DISCONNECTING);
g_print ("NM_STATE_CONNECTING = %d\n", NM_STATE_CONNECTING);
g_print ("NM_STATE_CONNECTED_LOCAL = %d\n", NM_STATE_CONNECTED_LOCAL);
g_print ("NM_STATE_CONNECTED_SITE = %d\n", NM_STATE_CONNECTED_SITE);
g_print ("NM_STATE_CONNECTED_GLOBAL = %d\n", NM_STATE_CONNECTED_GLOBAL);

g_print ("NM_DEVICE_STATE_UNKNOWN = %d\n", NM_DEVICE_STATE_UNKNOWN);
g_print ("NM_DEVICE_STATE_UNMANAGED = %d\n", NM_DEVICE_STATE_UNMANAGED);
g_print ("NM_DEVICE_STATE_UNAVAILABLE = %d\n", NM_DEVICE_STATE_UNAVAILABLE);
g_print ("NM_DEVICE_STATE_DISCONNECTED = %d\n", NM_DEVICE_STATE_DISCONNECTED);
g_print ("NM_DEVICE_STATE_PREPARE = %d\n", NM_DEVICE_STATE_PREPARE);
g_print ("NM_DEVICE_STATE_CONFIG = %d\n", NM_DEVICE_STATE_CONFIG);
g_print ("NM_DEVICE_STATE_NEED_AUTH = %d\n", NM_DEVICE_STATE_NEED_AUTH);
g_print ("NM_DEVICE_STATE_IP_CONFIG = %d\n", NM_DEVICE_STATE_IP_CONFIG);
g_print ("NM_DEVICE_STATE_IP_CHECK = %d\n", NM_DEVICE_STATE_IP_CHECK);
g_print ("NM_DEVICE_STATE_SECONDARIES = %d\n", NM_DEVICE_STATE_SECONDARIES);
g_print ("NM_DEVICE_STATE_ACTIVATED = %d\n", NM_DEVICE_STATE_ACTIVATED);
g_print ("NM_DEVICE_STATE_DEACTIVATING = %d\n", NM_DEVICE_STATE_DEACTIVATING);
g_print ("NM_DEVICE_STATE_FAILED = %d\n", NM_DEVICE_STATE_FAILED);
#endif

  visible = TRUE;

  g_signal_connect (priv->nm_device, "notify::state",
                    G_CALLBACK (device_state_changed), page);
  g_signal_connect (priv->nm_client, "notify::active-connections",
                    G_CALLBACK (active_connections_changed), page);

  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->network_list), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->network_list), update_header_func, NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->network_list), ap_sort, NULL, NULL);
  g_signal_connect (priv->network_list, "row-activated",
                    G_CALLBACK (row_activated), page);

  g_timeout_add (100, (GSourceFunc)sync_complete, page);

 out:
  gtk_widget_set_visible (GTK_WIDGET (page), visible);
}

static void
greeter_network_page_dispose (GObject *object)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (object);
  GreeterNetworkPagePrivate *priv = page->priv;

  g_clear_object (&priv->nm_client);
  g_clear_object (&priv->nm_device);
//  g_clear_object (&priv->icons);
//  g_main_loop_unref (priv->loop);

  cancel_periodic_refresh (page);

  G_OBJECT_CLASS (greeter_network_page_parent_class)->dispose (object);
}

static gboolean
greeter_network_page_should_show (GreeterPage *page)
{
  gboolean should_show = FALSE;
  GreeterNetworkPage *self = GREETER_NETWORK_PAGE (page);
  GreeterNetworkPagePrivate *priv = self->priv;
  GreeterPageManager *manager = page->manager;

  if (greeter_page_manager_get_mode (manager) == MODE_OFFLINE || !priv->nm_client)
    goto out;

  if (nm_client_get_state (priv->nm_client) > NM_STATE_CONNECTING)
    goto out;

  should_show = TRUE;

out:
  gtk_widget_set_visible (GTK_WIDGET (self), should_show);
  return should_show;
}

static void
greeter_network_page_init (GreeterNetworkPage *page)
{
  page->priv = greeter_network_page_get_instance_private (page);

  gtk_widget_init_template (GTK_WIDGET (page));

  greeter_page_set_title (GREETER_PAGE (page), _("Network Settings"));

//  page->priv->loop = g_main_loop_new (NULL, FALSE);
}

static void
greeter_network_page_class_init (GreeterNetworkPageClass *klass)
{
  GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/kr/gooroom/greeter/greeter-network-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, network_list);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, scrolled_window);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, no_network_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, no_device_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, device_turn_on_box);
//  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, no_network_label);
//  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, no_network_spinner);
//  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, turn_on_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GreeterNetworkPage, turn_on_switch);

  page_class->page_id = PAGE_ID;
  page_class->should_show  = greeter_network_page_should_show;

  object_class->constructed = greeter_network_page_constructed;
  object_class->dispose = greeter_network_page_dispose;
}

GreeterPage *
greeter_prepare_network_page (GreeterPageManager *manager)
{
  return g_object_new (GREETER_TYPE_NETWORK_PAGE,
                       "manager", manager,
                       NULL);
}
