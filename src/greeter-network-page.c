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
#include "network-dialogs.h"
#include "greeter-network-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>


typedef enum {
  NM_AP_SEC_UNKNOWN,
  NM_AP_SEC_NONE,
  NM_AP_SEC_WEP,
  NM_AP_SEC_WPA,
  NM_AP_SEC_WPA2
} NMAccessPointSecurity;

struct _GreeterNetworkPagePrivate {
  GtkWidget *network_box;
  GtkWidget *device_frame;
  GtkWidget *device_list;
  GtkWidget *wired_item;
  GtkWidget *wired_label;
  GtkWidget *wired_switch;
  GtkWidget *wifi_item;
  GtkWidget *wifi_label;
  GtkWidget *wifi_switch;
  GtkWidget *wifi_list_frame;
  GtkWidget *wifi_list;
  GtkWidget *spinner;
  GtkWidget *no_nm_box;
  GtkWidget *no_device_box;
  GtkWidget *no_network_box;
  GtkWidget *exit_button1;
  GtkWidget *exit_button2;
  GtkWidget *network_enable_button;

  NMClient *nm_client;

  NMDevice *nm_device_eth;
  NMDevice *nm_device_wifi;

  gboolean refreshing;
  gboolean old_network_enabled;

  guint refresh_timeout_id;
};

static gboolean refresh_wireless_list (GreeterNetworkPage *page);
static gboolean wired_switch_toggled_cb (GtkSwitch *sw, gboolean state, gpointer user_data);

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
  return TRUE;
}

static void
modal_window_show (GreeterNetworkPage *page)
{
  GtkWidget *window;
    GtkWidget *toplevel;
  GdkRectangle geometry;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));

  get_monitor_geometry (toplevel, &geometry);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_accept_focus (GTK_WINDOW (window), FALSE);
//  gtk_window_set_keep_above (GTK_WINDOW (window), FALSE);
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

  if (active)
    ssid_active = nm_access_point_get_ssid (active);
  if (ssid_active && nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
                                         g_bytes_get_data (ssid_active, NULL), g_bytes_get_size (ssid_active), TRUE)) {
    switch (nm_device_get_state (priv->nm_device_wifi))
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
  gtk_widget_set_margin_top (widget, 6);
  gtk_widget_set_margin_bottom (widget, 6);
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
  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (strength));

  widget = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (priv->wifi_list), widget);
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
  gtk_widget_set_margin_top (widget, 6);
  gtk_widget_set_margin_bottom (widget, 6);
  gtk_box_pack_start (GTK_BOX (row), widget, FALSE, FALSE, 0);
  gtk_widget_show_all (row);

  g_object_set_data (G_OBJECT (row), "strength", GUINT_TO_POINTER (0));

  gtk_container_add (GTK_CONTAINER (priv->wifi_list), row);
}

static void
cancel_periodic_refresh (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;

  g_debug ("Stopping periodic/scheduled Wi-Fi list refresh");

  if (priv->refresh_timeout_id != 0) {
    g_clear_handle_id (&priv->refresh_timeout_id, g_source_remove);
    priv->refresh_timeout_id = 0;
  }
}

