#include <gtk/gtk.h>
#include <gst/gst.h>
#include <math.h>
#include "movie.h"

struct movie_work_t {
    int mode;	// 0: getting thumbnail,  1: playing
    
    GMainContext *ctxt;
    GMainLoop *loop;
    gchar *uri;
    GstElement *pipeline, *pixbuf;
    int step;
    gint64 duration;
    
    GdkPixbuf *pb;
    
    guint timer_id;
    void (*play_callback)(GdkPixbuf *);
};

static gboolean bus_callback__playing(GstBus *bus, GstMessage *message, gpointer user_data)
{
    struct movie_work_t *mw = user_data;
    
    printf("%s\t%s\n", GST_MESSAGE_TYPE_NAME(message), mw->uri);
    
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
	return FALSE;
	
    case GST_MESSAGE_ELEMENT:
	if (TRUE) {
	    const GstStructure *struc = gst_message_get_structure(message);
	    if (gst_structure_has_name(struc, "pixbuf")) {
		const GValue *val = gst_structure_get_value(struc, "pixbuf");
		GdkPixbuf *pb = g_value_peek_pointer(val);
		
		(*mw->play_callback)(pb);
	    }
	}
	break;

    default:	// suppress warnings.
	break;
    }
    
    return TRUE;
}

static gboolean timer_callback__playing(gpointer user_data)
{
    struct movie_work_t *mw = user_data;
    GdkPixbuf *pb;
    
#if 0
    g_object_get(G_OBJECT(mw->pixbuf), "last-pixbuf", &pb, NULL);
    (mw->play_callback)(pb);
#endif
    
    return TRUE;
}

static gboolean bus_callback__thumbnail(GstBus *bus, GstMessage *message, gpointer user_data)
{
    struct movie_work_t *mw = user_data;
    
    // printf("%s\t%s\n", GST_MESSAGE_TYPE_NAME(message), mw->uri);
    
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
	g_main_loop_quit(mw->loop);
	break;
	
    case GST_MESSAGE_ASYNC_DONE:
	switch (mw->step) {
	case 0:		// It is done to change state to "paused".
	    if (!gst_element_query_duration(mw->pipeline, GST_FORMAT_TIME, &mw->duration)) {
		printf("can't get duration.\n");
		g_main_loop_quit(mw->loop);
		break;
	    }
	    
#define NS 1000000000.0
	    gint64 pos = 0;
	    if (mw->duration >= 4 * NS)
		pos = sqrt(mw->duration / NS) * NS;
	    gst_element_seek_simple(mw->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, pos);
#undef NS
	    
	    mw->step++;
	    break;
	    
	case 1:		// seeking done.
	    g_object_get(G_OBJECT(mw->pixbuf), "last-pixbuf", &mw->pb, NULL);
	    g_main_loop_quit(mw->loop);
	    break;
	}
	break;
	
    default:	// suppress warnings.
	break;
    }
    
    return TRUE;
}

struct movie_work_t *movie_play(const gchar *fullpath, void (*display)(GdkPixbuf *))
{
    struct movie_work_t *mw = g_new0(struct movie_work_t, 1);
    mw->mode = 1;
    
    mw->pipeline = gst_element_factory_make ("playbin", NULL);
    mw->uri = g_strdup_printf("file://%s", fullpath);
    mw->pixbuf = gst_element_factory_make("gdkpixbufsink", NULL);
    g_object_set(G_OBJECT(mw->pipeline),
	    "uri", mw->uri,
	    "video-sink", mw->pixbuf,
	    NULL);
    
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(mw->pipeline));
    gst_bus_add_watch(bus, bus_callback__playing, mw);
    gst_object_unref(bus);
    
    gst_element_set_state (mw->pipeline, GST_STATE_PLAYING);
    
    mw->play_callback = display;
    
    mw->timer_id = g_timeout_add_full(G_PRIORITY_HIGH_IDLE + 50, 17, timer_callback__playing, mw, NULL);
    
    return mw;
}

void movie_stop(struct movie_work_t *mw)
{
    if (mw->timer_id != 0)
	g_source_remove(mw->timer_id);
    if (mw->pipeline != NULL) {
	gst_element_set_state (mw->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(mw->pipeline));		// pixbuf also be unref'ed.
    }
    if (mw->loop != NULL)
	g_main_loop_unref(mw->loop);
    if (mw->ctxt != NULL) {
	g_main_context_pop_thread_default(mw->ctxt);
	g_main_context_unref(mw->ctxt);
    }
    if (mw->uri != NULL)
	g_free(mw->uri);
    g_free(mw);
}

GdkPixbuf *movie_get_thumbnail(const gchar *fullpath)
{
    struct movie_work_t *mw = g_new0(struct movie_work_t, 1);
    
    mw->ctxt = g_main_context_new();
    g_main_context_push_thread_default(mw->ctxt);
    mw->loop = g_main_loop_new(mw->ctxt, FALSE);
    
    mw->pipeline = gst_element_factory_make ("playbin", NULL);
    mw->uri = g_strdup_printf("file://%s", fullpath);
    mw->pixbuf = gst_element_factory_make("gdkpixbufsink", NULL);
    g_object_set(G_OBJECT(mw->pipeline),
	    "uri", mw->uri,
	    "video-sink", mw->pixbuf,
	    NULL);
    
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(mw->pipeline));
    gst_bus_add_watch(bus, bus_callback__thumbnail, mw);
    gst_object_unref(bus);
    
    gst_element_set_state (mw->pipeline, GST_STATE_PAUSED);
    
    g_main_loop_run(mw->loop);
    
    if (mw->pipeline != NULL) {
	gst_element_set_state (mw->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(mw->pipeline));		// pixbuf also be unref'ed.
    }
    if (mw->loop != NULL)
	g_main_loop_unref(mw->loop);
    if (mw->ctxt != NULL) {
	g_main_context_pop_thread_default(mw->ctxt);
	g_main_context_unref(mw->ctxt);
    }
    if (mw->uri != NULL)
	g_free(mw->uri);
    GdkPixbuf *ret = mw->pb;
    g_free(mw);
    
    return ret;
}
