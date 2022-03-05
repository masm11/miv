/*    miv - Masm11's Image Viewer
 *    Copyright (C) 2018-2022  Yuuki Harano
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ITEMS_CREATOR_H__INCLUDED
#define ITEMS_CREATOR_H__INCLUDED

struct items_creator_t *items_creator_new(GList *fullpaths, gpointer user_data);
void items_creator_destroy(struct items_creator_t *w);

void items_creator_set_add_handler(struct items_creator_t *w, GtkWidget *(*handler)(const gchar *fullpath, gpointer user_data));
void items_creator_set_replace_handler(struct items_creator_t *w, void (*handler)(GtkWidget *item, GdkPixbuf *pixbuf, gpointer user_data));
void items_creator_prioritize(struct items_creator_t *w, GtkWidget *item);

#endif	/* ifndef ITEMS_CREATOR_H__INCLUDED */
