/*
 * Copyright (C) 2012 Red Hat
 * Copyright (C) 2015 - 2020 Gooroom <gooroom@gooroom.kr>
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

#include "config.h"

#include <glib-object.h>

#include "greeter-page.h"

struct _GreeterPagePrivate
{
	char *title;

	guint complete : 1;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GreeterPage, greeter_page, GTK_TYPE_BIN);

enum
{
	PROP_0,
	PROP_PARENT,
	PROP_MANAGER,
	PROP_TITLE,
	PROP_COMPLETE,
	PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

static void
greeter_page_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	GreeterPage *page = GREETER_PAGE (object);
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
	switch (prop_id)
	{
		case PROP_PARENT:
			g_value_set_pointer (value, (gpointer) page->parent);
		break;
		case PROP_MANAGER:
			g_value_set_pointer (value, (gpointer) page->manager);
		break;
		case PROP_TITLE:
			g_value_set_string (value, priv->title);
		break;
		case PROP_COMPLETE:
			g_value_set_boolean (value, priv->complete);
		break;
//		case PROP_SKIPPABLE:
//			g_value_set_boolean (value, priv->skippable);
//		break;
//		case PROP_NEEDS_ACCEPT:
//			g_value_set_boolean (value, priv->needs_accept);
//		break;
//		case PROP_APPLYING:
//			g_value_set_boolean (value, greeter_page_get_applying (page));
//		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_page_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	GreeterPage *page = GREETER_PAGE (object);
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
	switch (prop_id)
	{
		case PROP_PARENT:
			page->parent = g_value_get_pointer (value);
		break;
		case PROP_MANAGER:
			page->manager = g_value_get_pointer (value);
		break;
		case PROP_TITLE:
			greeter_page_set_title (page, (char *) g_value_get_string (value));
		break;
		case PROP_COMPLETE:
			priv->complete = g_value_get_boolean (value);
		break;
//		case PROP_SKIPPABLE:
//			priv->skippable = g_value_get_boolean (value);
//		break;
//		case PROP_NEEDS_ACCEPT:
//			priv->needs_accept = g_value_get_boolean (value);
//		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
greeter_page_finalize (GObject *object)
{
	GreeterPage *page = GREETER_PAGE (object);
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);

	g_free (priv->title);
//	g_assert (!priv->applying);
//	g_assert (priv->apply_cb == NULL);
//	g_assert (priv->apply_cancel == NULL);

	G_OBJECT_CLASS (greeter_page_parent_class)->finalize (object);
}

static void
greeter_page_dispose (GObject *object)
{
//	GreeterPage *page = GREETER_PAGE (object);
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);

//	if (priv->apply_cancel)
//		g_cancellable_cancel (priv->apply_cancel);

	G_OBJECT_CLASS (greeter_page_parent_class)->dispose (object);
}

static void
greeter_page_constructed (GObject *object)
{
//	GreeterPage *page = GREETER_PAGE (object);

	G_OBJECT_CLASS (greeter_page_parent_class)->constructed (object);
}

//static gboolean
//greeter_page_real_apply (GreeterPage      *page,
//                         GCancellable *cancellable)
//{
//	return FALSE;
//}

static void
greeter_page_class_init (GreeterPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = greeter_page_constructed;
	object_class->dispose = greeter_page_dispose;
	object_class->finalize = greeter_page_finalize;
	object_class->get_property = greeter_page_get_property;
	object_class->set_property = greeter_page_set_property;

//	klass->apply = greeter_page_real_apply;

	obj_props[PROP_PARENT] =
		g_param_spec_pointer ("parent", "", "",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	obj_props[PROP_MANAGER] =
		g_param_spec_pointer ("manager", "", "",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	obj_props[PROP_TITLE] =
		g_param_spec_string ("title", "", "", "",
				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
	obj_props[PROP_COMPLETE] =
		g_param_spec_boolean ("complete", "", "", FALSE,
				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
//	obj_props[PROP_SKIPPABLE] =
//		g_param_spec_boolean ("skippable", "", "", FALSE,
//				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
//	obj_props[PROP_NEEDS_ACCEPT] =
//		g_param_spec_boolean ("needs-accept", "", "", FALSE,
//				G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);
//	obj_props[PROP_APPLYING] =
//		g_param_spec_boolean ("applying", "", "", FALSE,
//				G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

	g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
greeter_page_init (GreeterPage *page)
{
//	gtk_widget_set_margin_start (GTK_WIDGET (page), 10);
//	gtk_widget_set_margin_top (GTK_WIDGET (page), 10);
//	gtk_widget_set_margin_bottom (GTK_WIDGET (page), 10);
//	gtk_widget_set_margin_end (GTK_WIDGET (page), 10);
}

char *
greeter_page_get_title (GreeterPage *page)
{
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
	if (priv->title != NULL)
		return priv->title;
	else
		return "";
}

void
greeter_page_set_title (GreeterPage *page, char *title)
{
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
	g_clear_pointer (&priv->title, g_free);
	priv->title = g_strdup (title);

	g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_TITLE]);
}

gboolean
greeter_page_get_complete (GreeterPage *page)
{
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
	return priv->complete;
}

void
greeter_page_set_complete (GreeterPage *page, gboolean complete)
{
	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
	priv->complete = complete;

	g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_COMPLETE]);
}

//gboolean
//greeter_page_get_skippable (GreeterPage *page)
//{
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//	return priv->skippable;
//}
//
//void
//greeter_page_set_skippable (GreeterPage *page, gboolean skippable)
//{
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//	priv->skippable = skippable;
//	g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_SKIPPABLE]);
//}
//
//gboolean
//greeter_page_get_needs_accept (GreeterPage *page)
//{
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//	return priv->needs_accept;
//}
//
//void
//greeter_page_set_needs_accept (GreeterPage *page, gboolean needs_accept)
//{
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//	priv->needs_accept = needs_accept;
//	g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_NEEDS_ACCEPT]);
//}

//void
//greeter_page_apply_begin (GreeterPage              *page,
//                          GreeterPageApplyCallback  callback,
//                          gpointer                  user_data)
//{
//	GreeterPageClass *klass;
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//
//	g_return_if_fail (GREETER_IS_PAGE (page));
//	g_return_if_fail (priv->applying == FALSE);
//
//	klass = GREETER_PAGE_GET_CLASS (page);
//
//	priv->apply_cb = callback;
//	priv->apply_data = user_data;
//	priv->apply_cancel = g_cancellable_new ();
//	priv->applying = TRUE;
//
//	if (!klass->apply (page, priv->apply_cancel))
//	{
//		/* Shortcut case where we don't want apply, to avoid flicker */
//		greeter_page_apply_complete (page, TRUE);
//	}
//
//	g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_APPLYING]);
//}
//
//void
//greeter_page_apply_complete (GreeterPage *page,
//                             gboolean valid)
//{
//	GreeterPageApplyCallback callback;
//	gpointer user_data;
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//
//	g_return_if_fail (GREETER_IS_PAGE (page));
//	g_return_if_fail (priv->applying == TRUE);
//
//	callback = priv->apply_cb;
//	priv->apply_cb = NULL;
//	user_data = priv->apply_data;
//	priv->apply_data = NULL;
//
//	g_clear_object (&priv->apply_cancel);
//	priv->applying = FALSE;
//	g_object_notify_by_pspec (G_OBJECT (page), obj_props[PROP_APPLYING]);
//
//	if (callback)
//		(callback) (page, valid, user_data);
//}
//
//gboolean
//greeter_page_get_applying (GreeterPage *page)
//{
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//	return priv->applying;
//}
//
//void
//greeter_page_apply_cancel (GreeterPage *page)
//{
//	GreeterPagePrivate *priv = greeter_page_get_instance_private (page);
//	g_cancellable_cancel (priv->apply_cancel);
//}

//void
//greeter_page_save_data (GreeterPage *page)
//{
//	if (GREETER_PAGE_GET_CLASS (page)->save_data)
//		GREETER_PAGE_GET_CLASS (page)->save_data (page);
//}

void
greeter_page_shown (GreeterPage *page)
{
	if (GREETER_PAGE_GET_CLASS (page)->shown)
		GREETER_PAGE_GET_CLASS (page)->shown (page);
}

void
greeter_page_out (GreeterPage *page,
                  gboolean     next)
{
	if (GREETER_PAGE_GET_CLASS (page)->out)
		GREETER_PAGE_GET_CLASS (page)->out (page, next);
}

gboolean
greeter_page_should_show (GreeterPage *page)
{
	if (GREETER_PAGE_GET_CLASS (page)->should_show)
		return GREETER_PAGE_GET_CLASS (page)->should_show (page);

	return TRUE;
}

gboolean
greeter_page_key_press_event (GreeterPage *page,
                              GdkEventKey *event)
{
	if (GREETER_PAGE_GET_CLASS (page)->key_press_event)
		return GREETER_PAGE_GET_CLASS (page)->key_press_event (page, event);

	return FALSE;
}
