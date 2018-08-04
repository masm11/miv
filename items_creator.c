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

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <math.h>
#include <time.h>
#include "movie.h"
#include "priority.h"
#include "items_creator.h"

struct item_t {
    const gchar *fullpath;
    GdkPixbuf *pixbuf;
    GtkWidget *item;
    
    glong last_high_time;	// when prioritized. msec.
};

struct items_creator_t {
    gpointer user_data;
    
    GThread *thr;
    
    int fds[2];
    GIOChannel *ioch;
    guint io_id;
    
    GList *add_list;		// items to add.
    guint add_list_idle_id;
    guint add_list_timer_id;
    GtkWidget *(*add_handler)(const gchar *fullpath, gpointer user_data);
    void (*replace_handler)(GtkWidget *item, GdkPixbuf *pixbuf, gpointer user_data);
    
    GMutex pending_mutex;
    GCond pending_cond;
    GMutex done_mutex;
    
    guint high_idle_id;
    guint low_idle_id;
    GList *pending_high_item_list;
    GList *pending_low_item_list;
    GList *done_high_item_list;
    GList *done_low_item_list;
    
    gboolean finishing;
};

static inline glong now(void) {
    struct timespec ts;
    if (G_UNLIKELY(clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == -1)) {
	perror("clock_gettime");
	exit(1);
    }
    return (glong) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static inline struct item_t *alloc_item(const gchar *fullpath)
{
    struct item_t *ip = g_new0(struct item_t, 1);
    ip->fullpath = fullpath;
    return ip;
}

static inline void free_item(struct item_t *ip)
{
    if (G_LIKELY(ip->pixbuf != NULL))
	g_object_unref(ip->pixbuf);
    g_free(ip);
}

static GdkPixbuf *create_pixbuf(const gchar *fullpath)
{
#define SIZE 100.0
    GError *err = NULL;
    GdkPixbuf *pb = NULL, *pb_old = gdk_pixbuf_new_from_file(fullpath, &err);
    gboolean is_movie = FALSE;
    if (pb_old == NULL) {
	pb_old = movie_get_thumbnail(fullpath);
	if (pb_old != NULL)
	    is_movie = TRUE;
    }
    if (pb_old != NULL) {
	int orig_w = gdk_pixbuf_get_width(pb_old);
	int orig_h = gdk_pixbuf_get_height(pb_old);
	double scalew = SIZE / orig_w;
	double scaleh = SIZE / orig_h;
	double scale = fmin(scalew, scaleh);
	int w = orig_w * scale;
	int h = orig_h * scale;
	pb = gdk_pixbuf_scale_simple(pb_old, w, h, GDK_INTERP_NEAREST);
	if (is_movie) {
	    static gboolean true = TRUE;
	    g_object_set_data(G_OBJECT(pb), "miv-is-movie", &true);
	}
    }
#undef SIZE
    return pb;
}

static void *items_creator_thread(void *parm)
{
    struct items_creator_t *w = parm;
    
    while (TRUE) {
	struct item_t *ip = NULL;
	gboolean high = FALSE;
	
	g_mutex_lock(&w->pending_mutex);
	while (TRUE) {
	    if (G_UNLIKELY(w->finishing)) {
		g_mutex_unlock(&w->pending_mutex);
		goto finish;
	    }
	    if (G_LIKELY(ip == NULL)) {
		if (w->pending_high_item_list != NULL) {
		    ip = w->pending_high_item_list->data;
		    w->pending_high_item_list = g_list_remove(w->pending_high_item_list, ip);
		    high = TRUE;
		}
	    }
	    if (ip == NULL) {
		if (w->pending_low_item_list != NULL) {
		    ip = w->pending_low_item_list->data;
		    w->pending_low_item_list = g_list_remove(w->pending_low_item_list, ip);
		    high = FALSE;
		}
	    }
	    if (G_LIKELY(ip != NULL))
		break;
	    g_cond_wait(&w->pending_cond, &w->pending_mutex);
	}
	g_mutex_unlock(&w->pending_mutex);
	
	if ((ip->pixbuf = create_pixbuf(ip->fullpath)) == NULL) {
	    free_item(ip);
	    continue;
	}
	
	g_mutex_lock(&w->done_mutex);
	if (high) {
	    w->done_high_item_list = g_list_append(w->done_high_item_list, ip);
	    write(w->fds[1], "2", 1);
	} else {
	    w->done_low_item_list = g_list_append(w->done_low_item_list, ip);
	    write(w->fds[1], "1", 1);
	}
	g_mutex_unlock(&w->done_mutex);
    }
    
 finish:
    return NULL;
}

static gboolean handler_to_add(struct items_creator_t *w)
{
    struct item_t *ip;
    
    if (G_UNLIKELY(w->add_list == NULL)) {
	w->add_list_idle_id = 0;
	w->add_list_timer_id = 0;
	return FALSE;
    }
    
    ip = w->add_list->data;
    w->add_list = g_list_remove(w->add_list, ip);
    
    ip->item = (*w->add_handler)(ip->fullpath, w->user_data);
    
    g_mutex_lock(&w->pending_mutex);
    w->pending_low_item_list = g_list_append(w->pending_low_item_list, ip);
    g_cond_signal(&w->pending_cond);
    g_mutex_unlock(&w->pending_mutex);
    
    return TRUE;
}

static gboolean idle_handler_to_add(gpointer user_data)
{
    struct items_creator_t *w = user_data;
    gboolean r = handler_to_add(w);
    if (!r)
	w->add_list_idle_id = 0;
    return r;
}

static gboolean timer_handler_to_add(gpointer user_data)
{
    struct items_creator_t *w = user_data;
    gboolean r = handler_to_add(w);
    if (!r)
	w->add_list_timer_id = 0;
    return r;
}

static gboolean idle_handler_to_replace_low(gpointer user_data);
static gboolean idle_handler_to_replace_high(gpointer user_data)
{
    struct items_creator_t *w = user_data;
    struct item_t *ip = NULL;
    
    glong n = now();
    
    g_mutex_lock(&w->done_mutex);
    while (G_LIKELY(w->done_high_item_list != NULL)) {
	ip = w->done_high_item_list->data;
	w->done_high_item_list = g_list_remove(w->done_high_item_list, ip);
	
	if (n - ip->last_high_time < 1000)
	    break;
	
	w->done_low_item_list = g_list_prepend(w->done_low_item_list, ip);
	if (w->low_idle_id == 0)
	    w->low_idle_id = g_idle_add_full(PRIORITY_LOW, idle_handler_to_replace_low, w, NULL);
	ip = NULL;
    }
    g_mutex_unlock(&w->done_mutex);
    
    if (ip != NULL) {
	(*w->replace_handler)(ip->item, ip->pixbuf, w->user_data);
	return TRUE;
    } else {
	w->high_idle_id = 0;
	return FALSE;
    }
}

static gboolean idle_handler_to_replace_low(gpointer user_data)
{
    struct items_creator_t *w = user_data;
    struct item_t *ip = NULL;
    
    g_mutex_lock(&w->done_mutex);
    if (G_LIKELY(w->done_low_item_list != NULL)) {
	ip = w->done_low_item_list->data;
	w->done_low_item_list = g_list_remove(w->done_low_item_list, ip);
    }
    g_mutex_unlock(&w->done_mutex);
    
    if (G_LIKELY(ip != NULL)) {
	(*w->replace_handler)(ip->item, ip->pixbuf, w->user_data);
	free_item(ip);
	return TRUE;
    } else {
	w->low_idle_id = 0;
	return FALSE;
    }
}

static gboolean ready(GIOChannel *ch, GIOCondition cond, gpointer user_data)
{
    struct items_creator_t *w = user_data;
    char buf[1];
    gsize s;
    GError *err = NULL;
    
    if (G_UNLIKELY(g_io_channel_read_chars(w->ioch, buf, 1, &s, &err) != G_IO_STATUS_NORMAL)) {
	printf("g_io_channel_read_chars() failed.\n");
	return FALSE;
    }
    
    switch (buf[0]) {
    case '1':	// low
	if (w->low_idle_id == 0)
	    w->low_idle_id = g_idle_add_full(PRIORITY_LOW, idle_handler_to_replace_low, w, NULL);
	break;
    case '2':	// high
	if (w->high_idle_id == 0)
	    w->high_idle_id = g_idle_add_full(PRIORITY_HIGH, idle_handler_to_replace_high, w, NULL);
	break;
    default:
	printf("bad byte read.");
	break;
    }
    
    return TRUE;
}

struct items_creator_t *items_creator_new(GList *fullpaths, gpointer user_data)
{
    struct items_creator_t *w = g_new0(struct items_creator_t, 1);
    
    g_mutex_init(&w->pending_mutex);
    g_cond_init(&w->pending_cond);
    g_mutex_init(&w->done_mutex);
    
    w->user_data = user_data;
    
    while (fullpaths != NULL) {
	const gchar *fullpath = fullpaths->data;
	fullpaths = g_list_remove(fullpaths, fullpath);
	
	struct item_t *ip = alloc_item(fullpath);
	w->add_list = g_list_append(w->add_list, ip);
    }
    
    w->add_list_idle_id = g_idle_add_full(PRIORITY_ADD, idle_handler_to_add, w, NULL);
    w->add_list_timer_id = g_timeout_add(200, timer_handler_to_add, w);	/* add items slowly even if relayout is busy. */
    
    if (G_UNLIKELY(pipe(w->fds) == -1)) {
	perror("pipe");
	exit(1);
    }
    w->ioch = g_io_channel_unix_new(w->fds[0]);
    g_io_channel_set_encoding(w->ioch, NULL, NULL);
    w->io_id = g_io_add_watch(w->ioch, G_IO_IN, ready, w);
    
    w->thr = g_thread_new(NULL, items_creator_thread, w);
    
    return w;
}

void items_creator_destroy(struct items_creator_t *w)
{
    g_mutex_lock(&w->pending_mutex);
    w->finishing = TRUE;
    g_cond_signal(&w->pending_cond);
    g_mutex_unlock(&w->pending_mutex);
    
    g_thread_join(w->thr);
    
    void stop_idle(guint id) {
	if (id != 0)
	    g_source_remove(id);
    }
    stop_idle(w->high_idle_id);
    stop_idle(w->low_idle_id);
    stop_idle(w->add_list_idle_id);
    stop_idle(w->add_list_timer_id);
    stop_idle(w->io_id);
    
    void free_list(GList *lp) {
	while (lp != NULL) {
	    struct item_t *ip = lp->data;
	    lp = g_list_remove(lp, ip);
	    free_item(ip);
	}
    }
    free_list(w->add_list);
    free_list(w->pending_high_item_list);
    free_list(w->pending_low_item_list);
    free_list(w->done_high_item_list);
    free_list(w->done_low_item_list);
    
    g_io_channel_unref(w->ioch);
    close(w->fds[0]);
    close(w->fds[1]);
    
    g_mutex_clear(&w->pending_mutex);
    g_cond_clear(&w->pending_cond);
    g_mutex_clear(&w->done_mutex);
}

void items_creator_set_add_handler(struct items_creator_t *w, GtkWidget *(*handler)(const gchar *fullpath, gpointer user_data))
{
    w->add_handler = handler;
}

void items_creator_set_replace_handler(struct items_creator_t *w, void (*handler)(GtkWidget *item, GdkPixbuf *pixbuf, gpointer user_data))
{
    w->replace_handler = handler;
}

void items_creator_prioritize(struct items_creator_t *w, GtkWidget *item)
{
    gboolean item_comparator(gconstpointer a, gconstpointer b) {
	const struct item_t *ip = a;
	const GtkWidget *w = b;
	return ip->item != w;
    }
    
    gboolean migrate_item(GList **fromp, GList **top, GtkWidget *item, glong now) {
	GList *from = *fromp, *to = *top;
	GList *lp = g_list_find_custom(from, item, item_comparator);
	gboolean r = FALSE;
	if (lp != NULL) {
	    struct item_t *ip = lp->data;
	    ip->last_high_time = now;
	    from = g_list_delete_link(from, lp);
	    to = g_list_append(to, ip);
	    r = TRUE;
	}
	*fromp = from;
	*top = to;
	return r;
    }
    
    glong n = now();
    
    g_mutex_lock(&w->pending_mutex);
    if (migrate_item(&w->pending_low_item_list, &w->pending_high_item_list, item, n))
	g_cond_signal(&w->pending_cond);
    g_mutex_unlock(&w->pending_mutex);
    
    g_mutex_lock(&w->done_mutex);
    if (migrate_item(&w->done_low_item_list, &w->done_high_item_list, item, n)) {
	if (w->high_idle_id == 0)
	    w->high_idle_id = g_idle_add_full(PRIORITY_HIGH, idle_handler_to_replace_high, w, NULL);
    }
    g_mutex_unlock(&w->done_mutex);
}