static gboolean
refresh_again (gpointer user_data)
{
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
  GreeterNetworkPagePrivate *priv = page->priv;

  cancel_periodic_refresh (page);

  priv->refresh_timeout_id = g_timeout_add (1000,
                                            (GSourceFunc) refresh_wireless_list,
                                            page);
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
  gboolean enabled, hw_enabled;
  gboolean fast_refresh = FALSE;

  g_debug ("Refreshing Wi-Fi networks list");

  priv->refreshing = TRUE;

  if (!NM_IS_DEVICE_WIFI (priv->nm_device_wifi))
    return G_SOURCE_REMOVE;

  cancel_periodic_refresh (page);

  active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (priv->nm_device_wifi));

  children = gtk_container_get_children (GTK_CONTAINER (priv->wifi_list));
  for (l = children; l; l = l->next)
    gtk_container_remove (GTK_CONTAINER (priv->wifi_list), l->data);
  g_list_free (children);

  enabled = nm_client_wireless_get_enabled (priv->nm_client);
  hw_enabled = nm_client_wireless_hardware_get_enabled (priv->nm_client);
  aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (priv->nm_device_wifi));

  gtk_widget_hide (priv->spinner);
  gtk_widget_show (priv->wifi_list_frame);

  g_debug ("Refreshing Wi-Fi networks list length = %d", aps->len);

  if (aps == NULL || aps->len == 0) {
    hw_enabled = nm_client_wireless_hardware_get_enabled (priv->nm_client);

    if (!enabled || !hw_enabled) {
      gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), FALSE);
      if (!hw_enabled)
        gtk_widget_set_sensitive (priv->wifi_item, FALSE);
    } else {
      gtk_widget_show (priv->spinner);
      fast_refresh = TRUE;
    }

    gtk_label_set_text (GTK_LABEL (priv->wifi_label), _("Wireless - No Use"));
    gtk_widget_hide (priv->wifi_list_frame);
    fast_refresh = TRUE;
    goto out;
  }

  gtk_label_set_text (GTK_LABEL (priv->wifi_label), _("Wireless - Use"));
  gtk_widget_set_sensitive (priv->wifi_item, TRUE);
  gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), TRUE);

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
  gboolean activated = FALSE;
  GreeterNetworkPagePrivate *priv = page->priv;

  if (priv->nm_device_eth) {
    activated = (nm_device_get_state (priv->nm_device_eth) == NM_DEVICE_STATE_ACTIVATED);
  }

  if (priv->nm_device_wifi) {
    activated |= (nm_device_get_state (priv->nm_device_wifi) == NM_DEVICE_STATE_ACTIVATED);
  }

  greeter_page_set_complete (GREETER_PAGE (page), activated);

  return FALSE;
}

static void
connection_activate_cb (GObject *object,
                        GAsyncResult *result,
                        gpointer user_data)
{
  GError *error = NULL;
  NMActiveConnection *connection;
  NMClient *client = NM_CLIENT (object);

  connection = nm_client_activate_connection_finish (client, result, &error);
  if (connection) {
    g_object_unref (connection);
  } else {
    /* failed to activate */
    g_warning ("Failed to activate a connection: %s", error->message);
    g_error_free (error);
  }
}

static void
connection_add_activate_cb (GObject *object,
                            GAsyncResult *result,
                            gpointer user_data)
{
  NMClient *client = NM_CLIENT (object);
  NMActiveConnection *connection;
  GError *error = NULL;

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
    connect_to_hidden_network (page);
    goto out;
  }

  if (object_path == NULL || object_path[0] == 0)
    return;

  list = nm_client_get_connections (priv->nm_client);
  filtered = nm_device_filter_connections (priv->nm_device_wifi, list);

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
    nm_client_activate_connection_async (priv->nm_client,
                                         connection_to_activate,
                                         priv->nm_device_wifi, NULL,
                                         NULL,
                                         connection_activate_cb, page);

    return;
  }

  nm_client_add_and_activate_connection_async (priv->nm_client,
                                               NULL,
                                               priv->nm_device_wifi, object_path,
                                               NULL,
                                               connection_add_activate_cb, page);


 out:
  schedule_refresh_wireless_list (page);
}

static void
device_state_changed_cb (NMDevice            *device,
                         NMDeviceState        new_state,
                         NMDeviceState        old_state,
                         NMDeviceStateReason  reason,
                         gpointer             user_data)

{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  GreeterNetworkPagePrivate *priv = page->priv;

  if (NM_IS_DEVICE_ETHERNET (device)) {
    const char *text;
    gboolean active = FALSE;
    NMDeviceState state = nm_device_get_state (device);
  
    switch (state) {
      case NM_DEVICE_STATE_UNMANAGED:
      case NM_DEVICE_STATE_UNAVAILABLE:
      case NM_DEVICE_STATE_DISCONNECTED:
      case NM_DEVICE_STATE_DEACTIVATING:
      case NM_DEVICE_STATE_FAILED:
        active = FALSE;
      break;

      default:
        active = TRUE;
      break;
    }
    text = active ? _("Wired - Connected") : _("Wired - Not Connected");
    gtk_label_set_text (GTK_LABEL (priv->wired_label), text);
    g_signal_handlers_block_by_func (priv->wired_switch, wired_switch_toggled_cb, page);
    gtk_switch_set_active (GTK_SWITCH (priv->wired_switch), active);
    g_signal_handlers_unblock_by_func (priv->wired_switch, wired_switch_toggled_cb, page);
  } else if (NM_IS_DEVICE_WIFI (device)) {
    schedule_refresh_wireless_list (page);
  }

  g_idle_add ((GSourceFunc)sync_complete, page);
}

