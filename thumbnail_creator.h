#ifndef THUMBNAIL_CREATOR_H__INCLUDED
#define THUMBNAIL_CREATOR_H__INCLUDED

struct thumbnail_creator_job_t {
    const gchar *fullpath;
    GdkPixbuf *pixbuf;		// thumbnail image, or NULL if error.
    GtkWidget *vbox;
};

void thumbnail_creator_put_job(struct thumbnail_creator_job_t *job);
GList *thumbnail_creator_get_done(void);
void thumbnail_creator_prioritize(struct thumbnail_creator_job_t *job);
GList *thumbnail_creator_cancel(void);
GIOChannel *thumbnail_creator_init(void);

struct thumbnail_creator_job_t *thumbnail_creator_job_new(const gchar *fullpath, GtkWidget *vbox);
void thumbnail_creator_job_free(struct thumbnail_creator_job_t *job);

#endif	/* ifndef THUMBNAIL_CREATOR_H__INCLUDED */
