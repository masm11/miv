#ifndef ITEMS_CREATOR_H__INCLUDED
#define ITEMS_CREATOR_H__INCLUDED

struct items_creator_t *items_creator_new(GList *fullpaths, gpointer user_data);

void items_creator_set_add_handler(struct items_creator_t *w, GtkWidget *(*add_handler)(const gchar *fullpath, gpointer user_data));

#endif	/* ifndef ITEMS_CREATOR_H__INCLUDED */
