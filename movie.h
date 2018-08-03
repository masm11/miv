#ifndef MOVIE_H__INCLUDED
#define MOVIE_H__INCLUDED

struct movie_work_t *movie_play(const gchar *fullpath, void (*display)(GdkPixbuf *));
void movie_stop(struct movie_work_t *mw);
GdkPixbuf *movie_get_thumbnail(const gchar *fullpath);

#endif	/* ifndef MOVIE_H__INCLUDED */