static void
update_page_ui (GreeterNetworkPage *page)
{
  gboolean active;
  const char *text;
  GreeterNetworkPagePrivate *priv = page->priv;

  gtk_widget_hide (priv->device_frame);
  gtk_widget_hide (priv->wifi_list_frame);
  gtk_widget_hide (priv->spinner);
  gtk_widget_hide (priv->no_nm_box);
  gtk_widget_hide (priv->no_network_box);
  gtk_widget_hide (priv->no_device_box);
  gtk_widget_set_sensitive (priv->network_enable_button, TRUE);

  if (!priv->nm_client) {
    gtk_widget_show (priv->no_nm_box);
    return;
  }

  if (!nm_client_networking_get_enabled (priv->nm_client)) {
    gtk_widget_show (priv->no_network_box);
    return;
  }

  if (!priv->nm_device_eth && !priv->nm_device_wifi) {
    gtk_widget_show (priv->no_device_box);
    return;
  }

  gtk_widget_show (priv->device_frame);

  if (priv->nm_device_eth) {
    active = (nm_device_get_state (priv->nm_device_eth) == NM_DEVICE_STATE_ACTIVATED);
    text = active ? _("Wired - Connected") : _("Wired - Not Connected");
  } else {
    text = _("Wired - No Device");
  }
  gtk_label_set_text (GTK_LABEL (priv->wired_label), text);

  if (priv->nm_device_wifi) {
    active = (nm_device_get_state (priv->nm_device_wifi) == NM_DEVICE_STATE_ACTIVATED);
    text = active ? _("Wireless - Use") : _("Wireless - No Use");
  } else {
    text = _("Wireless - No Device");
  }
  gtk_label_set_text (GTK_LABEL (priv->wifi_label), text);

  gtk_widget_set_sensitive (priv->wired_item, priv->nm_device_eth != NULL);
  gtk_widget_set_sensitive (priv->wifi_item, priv->nm_device_wifi != NULL);
}

static void
client_device_added_cb (NMClient *client,
                        NMDevice *device,
                        gpointer  user_data)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  GreeterNetworkPagePrivate *priv = page->priv;

  if (NM_IS_DEVICE_ETHERNET (device)) {
    g_clear_object (&priv->nm_device_eth);
    priv->nm_device_eth = g_object_ref (device);
    g_signal_connect (priv->nm_device_eth, "state-changed",
                      G_CALLBACK (device_state_changed_cb), page);
  } else if (NM_IS_DEVICE_WIFI (device)) {
    g_clear_object (&priv->nm_device_wifi);
    priv->nm_device_wifi = g_object_ref (device);
    g_signal_connect (priv->nm_device_wifi, "state-changed",
                      G_CALLBACK (device_state_changed_cb), page);
  } else {
  }

  update_page_ui (page);
}

static void
client_device_removed_cb (NMClient *client,
                          NMDevice *device,
                          gpointer  user_data)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  GreeterNetworkPagePrivate *priv = page->priv;

  if (NM_IS_DEVICE_ETHERNET (device)) {
    g_clear_object (&priv->nm_device_eth);
  } else if (NM_IS_DEVICE_WIFI (device)) {
    g_clear_object (&priv->nm_device_wifi);
  } else {
  }

  update_page_ui (page);
}

static gboolean
gtk_main_quit_idle (gpointer user_data)
{
  gtk_main_quit ();

  return FALSE;
}

static void
exit_button_clicked_cb (GtkButton *button,
                         gpointer   user_data)
{
  g_timeout_add (300, gtk_main_quit_idle, NULL);
}

static void
network_enable_button_clicked_cb (GtkButton *button,
                                  gpointer   user_data)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);

  gtk_widget_set_sensitive (page->priv->network_enable_button, FALSE);
  nm_client_networking_set_enabled (page->priv->nm_client, TRUE, NULL);
}

