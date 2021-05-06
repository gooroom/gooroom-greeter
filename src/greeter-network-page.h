/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
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

#ifndef __GREETER_NETWORK_PAGE_H__
#define __GREETER_NETWORK_PAGE_H__

#include "greeter-page.h"
#include "greeter-page-manager.h"

G_BEGIN_DECLS

#define GREETER_TYPE_NETWORK_PAGE            (greeter_network_page_get_type ())
#define GREETER_NETWORK_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GREETER_TYPE_NETWORK_PAGE, GreeterNetworkPage))
#define GREETER_NETWORK_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GREETER_TYPE_NETWORK_PAGE, GreeterNetworkPageClass))
#define GREETER_IS_NETWORK_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GREETER_TYPE_NETWORK_PAGE))
#define GREETER_IS_NETWORK_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GREETER_TYPE_NETWORK_PAGE))
#define GREETER_NETWORK_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GREETER_TYPE_NETWORK_PAGE, GreeterNetworkPageClass))

typedef struct _GreeterNetworkPage        GreeterNetworkPage;
typedef struct _GreeterNetworkPageClass   GreeterNetworkPageClass;
typedef struct _GreeterNetworkPagePrivate GreeterNetworkPagePrivate;

struct _GreeterNetworkPageClass
{
	GreeterPageClass __parent_class__;
};

struct _GreeterNetworkPage
{
	GreeterPage __parent__;

	GreeterNetworkPagePrivate *priv;
};

GType greeter_network_page_get_type (void);

GreeterPage *greeter_prepare_network_page (GreeterPageManager *manager,
                                           GtkWidget          *parent);

G_END_DECLS

#endif /* __GREETER_NETWORK_PAGE_H__ */
