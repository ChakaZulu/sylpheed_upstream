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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkstock.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "main.h"
#include "menu.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "messageview.h"
#include "foldersel.h"
#include "procmsg.h"
#include "procheader.h"
#include "sourcewindow.h"
#include "prefs_common.h"
#include "prefs_summary_column.h"
#include "prefs_filter.h"
#include "account.h"
#include "compose.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "statusbar.h"
#include "filter.h"
#include "folder.h"
#include "colorlabel.h"
#include "inc.h"
#include "imap.h"

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->summaryview_cid, str); \
	gtkut_widget_draw_now(mainwin->statusbar); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->summaryview_cid); \
}

#define GET_MSG_INFO(msginfo, iter__) \
{ \
	gtk_tree_model_get(GTK_TREE_MODEL(summaryview->store), iter__, \
			   S_COL_MSG_INFO, &msginfo, -1); \
}

#define SUMMARY_COL_MARK_WIDTH		21
#define SUMMARY_COL_UNREAD_WIDTH	24
#define SUMMARY_COL_MIME_WIDTH		17

static GdkPixbuf *mark_pixbuf;
static GdkPixbuf *deleted_pixbuf;

static GdkPixbuf *mail_pixbuf;
static GdkPixbuf *new_pixbuf;
static GdkPixbuf *unread_pixbuf;
static GdkPixbuf *replied_pixbuf;
static GdkPixbuf *forwarded_pixbuf;

static GdkPixbuf *clip_pixbuf;

static void summary_msgid_table_create	(SummaryView		*summaryview);
static void summary_msgid_table_destroy	(SummaryView		*summaryview);

static void summary_set_menu_sensitive	(SummaryView		*summaryview);

static void summary_scroll_to_selected	(SummaryView		*summaryview);

static guint summary_get_msgnum		(SummaryView		*summaryview,
					 GtkTreeRowReference	*row);

static gboolean summary_find_prev_msg	(SummaryView		*summaryview,
					 GtkTreeIter		*prev,
					 GtkTreeIter		*iter);
static gboolean summary_find_next_msg	(SummaryView		*summaryview,
					 GtkTreeIter		*next,
					 GtkTreeIter		*iter);

static gboolean summary_find_nearest_msg(SummaryView		*summaryview,
					 GtkTreeIter		*target,
					 GtkTreeIter		*iter);

static gboolean summary_find_prev_flagged_msg
					(SummaryView	*summaryview,
					 GtkTreeIter	*prev,
					 GtkTreeIter	*iter,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_prev);
static gboolean summary_find_next_flagged_msg
					(SummaryView	*summaryview,
					 GtkTreeIter	*next,
					 GtkTreeIter	*iter,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_next);

static gboolean summary_find_msg_by_msgnum
					(SummaryView		*summaryview,
					 guint			 msgnum,
					 GtkTreeIter		*found);

static void summary_update_status	(SummaryView		*summaryview);

/* display functions */
static void summary_status_show		(SummaryView		*summaryview);
static void summary_set_column_titles	(SummaryView		*summaryview);
static void summary_set_tree_model_from_list
					(SummaryView		*summaryview,
					 GSList			*mlist);
static void summary_display_msg		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_display_msg_full	(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 gboolean		 new_window,
					 gboolean		 all_headers,
					 gboolean		 redisplay);

static void summary_activate_selected	(SummaryView		*summaryview);

/* message handling */
static void summary_mark_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_mark_row_as_read	(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_mark_row_as_unread	(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_delete_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_unmark_row		(SummaryView		*summaryview,
					 GtkTreeIter		*iter);
static void summary_move_row_to		(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 FolderItem		*to_folder);
static void summary_copy_row_to		(SummaryView		*summaryview,
					 GtkTreeIter		*iter,
					 FolderItem		*to_folder);

static void summary_remove_invalid_messages
					(SummaryView		*summaryview);

static gint summary_execute_move	(SummaryView		*summaryview);
static gint summary_execute_copy	(SummaryView		*summaryview);
static gint summary_execute_delete	(SummaryView		*summaryview);

static void summary_modify_threads	(SummaryView		*summaryview);

static void summary_colorlabel_menu_item_activate_cb
					(GtkWidget	*widget,
					 gpointer	 data);
static void summary_colorlabel_menu_item_activate_item_cb
					(GtkMenuItem	*label_menu_item,
					 gpointer	 data);
static void summary_colorlabel_menu_create
					(SummaryView	*summaryview);

static GtkWidget *summary_tree_view_create
					(SummaryView	*summaryview);

/* callback functions */
static gboolean summary_toggle_pressed	(GtkWidget		*eventbox,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_pressed	(GtkWidget		*treeview,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_released	(GtkWidget		*treeview,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_key_pressed	(GtkWidget		*treeview,
					 GdkEventKey		*event,
					 SummaryView		*summaryview);

static void summary_row_expanded	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 SummaryView		*summaryview);
static void summary_row_collapsed	(GtkTreeView		*treeview,
					 GtkTreeIter		*iter,
					 GtkTreePath		*path,
					 SummaryView		*summaryview);

static gboolean summary_select_func	(GtkTreeSelection	*treeview,
					 GtkTreeModel		*model,
					 GtkTreePath		*path,
					 gboolean		 cur_selected,
					 gpointer		 data);

static void summary_selection_changed	(GtkTreeSelection	*selection,
					 SummaryView		*summaryview);

static void summary_col_resized		(GtkWidget		*widget,
					 GtkAllocation		*allocation,
					 SummaryView		*summaryview);

static void summary_reply_cb		(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);
static void summary_show_all_header_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);

static void summary_add_address_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);

static void summary_column_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);

static void summary_drag_begin		(GtkWidget	  *widget,
					 GdkDragContext	  *drag_context,
					 SummaryView	  *summaryview);
static void summary_drag_data_get       (GtkWidget        *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint             info,
					 guint             time,
					 SummaryView      *summaryview);

/* custom compare functions for sorting */
static gint summary_cmp_by_mark		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_unread	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_mime		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_num		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_size		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_date		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_from		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_label	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_to		(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);
static gint summary_cmp_by_subject	(GtkTreeModel		*model,
					 GtkTreeIter		*a,
					 GtkTreeIter		*b,
					 gpointer		 data);


/* must be synched with FolderSortKey */
static SummaryColumnType sort_key_to_col[] = {
	-1,
	S_COL_NUMBER,
	S_COL_SIZE,
	S_COL_DATE,
	S_COL_FROM,
	S_COL_SUBJECT,
	-1,
	S_COL_LABEL,
	S_COL_MARK,
	S_COL_UNREAD,
	S_COL_MIME,
	S_COL_TO
};

/* must be synched with SummaryColumnType */
static FolderSortKey col_to_sort_key[] = {
	SORT_BY_MARK,
	SORT_BY_UNREAD,
	SORT_BY_MIME,
	SORT_BY_SUBJECT,
	SORT_BY_FROM,
	SORT_BY_DATE,
	SORT_BY_SIZE,
	SORT_BY_NUMBER,
};

GtkTargetEntry summary_drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
};

static GtkItemFactoryEntry summary_popup_entries[] =
{
	{N_("/_Reply"),			NULL, summary_reply_cb,	COMPOSE_REPLY, NULL},
	{N_("/Repl_y to"),		NULL, NULL,		0, "<Branch>"},
	{N_("/Repl_y to/_all"),		NULL, summary_reply_cb,	COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/Repl_y to/_sender"),	NULL, summary_reply_cb,	COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/Repl_y to/mailing _list"),
					NULL, summary_reply_cb,	COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Forward"),		NULL, summary_reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/For_ward as attachment"),	NULL, summary_reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/Redirec_t"),		NULL, summary_reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/M_ove..."),		NULL, summary_move_to,	0, NULL},
	{N_("/_Copy..."),		NULL, summary_copy_to,	0, NULL},
	{N_("/_Delete"),		NULL, summary_delete,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_Mark/_Mark"),		NULL, summary_mark,	0, NULL},
	{N_("/_Mark/_Unmark"),		NULL, summary_unmark,	0, NULL},
	{N_("/_Mark/---"),		NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark/Mark as unr_ead"),	NULL, summary_mark_as_unread, 0, NULL},
	{N_("/_Mark/Mark as rea_d"),
					NULL, summary_mark_as_read, 0, NULL},
	{N_("/_Mark/Mark all _read"),	NULL, summary_mark_all_read, 0, NULL},
	{N_("/Color la_bel"),		NULL, NULL,		0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Re-_edit"),		NULL, summary_reedit,   0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Add sender to address boo_k"),
					NULL, summary_add_address_cb, 0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_View"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_View/Open in new _window"),
					NULL, summary_open_msg,	0, NULL},
	{N_("/_View/_Source"),		NULL, summary_view_source, 0, NULL},
	{N_("/_View/All _header"),	NULL, summary_show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Print..."),		NULL, summary_print,	0, NULL}
};


SummaryView *summary_create(void)
{
	SummaryView *summaryview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkWidget *hbox;
	GtkWidget *hbox_l;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *hbox_spc;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	gint n_entries;
	GList *child;

	debug_print(_("Creating summary view...\n"));
	summaryview = g_new0(SummaryView, 1);

	vbox = gtk_vbox_new(FALSE, 2);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(vbox,
				    prefs_common.summaryview_width,
				    prefs_common.summaryview_height);

	treeview = summary_tree_view_create(summaryview);
	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	store = GTK_TREE_STORE
		(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	/* create status label */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	hbox_l = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox_l, TRUE, TRUE, 0);

	statlabel_folder = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_folder, FALSE, FALSE, 2);
	statlabel_select = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_select, FALSE, FALSE, 12);

	/* toggle view button */
	toggle_eventbox = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(hbox), toggle_eventbox, FALSE, FALSE, 4);
	toggle_arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(toggle_eventbox), toggle_arrow);
	g_signal_connect(G_OBJECT(toggle_eventbox), "button_press_event",
			 G_CALLBACK(summary_toggle_pressed), summaryview);

	statlabel_msgs = gtk_label_new("");
	gtk_box_pack_end(GTK_BOX(hbox), statlabel_msgs, FALSE, FALSE, 4);

	hbox_spc = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), hbox_spc, FALSE, FALSE, 6);

	/* create popup menu */
	n_entries = sizeof(summary_popup_entries) /
		sizeof(summary_popup_entries[0]);
	popupmenu = menu_create_items(summary_popup_entries, n_entries,
				      "<SummaryView>", &popupfactory,
				      summaryview);

	summaryview->vbox = vbox;
	summaryview->scrolledwin = scrolledwin;
	summaryview->treeview = treeview;
	summaryview->store = store;
	summaryview->selection = selection;
	summaryview->hbox = hbox;
	summaryview->hbox_l = hbox_l;
	summaryview->statlabel_folder = statlabel_folder;
	summaryview->statlabel_select = statlabel_select;
	summaryview->statlabel_msgs = statlabel_msgs;
	summaryview->toggle_eventbox = toggle_eventbox;
	summaryview->toggle_arrow = toggle_arrow;
	summaryview->popupmenu = popupmenu;
	summaryview->popupfactory = popupfactory;
	summaryview->lock_count = 0;

	summaryview->reedit_menuitem =
		gtk_item_factory_get_widget(popupfactory, "/Re-edit");
	child = g_list_find(GTK_MENU_SHELL(popupmenu)->children,
			    summaryview->reedit_menuitem);
	summaryview->reedit_separator = GTK_WIDGET(child->next->data);

	gtk_widget_show_all(vbox);

	return summaryview;
}

void summary_init(SummaryView *summaryview)
{
	GtkWidget *pixmap;
	PangoFontDescription *font_desc;
	gint size;

	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_MARK,
			 &mark_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_DELETED,
			 &deleted_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_MAIL_SMALL,
			 &mail_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_NEW,
			 &new_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_UNREAD,
			 &unread_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_REPLIED,
			 &replied_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_FORWARDED,
			 &forwarded_pixbuf);
	stock_pixbuf_gdk(summaryview->treeview, STOCK_PIXMAP_CLIP,
			 &clip_pixbuf);

	font_desc = pango_font_description_new();
	size = pango_font_description_get_size
		(summaryview->statlabel_folder->style->font_desc);
	pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
	gtk_widget_modify_font(summaryview->statlabel_folder, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_select, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_msgs, font_desc);
	pango_font_description_free(font_desc);

	pixmap = stock_pixmap_widget(summaryview->hbox_l, STOCK_PIXMAP_DIR_OPEN);
	gtk_box_pack_start(GTK_BOX(summaryview->hbox_l), pixmap, FALSE, FALSE, 4);
	gtk_box_reorder_child(GTK_BOX(summaryview->hbox_l), pixmap, 0);
	gtk_widget_show(pixmap);

	summary_clear_list(summaryview);
	summary_set_column_order(summaryview);
	summary_set_column_titles(summaryview);
	summary_colorlabel_menu_create(summaryview);
	summary_set_menu_sensitive(summaryview);
}

