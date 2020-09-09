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

#ifndef __GREETER_GPKI_PAGE_H__
#define __GREETER_GPKI_PAGE_H__

#include "greeter-page.h"
#include "greeter-page-manager.h"

G_BEGIN_DECLS

#define GREETER_TYPE_GPKI_PAGE            (greeter_gpki_page_get_type ())
#define GREETER_GPKI_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GREETER_TYPE_GPKI_PAGE, GreeterGPKIPage))
#define GREETER_GPKI_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GREETER_TYPE_GPKI_PAGE, GreeterGPKIPageClass))
#define GREETER_IS_GPKI_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GREETER_TYPE_GPKI_PAGE))
#define GREETER_IS_GPKI_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GREETER_TYPE_GPKI_PAGE))
#define GREETER_GPKI_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GREETER_TYPE_GPKI_PAGE, GreeterGPKIPageClass))

typedef struct _GreeterGPKIPage        GreeterGPKIPage;
typedef struct _GreeterGPKIPageClass   GreeterGPKIPageClass;
typedef struct _GreeterGPKIPagePrivate GreeterGPKIPagePrivate;

struct _GreeterGPKIPageClass
{
	GreeterPageClass parent_class;
};

struct _GreeterGPKIPage
{
	GreeterPage parent;

	GreeterGPKIPagePrivate *priv;
};

GType greeter_gpki_page_get_type (void);

GreeterPage *greeter_prepare_gpki_page (GreeterPageManager *manager);

G_END_DECLS

#endif /* __GREETER_GPKI_PAGE_H__ */