/* return value must not be freed! */
static const gchar *
get_mac_address_of_device (NMDevice *device)
{
  const gchar *mac = NULL;

  switch (nm_device_get_device_type (device)) {
    case NM_DEVICE_TYPE_WIFI:
    {
      NMDeviceWifi *device_wifi = NM_DEVICE_WIFI (device);
      mac = nm_device_wifi_get_hw_address (device_wifi);
      break;
    }

    case NM_DEVICE_TYPE_ETHERNET:
    {
      NMDeviceEthernet *device_ethernet = NM_DEVICE_ETHERNET (device);
      mac = nm_device_ethernet_get_hw_address (device_ethernet);
      break;
    }

    default:
      break;
  }

  /* no MAC address found */
  return mac;
}

/* return value must be freed by caller with g_free() */
static gchar *
get_mac_address_of_connection (NMConnection *connection)
{
  if (!connection)
    return NULL;

  /* check the connection type */
  if (nm_connection_is_type (connection, NM_SETTING_WIRELESS_SETTING_NAME)) {
    /* check wireless settings */
    NMSettingWireless *s_wireless = nm_connection_get_setting_wireless (connection);
    if (!s_wireless)
      return NULL;
    return g_strdup (nm_setting_wireless_get_mac_address (s_wireless));
  } else if (nm_connection_is_type (connection, NM_SETTING_WIRED_SETTING_NAME)) {
    /* check wired settings */
    NMSettingWired *s_wired = nm_connection_get_setting_wired (connection);
    if (!s_wired)
      return NULL;
    return g_strdup (nm_setting_wired_get_mac_address (s_wired));
  }

  /* no MAC address found */
  return NULL;
}

/* returns TRUE if both MACs are equal */
static gboolean
compare_mac_device_with_mac_connection (NMDevice *device, NMConnection *connection)
{
  const gchar *mac_dev = NULL;
  gchar *mac_conn = NULL;

  mac_dev = get_mac_address_of_device (device);
  if (mac_dev != NULL) {
    mac_conn = get_mac_address_of_connection (connection);
    if (mac_conn) {
      /* compare both MACs */
      if (g_strcmp0 (mac_dev, mac_conn) == 0) {
        g_free (mac_conn);
        return TRUE;
      }
      g_free (mac_conn);
    }
  }
  return FALSE;
}

static GSList *
get_valid_connections (NMClient *client, NMDevice *device)
{
  GSList *valid;
  NMConnection *connection;
  NMSettingConnection *s_con;
  NMActiveConnection *ac;
  const char *active_uuid;
  const GPtrArray *all;
  GPtrArray *filtered;
  guint i;

  all = nm_client_get_connections (client);
  filtered = nm_device_filter_connections (device, all);

  ac = nm_device_get_active_connection (device);
  active_uuid = ac ? nm_active_connection_get_uuid (ac) : NULL;

  valid = NULL;
  for (i = 0; i < filtered->len; i++) {
    connection = g_ptr_array_index (filtered, i);
    s_con = nm_connection_get_setting_connection (connection);
    if (!s_con)
      continue;

    if (nm_setting_connection_get_master (s_con) &&
        g_strcmp0 (nm_setting_connection_get_uuid (s_con), active_uuid) != 0)
      continue;

    valid = g_slist_prepend (valid, connection);
  }
  g_ptr_array_free (filtered, FALSE);

  return g_slist_reverse (valid);
}

static NMConnection *
get_find_connection (NMClient *client, NMDevice *device)
{
  GSList *list, *iterator;
  NMActiveConnection *ac;
  NMConnection *connection = NULL;

  /* is the device available in a active connection? */
  ac = nm_device_get_active_connection (device);
  if (ac)
    return (NMConnection*) nm_active_connection_get_connection (ac);

  /* not found in active connections - check all available connections */
  list = get_valid_connections (client, device);
  if (list != NULL) {
    /* if list has only one connection, use this connection */
    if (g_slist_length (list) == 1) {
      connection = list->data;
      goto out;
    }

    /* is there connection with the MAC address of the device? */
    for (iterator = list; iterator; iterator = iterator->next) {
      connection = iterator->data;
      if (compare_mac_device_with_mac_connection (device, connection)) {
        goto out;
      }
    }
  }

  /* no connection found for the given device */
  connection = NULL;

out:
  g_slist_free (list);

  return connection;
}