gboolean summary_show(SummaryView *summaryview, FolderItem *item,
		      gboolean update_cache)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GSList *mlist = NULL;
	gchar *buf;
	gboolean is_refresh;
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;
	gboolean moved;

	if (summary_is_locked(summaryview)) return FALSE;

	inc_lock();
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	is_refresh = (item == summaryview->folder_item &&
		      update_cache == FALSE) ? TRUE : FALSE;
	if (is_refresh) {
		selected_msgnum = summary_get_msgnum(summaryview,
						     summaryview->selected);
		displayed_msgnum = summary_get_msgnum(summaryview,
						      summaryview->displayed);
	}

	/* process the marks if any */
	if (summaryview->mainwin->lock_count == 0 &&
	    (summaryview->moved > 0 || summaryview->copied > 0)) {
		AlertValue val;

		val = alertpanel(_("Process mark"),
				 _("Some marks are left. Process it?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, GTK_STOCK_CANCEL);
		if (G_ALERTDEFAULT == val) {
			summary_unlock(summaryview);
			summary_execute(summaryview);
			summary_lock(summaryview);
			GTK_EVENTS_FLUSH();
		} else if (G_ALERTALTERNATE == val) {
			summary_write_cache(summaryview);
			GTK_EVENTS_FLUSH();
		} else {
			summary_unlock(summaryview);
			inc_unlock();
			return FALSE;
		}
	} else
		summary_write_cache(summaryview);

	gtk_tree_row_reference_free(summaryview->folderview->opened);
	summaryview->folderview->opened =
		gtk_tree_row_reference_copy(summaryview->folderview->selected);

	summary_clear_list(summaryview);
	summary_set_column_titles(summaryview);

	buf = NULL;
	if (!item || !item->path || !item->parent || item->no_select ||
	    (FOLDER_TYPE(item->folder) == F_MH &&
	     ((buf = folder_item_get_path(item)) == NULL ||
	      change_dir(buf) < 0))) {
		g_free(buf);
		debug_print("empty folder\n\n");
		summary_clear_all(summaryview);
		summaryview->folder_item = item;
		summary_unlock(summaryview);
		inc_unlock();
		return TRUE;
	}
	g_free(buf);

	if (!is_refresh)
		messageview_clear(summaryview->messageview);

	summaryview->folder_item = item;

	g_signal_handlers_block_matched(G_OBJECT(treeview),
					(GSignalMatchType)G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, summaryview);

	buf = g_strdup_printf(_("Scanning folder (%s)..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(summaryview->mainwin);

	mlist = folder_item_get_msg_list(item, !update_cache);

	statusbar_pop_all();
	STATUSBAR_POP(summaryview->mainwin);

	/* set tree store and hash table from the msginfo list, and
	   create the thread */
	summary_set_tree_model_from_list(summaryview, mlist);

	if (mlist)
		gtk_widget_grab_focus(GTK_WIDGET(treeview));

	g_slist_free(mlist);

	summary_write_cache(summaryview);

	item->opened = TRUE;

	g_signal_handlers_unblock_matched(G_OBJECT(treeview),
					  (GSignalMatchType)G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, summaryview);

	if (is_refresh) {
		if (summary_find_msg_by_msgnum(summaryview, displayed_msgnum,
					       &iter)) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed =
				gtk_tree_row_reference_new(model, path);
			gtk_tree_path_free(path);
		} else
			messageview_clear(summaryview->messageview);

		summary_select_by_msgnum(summaryview, selected_msgnum);

		if (!summaryview->selected) {
			/* no selected message - select first unread
			   message, but do not display it */
			if (summary_find_next_flagged_msg
				(summaryview, &iter, NULL, MSG_UNREAD, FALSE)) {
				summary_select_row(summaryview, &iter,
						   FALSE, TRUE);
			} else if (item->total > 0) {
				g_signal_emit_by_name
					(treeview, "move-cursor",
					 GTK_MOVEMENT_BUFFER_ENDS,
					 item->sort_type == SORT_DESCENDING ?
					 -1 : 1, &moved);
			}
		}
	} else {
		/* select first unread message */
		if (summary_find_next_flagged_msg(summaryview, &iter, NULL,
						  MSG_UNREAD, FALSE)) {
			if (prefs_common.open_unread_on_enter ||
			    prefs_common.always_show_msg) {
				summary_unlock(summaryview);
				summary_select_row(summaryview, &iter,
						   TRUE, TRUE);
				summary_lock(summaryview);
			} else
				summary_select_row(summaryview, &iter,
						   FALSE, TRUE);
		} else {
			summary_unlock(summaryview);
			g_signal_emit_by_name
				(treeview, "move-cursor",
				 GTK_MOVEMENT_BUFFER_ENDS,
				 item->sort_type == SORT_DESCENDING ?
				 -1 : 1, &moved);
			summary_lock(summaryview);
		}
	}

	summary_set_column_titles(summaryview);
	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);

	debug_print("\n");
	STATUSBAR_PUSH(summaryview->mainwin, _("Done."));

	main_window_cursor_normal(summaryview->mainwin);
	summary_unlock(summaryview);
	inc_unlock();

	return TRUE;
}

static gboolean summary_free_msginfo_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	procmsg_msginfo_free(msginfo);

	return FALSE;
}

void summary_clear_list(SummaryView *summaryview)
{
	GtkTreeView *treeview = GTK_TREE_VIEW(summaryview->treeview);
	GtkAdjustment *adj;

	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
			       summary_free_msginfo_func, NULL);

	if (summaryview->folder_item) {
		folder_item_close(summaryview->folder_item);
		summaryview->folder_item = NULL;
	}

	summaryview->display_msg = FALSE;

	summaryview->selected = NULL;
	summaryview->displayed = NULL;
	summaryview->total_size = 0;
	summaryview->deleted = summaryview->moved = 0;
	summaryview->copied = 0;

	summary_msgid_table_destroy(summaryview);

	summaryview->mlist = NULL;
	if (summaryview->folder_table) {
		g_hash_table_destroy(summaryview->folder_table);
		summaryview->folder_table = NULL;
	}
	summaryview->filtered = 0;

	summaryview->can_toggle_selection = TRUE;
	summaryview->on_drag = FALSE;
	if (summaryview->pressed_path) {
		gtk_tree_path_free(summaryview->pressed_path);
		summaryview->pressed_path = NULL;
	}

	gtk_tree_view_set_model(treeview, NULL);
	gtk_tree_store_clear(summaryview->store);
	gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(summaryview->store));

	/* ensure that the "value-changed" signal is always emitted */
	adj = gtk_tree_view_get_vadjustment(treeview);
	adj->value = 0.0;

	gtkut_tree_sortable_unset_sort_column_id
		(GTK_TREE_SORTABLE(summaryview->store));
}

void summary_clear_all(SummaryView *summaryview)
{
	messageview_clear(summaryview->messageview);
	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
	summary_status_show(summaryview);
}

void summary_lock(SummaryView *summaryview)
{
	summaryview->lock_count++;
}

void summary_unlock(SummaryView *summaryview)
{
	if (summaryview->lock_count)
		summaryview->lock_count--;
}

gboolean summary_is_locked(SummaryView *summaryview)
{
	return summaryview->lock_count > 0;
}

SummarySelection summary_get_selection_type(SummaryView *summaryview)
{
	SummarySelection selection;
	gint count;

	count = gtk_tree_selection_count_selected_rows(summaryview->selection);

	if (!summaryview->folder_item || summaryview->folder_item->total == 0)
		selection = SUMMARY_NONE;
	else if (count == 0)
		selection = SUMMARY_SELECTED_NONE;
	else if (count == 1)
		selection = SUMMARY_SELECTED_SINGLE;
	else
		selection = SUMMARY_SELECTED_MULTIPLE;

	return selection;
}

GSList *summary_get_selected_msg_list(SummaryView *summaryview)
{
	GSList *mlist = NULL;
	GList *rows;
	GList *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		msginfo = NULL;
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		mlist = g_slist_prepend(mlist, msginfo);
		gtk_tree_path_free((GtkTreePath *)cur->data);
	}

	g_list_free(rows);

	mlist = g_slist_reverse(mlist);

	return mlist;
}

GSList *summary_get_msg_list(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GSList *mlist = NULL;
	MsgInfo *msginfo;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		mlist = g_slist_prepend(mlist, msginfo);
		valid = gtkut_tree_model_next(model, &iter);
	}

	return g_slist_reverse(mlist);
}

static gboolean summary_msgid_table_create_func(GtkTreeModel *model,
						GtkTreePath *path,
						GtkTreeIter *iter,
						gpointer data)
{
	GHashTable *msgid_table = (GHashTable *)data;
	MsgInfo *msginfo;
	GtkTreeIter *iter_;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo->msgid && msginfo->msgid[0] != '\0') {
		iter_ = gtk_tree_iter_copy(iter);
		g_hash_table_replace(msgid_table, msginfo->msgid, iter_);
	}

	return FALSE;
}

static void summary_msgid_table_create(SummaryView *summaryview)
{
	GHashTable *msgid_table;

	g_return_if_fail(summaryview->msgid_table == NULL);

	msgid_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
					    (GDestroyNotify)gtk_tree_iter_free);

	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
			       summary_msgid_table_create_func, msgid_table);

	summaryview->msgid_table = msgid_table;
}

static void summary_msgid_table_destroy(SummaryView *summaryview)
{
	if (!summaryview->msgid_table)
		return;

	g_hash_table_destroy(summaryview->msgid_table);
	summaryview->msgid_table = NULL;
}

static void summary_set_menu_sensitive(SummaryView *summaryview)
{
	GtkItemFactory *ifactory = summaryview->popupfactory;
	SummarySelection selection;
	GtkWidget *menuitem;
	gboolean sens;

	selection = summary_get_selection_type(summaryview);
	sens = (selection == SUMMARY_SELECTED_MULTIPLE) ? FALSE : TRUE;

	main_window_set_menu_sensitive(summaryview->mainwin);

	if (summaryview->folder_item &&
	    (summaryview->folder_item->stype == F_OUTBOX ||
	     summaryview->folder_item->stype == F_DRAFT  ||
	     summaryview->folder_item->stype == F_QUEUE)) {
		gtk_widget_show(summaryview->reedit_menuitem);
		gtk_widget_show(summaryview->reedit_separator);
		menu_set_sensitive(ifactory, "/Re-edit", sens);
	} else {
		gtk_widget_hide(summaryview->reedit_menuitem);
		gtk_widget_hide(summaryview->reedit_separator);
		menu_set_sensitive(ifactory, "/Re-edit", FALSE);
	}

	if (selection == SUMMARY_NONE) {
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		return;
	}

	if (summaryview->folder_item &&
	    FOLDER_TYPE(summaryview->folder_item->folder) != F_NEWS) {
		menu_set_sensitive(ifactory, "/Move...", TRUE);
		menu_set_sensitive(ifactory, "/Delete", TRUE);
	} else {
		menu_set_sensitive(ifactory, "/Move...", FALSE);
		menu_set_sensitive(ifactory, "/Delete", FALSE);
	}

	menu_set_sensitive(ifactory, "/Copy...", TRUE);

	menu_set_sensitive(ifactory, "/Mark", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unmark", TRUE);

	menu_set_sensitive(ifactory, "/Mark/Mark as unread", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark as read",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark all read",  TRUE);

	menu_set_sensitive(ifactory, "/Color label", TRUE);

	menu_set_sensitive(ifactory, "/Reply",			  sens);
	menu_set_sensitive(ifactory, "/Reply to",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/all",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/sender",	  sens);
	menu_set_sensitive(ifactory, "/Reply to/mailing list",	  sens);
	menu_set_sensitive(ifactory, "/Forward",		  TRUE);
	menu_set_sensitive(ifactory, "/Forward as attachment",	  TRUE);
	menu_set_sensitive(ifactory, "/Redirect",		  sens);

	menu_set_sensitive(ifactory, "/Add sender to address book", sens);

	menu_set_sensitive(ifactory, "/View", sens);
	menu_set_sensitive(ifactory, "/View/Open in new window", sens);
	menu_set_sensitive(ifactory, "/View/Source", sens);
	menu_set_sensitive(ifactory, "/View/All header", sens);

	menu_set_sensitive(ifactory, "/Print...",   TRUE);

	summary_lock(summaryview);
	menuitem = gtk_item_factory_get_widget(ifactory, "/View/All header");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 summaryview->messageview->textview->show_all_headers);
	summary_unlock(summaryview);
}

static void summary_select_prev_flagged(SummaryView *summaryview,
					MsgPermFlags flags,
					gboolean start_from_prev,
					const gchar *title,
					const gchar *ask_msg,
					const gchar *notice)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter prev, iter;
	gboolean found;

	if (!gtkut_tree_row_reference_get_iter(model, summaryview->selected,
					       &iter))
		return;

	found = summary_find_prev_flagged_msg
		(summaryview, &prev, &iter, flags, start_from_prev);

	if (!found) {
		AlertValue val;

		val = alertpanel(title, ask_msg, GTK_STOCK_YES, GTK_STOCK_NO,
				 NULL);
		if (val != G_ALERTDEFAULT) return;
		found = summary_find_prev_flagged_msg(summaryview, &prev, NULL,
						      flags, start_from_prev);
	}

	if (!found) {
		if (notice)
			alertpanel_notice(notice);
	} else
		summary_select_row
			(summaryview, &prev,
			 messageview_is_visible(summaryview->messageview),
			 FALSE);
}

static void summary_select_next_flagged(SummaryView *summaryview,
					MsgPermFlags flags,
					gboolean start_from_next,
					const gchar *title,
					const gchar *ask_msg,
					const gchar *notice)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter next, iter;
	gboolean found;

	if (!gtkut_tree_row_reference_get_iter(model, summaryview->selected,
					       &iter)) {
		if (!gtk_tree_model_get_iter_first(model, &iter))
			return;
	}

	found = summary_find_next_flagged_msg
		(summaryview, &next, &iter, flags, start_from_next);

	if (!found) {
		AlertValue val;

		val = alertpanel(title, ask_msg, GTK_STOCK_YES, GTK_STOCK_NO,
				 NULL);
		if (val != G_ALERTDEFAULT) return;
		found = summary_find_next_flagged_msg(summaryview, &next, NULL,
						      flags, start_from_next);
	}

	if (!found) {
		if (notice)
			alertpanel_notice(notice);
	} else
		summary_select_row
			(summaryview, &next,
			 messageview_is_visible(summaryview->messageview),
			 FALSE);
}

static void summary_select_next_flagged_or_folder(SummaryView *summaryview,
						  MsgPermFlags flags,
						  const gchar *title,
						  const gchar *ask_msg,
						  const gchar *notice)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, next;

	if (!gtkut_tree_row_reference_get_iter(model, summaryview->selected,
					       &iter)) {
		if (!gtk_tree_model_get_iter_first(model, &iter))
			return;
	}

	while (summary_find_next_flagged_msg
		(summaryview, &next, &iter, flags, FALSE) == FALSE) {
		AlertValue val;

		val = alertpanel(title, ask_msg,
				 GTK_STOCK_YES, _("Search again"),
				 GTK_STOCK_NO);

		if (val == G_ALERTDEFAULT) {
			folderview_select_next_unread(summaryview->folderview);
			return;
		} else if (val == G_ALERTALTERNATE) {
			if (!gtk_tree_model_get_iter_first(model, &iter))
				return;
		} else
			return;
	}

	summary_select_row(summaryview, &next,
			   messageview_is_visible(summaryview->messageview),
			   FALSE);
}

void summary_select_prev_unread(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_UNREAD, FALSE,
				    _("No more unread messages"),
				    _("No unread message found. "
				      "Search from the end?"),
				    _("No unread messages."));
}

