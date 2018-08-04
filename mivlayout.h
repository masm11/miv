/*    miv - Masm11's Image Viewer
 *    Copyright (C) 2018  Yuuki Harano
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

#ifndef MIVLAYOUT_H__INCLUDED
#define MIVLAYOUT_H__INCLUDED

#include <gtk/gtk.h>

#define MIV_TYPE_LAYOUT            (miv_layout_get_type ())
#define MIV_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MIV_TYPE_LAYOUT, MivLayout))
#define MIV_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MIV_TYPE_LAYOUT, MivLayoutClass))
#define MIV_IS_LAYOUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MIV_TYPE_LAYOUT))
#define MIV_IS_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MIV_TYPE_LAYOUT))
#define MIV_LAYOUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MIV_TYPE_LAYOUT, MivLayoutClass))

typedef struct _MivLayout              MivLayout;
typedef struct _MivLayoutPrivate       MivLayoutPrivate;
typedef struct _MivLayoutClass         MivLayoutClass;

struct _MivLayout
{
  GtkContainer container;

  /*< private >*/
  MivLayoutPrivate *priv;
};

struct _MivLayoutClass
{
  GtkContainerClass parent_class;

  void (*translation_changed)(MivLayout *layout, gpointer data);
};


GType          miv_layout_get_type          (void) G_GNUC_CONST;

GtkWidget*     miv_layout_new               (void);

void           miv_layout_set_image         (MivLayout     *layout,
                                             GtkWidget     *child_widget);

void           miv_layout_set_selection_view(MivLayout     *layout,
                                             GtkWidget     *child_widget);

void           miv_layout_set_labels        (MivLayout     *layout,
                                             GtkWidget     *child_widget);

void           miv_layout_set_image_position (MivLayout     *layout,
                                              int           x,
                                              int           y);

void           miv_layout_set_fullscreen_mode (MivLayout    *layout,
                                               gboolean is_fullscreen);

void           miv_layout_translate_image     (MivLayout *layout,
                                               int dx,
                                               int dy);

void           miv_layout_reset_translation   (MivLayout *layout);

#endif	/* ifndef MIVLAYOUT_H__INCLUDED */
