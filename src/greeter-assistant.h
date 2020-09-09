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

#ifndef __GREETER_ASSISTANT_H__
#define __GREETER_ASSISTANT_H__

#include "config.h"

#include <gtk/gtk.h>
#include <lightdm.h>

#include "greeter-page.h"

G_BEGIN_DECLS

#define GREETER_TYPE_ASSISTANT         (greeter_assistant_get_type ())
#define GREETER_ASSISTANT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GREETER_TYPE_ASSISTANT, GreeterAssistant))
#define GREETER_ASSISTANT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k),  GREETER_TYPE_ASSISTANT, GreeterAssistantClass))
#define GREETER_IS_ASSISTANT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GREETER_TYPE_ASSISTANT))
#define GREETER_IS_ASSISTANT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),  GREETER_TYPE_ASSISTANT))
#define GREETER_ASSISTANT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  GREETER_TYPE_ASSISTANT, GreeterAssistantClass))

typedef struct _GreeterAssistantPrivate GreeterAssistantPrivate;
typedef struct _GreeterAssistant        GreeterAssistant;
typedef struct _GreeterAssistantClass   GreeterAssistantClass;

struct _GreeterAssistant
{
	GtkBox __parent__;

	GreeterAssistantPrivate *priv;
};

struct _GreeterAssistantClass
{
	GtkBoxClass __parent_class__;
};


GType        greeter_assistant_get_type               (void) G_GNUC_CONST;

GtkWidget   *greeter_assistant_new                    (void);

void         greeter_assistant_add_page               (GreeterAssistant *assistant,
                                                       GreeterPage      *page);

void         greeter_assistant_next_page              (GreeterAssistant *assistant);
void         greeter_assistant_prev_page              (GreeterAssistant *assistant);
void         greeter_assistant_first_page             (GreeterAssistant *assistant);

const gchar *greeter_assistant_get_title              (GreeterAssistant *assistant);
GreeterPage *greeter_assistant_get_current_page       (GreeterAssistant *assistant);

G_END_DECLS

#endif /* __GREETER_ASSISTANT_H__ */
