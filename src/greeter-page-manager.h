/*
 * Copyright (C) 2015 - 2020 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __GREETER_PAGE_MANAGER_H__
#define __GREETER_PAGE_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GREETER_TYPE_PAGE_MANAGER         (greeter_page_manager_get_type ())
#define GREETER_PAGE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GREETER_TYPE_PAGE_MANAGER, GreeterPageManager))
#define GREETER_PAGE_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GREETER_TYPE_PAGE_MANAGER, GreeterPageManagerClass))
#define GREETER_IS_PAGE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GREETER_TYPE_PAGE_MANAGER))
#define GREETER_IS_PAGE_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GREETER_TYPE_PAGE_MANAGER))
#define GREETER_PAGE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GREETER_TYPE_PAGE_MANAGER, GreeterPageManagerClass))

typedef struct _GreeterPageManager        GreeterPageManager;
typedef struct _GreeterPageManagerClass   GreeterPageManagerClass;
typedef struct _GreeterPageManagerPrivate GreeterPageManagerPrivate;

enum
{
	MODE_ONLINE = 0,
	MODE_OFFLINE
};

struct _GreeterPageManager
{
	GObject          __parent__;

	GreeterPageManagerPrivate *priv;
};

struct _GreeterPageManagerClass
{
	GObjectClass __parent_class__;

	void (*go_next)      (GreeterPageManager *manager);
	void (*go_first)     (GreeterPageManager *manager);
	void (*mode_changed) (GreeterPageManager *manager, int mode);
};


GType               greeter_page_manager_get_type     (void);

GreeterPageManager *greeter_page_manager_new          (void);

int                 greeter_page_manager_get_mode     (GreeterPageManager *manager);
void                greeter_page_manager_set_mode     (GreeterPageManager *manager,
                                                       int                 mode);

void                greeter_page_manager_go_next      (GreeterPageManager *manager);
void                greeter_page_manager_go_first     (GreeterPageManager *manager);

void                greeter_page_manager_show_splash  (GreeterPageManager *manager,
                                                       GtkWidget          *parent,
                                                       const char         *message);
void                greeter_page_manager_hide_splash  (GreeterPageManager *manager);

void                greeter_page_manager_update_splash_message (GreeterPageManager *manager,
                                                                const char         *message);



G_END_DECLS

#endif /* __GREETER_PAGE_MANAGER_H */
