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

#ifndef MIVSELECTION_H__INCLUDED
#define MIVSELECTION_H__INCLUDED

struct miv_selection_t;

void image_selection_view_key_event(GtkWidget *widget, GdkEventKey *event, struct miv_selection_t *sw);
struct miv_selection_t *miv_selection_create(const gchar *dirname, gboolean display_first);
GtkWidget *miv_selection_get_widget(struct miv_selection_t *sw);

#endif	/* ifndef MIVSELECTION_H__INCLUDED */
