#ifndef MIVSELECTION_H__INCLUDED
#define MIVSELECTION_H__INCLUDED

struct miv_selection_t;

void image_selection_view_key_event(GtkWidget *widget, GdkEventKey *event, struct miv_selection_t *sw);
struct miv_selection_t *miv_selection_create(const gchar *dirname, gboolean display_first);
GtkWidget *miv_selection_get_widget(struct miv_selection_t *sw);

#endif	/* ifndef MIVSELECTION_H__INCLUDED */