void summary_select_next_unread(SummaryView *summaryview)
{
	summary_select_next_flagged_or_folder(summaryview, MSG_UNREAD,
					      _("No more unread messages"),
					      _("No unread message found. "
						"Go to next folder?"),
					      NULL);
}

void summary_select_prev_new(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_NEW, FALSE,
				    _("No more new messages"),
				    _("No new message found. "
				      "Search from the end?"),
				    _("No new messages."));
}

void summary_select_next_new(SummaryView *summaryview)
{
	summary_select_next_flagged_or_folder(summaryview, MSG_NEW,
					      _("No more new messages"),
					      _("No new message found. "
						"Go to next folder?"),
					      NULL);
}

void summary_select_prev_marked(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_MARKED, TRUE,
				    _("No more marked messages"),
				    _("No marked message found. "
				      "Search from the end?"),
				    _("No marked messages."));
}

void summary_select_next_marked(SummaryView *summaryview)
{
	summary_select_next_flagged(summaryview, MSG_MARKED, TRUE,
				    _("No more marked messages"),
				    _("No marked message found. "
				      "Search from the beginning?"),
				    _("No marked messages."));
}

void summary_select_prev_labeled(SummaryView *summaryview)
{
	summary_select_prev_flagged(summaryview, MSG_CLABEL_FLAG_MASK, TRUE,
				    _("No more labeled messages"),
				    _("No labeled message found. "
				      "Search from the end?"),
				    _("No labeled messages."));
}

void summary_select_next_labeled(SummaryView *summaryview)
{
	summary_select_next_flagged(summaryview, MSG_CLABEL_FLAG_MASK, TRUE,
				    _("No more labeled messages"),
				    _("No labeled message found. "
				      "Search from the beginning?"),
				    _("No labeled messages."));
}

void summary_select_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkTreeIter iter;

	if (summary_find_msg_by_msgnum(summaryview, msgnum, &iter))
		summary_select_row(summaryview, &iter, FALSE, TRUE);
}

/**
 * summary_select_row:
 * @summaryview: Summary view.
 * @node: Summary tree node.
 * @display_msg: TRUE to display the selected message.
 * @do_refresh: TRUE to refresh the widget.
 *
 * Select @node (bringing it into view by scrolling and expanding its
 * thread, if necessary) and unselect all others.  If @display_msg is
 * TRUE, display the corresponding message in the message view.
 * If @do_refresh is TRUE, the widget is refreshed.
 **/
void summary_select_row(SummaryView *summaryview, GtkTreeIter *iter,
			 gboolean display_msg, gboolean do_refresh)
{
	GtkTreePath *path;

	if (!iter)
		return;

	gtkut_tree_view_expand_parent_all
		(GTK_TREE_VIEW(summaryview->treeview), iter);

	if (do_refresh)
		gtk_widget_grab_focus(summaryview->treeview);

	summaryview->display_msg = display_msg;
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(summaryview->store),
				       iter);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(summaryview->treeview), path,
				 NULL, FALSE);
	if (do_refresh) {
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(summaryview->treeview),
			 path, NULL, TRUE, 0.5, 0.0);
	} else {
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(summaryview->treeview),
			 path, NULL, FALSE, 0.0, 0.0);
	}

	gtk_tree_path_free(path);
}

static void summary_scroll_to_selected(SummaryView *summaryview)
{
	GtkTreePath *path;

	if (!summaryview->selected)
		return;

	path = gtk_tree_row_reference_get_path(summaryview->selected);
	if (path) {
		gtk_tree_view_scroll_to_cell
			(GTK_TREE_VIEW(summaryview->treeview),
			 path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free(path);
	}
}

static guint summary_get_msgnum(SummaryView *summaryview,
				GtkTreeRowReference *row)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;

	if (!row)
		return 0;
	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store), row, &iter))
		return 0;

	gtk_tree_model_get(GTK_TREE_MODEL(summaryview->store), &iter,
			   S_COL_MSG_INFO, &msginfo, -1);

	return msginfo->msgnum;
}

static gboolean summary_find_prev_msg(SummaryView *summaryview,
				      GtkTreeIter *prev, GtkTreeIter *iter)
{
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (!iter)
		return FALSE;

	iter_ = *iter;

	while (valid) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
		    !MSG_IS_DELETED(msginfo->flags)) {
			*prev = iter_;
			return TRUE;
		}
		valid = gtkut_tree_model_prev
			(GTK_TREE_MODEL(summaryview->store), &iter_);
	}

	return FALSE;
}

static gboolean summary_find_next_msg(SummaryView *summaryview,
				      GtkTreeIter *next, GtkTreeIter *iter)
{
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (!iter)
		return FALSE;

	iter_ = *iter;

	while (valid) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && !MSG_IS_INVALID(msginfo->flags) &&
		    !MSG_IS_DELETED(msginfo->flags)) {
			*next = iter_;
			return TRUE;
		}
		valid = gtkut_tree_model_next
			(GTK_TREE_MODEL(summaryview->store), &iter_);
	}

	return FALSE;
}

static gboolean summary_find_nearest_msg(SummaryView *summaryview,
					 GtkTreeIter *target, GtkTreeIter *iter)
{
	gboolean valid;

	valid = summary_find_next_msg(summaryview, target, iter);
	if (!valid)
		valid = summary_find_prev_msg(summaryview, target, iter);

	return valid;
}

static gboolean summary_find_prev_flagged_msg(SummaryView *summaryview,
					      GtkTreeIter *prev,
					      GtkTreeIter *iter,
					      MsgPermFlags flags,
					      gboolean start_from_prev)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (iter) {
		iter_ = *iter;
		if (start_from_prev)
			valid = gtkut_tree_model_prev(model, &iter_);
	} else
		valid = gtkut_tree_model_get_iter_last(model, &iter_);

	for (; valid == TRUE; valid = gtkut_tree_model_prev(model, &iter_)) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) {
			*prev = iter_;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean summary_find_next_flagged_msg(SummaryView *summaryview,
					      GtkTreeIter *next,
					      GtkTreeIter *iter,
					      MsgPermFlags flags,
					      gboolean start_from_next)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid = TRUE;

	if (iter) {
		iter_ = *iter;
		if (start_from_next)
			valid = gtkut_tree_model_next(model, &iter_);
	} else
		valid = gtk_tree_model_get_iter_first(model, &iter_);

	for (; valid == TRUE; valid = gtkut_tree_model_next(model, &iter_)) {
		GET_MSG_INFO(msginfo, &iter_);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) {
			*next = iter_;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean summary_find_msg_by_msgnum(SummaryView *summaryview,
					   guint msgnum, GtkTreeIter *found)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo;
	gboolean valid;

	for (valid = gtk_tree_model_get_iter_first(model, &iter);
	     valid == TRUE; valid = gtkut_tree_model_next(model, &iter)) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		if (msginfo && msginfo->msgnum == msgnum) {
			*found = iter;
			return TRUE;
		}
	}

	return FALSE;
}

static guint attract_hash_func(gconstpointer key)
{
	gchar *str;
	gchar *p;
	guint h;

	Xstrdup_a(str, (const gchar *)key, return 0);
	trim_subject_for_compare(str);

	p = str;
	h = *p;

	if (h) {
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + *p;
	}

	return h;
}

static gint attract_compare_func(gconstpointer a, gconstpointer b)
{
	return subject_compare((const gchar *)a, (const gchar *)b) == 0;
}

void summary_attract_by_subject(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo, *dest_msginfo;
	GHashTable *subject_table, *order_table;
	GSList *mlist = NULL, *list, *dest, *last = NULL, *next = NULL;
	gboolean valid;
	gint count, i;
	gint *new_order;

	if (summaryview->folder_item &&
	    summaryview->folder_item->sort_key != SORT_BY_NONE)
		return;

	valid = gtk_tree_model_get_iter_first(model, &iter);
	if (!valid)
		return;

	debug_print("Attracting messages by subject...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Attracting messages by subject..."));

	main_window_cursor_wait(summaryview->mainwin);

	order_table = g_hash_table_new(NULL, NULL);

	for (count = 1; valid == TRUE; ++count) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		g_hash_table_insert(order_table, msginfo,
				    GINT_TO_POINTER(count));
		mlist = g_slist_prepend(mlist, msginfo);
		valid = gtk_tree_model_iter_next(model, &iter);
	}
	--count;

	mlist = g_slist_reverse(mlist);

	subject_table = g_hash_table_new(attract_hash_func,
					 attract_compare_func);

	for (list = mlist; list != NULL; list = next) {
		msginfo = (MsgInfo *)list->data;

		next = list->next;

		if (!msginfo->subject) {
			last = list;
			continue;
		}

		/* find attracting node */
		dest = g_hash_table_lookup(subject_table, msginfo->subject);

		if (dest) {
			dest_msginfo = (MsgInfo *)dest->data;

			/* if the time difference is more than 30 days,
			   don't attract */
			if (ABS(msginfo->date_t - dest_msginfo->date_t)
			    > 60 * 60 * 24 * 30) {
				last = list;
				continue;
			}

			if (dest->next != list) {
				last->next = list->next;
				list->next = dest->next;
				dest->next = list;
			} else
				last = list;
		} else
			last = list;

		g_hash_table_replace(subject_table, msginfo->subject, list);
	}

	g_hash_table_destroy(subject_table);

	new_order = g_new(gint, count);
	for (list = mlist, i = 0; list != NULL; list = list->next, ++i) {
		gint old_pos;

		msginfo = (MsgInfo *)list->data;

		old_pos = GPOINTER_TO_INT
			(g_hash_table_lookup(order_table, msginfo));
		new_order[i] = old_pos - 1;
	}
	gtk_tree_store_reorder(GTK_TREE_STORE(model), NULL, new_order);
	g_free(new_order);

	g_slist_free(mlist);
	g_hash_table_destroy(order_table);

	summary_scroll_to_selected(summaryview);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_update_status(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	gboolean valid;
	MsgInfo *msginfo;

	summaryview->total_size =
	summaryview->deleted = summaryview->moved = summaryview->copied = 0;

	valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);

		if (MSG_IS_DELETED(msginfo->flags))
			summaryview->deleted++;
		if (MSG_IS_MOVE(msginfo->flags))
			summaryview->moved++;
		if (MSG_IS_COPY(msginfo->flags))
			summaryview->copied++;
		summaryview->total_size += msginfo->size;

		valid = gtkut_tree_model_next(model, &iter);
	}
}

static void summary_status_show(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	gchar *str;
	gchar *del, *mv, *cp;
	gchar *sel;
	gchar *spc;
	GList *rowlist, *cur;
	guint n_selected = 0;
	off_t sel_size = 0;
	MsgInfo *msginfo;

	if (!summaryview->folder_item) {
		gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_select), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs),   "");
		return;
	}

	rowlist = gtk_tree_selection_get_selected_rows(summaryview->selection,
						       NULL);
	for (cur = rowlist; cur != NULL; cur = cur->next) {
		GtkTreeIter iter;
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		if (!msginfo)
			g_warning("summary_status_show(): msginfo == NULL\n");
		else {
			sel_size += msginfo->size;
			n_selected++;
		}
		gtk_tree_path_free(path);
	}
	g_list_free(rowlist);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) {
		gchar *group;
		group = get_abbrev_newsgroup_name
			(g_basename(summaryview->folder_item->path),
			 prefs_common.ng_abbrev_len);
		str = trim_string_before(group, 32);
		g_free(group);
	} else
		str = trim_string_before(summaryview->folder_item->path, 32);
	gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), str);
	g_free(str);

	if (summaryview->deleted)
		del = g_strdup_printf(_("%d deleted"), summaryview->deleted);
	else
		del = g_strdup("");
	if (summaryview->moved)
		mv = g_strdup_printf(_("%s%d moved"),
				     summaryview->deleted ? _(", ") : "",
				     summaryview->moved);
	else
		mv = g_strdup("");
	if (summaryview->copied)
		cp = g_strdup_printf(_("%s%d copied"),
				     summaryview->deleted ||
				     summaryview->moved ? _(", ") : "",
				     summaryview->copied);
	else
		cp = g_strdup("");

	if (summaryview->deleted || summaryview->moved || summaryview->copied)
		spc = "    ";
	else
		spc = "";

	if (n_selected)
		sel = g_strdup_printf(" (%s)", to_human_readable(sel_size));
	else
		sel = g_strdup("");
	str = g_strconcat(n_selected ? itos(n_selected) : "",
			  n_selected ? _(" item(s) selected") : "",
			  sel, spc, del, mv, cp, NULL);
	gtk_label_set(GTK_LABEL(summaryview->statlabel_select), str);
	g_free(str);
	g_free(sel);
	g_free(del);
	g_free(mv);
	g_free(cp);

	if (FOLDER_IS_LOCAL(summaryview->folder_item->folder)) {
		str = g_strdup_printf(_("%d new, %d unread, %d total (%s)"),
				      summaryview->folder_item->new,
				      summaryview->folder_item->unread,
				      summaryview->folder_item->total,
				      to_human_readable(summaryview->total_size));
	} else {
		str = g_strdup_printf(_("%d new, %d unread, %d total"),
				      summaryview->folder_item->new,
				      summaryview->folder_item->unread,
				      summaryview->folder_item->total);
	}
	gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs), str);
	g_free(str);

	folderview_update_opened_msg_num(summaryview->folderview);
}

static void summary_set_column_titles(SummaryView *summaryview)
{
}

