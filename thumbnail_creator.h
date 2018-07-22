#ifndef THUMBNAIL_CREATOR_H__INCLUDED
#define THUMBNAIL_CREATOR_H__INCLUDED

struct thumbnail_creator_job_t {
    gchar *fullpath;
    GdkPixbuf *pixbuf;		// thumbnail image, or NULL if error.
    GtkWidget *vbox;
};

void thumbnail_creator_put(struct thumbnail_creator_job_t *job);
GList *thumbnail_creator_get(void);
GList *thumbnail_creator_cancel(void);
GIOChannel *thumbnail_creator_init(void);

#endif	/* ifndef THUMBNAIL_CREATOR_H__INCLUDED */