static void
activate_connection_cb (GObject      *client,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GError *error = NULL;
  NMActiveConnection *active;
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  GreeterNetworkPagePrivate *priv = page->priv;

  active = nm_client_activate_connection_finish (NM_CLIENT (client), result, NULL);
  g_clear_object (&active);

  if (error) {
    g_warning ("Wired connection activating failed");
    g_error_free (error);

    g_signal_handlers_block_by_func (priv->wired_switch, wired_switch_toggled_cb, page);
    gtk_switch_set_active (GTK_SWITCH (priv->wired_switch), FALSE);
    g_signal_handlers_unblock_by_func (priv->wired_switch, wired_switch_toggled_cb, page);
    return;
  }

  gtk_widget_set_sensitive (priv->wired_switch, TRUE);
}

static gboolean
wired_switch_toggled_cb (GtkSwitch *sw,
                         gboolean   state,
                         gpointer   user_data)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  GreeterNetworkPagePrivate *priv = page->priv;

  if (!priv->nm_device_eth)
    return TRUE;

  gtk_widget_set_sensitive (priv->wired_switch, FALSE);

  if (gtk_switch_get_active (sw)) {
    NMConnection *connection = NULL;

    /* is the device available in a active connection? */
    connection = get_find_connection (priv->nm_client, priv->nm_device_eth);
    if (connection != NULL) {
      nm_client_activate_connection_async (priv->nm_client,
                                           connection, priv->nm_device_eth,
                                           NULL, NULL, activate_connection_cb, page);
    } 
    return FALSE;
  }

  nm_device_disconnect (priv->nm_device_eth, NULL, NULL);
  gtk_widget_set_sensitive (priv->wired_switch, TRUE);

  return FALSE;
}

static gboolean
greeter_network_page_should_show (GreeterPage *page)
{
  gboolean should_show = FALSE;
  GreeterPageManager *manager = page->manager;

  if (greeter_page_manager_get_mode (manager) == MODE_OFFLINE)
    goto out;

  should_show = TRUE;

out:
  return should_show;
}

static void
start_action_for_networking_enabled (GreeterNetworkPage *page)
{
  const GPtrArray *devices;
  GreeterNetworkPagePrivate *priv = page->priv;

  devices = nm_client_get_devices (priv->nm_client);
  if (devices) {
    guint i;
    for (i = 0; i < devices->len; i++) {
      NMDevice *device = g_ptr_array_index (devices, i);

      if (!nm_device_get_managed (device))
        continue;

      if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_ETHERNET) {
        priv->nm_device_eth = g_object_ref (device);
        continue;
      }

      if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI) {
        /* FIXME deal with multiple, dynamic devices */
        priv->nm_device_wifi = g_object_ref (device);
        continue;
      }
    }
  }

  if (priv->nm_device_wifi == NULL && priv->nm_device_eth == NULL) {
    g_warning ("No network device found, hiding network page");
    update_page_ui (page);
    return;
  }

  if (priv->nm_device_eth) {
    g_signal_connect (priv->nm_device_eth, "state-changed",
                      G_CALLBACK (device_state_changed_cb), page);

    device_state_changed_cb (priv->nm_device_eth,
                             NM_STATE_UNKNOWN,
                             NM_STATE_UNKNOWN,
                             NM_DEVICE_STATE_REASON_NONE, page);
  }

  if (priv->nm_device_wifi) {
    g_signal_connect (priv->nm_device_wifi, "state-changed",
                      G_CALLBACK (device_state_changed_cb), page);

    device_state_changed_cb (priv->nm_device_wifi,
                             NM_STATE_UNKNOWN,
                             NM_STATE_UNKNOWN,
                             NM_DEVICE_STATE_REASON_NONE, page);
  }
}

static void
start_action_for_networking_disabled (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv = page->priv;

  cancel_periodic_refresh (page);

  g_clear_object (&priv->nm_device_eth);
  g_clear_object (&priv->nm_device_wifi);
}