void summary_sort(SummaryView *summaryview,
		  FolderSortKey sort_key, FolderSortType sort_type)
{
	FolderItem *item = summaryview->folder_item;
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(summaryview->store);
	SummaryColumnType col_type;

	g_return_if_fail(sort_key >= SORT_BY_NONE && sort_key <= SORT_BY_TO);

	if (!item || !item->path || !item->parent || item->no_select) return;

	col_type = sort_key_to_col[sort_key];

	if (col_type == -1) {
		item->sort_key = SORT_BY_NONE;
		item->sort_type = SORT_ASCENDING;
		gtkut_tree_sortable_unset_sort_column_id(sortable);
		summary_set_column_titles(summaryview);
		summary_set_menu_sensitive(summaryview);
		return;
	}

	debug_print("Sorting summary by key: %d...\n", sort_key);
	STATUSBAR_PUSH(summaryview->mainwin, _("Sorting summary..."));

	main_window_cursor_wait(summaryview->mainwin);

	item->sort_key = sort_key;
	item->sort_type = sort_type;

	gtk_tree_sortable_set_sort_column_id(sortable, col_type,
					     (GtkSortType)sort_type);

	summary_set_column_titles(summaryview);
	summary_set_menu_sensitive(summaryview);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static gboolean summary_have_unread_children(SummaryView *summaryview,
					     GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	if (MSG_IS_UNREAD(msginfo->flags))
		return TRUE;

	valid = gtk_tree_model_iter_children(model, &iter_, iter);

	while (valid) {
		if (summary_have_unread_children(summaryview, &iter_))
			return TRUE;

		valid = gtk_tree_model_iter_next(model, &iter_);
	}

	return FALSE;
}

static void summary_set_row(SummaryView *summaryview, GtkTreeIter *iter,
			    MsgInfo *msginfo)
{
	GtkTreeStore *store = GTK_TREE_STORE(summaryview->store);
	gchar date_modified[80];
	const gchar *date_s;
	gchar *sw_from_s = NULL;
	gchar *subject_s = NULL;
	GdkPixbuf *mark_pix = NULL;
	GdkPixbuf *unread_pix = NULL;
	GdkPixbuf *mime_pix = NULL;
	GdkColor *foreground = NULL;
	gboolean use_bold = FALSE;
	MsgFlags flags;
	GdkColor color;
	gint color_val;

	if (!msginfo) {
		GET_MSG_INFO(msginfo, iter);
	}

	if (msginfo->date_t) {
		procheader_date_get_localtime(date_modified,
					      sizeof(date_modified),
					      msginfo->date_t);
		date_s = date_modified;
	} else if (msginfo->date)
		date_s = msginfo->date;
	else
		date_s = _("(No Date)");
	if (prefs_common.swap_from && msginfo->from && msginfo->to &&
	    cur_account && cur_account->address) {
		gchar *from;

		Xstrdup_a(from, msginfo->from, return);
		extract_address(from);
		if (!strcmp(from, cur_account->address))
			sw_from_s = g_strconcat("-->", msginfo->to, NULL);
	}

	if (msginfo->subject) {
		if (msginfo->folder && msginfo->folder->trim_summary_subject) {
			subject_s = g_strdup(msginfo->subject);
			trim_subject(subject_s);
		}
	}

	flags = msginfo->flags;

	/* set flag pixbufs */
	if (MSG_IS_DELETED(flags)) {
		mark_pix = deleted_pixbuf;
		foreground = &summaryview->color_dim;
	} else if (MSG_IS_MOVE(flags)) {
		/* mark_pix = move_pixbuf; */
		foreground = &summaryview->color_marked;
	} else if (MSG_IS_COPY(flags)) {
		/* mark_pix = copy_pixbuf; */
		foreground = &summaryview->color_marked;
	} else if (MSG_IS_MARKED(flags))
		mark_pix = mark_pixbuf;

	if (MSG_IS_NEW(flags))
		unread_pix = new_pixbuf;
	else if (MSG_IS_UNREAD(flags))
		unread_pix = unread_pixbuf;
	else if (MSG_IS_REPLIED(flags))
		unread_pix = replied_pixbuf;
	else if (MSG_IS_FORWARDED(flags))
		unread_pix = forwarded_pixbuf;

	if (MSG_IS_MIME(flags))
		mime_pix = clip_pixbuf;

	if (prefs_common.bold_unread && MSG_IS_UNREAD(flags))
		use_bold = TRUE;

	color_val = MSG_GET_COLORLABEL_VALUE(flags);
	if (color_val != 0) {
		color = colorlabel_get_color(color_val - 1);
		foreground = &color;
	}

	gtk_tree_store_set(store, iter,
			   S_COL_MARK, mark_pix,
			   S_COL_UNREAD, unread_pix,
			   S_COL_MIME, mime_pix,
			   S_COL_SUBJECT, subject_s ? subject_s :
			   		  msginfo->subject ? msginfo->subject :
					  _("(No Subject)"),
			   S_COL_FROM, sw_from_s ? sw_from_s :
			   	       msginfo->fromname ? msginfo->fromname :
				       _("(No From)"),
			   S_COL_DATE, date_s,
			   S_COL_SIZE, to_human_readable(msginfo->size),
			   S_COL_NUMBER, msginfo->msgnum,

			   S_COL_MSG_INFO, msginfo,

			   S_COL_LABEL, color_val,
			   S_COL_TO, NULL,

			   S_COL_FOREGROUND, foreground,
			   S_COL_BOLD, use_bold,
			   -1);
}

static void summary_insert_gnode(SummaryView *summaryview, GtkTreeStore *store,
				 GtkTreeIter *iter, GtkTreeIter *parent,
				 GtkTreeIter *sibling, GNode *gnode)
{
	MsgInfo *msginfo = (MsgInfo *)gnode->data;

	if (parent && !sibling)
		gtk_tree_store_append(store, iter, parent);
	else
		gtk_tree_store_insert_after(store, iter, parent, sibling);

	summary_set_row(summaryview, iter, msginfo);

	for (gnode = gnode->children; gnode != NULL; gnode = gnode->next) {
		GtkTreeIter child;

		summary_insert_gnode(summaryview, store, &child, iter, NULL,
				     gnode);
	}
}

static void summary_set_tree_model_from_list(SummaryView *summaryview,
					     GSList *mlist)
{
	GtkTreeStore *store = GTK_TREE_STORE(summaryview->store);
	GtkTreeIter iter;
	MsgInfo *msginfo;
	GSList *cur;

	if (!mlist) return;

	debug_print(_("\tSetting summary from message data..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Setting summary from message data..."));
	gdk_flush();

	/* temporarily remove the model for speed up */
	gtk_tree_view_set_model(GTK_TREE_VIEW(summaryview->treeview), NULL);

	if (summaryview->folder_item->threaded) {
		GNode *root, *gnode;

		root = procmsg_get_thread_tree(mlist);

		for (gnode = root->children; gnode != NULL;
		     gnode = gnode->next) {
			summary_insert_gnode
				(summaryview, store, &iter, NULL, NULL, gnode);
			if (gnode->children && !prefs_common.expand_thread &&
			    prefs_common.bold_unread &&
			    summary_have_unread_children(summaryview, &iter)) {
				gtk_tree_store_set(store, &iter,
						   S_COL_BOLD, TRUE, -1);
			}
		}

		g_node_destroy(root);

		for (cur = mlist; cur != NULL; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;

			if (MSG_IS_DELETED(msginfo->flags))
				summaryview->deleted++;
			summaryview->total_size += msginfo->size;
		}
	} else {
		GtkTreeIter iter;

		mlist = g_slist_reverse(mlist);
		for (cur = mlist; cur != NULL; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;

			gtk_tree_store_prepend(store, &iter, NULL);
			summary_set_row(summaryview, &iter, msginfo);

			if (MSG_IS_DELETED(msginfo->flags))
				summaryview->deleted++;
			summaryview->total_size += msginfo->size;
		}
		/* mlist = g_slist_reverse(mlist); */
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(summaryview->treeview),
				GTK_TREE_MODEL(store));

	if (summaryview->folder_item->threaded && prefs_common.expand_thread)
		gtk_tree_view_expand_all
			(GTK_TREE_VIEW(summaryview->treeview));

	if (summaryview->folder_item->sort_key != SORT_BY_NONE) {
		summary_sort(summaryview, summaryview->folder_item->sort_key,
			     summaryview->folder_item->sort_type);
	}

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
}

struct wcachefp
{
	FILE *cache_fp;
	FILE *mark_fp;
};

static gboolean summary_write_cache_func(GtkTreeModel *model,
					 GtkTreePath *path, GtkTreeIter *iter,
					 gpointer data)
{
	struct wcachefp *fps = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo->folder->mark_queue != NULL) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW);
	}

	procmsg_write_cache(msginfo, fps->cache_fp);
	procmsg_write_flags(msginfo, fps->mark_fp);

	return FALSE;
}

gint summary_write_cache(SummaryView *summaryview)
{
	struct wcachefp fps;
	FolderItem *item;
	gchar *buf;

	item = summaryview->folder_item;
	if (!item || !item->path)
		return -1;

	fps.cache_fp = procmsg_open_cache_file(item, DATA_WRITE);
	if (fps.cache_fp == NULL)
		return -1;
	fps.mark_fp = procmsg_open_mark_file(item, DATA_WRITE);
	if (fps.mark_fp == NULL) {
		fclose(fps.cache_fp);
		return -1;
	}

	buf = g_strdup_printf(_("Writing summary cache (%s)..."), item->path);
	debug_print(buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	gdk_flush();
	g_free(buf);

	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
			       summary_write_cache_func, &fps);

	procmsg_flush_mark_queue(item, fps.mark_fp);
	item->unmarked_num = 0;

	fclose(fps.cache_fp);
	fclose(fps.mark_fp);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	return 0;
}

static gboolean summary_row_is_displayed(SummaryView *summaryview,
					 GtkTreeIter *iter)
{
	GtkTreePath *disp_path, *path;
	gint ret;

	if (!summaryview->displayed || !iter)
		return FALSE;

	disp_path = gtk_tree_row_reference_get_path(summaryview->displayed);
	if (!disp_path)
		return FALSE;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(summaryview->store),
				       iter);
	if (!path) {
		gtk_tree_path_free(disp_path);
		return FALSE;
	}

	ret = gtk_tree_path_compare(disp_path, path);
	gtk_tree_path_free(path);
	gtk_tree_path_free(disp_path);

	return (ret == 0);
}

static void summary_display_msg(SummaryView *summaryview, GtkTreeIter *iter)
{
	summary_display_msg_full(summaryview, iter, FALSE, FALSE, FALSE);
}

static void summary_display_msg_full(SummaryView *summaryview,
				     GtkTreeIter *iter,
				     gboolean new_window, gboolean all_headers,
				     gboolean redisplay)
{
	MsgInfo *msginfo = NULL;
	gint val;

	g_return_if_fail(iter != NULL);

	if (!new_window && !redisplay &&
	    summary_row_is_displayed(summaryview, iter))
		return;

	if (summary_is_locked(summaryview)) return;
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	gtk_tree_model_get(GTK_TREE_MODEL(summaryview->store), iter,
			   S_COL_MSG_INFO, &msginfo, -1);

	if (new_window) {
		MessageView *msgview;

		msgview = messageview_create_with_new_window();
		val = messageview_show(msgview, msginfo, all_headers);
	} else {
		MessageView *msgview;
		GtkTreePath *path;

		msgview = summaryview->messageview;

		gtk_tree_row_reference_free(summaryview->displayed);
		path = gtk_tree_model_get_path
			(GTK_TREE_MODEL(summaryview->store), iter);
		summaryview->displayed =
			gtk_tree_row_reference_new
				(GTK_TREE_MODEL(summaryview->store), path);
		if (!messageview_is_visible(msgview)) {
			main_window_toggle_message_view(summaryview->mainwin);
			GTK_EVENTS_FLUSH();
			gtk_tree_view_scroll_to_cell
				(GTK_TREE_VIEW(summaryview->treeview),
				 path, NULL, FALSE, 0.0, 0.0);
		}
		val = messageview_show(msgview, msginfo, all_headers);
		if (msgview->type == MVIEW_TEXT ||
		    (msgview->type == MVIEW_MIME &&
		     (msgview->mimeview->opened == NULL ||
		      gtk_notebook_get_current_page
			(GTK_NOTEBOOK(msgview->notebook)) == 0)))
			gtk_widget_grab_focus(summaryview->treeview);
		gtk_tree_path_free(path);
	}

	if (val == 0 &&
	    (new_window || !prefs_common.mark_as_read_on_new_window)) {
		if (MSG_IS_NEW(msginfo->flags))
			summaryview->folder_item->new--;
		if (MSG_IS_UNREAD(msginfo->flags))
			summaryview->folder_item->unread--;
		if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
			MSG_UNSET_PERM_FLAGS
				(msginfo->flags, MSG_NEW | MSG_UNREAD);
			if (MSG_IS_IMAP(msginfo->flags))
				imap_msg_unset_perm_flags
					(msginfo, MSG_NEW | MSG_UNREAD);
			summary_set_row(summaryview, iter, msginfo);
			summary_status_show(summaryview);
		}
	}

	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);

	statusbar_pop_all();

	summary_unlock(summaryview);
}

void summary_display_msg_selected(SummaryView *summaryview,
				  gboolean new_window, gboolean all_headers)
{
	GtkTreeIter iter;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->selected) {
		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->selected, &iter)) {
			summary_display_msg_full(summaryview, &iter,
						 new_window, all_headers, TRUE);
		}
	}
}

void summary_redisplay_msg(SummaryView *summaryview)
{
	GtkTreeIter iter;

	if (summaryview->displayed) {
		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->displayed, &iter)) {
			summary_display_msg_full(summaryview, &iter,
						 FALSE, FALSE, TRUE);
		}
	}
}

void summary_open_msg(SummaryView *summaryview)
{
	summary_display_msg_selected(summaryview, TRUE, FALSE);
}

static void summary_activate_selected(SummaryView *summaryview)
{
	if (!summaryview->folder_item)
		return;

	if (summaryview->folder_item->stype == F_OUTBOX ||
	    summaryview->folder_item->stype == F_DRAFT  ||
	    summaryview->folder_item->stype == F_QUEUE)
		summary_reedit(summaryview);
	else
		summary_open_msg(summaryview);

	summaryview->display_msg = FALSE;
}

void summary_view_source(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo;
	SourceWindow *srcwin;

	if (summaryview->selected) {
		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->selected, &iter)) {
			GET_MSG_INFO(msginfo, &iter);

			srcwin = source_window_create();
			source_window_show_msg(srcwin, msginfo);
			source_window_show(srcwin);
		}
	}
}

void summary_reedit(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo;

	if (!summaryview->selected) return;
	if (!summaryview->folder_item) return;
	if (summaryview->folder_item->stype != F_OUTBOX &&
	    summaryview->folder_item->stype != F_DRAFT  &&
	    summaryview->folder_item->stype != F_QUEUE) return;

	if (gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter)) {
		GET_MSG_INFO(msginfo, &iter);
		compose_reedit(msginfo);
	}
}

