/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __MIMEVIEW_H__
#define __MIMEVIEW_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>

typedef struct _MimeView	MimeView;

#include "textview.h"
#include "imageview.h"
#include "messageview.h"
#include "procmime.h"

typedef enum
{
	MIMEVIEW_TEXT,
	MIMEVIEW_IMAGE
} MimeViewType;

struct _MimeView
{
	GtkWidget *notebook;
	GtkWidget *vbox;

	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mime_vbox;

	MimeViewType type;

	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;

	GtkCTreeNode *opened;

	TextView *textview;
	ImageView *imageview;

	MessageView *messageview;

	MimeInfo *mimeinfo;

	gchar *file;
};

MimeView *mimeview_create	(void);
void mimeview_init		(MimeView	*mimeview);
void mimeview_show_message	(MimeView	*mimeview,
				 MimeInfo	*mimeinfo,
				 const gchar	*file);
void mimeview_clear		(MimeView	*mimeview);
void mimeview_destroy		(MimeView	*mimeview);

MimeInfo *mimeview_get_selected_part	(MimeView	*mimeview);

gboolean mimeview_step			(MimeView	*mimeview,
					 GtkScrollType	 type);

void mimeview_pass_key_press_event	(MimeView	*mimeview,
					 GdkEventKey	*event);

#endif /* __MIMEVIEW_H__ */