static void
nm_client_networking_enable_changed_cb (NMClient   *client,
                                        GParamSpec *pspec,
                                        gpointer    user_data)
{
  gboolean cur_network_enabled;
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (user_data);
  GreeterNetworkPagePrivate *priv = page->priv;

  cur_network_enabled = nm_client_networking_get_enabled (client);

  if (priv->old_network_enabled == cur_network_enabled)
    return;

  if (cur_network_enabled)  {
    priv->old_network_enabled = TRUE;
    start_action_for_networking_enabled (page);
  } else {
    priv->old_network_enabled = FALSE;
    start_action_for_networking_disabled (page);
  }

  update_page_ui (page);
}

static void
greeter_network_page_constructed (GObject *object)
{
  GError *error = NULL;
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (object);
  GreeterNetworkPagePrivate *priv = page->priv;

  G_OBJECT_CLASS (greeter_network_page_parent_class)->constructed (object);

  priv->nm_client = nm_client_new (NULL, &error);
  if (!priv->nm_client) {
    g_warning ("Can't create NetworkManager client, hiding network page: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  g_signal_connect (priv->nm_client, "device-added",
                    G_CALLBACK (client_device_added_cb), page);
  g_signal_connect (priv->nm_client, "device-removed",
                    G_CALLBACK (client_device_removed_cb), page);
  g_signal_connect (priv->nm_client, "notify::networking-enabled",
                    G_CALLBACK (nm_client_networking_enable_changed_cb), page);

  g_object_bind_property (priv->nm_client, "wireless-enabled",
                          priv->wifi_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  priv->old_network_enabled = nm_client_networking_get_enabled (priv->nm_client);

  if (!priv->old_network_enabled) {
    g_warning ("Networking is diabled , hiding network page");
    goto out;
  }

  start_action_for_networking_enabled (page);

out:
  update_page_ui (page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
greeter_network_page_dispose (GObject *object)
{
  GreeterNetworkPage *page = GREETER_NETWORK_PAGE (object);
  GreeterNetworkPagePrivate *priv = page->priv;

  cancel_periodic_refresh (page);

  g_clear_object (&priv->nm_client);
  g_clear_object (&priv->nm_device_eth);
  g_clear_object (&priv->nm_device_wifi);

  G_OBJECT_CLASS (greeter_network_page_parent_class)->dispose (object);
}

static void
greeter_network_page_init (GreeterNetworkPage *page)
{
  GreeterNetworkPagePrivate *priv;
  priv = page->priv = greeter_network_page_get_instance_private (page);

  priv->nm_device_eth = NULL;
  priv->nm_device_wifi = NULL;
  priv->old_network_enabled = TRUE;

  gtk_widget_init_template (GTK_WIDGET (page));

  greeter_page_set_title (GREETER_PAGE (page), _("Network Settings"));

  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->device_list), update_header_func, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->wifi_list), update_header_func, NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->wifi_list), ap_sort, NULL, NULL);

  g_signal_connect (priv->wired_switch, "state-set",
                    G_CALLBACK (wired_switch_toggled_cb), page);
  g_signal_connect (priv->wifi_list, "row-activated",
                    G_CALLBACK (row_activated), page);
  g_signal_connect (priv->exit_button1, "clicked",
                    G_CALLBACK (exit_button_clicked_cb), page);
  g_signal_connect (priv->exit_button2, "clicked",
                    G_CALLBACK (exit_button_clicked_cb), page);
  g_signal_connect (priv->network_enable_button, "clicked",
                    G_CALLBACK (network_enable_button_clicked_cb), page);
}

static void
greeter_network_page_class_init (GreeterNetworkPageClass *klass)
{
  GreeterPageClass *page_class = GREETER_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/kr/gooroom/greeter/greeter-network-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, network_box);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, device_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, device_list);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wired_item);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wired_label);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wired_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wifi_item);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wifi_label);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wifi_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wifi_list_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, wifi_list);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, spinner);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, no_nm_box);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, no_device_box);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, no_network_box);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, exit_button1);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, exit_button2);
  gtk_widget_class_bind_template_child_private (widget_class, GreeterNetworkPage, network_enable_button);

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