gboolean summary_step(SummaryView *summaryview, GtkScrollType type)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	gboolean moved;

	if (summary_is_locked(summaryview)) return FALSE;

	if (type == GTK_SCROLL_STEP_FORWARD) {
		if (summaryview->selected) {
			if (gtkut_tree_row_reference_get_iter
				(model, summaryview->selected, &iter) &&
			    gtkut_tree_model_next(model, &iter))
				gtkut_tree_view_expand_parent_all
					(GTK_TREE_VIEW(summaryview->treeview),
					 &iter);
			else
				return FALSE;
		}
	}

	if (messageview_is_visible(summaryview->messageview))
		summaryview->display_msg = TRUE;

	gtk_widget_grab_focus(summaryview->treeview);
	g_signal_emit_by_name(G_OBJECT(summaryview->treeview), "move-cursor",
			      GTK_MOVEMENT_DISPLAY_LINES,
			      type == GTK_SCROLL_STEP_FORWARD ? 1 : -1,
			      &moved);

	return TRUE;
}

void summary_toggle_view(SummaryView *summaryview)
{
	if (!messageview_is_visible(summaryview->messageview) &&
	    summaryview->selected)
		summary_display_msg_selected(summaryview, FALSE, FALSE);
	else
		main_window_toggle_message_view(summaryview->mainwin);
}

void summary_update_selected_rows(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GtkTreePath *path;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_set_row(summaryview, &iter, NULL);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);
}

static void summary_mark_row(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
	summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %d is marked\n"), msginfo->msgnum);
}

void summary_mark(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row(summaryview, &iter);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_set_perm_flags(msglist, MSG_MARKED);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

static void summary_mark_row_as_read(SummaryView *summaryview,
				     GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	if (MSG_IS_NEW(msginfo->flags))
		summaryview->folder_item->new--;
	if (MSG_IS_UNREAD(msginfo->flags))
		summaryview->folder_item->unread--;
	if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
		summary_set_row(summaryview, iter, msginfo);
		debug_print(_("Message %d is marked as being read\n"),
			    msginfo->msgnum);
	}
}

void summary_mark_as_read(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);

	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row_as_read(summaryview, &iter);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

void summary_mark_all_read(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid) {
		summary_mark_row_as_read(summaryview, &iter);
		valid = gtkut_tree_model_next(model, &iter);
	}

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_NEW | MSG_UNREAD);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

static void summary_mark_row_as_unread(SummaryView *summaryview,
				       GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	if (MSG_IS_DELETED(msginfo->flags)) {
		msginfo->to_folder = NULL;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
		summaryview->deleted--;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_REPLIED | MSG_FORWARDED);
	if (!MSG_IS_UNREAD(msginfo->flags)) {
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
		summaryview->folder_item->unread++;
		debug_print(_("Message %d is marked as unread\n"),
			    msginfo->msgnum);
	}
	summary_set_row(summaryview, iter, msginfo);
}

void summary_mark_as_unread(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		summary_mark_row_as_unread(summaryview, &iter);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;
		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_REPLIED);
		imap_msg_list_set_perm_flags(msglist, MSG_UNREAD);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

static void summary_delete_row(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	if (MSG_IS_DELETED(msginfo->flags)) return;

	msginfo->to_folder = NULL;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	summaryview->deleted++;

	if (!prefs_common.immediate_exec && 
	    summaryview->folder_item->stype != F_TRASH)
		summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %s/%d is set to delete\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_delete(SummaryView *summaryview)
{
	FolderItem *item = summaryview->folder_item;
	GList *rows, *cur;
	GtkTreeIter last_sel, next;

	if (!item || FOLDER_TYPE(item->folder) == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

	/* if current folder is trash, ask for confirmation */
	if (item->stype == F_TRASH) {
		AlertValue aval;

		aval = alertpanel(_("Delete message(s)"),
				  _("Do you really want to delete message(s) from the trash?"),
				  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (aval != G_ALERTDEFAULT) return;
	}

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);

	/* next code sets current row focus right. We need to find a row
	 * that is not deleted. */
	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(summaryview->store),
					&last_sel, (GtkTreePath *)cur->data);
		summary_delete_row(summaryview, &last_sel);
	}

	if (prefs_common.immediate_exec || item->stype == F_TRASH) {
		summary_execute(summaryview);
	} else {
		if (summary_find_nearest_msg(summaryview, &next, &last_sel)) {
			summary_select_row
				(summaryview, &next,
				 messageview_is_visible
					(summaryview->messageview),
				 FALSE);
		}
		summary_status_show(summaryview);
	}
}

static gboolean summary_delete_duplicated_func(GtkTreeModel *model,
					       GtkTreePath *path,
					       GtkTreeIter *iter,
					       gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo;
	GtkTreeIter *found;
	GtkTreePath *found_path;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (!msginfo || !msginfo->msgid || !*msginfo->msgid)
		return FALSE;

	found = g_hash_table_lookup(summaryview->msgid_table, msginfo->msgid);

	if (found) {
		found_path = gtk_tree_model_get_path(model, found);
		if (gtk_tree_path_compare(path, found_path) != 0)
			summary_delete_row(summaryview, iter);
		gtk_tree_path_free(found_path);
	}

	return FALSE;
}

void summary_delete_duplicated(SummaryView *summaryview)
{
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;
	if (summaryview->folder_item->stype == F_TRASH) return;

	main_window_cursor_wait(summaryview->mainwin);
	debug_print("Deleting duplicated messages...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Deleting duplicated messages..."));

	summary_msgid_table_create(summaryview);

	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
			       summary_delete_duplicated_func, summaryview);

	summary_msgid_table_destroy(summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_unmark_row(SummaryView *summaryview, GtkTreeIter *iter)
{
	MsgInfo *msginfo = NULL;

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED | MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %s/%d is unmarked\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_unmark(SummaryView *summaryview)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summary_unmark_row(summaryview, &iter);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_IMAP) {
		GSList *msglist;

		msglist = summary_get_selected_msg_list(summaryview);
		imap_msg_list_unset_perm_flags(msglist, MSG_MARKED);
		g_slist_free(msglist);
	}

	summary_status_show(summaryview);
}

static void summary_move_row_to(SummaryView *summaryview, GtkTreeIter *iter,
				FolderItem *to_folder)
{
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
	if (!MSG_IS_MOVE(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summaryview->moved++;
	}
	if (!prefs_common.immediate_exec)
		summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %d is set to move to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

void summary_move_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_warning(_("Destination is same as current folder."));
		return;
	}

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		summary_move_row_to(summaryview, &iter, to_folder);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		summary_status_show(summaryview);
	}
}

void summary_move_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_MOVE, NULL);
	summary_move_selected_to(summaryview, to_folder);
}

static void summary_copy_row_to(SummaryView *summaryview, GtkTreeIter *iter,
				FolderItem *to_folder)
{
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	GET_MSG_INFO(msginfo, iter);

	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
	if (!MSG_IS_COPY(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summaryview->copied++;
	}
	if (!prefs_common.immediate_exec)
		summary_set_row(summaryview, iter, msginfo);

	debug_print(_("Message %d is set to copy to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

void summary_copy_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *rows, *cur;
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;

	if (!to_folder) return;
	if (!summaryview->folder_item) return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_warning
			(_("Destination for copy is same as current folder."));
		return;
	}

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		GtkTreePath *path = (GtkTreePath *)cur->data;

		gtk_tree_model_get_iter(model, &iter, path);
		summary_copy_row_to(summaryview, &iter, to_folder);
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		summary_status_show(summaryview);
	}
}

void summary_copy_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_COPY, NULL);
	summary_copy_selected_to(summaryview, to_folder);
}

void summary_add_address(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;
	gchar *from;

	if (!summaryview->selected) return;

	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter))
		return;

	GET_MSG_INFO(msginfo, &iter);
	Xstrdup_a(from, msginfo->from, return);
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
}

void summary_select_all(SummaryView *summaryview)
{
	gtk_tree_selection_select_all(summaryview->selection);
}

void summary_unselect_all(SummaryView *summaryview)
{
	gtk_tree_selection_unselect_all(summaryview->selection);
}

void summary_select_thread(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, parent, child, next;
	GtkTreePath *start_path, *end_path;
	gboolean valid;

	valid = gtkut_tree_row_reference_get_iter(model, summaryview->selected,
						  &iter);
	if (!valid)
		return;

	while (gtk_tree_model_iter_parent(model, &parent, &iter))
		iter = parent;

	if (!gtk_tree_model_iter_children(model, &child, &iter))
		return;

	start_path = gtk_tree_model_get_path(model, &iter);

	for (;;) {
		next = iter = child;
		while (gtk_tree_model_iter_next(model, &next))
			iter = next;
		if (!gtk_tree_model_iter_children(model, &child, &iter))
			break;
	}

	end_path = gtk_tree_model_get_path(model, &iter);

	gtk_tree_view_expand_row(GTK_TREE_VIEW(summaryview->treeview),
				 start_path, TRUE);
	gtk_tree_selection_select_range(summaryview->selection,
					start_path, end_path);

	gtk_tree_path_free(end_path);
	gtk_tree_path_free(start_path);
}

void summary_save_as(SummaryView *summaryview)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;
	gchar *filename = NULL;
	gchar *src, *dest;

	if (!summaryview->selected) return;
	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter))
		return;

	GET_MSG_INFO(msginfo, &iter);
	if (!msginfo) return;

	if (msginfo->subject) {
		Xstrdup_a(filename, msginfo->subject, return);
		subst_for_filename(filename);
	}

	dest = filesel_save_as(filename);
	if (!dest) return;

	src = procmsg_get_message_file(msginfo);
	if (copy_file(src, dest, TRUE) < 0) {
		gchar *utf8_dest;

		utf8_dest = conv_filename_to_utf8(dest);
		alertpanel_error(_("Can't save the file `%s'."),
				 g_basename(utf8_dest));
		g_free(utf8_dest);
	}
	g_free(src);

	g_free(dest);
}

void summary_print(SummaryView *summaryview)
{
	MsgInfo *msginfo;
	GSList *mlist, *cur;
	gchar *cmdline;
	gchar *p;

	if (gtk_tree_selection_count_selected_rows(summaryview->selection) == 0)
		return;

	cmdline = input_dialog(_("Print"),
			       _("Enter the print command line:\n"
				 "(`%s' will be replaced with file name)"),
			       prefs_common.print_cmd);
	if (!cmdline) return;
	if (!(p = strchr(cmdline, '%')) || *(p + 1) != 's' ||
	    strchr(p + 2, '%')) {
		alertpanel_error(_("Print command line is invalid:\n`%s'"),
				 cmdline);
		g_free(cmdline);
		return;
	}

	mlist = summary_get_selected_msg_list(summaryview);
	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		if (msginfo) procmsg_print_message(msginfo, cmdline);
	}
	g_slist_free(mlist);

	g_free(cmdline);
}

gboolean summary_execute(SummaryView *summaryview)
{
	gint val = 0;

	if (!summaryview->folder_item) return FALSE;

	if (summary_is_locked(summaryview)) return FALSE;
	summary_lock(summaryview);

	val |= summary_execute_move(summaryview);
	val |= summary_execute_copy(summaryview);
	val |= summary_execute_delete(summaryview);

	summary_unlock(summaryview);

	summary_remove_invalid_messages(summaryview);

	statusbar_pop_all();
	STATUSBAR_POP(summaryview->mainwin);

	if (val != 0) {
		alertpanel_error(_("Error occurred while processing messages."));
	}

	return TRUE;
}

static void summary_remove_invalid_messages(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	MsgInfo *disp_msginfo = NULL, *msginfo;
	GtkTreeIter iter, next;
	GtkTreePath *path;
	gboolean valid;

	/* get currently displayed message */
	if (summaryview->displayed) {
		GtkTreeIter displayed;

		valid = gtkut_tree_row_reference_get_iter
			(model, summaryview->displayed, &displayed);
		if (valid) {
			gtk_tree_model_get(model, &displayed,
					   S_COL_MSG_INFO, &disp_msginfo, -1);
			if (MSG_IS_INVALID(disp_msginfo->flags)) {
				valid = FALSE;
				disp_msginfo = NULL;
			}
		}
		if (!valid) {
			/* g_print("displayed became invalid before removing\n"); */
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
	}

	if (summaryview->folder_item->threaded)
		summary_modify_threads(summaryview);

	/* update selection */
	valid = gtkut_tree_row_reference_get_iter(model, summaryview->selected,
						  &iter);
	if (valid) {
		valid = summary_find_nearest_msg(summaryview, &next, &iter);
		if (valid) {
			gtk_tree_model_get(model, &next,
					   S_COL_MSG_INFO, &msginfo, -1);
			if (disp_msginfo && disp_msginfo == msginfo) {
				/* g_print("replace displayed\n"); */
				path = gtk_tree_model_get_path(model, &next);
				gtk_tree_row_reference_free
					(summaryview->displayed);
				summaryview->displayed =
					gtk_tree_row_reference_new(model, path);
				gtk_tree_path_free(path);
			}
			summary_select_row
				(summaryview, &next,
				 (prefs_common.immediate_exec &&
				  messageview_is_visible
					(summaryview->messageview)),
				 FALSE);
		}
	}
	if (!valid) {
		gtk_tree_row_reference_free(summaryview->selected);
		summaryview->selected = NULL;
	}

	for (valid = gtk_tree_model_get_iter_first(model, &iter);
	     valid == TRUE; iter = next) {
		next = iter;
		valid = gtkut_tree_model_next(model, &next);

		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		if (!MSG_IS_INVALID(msginfo->flags))
			continue;

		if (gtk_tree_model_iter_has_child(model, &iter)) {
			g_warning("summary_remove_invalid_messages(): "
				  "tried to remove row which has child\n");
			continue;
		}

		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
		procmsg_msginfo_free(msginfo);
	}

	if (summaryview->displayed &&
	    !gtk_tree_row_reference_valid(summaryview->displayed)) {
		/* g_print("displayed became invalid after removing. searching disp_msginfo...\n"); */
		if (disp_msginfo &&
		    gtkut_tree_model_find_by_column_data
			(model, &iter, NULL, S_COL_MSG_INFO, disp_msginfo)) {
			/* g_print("replace displayed\n"); */
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed =
				gtk_tree_row_reference_new(model, path);
			gtk_tree_path_free(path);
		} else {
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
	}

	if (gtk_tree_model_get_iter_first(model, &iter))
		gtk_widget_grab_focus(summaryview->treeview);
	else {
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		gtk_widget_grab_focus(summaryview->folderview->treeview);
	}

	summary_write_cache(summaryview);

	summary_update_status(summaryview);
	summary_status_show(summaryview);
}

static gboolean summary_execute_move_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && MSG_IS_MOVE(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summary_set_row(summaryview, iter, msginfo);
	}

	return FALSE;
}

static gint summary_execute_move(SummaryView *summaryview)
{
	gint val = 0;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search moving messages and execute */
	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store), 
			       summary_execute_move_func, summaryview);

	if (summaryview->mlist) {
		summaryview->mlist = g_slist_reverse(summaryview->mlist);
		val = procmsg_move_messages(summaryview->mlist);

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table,
					       FALSE);

		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;

	return val;
}

