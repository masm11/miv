#ifndef MIVJOBQUEUE_H__INCLUDED
#define MIVJOBQUEUE_H__INCLUDED

#include <glib-object.h>

G_BEGIN_DECLS

#define MIV_TYPE_JOB_QUEUE miv_job_queue_get_type ()
G_DECLARE_FINAL_TYPE (MivJobQueue, miv_job_queue, MIV, JOB_QUEUE, GObject)

MivJobQueue *miv_job_queue_new(gint priority, GFunc worker);

void miv_job_queue_enqueue(
	MivJobQueue *queue, gpointer data, gpointer user_data, GDestroyNotify destroyer);
void miv_job_queue_cancel_all(MivJobQueue *queue);

G_END_DECLS

#endif /* ifndef MIVJOBQUEUE_H__INCLUDED */
