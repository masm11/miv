#include <gtk/gtk.h>

struct item_t {
    const gchar *fullpath;
    GdkPixbuf *pixbuf;
    GtkWidget *item;
    
    glong last_high_time;	// when prioritized. msec.
};

struct items_creator_t {
    gpointer user_data;
    
    GList *add_list;		// items to add.
    guint add_list_idle_id;
    GtkWidget *(*add_handler)(const gchar *fullpath, gpointer user_data);
    
    GList *high_item_list;
    GList *low_item_list;
};

static void *items_creator_thread(void *parm)
{
    struct items_creator_t *w = parm;
    
    
    
    return NULL;
}

static gboolean idle_handler_to_add(gpointer user_data)
{
    struct items_creator_t *w = user_data;
    struct item_t *ip;
    
    if (w->add_list == NULL) {
	w->add_list_idle_id = 0;
	return FALSE;
    }
    
    ip = w->add_list->data;
    w->add_list = g_list_remove(w->add_list, ip);
    
    ip->item = (*w->add_handler)(ip->fullpath, w->user_data);
    
    // fixme: needs lock.
    w->low_item_list = g_list_append(w->low_item_list, ip);
    
    return TRUE;
}

static gboolean idle_handler_to_replace_high(gpointer user_data)
{
    struct items_creator_t *w = user_data;
    
    
    return FALSE;
}

struct items_creator_t *items_creator_new(GList *fullpaths, gpointer user_data)
{
    struct items_creator_t *w = g_new0(struct items_creator_t, 1);
    
    w->user_data = user_data;
    
    while (fullpaths != NULL) {
	const gchar *fullpath = fullpaths->data;
	fullpaths = g_list_remove(fullpaths, fullpath);
	
	struct item_t *ip = g_new0(struct item_t, 1);
	ip->fullpath = fullpath;
	
	w->add_list = g_list_append(w->add_list, ip);
    }
    
    w->add_list_idle_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, idle_handler_to_add, w, NULL);
    
    return w;
}

void items_creator_set_add_handler(struct items_creator_t *w, GtkWidget *(*add_handler)(const gchar *fullpath, gpointer user_data))
{
    w->add_handler = add_handler;
}