static gboolean summary_execute_copy_func(GtkTreeModel *model,
					  GtkTreePath *path, GtkTreeIter *iter,
					  gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && MSG_IS_COPY(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summary_set_row(summaryview, iter, msginfo);
	}

	return FALSE;
}

static gint summary_execute_copy(SummaryView *summaryview)
{
	gint val = 0;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search copying messages and execute */
	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store), 
			       summary_execute_copy_func, summaryview);

	if (summaryview->mlist) {
		summaryview->mlist = g_slist_reverse(summaryview->mlist);
		val = procmsg_copy_messages(summaryview->mlist);

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table,
					       FALSE);

		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;

	return val;
}

static gboolean summary_execute_delete_func(GtkTreeModel *model,
					    GtkTreePath *path,
					    GtkTreeIter *iter, gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (msginfo && MSG_IS_DELETED(msginfo->flags)) {
		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);
	}

	return FALSE;
}

static gint summary_execute_delete(SummaryView *summaryview)
{
	FolderItem *trash;
	gint val = 0;

	trash = summaryview->folder_item->folder->trash;
	if (FOLDER_TYPE(summaryview->folder_item->folder) == F_MH) {
		g_return_val_if_fail(trash != NULL, 0);
	}

	/* search deleting messages and execute */
	gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store), 
			       summary_execute_delete_func, summaryview);

	if (!summaryview->mlist) return 0;

	summaryview->mlist = g_slist_reverse(summaryview->mlist);

	if (summaryview->folder_item != trash)
		val = folder_item_move_msgs(trash, summaryview->mlist);
	else
		val = folder_item_remove_msgs(trash, summaryview->mlist);

	g_slist_free(summaryview->mlist);
	summaryview->mlist = NULL;

	if (summaryview->folder_item != trash) {
		folder_item_scan(trash);
		folderview_update_item(trash, FALSE);
	}

	return val == -1 ? -1 : 0;
}

/* thread functions */

void summary_thread_build(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeStore *store = summaryview->store;
	GtkTreeIter iter, next;
	GSList *mlist;
	GNode *root, *node;
	GHashTable *node_table;
	MsgInfo *msginfo;
	gboolean valid;

	summary_lock(summaryview);

	debug_print(_("Building threads..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Building threads..."));
	main_window_cursor_wait(summaryview->mainwin);

	if (summaryview->folder_item)
		summaryview->folder_item->threaded = TRUE;

	mlist = summary_get_msg_list(summaryview);
	root = procmsg_get_thread_tree(mlist);
	node_table = g_hash_table_new(NULL, NULL);
	for (node = root->children; node != NULL; node = node->next) {
		g_hash_table_insert(node_table, node->data, node);
	}

	valid = gtk_tree_model_get_iter_first(model, &next);
	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);

		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);
		node = g_hash_table_lookup(node_table, msginfo);
		if (node) {
			GtkTreeIter child;

			for (node = node->children; node != NULL;
			     node = node->next) {
				summary_insert_gnode(summaryview, store, &child,
						     &iter, NULL, node);
			}
		} else
			gtk_tree_store_remove(store, &iter);
	}

	g_hash_table_destroy(node_table);
	g_node_destroy(root);
	g_slist_free(mlist);

	if (prefs_common.expand_thread)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(summaryview->treeview));

	summary_scroll_to_selected(summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static void summary_unthread_node_recursive(SummaryView *summaryview,
					    GtkTreeIter *iter,
					    GtkTreeIter *sibling)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter_, child;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	gtk_tree_store_insert_after(GTK_TREE_STORE(model), &iter_,
				    NULL, sibling);
	summary_set_row(summaryview, &iter_, msginfo);
	*sibling = iter_;

	valid = gtk_tree_model_iter_children(model, &child, iter);

	while (valid) {
		summary_unthread_node_recursive(summaryview, &child, sibling);
		valid = gtk_tree_model_iter_next(model, &child);
	}
}

static void summary_unthread_node(SummaryView *summaryview, GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter child, sibling, next;
	gboolean valid;

	sibling = *iter;

	valid = gtk_tree_model_iter_children(model, &next, iter);

	while (valid) {
		child = next;
		valid = gtk_tree_model_iter_next(model, &next);
		summary_unthread_node_recursive(summaryview, &child, &sibling);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &child);
	}
}

void summary_unthread(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, next;
	gboolean valid;

	summary_lock(summaryview);

	debug_print(_("Unthreading..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Unthreading..."));
	main_window_cursor_wait(summaryview->mainwin);

	if (summaryview->folder_item)
		summaryview->folder_item->threaded = FALSE;

	valid = gtk_tree_model_get_iter_first(model, &next);

	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);
		summary_unthread_node(summaryview, &iter);
	}

	summary_scroll_to_selected(summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static gboolean summary_has_invalid_node(GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter child;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);
	if (MSG_IS_INVALID(msginfo->flags))
		return TRUE;

	valid = gtk_tree_model_iter_children(model, &child, iter);

	while (valid) {
		if (summary_has_invalid_node(model, &child))
			return TRUE;
		valid = gtk_tree_model_iter_next(model, &child);
	}

	return FALSE;
}

static GNode *summary_get_modified_node(SummaryView *summaryview,
					GtkTreeIter *iter,
					GNode *parent, GNode *sibling)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GNode *node = NULL, *new_sibling;
	GtkTreeIter child;
	MsgInfo *msginfo;
	gboolean valid;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (!MSG_IS_INVALID(msginfo->flags)) {
		node = g_node_new(msginfo);
		g_node_insert_after(parent, sibling, node);
		parent = node;
		sibling = NULL;
	} else
		procmsg_msginfo_free(msginfo);

	valid = gtk_tree_model_iter_children(model, &child, iter);

	while (valid) {
		new_sibling = summary_get_modified_node(summaryview, &child,
							parent, sibling);
		if (new_sibling) {
			sibling = new_sibling;
			if (!node)
				node = sibling;
		}
		valid = gtk_tree_model_iter_next(model, &child);
	}

	return node;
}

#if 0
static gboolean traverse(GNode *node, gpointer data)
{
	gint i;

	if (!node->data)
		return FALSE;
	for (i = 0; i < g_node_depth(node); i++)
		g_print(" ");
	g_print("%s\n", ((MsgInfo *)node->data)->subject);
	return FALSE;
}
#endif

static void summary_modify_node(SummaryView *summaryview, GtkTreeIter *iter,
				GtkTreeIter *selected)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	MsgInfo *msginfo, *sel_msginfo = NULL;
	GNode *root, *cur;
	GtkTreeIter iter_, sibling;
	GtkTreePath *path, *sel_path;
	gboolean found = FALSE;

	if (!gtk_tree_model_iter_has_child(model, iter))
		return;
	if (!summary_has_invalid_node(model, iter))
		return;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	if (selected) {
		path = gtk_tree_model_get_path(model, iter);
		sel_path = gtk_tree_model_get_path(model, selected);
		if (gtk_tree_path_compare(path, sel_path) == 0 ||
		    gtk_tree_path_is_ancestor(path, sel_path))
			gtk_tree_model_get(model, selected,
					   S_COL_MSG_INFO, &sel_msginfo, -1);
		gtk_tree_path_free(sel_path);
		gtk_tree_path_free(path);
	}

	root = g_node_new(NULL);
	summary_get_modified_node(summaryview, iter, root, NULL);

	/* g_node_traverse(root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, traverse, NULL); */

	sibling = *iter;
	for (cur = root->children; cur != NULL; cur = cur->next) {
		summary_insert_gnode(summaryview, GTK_TREE_STORE(model),
				     &iter_, NULL, &sibling, cur);
		if (summaryview->folder_item->threaded &&
		    prefs_common.expand_thread) {
			path = gtk_tree_model_get_path(model, &iter_);
			gtk_tree_view_expand_row
				(GTK_TREE_VIEW(summaryview->treeview),
				 path, TRUE);
			gtk_tree_path_free(path);
		}
		if (sel_msginfo && !found) {
			found = gtkut_tree_model_find_by_column_data
				(model, selected, &iter_,
				 S_COL_MSG_INFO, sel_msginfo);
		}
		sibling = iter_;
	}

	g_node_destroy(root);

	gtk_tree_store_remove(GTK_TREE_STORE(model), iter);
}

static void summary_modify_threads(SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter, next, selected;
	GtkTreeIter *selected_p = NULL;
	gboolean valid, has_selection;

	summary_lock(summaryview);

	debug_print("Modifying threads for execution...");

	g_signal_handlers_block_by_func(summaryview->selection,
					summary_selection_changed, summaryview);

	has_selection = gtkut_tree_row_reference_get_iter
		(model, summaryview->selected, &selected);
	if (has_selection) {
		if (summary_find_nearest_msg(summaryview, &next, &selected)) {
			selected = next;
			selected_p = &selected;
		} else
			has_selection = FALSE;
	}

	valid = gtk_tree_model_get_iter_first(model, &next);

	while (valid) {
		iter = next;
		valid = gtk_tree_model_iter_next(model, &next);
		summary_modify_node(summaryview, &iter, selected_p);
	}

	g_signal_handlers_unblock_by_func(summaryview->selection,
					  summary_selection_changed,
					  summaryview);

	if (has_selection &&
	    !gtk_tree_row_reference_valid(summaryview->selected))
		summary_select_row(summaryview, &selected, FALSE, FALSE);

	debug_print("done.\n");

	summary_unlock(summaryview);
}

void summary_expand_threads(SummaryView *summaryview)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(summaryview->treeview));
}

void summary_collapse_threads(SummaryView *summaryview)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(summaryview->treeview));
}

static gboolean summary_filter_func(GtkTreeModel *model, GtkTreePath *path,
				    GtkTreeIter *iter, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo;
	FilterInfo *fltinfo;

	gtk_tree_model_get(model, iter, S_COL_MSG_INFO, &msginfo, -1);

	fltinfo = filter_info_new();
	fltinfo->flags = msginfo->flags;
	filter_apply_msginfo(prefs_common.fltlist, msginfo, fltinfo);
	if (fltinfo->actions[FLT_ACTION_MOVE] ||
	    fltinfo->actions[FLT_ACTION_COPY] ||
	    fltinfo->actions[FLT_ACTION_DELETE] ||
	    fltinfo->actions[FLT_ACTION_EXEC] ||
	    fltinfo->actions[FLT_ACTION_EXEC_ASYNC] ||
	    fltinfo->actions[FLT_ACTION_MARK] ||
	    fltinfo->actions[FLT_ACTION_COLOR_LABEL] ||
	    fltinfo->actions[FLT_ACTION_MARK_READ] ||
	    fltinfo->actions[FLT_ACTION_FORWARD] ||
	    fltinfo->actions[FLT_ACTION_FORWARD_AS_ATTACHMENT] ||
	    fltinfo->actions[FLT_ACTION_REDIRECT])
		summaryview->filtered++;

	if ((fltinfo->actions[FLT_ACTION_MARK] ||
	     fltinfo->actions[FLT_ACTION_COLOR_LABEL] ||
	     fltinfo->actions[FLT_ACTION_MARK_READ])) {
		msginfo->flags = fltinfo->flags;
		summary_set_row(summaryview, iter, msginfo);
	}

	if (fltinfo->actions[FLT_ACTION_MOVE] && fltinfo->move_dest)
		summary_move_row_to(summaryview, iter, fltinfo->move_dest);
	else if (fltinfo->actions[FLT_ACTION_DELETE])
		summary_delete_row(summaryview, iter);

	filter_info_free(fltinfo);

	return FALSE;
}

void summary_filter(SummaryView *summaryview, gboolean selected_only)
{
	if (!prefs_common.fltlist) return;

	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);

	debug_print(_("filtering..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Filtering..."));
	main_window_cursor_wait(summaryview->mainwin);

	summaryview->filtered = 0;

	if (selected_only)
		gtk_tree_selection_selected_foreach
			(summaryview->selection,
			 (GtkTreeSelectionForeachFunc)summary_filter_func,
			 summaryview);
	else
		gtk_tree_model_foreach(GTK_TREE_MODEL(summaryview->store),
				       summary_filter_func, summaryview);

	summary_unlock(summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	folderview_update_all_updated(FALSE);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	if (summaryview->filtered > 0) {
		gchar result_msg[BUFFSIZE];
		g_snprintf(result_msg, sizeof(result_msg),
			   _("%d message(s) have been filtered."),
			   summaryview->filtered);
		STATUSBAR_PUSH(summaryview->mainwin, result_msg);
	}
	summaryview->filtered = 0;
}

void summary_filter_open(SummaryView *summaryview, PrefsFilterType type)
{
	GtkTreeIter iter;
	MsgInfo *msginfo = NULL;
	gchar *header = NULL;
	gchar *key = NULL;

	if (!summaryview->selected) return;
	if (!gtkut_tree_row_reference_get_iter
		(GTK_TREE_MODEL(summaryview->store),
		 summaryview->selected, &iter))
		return;

	GET_MSG_INFO(msginfo, &iter);
	if (!msginfo) return;

	procmsg_get_filter_keyword(msginfo, &header, &key, type);
	prefs_filter_open(msginfo, header);

	g_free(header);
	g_free(key);
}

