#include <gtk/gtk.h>
#include <math.h>
#include "thumbnail_creator.h"

static GThread *thr;

static GMutex *mutex;
static GCond *cond;
static GList *list;

static int notifier_fd;
static GList *done_list;
static GMutex *done_mutex;

static gboolean canceling;

static void *thread(void *parm)
{
    while (TRUE) {
	g_mutex_lock(mutex);
	if (canceling) {
	    g_mutex_unlock(mutex);
	    goto finish;
	}
	while (list == NULL) {
	    g_cond_wait(cond, mutex);
	    if (canceling) {
		g_mutex_unlock(mutex);
		goto finish;
	    }
	}
	struct thumbnail_creator_job_t *job = list->data;
	list = g_list_remove(list, job);
	g_mutex_unlock(mutex);
	
#define SIZE 100.0
	GError *err = NULL;
	GdkPixbuf *pb = NULL, *pb_old = gdk_pixbuf_new_from_file(job->fullpath, &err);
	if (err == NULL) {
	    int orig_w = gdk_pixbuf_get_width(pb_old);
	    int orig_h = gdk_pixbuf_get_height(pb_old);
	    double scalew = SIZE / orig_w;
	    double scaleh = SIZE / orig_h;
	    double scale = fmin(scalew, scaleh);
	    int w = orig_w * scale;
	    int h = orig_h * scale;
	    pb = gdk_pixbuf_scale_simple(pb_old, w, h, GDK_INTERP_NEAREST);
	}
#undef SIZE
	
	job->pixbuf = pb;
	g_mutex_lock(done_mutex);
	done_list = g_list_prepend(done_list, job);
	write(notifier_fd, "1", 1);
	g_mutex_unlock(done_mutex);
    }
    
 finish:
    return NULL;
}

void thumbnail_creator_put(struct thumbnail_creator_job_t *job)
{
    g_mutex_lock(mutex);
    // fixme: search in list.
    list = g_list_prepend(list, job);
    g_cond_signal(cond);
    g_mutex_unlock(mutex);
}

GList *thumbnail_creator_get(void)
{
    g_mutex_lock(done_mutex);
    GList *lp = done_list;
    done_list = NULL;
    g_mutex_unlock(done_mutex);
    return lp;
}

GList *thumbnail_creator_cancel(void)
{
    g_mutex_lock(mutex);
    canceling = TRUE;
    g_cond_signal(cond);
    g_mutex_unlock(mutex);
    
    g_thread_join(thr);
    
    GList *jobs = list;
    list = NULL;
    
    jobs = g_list_concat(jobs, done_list);
    done_list = NULL;
    
    canceling = FALSE;
    
    thr = g_thread_new("tn_creator", thread, NULL);
    
    return jobs;
}

GIOChannel *thumbnail_creator_init(void)
{
    int fds[2];
    if (pipe(fds) == -1) {
	perror("pipe");
	exit(1);
    }
    
    notifier_fd = fds[1];
    
    mutex = g_new(GMutex, 1);
    g_mutex_init(mutex);
    
    cond = g_new(GCond, 1);
    g_cond_init(cond);
    
    done_mutex = g_new(GMutex, 1);
    g_mutex_init(done_mutex);
    
    list = done_list = NULL;
    
    canceling = FALSE;
    
    thr = g_thread_new("tn_creator", thread, NULL);
    
    GIOChannel *ch = g_io_channel_unix_new(fds[0]);
    g_io_channel_set_encoding(ch, NULL, NULL);
    return ch;
}
