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

#ifndef __GREETER_PAGE_H__
#define __GREETER_PAGE_H__

#include <gtk/gtk.h>

#include "greeter-page-manager.h"

G_BEGIN_DECLS

#define GREETER_TYPE_PAGE            (greeter_page_get_type ())
#define GREETER_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GREETER_TYPE_PAGE, GreeterPage))
#define GREETER_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GREETER_TYPE_PAGE, GreeterPageClass))
#define GREETER_IS_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GREETER_TYPE_PAGE))
#define GREETER_IS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GREETER_TYPE_PAGE))
#define GREETER_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GREETER_TYPE_PAGE, GreeterPageClass))

typedef struct _GreeterPage        GreeterPage;
typedef struct _GreeterPageClass   GreeterPageClass;
typedef struct _GreeterPagePrivate GreeterPagePrivate;


typedef void (* GreeterPageApplyCallback) (GreeterPage *page,
                                           gboolean     valid,
                                           gpointer     user_data);

struct _GreeterPage
{
	GtkBin __parent__;

	GtkWidget *parent;

	GreeterPageManager *manager;

	GreeterPagePrivate *priv;
};

struct _GreeterPageClass
{
	GtkBinClass __parent_class__;

	char *page_id;

	void      (*out)             (GreeterPage *page, gboolean next);
	void      (*shown)           (GreeterPage *page);
	gboolean  (*should_show)     (GreeterPage *page);
	gboolean  (*key_press_event) (GreeterPage *page, GdkEventKey *event);
};

GType greeter_page_get_type (void);

char *       greeter_page_get_title        (GreeterPage *page);
void         greeter_page_set_title        (GreeterPage *page,
                                            char        *title);

gboolean     greeter_page_get_complete     (GreeterPage *page);
void         greeter_page_set_complete     (GreeterPage *page,
                                            gboolean     complete);

void         greeter_page_out              (GreeterPage *page,
                                            gboolean     next);
void         greeter_page_shown            (GreeterPage *page);
gboolean     greeter_page_should_show      (GreeterPage *page);

gboolean     greeter_page_key_press_event  (GreeterPage *page,
                                            GdkEventKey *event);

G_END_DECLS

#endif /* __GREETER_PAGE_H__ */
