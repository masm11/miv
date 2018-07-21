#ifndef MIVSELECTION_H__INCLUDED
#define MIVSELECTION_H__INCLUDED

void image_selection_view_key_event(GtkWidget *widget, GdkEventKey *event);
GtkWidget *image_selection_view_create(const gchar *dirname, gboolean display_first);

#endif	/* ifndef MIVSELECTION_H__INCLUDED */
