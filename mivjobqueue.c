#include "mivjobqueue.h"

struct _MivJobQueue {
    GObject parent_instance;
    
    gint priority;
    guint idle_id;
    
    GFunc worker;
    
    GList *jobs, *last_job;
};

struct job_t {
    gpointer data;
    gpointer user_data;
    GDestroyNotify destroyer;
};

G_DEFINE_TYPE(MivJobQueue, miv_job_queue, G_TYPE_OBJECT)

static void miv_job_queue_class_init(MivJobQueueClass *klass);
static void miv_job_queue_init(MivJobQueue *self);
static void miv_job_queue_dispose(GObject *gobject);
static void miv_job_queue_finalize(GObject *gobject);

static void miv_job_queue_class_init(MivJobQueueClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    
    object_class->dispose = miv_job_queue_dispose;
    object_class->finalize = miv_job_queue_finalize;
}

static void miv_job_queue_init(MivJobQueue *self)
{
}

static void miv_job_queue_dispose(GObject *gobject)
{
    MivJobQueue *queue = MIV_JOB_QUEUE(gobject);
    
    miv_job_queue_cancel_all(queue);
    
    (*G_OBJECT_CLASS(miv_job_queue_parent_class)->dispose)(gobject);
}

static void miv_job_queue_finalize(GObject *gobject)
{
    MivJobQueue *queue = MIV_JOB_QUEUE(gobject);
    
    if (queue->idle_id != 0) {
	g_source_remove(queue->idle_id);
	queue->idle_id = 0;
    }
    
    (*G_OBJECT_CLASS(miv_job_queue_parent_class)->finalize)(gobject);
}

MivJobQueue *miv_job_queue_new(gint priority, GFunc worker)
{
    MivJobQueue *queue = g_object_new(MIV_TYPE_JOB_QUEUE, NULL);
    queue->priority = priority;
    queue->worker = worker;
    queue->idle_id = 0;
    queue->jobs = queue->last_job = NULL;
    return queue;
}

static gboolean miv_job_queue_iter(gpointer user_data)
{
    MivJobQueue *queue = user_data;
    
    GList *lp = queue->jobs;
    
    if (lp == NULL) {
	queue->idle_id = 0;
	return FALSE;	// This do g_source_remove().
    }
    
    struct job_t *job = lp->data;
    queue->jobs = g_list_remove(queue->jobs, job);
    if (queue->jobs == NULL)
	queue->last_job = NULL;
    
    (*queue->worker)(job->data, job->user_data);
    
    if (job->destroyer != NULL)
	(*job->destroyer)(job->data);
    g_free(job);
    
    return TRUE;
}

void miv_job_queue_enqueue(
	MivJobQueue *queue, gpointer data, gpointer user_data, GDestroyNotify destroyer)
{
    struct job_t *job = g_new0(struct job_t, 1);
    job->data = data;
    job->user_data = user_data;
    job->destroyer = destroyer;
    
    if (queue->jobs != NULL) {
	GList *lp = g_list_append(queue->last_job, job);
	queue->last_job = g_list_last(lp);
    } else {
	queue->jobs = g_list_append(queue->jobs, job);
	queue->last_job = queue->jobs;
    }
    
    if (queue->idle_id == 0)
	queue->idle_id = g_idle_add_full(queue->priority, miv_job_queue_iter, queue, NULL);
}

void miv_job_queue_cancel_all(MivJobQueue *queue)
{
    while (TRUE) {
	GList *lp;
	if ((lp = queue->jobs) == NULL)
	    break;
	struct job_t *job = lp->data;
	queue->jobs = g_list_remove(queue->jobs, job);
	
	if (job->destroyer != NULL)
	    (*job->destroyer)(job->data);
	g_free(job);
    }
    queue->last_job = NULL;
    
    if (queue->idle_id != 0) {
	g_source_remove(queue->idle_id);
	queue->idle_id = 0;
    }
}