void summary_reply(SummaryView *summaryview, ComposeMode mode)
{
	GSList *mlist;
	MsgInfo *msginfo;
	MsgInfo *displayed_msginfo = NULL;
	gchar *text = NULL;

	mlist = summary_get_selected_msg_list(summaryview);
	if (!mlist) return;
	msginfo = (MsgInfo *)mlist->data;

	if (summaryview->displayed) {
		GtkTreeIter iter;

		if (gtkut_tree_row_reference_get_iter
			(GTK_TREE_MODEL(summaryview->store),
			 summaryview->displayed, &iter)) {
			GET_MSG_INFO(displayed_msginfo, &iter);
		}
	}

	/* use selection only if the displayed message is selected */
	if (!mlist->next && msginfo == displayed_msginfo) {
		text = gtkut_text_view_get_selection
			(GTK_TEXT_VIEW(summaryview->messageview->textview->text));
		if (text && *text == '\0') {
			g_free(text);
			text = NULL;
		}
	}

	if (!COMPOSE_QUOTE_MODE(mode))
		mode |= prefs_common.reply_with_quote
			? COMPOSE_WITH_QUOTE : COMPOSE_WITHOUT_QUOTE;

	switch (COMPOSE_MODE(mode)) {
	case COMPOSE_REPLY:
	case COMPOSE_REPLY_TO_SENDER:
	case COMPOSE_REPLY_TO_ALL:
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, summaryview->folder_item, mode, text);
		break;
	case COMPOSE_FORWARD:
		compose_forward(mlist, summaryview->folder_item, FALSE, text);
		break;
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward(mlist, summaryview->folder_item, TRUE, NULL);
		break;
	case COMPOSE_REDIRECT:
		compose_redirect(msginfo, summaryview->folder_item);
		break;
	default:
		g_warning("summary_reply(): invalid mode: %d\n", mode);
	}

	summary_update_selected_rows(summaryview);
	g_free(text);
	g_slist_free(mlist);
}

/* color label */

#define N_COLOR_LABELS colorlabel_get_color_count()

static void summary_colorlabel_menu_item_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	guint color = GPOINTER_TO_UINT(data);
	SummaryView *summaryview;

	summaryview = g_object_get_data(G_OBJECT(widget), "summaryview");
	g_return_if_fail(summaryview != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(summaryview->colorlabel_menu),
			      "dont_toggle"))
		return;

	summary_set_colorlabel(summaryview, color, NULL);
}

/* summary_set_colorlabel() - labelcolor parameter is the color *flag*
 * for the messsage; not the color index */
void summary_set_colorlabel(SummaryView *summaryview, guint labelcolor,
			    GtkWidget *widget)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GList *rows, *cur;
	GtkTreeIter iter;
	MsgInfo *msginfo;

	rows = gtk_tree_selection_get_selected_rows(summaryview->selection,
						    NULL);
	for (cur = rows; cur != NULL; cur = cur->next) {
		gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)cur->data);
		gtk_tree_model_get(model, &iter, S_COL_MSG_INFO, &msginfo, -1);

		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_CLABEL_FLAG_MASK);
		MSG_SET_COLORLABEL_VALUE(msginfo->flags, labelcolor);
		summary_set_row(summaryview, &iter, msginfo);

		gtk_tree_path_free((GtkTreePath *)cur->data);
	}

	g_list_free(rows);
}

static void summary_colorlabel_menu_item_activate_item_cb(GtkMenuItem *menuitem,
							  gpointer data)
{
	SummaryView *summaryview;
	GtkMenuShell *menu;
	GtkCheckMenuItem **items;
	gint n;
	GList *menu_cur;
	GSList *mlist, *cur;

	summaryview = (SummaryView *)data;
	g_return_if_fail(summaryview != NULL);

	menu = GTK_MENU_SHELL(summaryview->colorlabel_menu);
	g_return_if_fail(menu != NULL);

	mlist = summary_get_selected_msg_list(summaryview);
	if (!mlist) return;

	Xalloca(items, (N_COLOR_LABELS + 1) * sizeof(GtkWidget *), return);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle", GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, menu_cur = menu->children; menu_cur != NULL;
	     menu_cur = menu_cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(menu_cur->data)) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(menu_cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(menu_cur->data);
			n++;
		}
	}

	if (n == (N_COLOR_LABELS + 1)) {
		/* iterate all messages and set the state of the appropriate
		 * items */
		for (cur = mlist; cur != NULL; cur = cur->next) {
			MsgInfo *msginfo = (MsgInfo *)cur->data;
			gint clabel;

			if (msginfo) {
				clabel = MSG_GET_COLORLABEL_VALUE
					(msginfo->flags);
				if (!gtk_check_menu_item_get_active
					(items[clabel]))
					gtk_check_menu_item_set_active
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)\n", n);

	/* reset "dont_toggle" state */
	g_object_set_data(G_OBJECT(menu), "dont_toggle", GINT_TO_POINTER(0));
}

static void summary_colorlabel_menu_create(SummaryView *summaryview)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	gint i;

	label_menuitem = gtk_item_factory_get_item(summaryview->popupfactory,
						   "/Color label");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(summary_colorlabel_menu_item_activate_item_cb),
			 summaryview);
	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();

	/* create sub items. for the menu item activation callback we pass the
	 * color flag value as data parameter. Also we attach a data pointer
	 * so we can always get back the SummaryView pointer. */

	item = gtk_check_menu_item_new_with_label(_("None"));
	gtk_menu_append(GTK_MENU(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(summary_colorlabel_menu_item_activate_cb),
			 GUINT_TO_POINTER(0));
	g_object_set_data(G_OBJECT(item), "summaryview", summaryview);
	gtk_widget_show(item);

	item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_widget_show(item);

	/* create pixmap/label menu items */
	for (i = 0; i < N_COLOR_LABELS; i++) {
		item = colorlabel_create_check_color_menu_item(i);
		gtk_menu_append(GTK_MENU(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(summary_colorlabel_menu_item_activate_cb),
				 GUINT_TO_POINTER(i + 1));
		g_object_set_data(G_OBJECT(item), "summaryview", summaryview);
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	summaryview->colorlabel_menu = menu;
}

static GtkWidget *summary_tree_view_create(SummaryView *summaryview)
{
	GtkWidget *treeview;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *image;
	SummaryColumnType type;

	for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++)
		summaryview->columns[type] = NULL;

	store = gtk_tree_store_new(N_SUMMARY_COLS,
				   GDK_TYPE_PIXBUF,
				   GDK_TYPE_PIXBUF,
				   GDK_TYPE_PIXBUF,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_UINT,
				   G_TYPE_POINTER,
				   G_TYPE_INT,
				   G_TYPE_POINTER,
				   GDK_TYPE_COLOR,
				   G_TYPE_BOOLEAN);

#define SET_SORT(col, func)						\
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),	\
					col, func, summaryview, NULL)

	SET_SORT(S_COL_MARK, summary_cmp_by_mark);
	SET_SORT(S_COL_UNREAD, summary_cmp_by_unread);
	SET_SORT(S_COL_MIME, summary_cmp_by_mime);
	SET_SORT(S_COL_SUBJECT, summary_cmp_by_subject);
	SET_SORT(S_COL_FROM, summary_cmp_by_from);
	SET_SORT(S_COL_DATE, summary_cmp_by_date);
	SET_SORT(S_COL_SIZE, summary_cmp_by_size);
	SET_SORT(S_COL_NUMBER, summary_cmp_by_num);
	SET_SORT(S_COL_LABEL, summary_cmp_by_label);
	SET_SORT(S_COL_TO, summary_cmp_by_to);

#undef SET_SORT

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview),
				     prefs_common.enable_rules_hint);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), S_COL_SUBJECT);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), FALSE);
	g_object_set(treeview, "fixed-height-mode", TRUE, NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function(selection, summary_select_func,
					       summaryview, NULL);

#define ADD_COLUMN(title, type, col, text_attr, width, align)		\
{									\
	renderer = gtk_cell_renderer_ ## type ## _new();		\
	g_object_set(renderer, "xalign", align, "ypad", 0, NULL);	\
	column = gtk_tree_view_column_new_with_attributes		\
		(title, renderer, # type , col, NULL);			\
	summaryview->columns[col] = column;				\
	if (text_attr) {						\
		gtk_tree_view_column_set_attributes			\
			(column, renderer,				\
			 # type, col,					\
			 "foreground-gdk", S_COL_FOREGROUND,		\
			 "weight-set", S_COL_BOLD,			\
			 NULL);						\
		gtk_cell_renderer_text_set_fixed_height_from_font	\
			(GTK_CELL_RENDERER_TEXT(renderer), 1);		\
		g_object_set(G_OBJECT(renderer),			\
			     "weight", PANGO_WEIGHT_BOLD, NULL);	\
		gtk_tree_view_column_set_resizable(column, TRUE);	\
	}								\
	gtk_tree_view_column_set_alignment(column, align);		\
	gtk_tree_view_column_set_sizing					\
		(column, GTK_TREE_VIEW_COLUMN_FIXED);			\
	gtk_tree_view_column_set_fixed_width(column, width);		\
	gtk_tree_view_column_set_min_width(column, 8);			\
	gtk_tree_view_column_set_sort_column_id(column, col);		\
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);	\
	g_signal_connect(G_OBJECT(column->button), "clicked",		\
			 G_CALLBACK(summary_column_clicked),		\
			 summaryview);					\
	g_signal_connect(G_OBJECT(column->button), "size-allocate",	\
			 G_CALLBACK(summary_col_resized), summaryview);	\
}

	ADD_COLUMN(NULL, pixbuf, S_COL_MARK, FALSE,
		   SUMMARY_COL_MARK_WIDTH, 0.5);
	image = stock_pixbuf_widget(treeview, STOCK_PIXMAP_MARK);
	gtk_widget_show(image);
	gtk_tree_view_column_set_widget(column, image);
	ADD_COLUMN(NULL, pixbuf, S_COL_UNREAD, FALSE,
		   SUMMARY_COL_UNREAD_WIDTH, 0.5);
	image = stock_pixbuf_widget(treeview, STOCK_PIXMAP_MAIL_SMALL);
	gtk_widget_show(image);
	gtk_tree_view_column_set_widget(column, image);
	ADD_COLUMN(NULL, pixbuf, S_COL_MIME, FALSE,
		   SUMMARY_COL_MIME_WIDTH, 0.5);
	image = stock_pixbuf_widget(treeview, STOCK_PIXMAP_CLIP);
	gtk_widget_show(image);
	gtk_tree_view_column_set_widget(column, image);

	ADD_COLUMN(_("Subject"), text, S_COL_SUBJECT, TRUE,
		   prefs_common.summary_col_size[S_COL_SUBJECT], 0.0);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(treeview), column);
	ADD_COLUMN(_("From"), text, S_COL_FROM, TRUE,
		   prefs_common.summary_col_size[S_COL_FROM], 0.0);
	ADD_COLUMN(_("Date"), text, S_COL_DATE, TRUE,
		   prefs_common.summary_col_size[S_COL_DATE], 0.0);
	ADD_COLUMN(_("Size"), text, S_COL_SIZE, TRUE,
		   prefs_common.summary_col_size[S_COL_SIZE], 1.0);
	ADD_COLUMN(_("No."), text, S_COL_NUMBER, TRUE,
		   prefs_common.summary_col_size[S_COL_NUMBER], 1.0);

#undef ADD_COLUMN

	g_object_set_data(G_OBJECT(treeview), "user_data", summaryview);

	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(summary_button_pressed), summaryview);
	g_signal_connect(G_OBJECT(treeview), "button_release_event",
			 G_CALLBACK(summary_button_released), summaryview);
	g_signal_connect(G_OBJECT(treeview), "key_press_event",
			 G_CALLBACK(summary_key_pressed), summaryview);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(summary_selection_changed), summaryview);

	g_signal_connect(G_OBJECT(treeview), "row-expanded",
			 G_CALLBACK(summary_row_expanded), summaryview);
	g_signal_connect(G_OBJECT(treeview), "row-collapsed",
			 G_CALLBACK(summary_row_collapsed), summaryview);

	gtk_tree_view_enable_model_drag_source
		(GTK_TREE_VIEW(treeview),
		 0, summary_drag_types, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect_after(G_OBJECT(treeview), "drag-begin",
			 G_CALLBACK(summary_drag_begin), summaryview);
	g_signal_connect(G_OBJECT(treeview), "drag-data-get",
			 G_CALLBACK(summary_drag_data_get), summaryview);

	return treeview;
}

void summary_set_column_order(SummaryView *summaryview)
{
	const SummaryColumnState *col_state;
	SummaryColumnType type;
	GtkTreeViewColumn *column, *last_column = NULL;
	gint pos;

	col_state = prefs_summary_column_get_config();

	for (pos = 0; pos < N_SUMMARY_VISIBLE_COLS; pos++) {
		summaryview->col_state[pos] = col_state[pos];
		type = col_state[pos].type;
		summaryview->col_pos[type] = pos;
		column = summaryview->columns[type];

		gtk_tree_view_move_column_after
			(GTK_TREE_VIEW(summaryview->treeview),
			 column, last_column);
		gtk_tree_view_column_set_visible
			(column, col_state[pos].visible);
		last_column = column;

		debug_print("summary_set_column_order: "
			    "pos %d : type %d, vislble %d\n",
			    pos, type, summaryview->col_state[pos].visible);
	}
}


/* callback functions */

static gboolean summary_toggle_pressed(GtkWidget *eventbox,
				       GdkEventButton *event,
				       SummaryView *summaryview)
{
	if (!event) return FALSE;

	summary_toggle_view(summaryview);
	return FALSE;
}

