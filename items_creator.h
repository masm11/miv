#ifndef ITEMS_CREATOR_H__INCLUDED
#define ITEMS_CREATOR_H__INCLUDED

struct items_creator_t *items_creator_new(GList *fullpaths, gpointer user_data);

void items_creator_set_add_handler(struct items_creator_t *w, GtkWidget *(*handler)(const gchar *fullpath, gpointer user_data));
void items_creator_set_replace_handler(struct items_creator_t *w, void (*handler)(GtkWidget *item, GdkPixbuf *pixbuf, gpointer user_data));
void items_creator_prioritize(struct items_creator_t *w, GtkWidget *item);

#endif	/* ifndef ITEMS_CREATOR_H__INCLUDED */