static gboolean summary_button_pressed(GtkWidget *treeview,
				       GdkEventButton *event,
				       SummaryView *summaryview)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *column = NULL;
	gboolean is_selected;
	gboolean mod_pressed;
	gint px, py;

	if (!event) return FALSE;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
					   event->x, event->y, &path, &column,
					   NULL, NULL))
		return FALSE;

	/* pass through if the border of column titles is clicked */
	gtk_widget_get_pointer(treeview, &px, &py);
	if (py == (gint)event->y)
		return FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(summaryview->store),
				     &iter, path))
		return FALSE;

	is_selected = gtk_tree_selection_path_is_selected
		(summaryview->selection, path);
	mod_pressed = ((event->state &
		        (GDK_SHIFT_MASK|GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0);

	if ((event->button == 1 || event->button == 2)) {
		MsgInfo *msginfo;

		GET_MSG_INFO(msginfo, &iter);

		if (column == summaryview->columns[S_COL_MARK]) {
			if (!MSG_IS_DELETED(msginfo->flags) &&
			    !MSG_IS_MOVE(msginfo->flags) &&
			    !MSG_IS_COPY(msginfo->flags)) {
				if (MSG_IS_MARKED(msginfo->flags)) {
					summary_unmark_row(summaryview, &iter);
					if (MSG_IS_IMAP(msginfo->flags))
						imap_msg_unset_perm_flags
							(msginfo, MSG_MARKED);
				} else {
					summary_mark_row(summaryview, &iter);
					if (MSG_IS_IMAP(msginfo->flags))
						imap_msg_set_perm_flags
							(msginfo, MSG_MARKED);
				}
			}
			gtk_tree_path_free(path);
			return TRUE;
		} else if (column == summaryview->columns[S_COL_UNREAD]) {
			if (MSG_IS_UNREAD(msginfo->flags)) {
				summary_mark_row_as_read(summaryview, &iter);
				if (MSG_IS_IMAP(msginfo->flags))
					imap_msg_unset_perm_flags
						(msginfo, MSG_NEW | MSG_UNREAD);
				summary_status_show(summaryview);
			} else if (!MSG_IS_REPLIED(msginfo->flags) &&
				   !MSG_IS_FORWARDED(msginfo->flags)) {
				summary_mark_row_as_unread(summaryview, &iter);
				if (MSG_IS_IMAP(msginfo->flags))
					imap_msg_set_perm_flags
						(msginfo, MSG_UNREAD);
				summary_status_show(summaryview);
			}
			gtk_tree_path_free(path);
			return TRUE;
		}
	}

	if (event->button == 1) {
		if (summary_get_selection_type(summaryview) ==
			SUMMARY_SELECTED_MULTIPLE && is_selected &&
			!mod_pressed) {
			summaryview->can_toggle_selection = FALSE;
			summaryview->pressed_path = gtk_tree_path_copy(path);
		} else {
			if (event->type == GDK_2BUTTON_PRESS && is_selected)
				summary_activate_selected(summaryview);
			else {
				summaryview->can_toggle_selection = TRUE;
				if (!mod_pressed &&
				    messageview_is_visible(summaryview->messageview))
					summaryview->display_msg = TRUE;
			}
		}
	} else if (event->button == 2) {
		summary_select_row(summaryview, &iter, TRUE, FALSE);
		gtk_tree_path_free(path);
		return TRUE;
	} else if (event->button == 3) {
		/* right clicked */
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
		if (is_selected) {
			gtk_tree_path_free(path);
			return TRUE;
		}
	}

	gtk_tree_path_free(path);

	return FALSE;
}

static gboolean summary_button_released(GtkWidget *treeview,
					GdkEventButton *event,
					SummaryView *summaryview)
{
	if (!summaryview->can_toggle_selection && !summaryview->on_drag &&
	    summaryview->pressed_path) {
		summaryview->can_toggle_selection = TRUE;
		summaryview->display_msg =
			messageview_is_visible(summaryview->messageview);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview),
					 summaryview->pressed_path,
					 NULL, FALSE);
	}

	summaryview->can_toggle_selection = TRUE;
	summaryview->on_drag = FALSE;
	if (summaryview->pressed_path) {
		gtk_tree_path_free(summaryview->pressed_path);
		summaryview->pressed_path = NULL;
	}

	return FALSE;
}

void summary_pass_key_press_event(SummaryView *summaryview, GdkEventKey *event)
{
	summary_key_pressed(summaryview->treeview, event, summaryview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static gboolean summary_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    SummaryView *summaryview)
{
	MessageView *messageview;
	TextView *textview;
	GtkAdjustment *adj;
	gboolean mod_pressed;

	if (summary_is_locked(summaryview)) return FALSE;
	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_Left:		/* Move focus */
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(summaryview->scrolledwin));
		if (adj->lower != adj->value)
			return FALSE;
		/* FALLTHROUGH */
	case GDK_Escape:
		gtk_widget_grab_focus(summaryview->folderview->treeview);
		return TRUE;
	default:
		break;
	}

	if (!summaryview->selected)
		return FALSE;

	messageview = summaryview->messageview;
	if (messageview->type == MVIEW_MIME &&
	    gtk_notebook_get_current_page
		(GTK_NOTEBOOK(messageview->notebook)) == 1)
		textview = messageview->mimeview->textview;
	else
		textview = messageview->textview;

	mod_pressed =
		((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);

	switch (event->keyval) {
	case GDK_space:		/* Page down or go to the next */
		if (summaryview->selected &&
		    !gtkut_tree_row_reference_equal(summaryview->displayed,
						    summaryview->selected)) {
			summary_display_msg_selected(summaryview, FALSE, FALSE);
		} else if (mod_pressed) {
			if (!textview_scroll_page(textview, TRUE))
				summary_select_prev_unread(summaryview);
		} else {
			if (!textview_scroll_page(textview, FALSE))
				summary_select_next_unread(summaryview);
		}
		return TRUE;
	case GDK_BackSpace:	/* Page up */
		textview_scroll_page(textview, TRUE);
		return TRUE;
	case GDK_Return:	/* Scroll up/down one line */
		if (summaryview->selected &&
		    !gtkut_tree_row_reference_equal(summaryview->displayed,
						    summaryview->selected)) {
			summary_display_msg_selected(summaryview, FALSE, FALSE);
		} else
			textview_scroll_one_line(textview, mod_pressed);
		return TRUE;
	case GDK_Delete:
		BREAK_ON_MODIFIER_KEY();
		summary_delete(summaryview);
		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static void summary_set_bold_recursive(SummaryView *summaryview,
				       GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter child;
	MsgInfo *msginfo;
	gboolean valid;

	if (!gtk_tree_model_iter_has_child(model, iter))
		return;

	GET_MSG_INFO(msginfo, iter);
	if (!MSG_IS_UNREAD(msginfo->flags)) {
		gtk_tree_store_set(summaryview->store, iter,
				   S_COL_BOLD, FALSE, -1);
	}

	valid = gtk_tree_model_iter_children(model, &child, iter);
	while (valid) {
		summary_set_bold_recursive(summaryview, &child);
		valid = gtk_tree_model_iter_next(model, &child);
	}
}

static void summary_row_expanded(GtkTreeView *treeview, GtkTreeIter *iter,
				 GtkTreePath *path, SummaryView *summaryview)
{
	gtk_tree_view_expand_row(treeview, path, TRUE);

	if (prefs_common.bold_unread)
		summary_set_bold_recursive(summaryview, iter);

	/* workaround for last row expand problem */
	g_object_set(treeview, "fixed-height-mode", FALSE, NULL);
	gtk_widget_queue_resize(GTK_WIDGET(treeview));
	g_object_set(treeview, "fixed-height-mode", TRUE, NULL);
}

static void summary_row_collapsed(GtkTreeView *treeview, GtkTreeIter *iter,
				  GtkTreePath *path, SummaryView *summaryview)
{
	if (prefs_common.bold_unread &&
	    summary_have_unread_children(summaryview, iter)) {
		gtk_tree_store_set(summaryview->store, iter, S_COL_BOLD, TRUE,
				   -1);
	}
}

static gboolean summary_select_func(GtkTreeSelection *treeview,
				    GtkTreeModel *model, GtkTreePath *path,
				    gboolean cur_selected, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;

	return summaryview->can_toggle_selection;
}

static void summary_selection_changed(GtkTreeSelection *selection,
				      SummaryView *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *list;
	gboolean single_selection = FALSE;

	summary_status_show(summaryview);

	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	gtk_tree_row_reference_free(summaryview->selected);
	if (list) {
		if (list->next == NULL)
			single_selection = TRUE;
		path = (GtkTreePath *)list->data;
		gtk_tree_model_get_iter(model, &iter, path);
		summaryview->selected =
			gtk_tree_row_reference_new(model, path);
		g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
		g_list_free(list);
	} else
		summaryview->selected = NULL;

	if (!single_selection) {
		summaryview->display_msg = FALSE;
		if (summaryview->displayed && prefs_common.always_show_msg) {
			messageview_clear(summaryview->messageview);
			gtk_tree_row_reference_free(summaryview->displayed);
			summaryview->displayed = NULL;
		}
		summary_set_menu_sensitive(summaryview);
		main_window_set_toolbar_sensitive(summaryview->mainwin);
		return;
	}

	if (summaryview->display_msg ||
	    (prefs_common.always_show_msg &&
	     messageview_is_visible(summaryview->messageview))) {
		summaryview->display_msg = FALSE;
		if (!gtkut_tree_row_reference_equal(summaryview->displayed,
						    summaryview->selected)) {
			summary_display_msg(summaryview, &iter);
			return;
		}
	}

	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
}

static void summary_col_resized(GtkWidget *widget, GtkAllocation *allocation,
				SummaryView *summaryview)
{
	SummaryColumnType type;

	for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++) {
		if (summaryview->columns[type]->button == widget) {
			prefs_common.summary_col_size[type] = allocation->width;
			break;
		}
	}
}

static void summary_reply_cb(SummaryView *summaryview, guint action,
			     GtkWidget *widget)
{
	summary_reply(summaryview, (ComposeMode)action);
}

static void summary_show_all_header_cb(SummaryView *summaryview,
				       guint action, GtkWidget *widget)
{
	summary_display_msg_selected(summaryview, FALSE,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

static void summary_add_address_cb(SummaryView *summaryview,
				   guint action, GtkWidget *widget)
{
	summary_add_address(summaryview);
}

static void summary_sort_by_column_click(SummaryView *summaryview,
					 SummaryColumnType type)
{
	FolderItem *item = summaryview->folder_item;

	if (!item) return;

	if (item->sort_key == col_to_sort_key[type])
		summary_sort(summaryview, col_to_sort_key[type],
			     item->sort_type == SORT_ASCENDING
			     ? SORT_DESCENDING : SORT_ASCENDING);
	else
		summary_sort(summaryview, col_to_sort_key[type],
			     SORT_ASCENDING);
}

static void summary_column_clicked(GtkWidget *button, SummaryView *summaryview)
{
	SummaryColumnType type;

	for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++) {
		if (summaryview->columns[type]->button == button) {
			summary_sort_by_column_click(summaryview, type);
			break;
		}
	}
}

static void summary_drag_begin(GtkWidget *widget, GdkDragContext *drag_context,
			       SummaryView *summaryview)
{
	summaryview->on_drag = TRUE;
	gtk_drag_set_icon_default(drag_context);
}

static void summary_drag_data_get(GtkWidget        *widget,
				  GdkDragContext   *drag_context,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time,
				  SummaryView      *summaryview)
{
	GtkTreeModel *model = GTK_TREE_MODEL(summaryview->store);

	if (info == TARGET_MAIL_URI_LIST) {
		GList *rows, *cur;
		gchar *mail_list = NULL, *uri, *file;
		MsgInfo *msginfo;
		GtkTreeIter iter;

		rows = gtk_tree_selection_get_selected_rows
			(summaryview->selection, NULL);

		for (cur = rows; cur != NULL; cur = cur->next) {
			gtk_tree_model_get_iter(model, &iter,
						(GtkTreePath *)cur->data);
			gtk_tree_model_get(model, &iter, S_COL_MSG_INFO,
					   &msginfo, -1);
			file = procmsg_get_message_file(msginfo);
			if (!file) continue;
			uri = g_strconcat("file://", file, NULL);
			g_free(file);

			if (!mail_list) {
				mail_list = uri;
			} else {
				file = g_strconcat(mail_list, uri, NULL);
				g_free(mail_list);
				g_free(uri);
				mail_list = file;
			}

			gtk_tree_path_free((GtkTreePath *)cur->data);
		}

		if (mail_list != NULL) {
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       mail_list, strlen(mail_list));
			g_free(mail_list);
		}

		g_list_free(rows);
	} else if (info == TARGET_DUMMY) {
		if (gtk_tree_selection_count_selected_rows
			(summaryview->selection) > 0)
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       "Dummy", 6);
	}
}


/* custom compare functions for sorting */

#define CMP_FUNC_DEF(func_name, val)					\
static gint func_name(GtkTreeModel *model,				\
		      GtkTreeIter *a, GtkTreeIter *b, gpointer data)	\
{									\
	MsgInfo *msginfo_a, *msginfo_b;					\
									\
	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);	\
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);	\
									\
	if (!msginfo_a || !msginfo_b)					\
		return 0;						\
									\
	return (val);							\
}

CMP_FUNC_DEF(summary_cmp_by_mark,
	     MSG_IS_MARKED(msginfo_a->flags) - MSG_IS_MARKED(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_unread,
	     MSG_IS_UNREAD(msginfo_a->flags) - MSG_IS_UNREAD(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_mime,
	     MSG_IS_MIME(msginfo_a->flags) - MSG_IS_MIME(msginfo_b->flags))
CMP_FUNC_DEF(summary_cmp_by_label,
	     MSG_GET_COLORLABEL(msginfo_a->flags) -
	     MSG_GET_COLORLABEL(msginfo_b->flags))

CMP_FUNC_DEF(summary_cmp_by_num, msginfo_a->msgnum - msginfo_b->msgnum)
CMP_FUNC_DEF(summary_cmp_by_size, msginfo_a->size - msginfo_b->size)
CMP_FUNC_DEF(summary_cmp_by_date, msginfo_a->date_t - msginfo_b->date_t)

#undef CMP_FUNC_DEF
#define CMP_FUNC_DEF(func_name, var_name)				\
static gint func_name(GtkTreeModel *model,				\
		      GtkTreeIter *a, GtkTreeIter *b, gpointer data)	\
{									\
	MsgInfo *msginfo_a, *msginfo_b;					\
									\
	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);	\
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);	\
									\
	if (msginfo_a->var_name == NULL)				\
		return -(msginfo_b->var_name != NULL);			\
	if (msginfo_b->var_name == NULL)				\
		return (msginfo_a->var_name != NULL);			\
									\
	return g_strcasecmp(msginfo_a->var_name, msginfo_b->var_name);	\
}

CMP_FUNC_DEF(summary_cmp_by_from, fromname)
CMP_FUNC_DEF(summary_cmp_by_to, to);

#undef CMP_FUNC_DEF

static gint summary_cmp_by_subject(GtkTreeModel *model,			\
				   GtkTreeIter *a, GtkTreeIter *b,	\
				   gpointer data)			\
{									\
	MsgInfo *msginfo_a, *msginfo_b;					\
									\
	gtk_tree_model_get(model, a, S_COL_MSG_INFO, &msginfo_a, -1);	\
	gtk_tree_model_get(model, b, S_COL_MSG_INFO, &msginfo_b, -1);	\
									\
	if (msginfo_a->subject == NULL)					\
		return -(msginfo_b->subject != NULL);			\
	if (msginfo_b->subject == NULL)					\
		return (msginfo_a->subject != NULL);			\
									\
	return subject_compare_for_sort					\
		(msginfo_a->subject, msginfo_b->subject);		\
}
