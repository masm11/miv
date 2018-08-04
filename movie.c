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
#include "priority.h"
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
    
    guint idle_id;
    void (*play_callback)(GdkPixbuf *);
};

static gboolean callback_display(gpointer user_data)
{
    struct movie_work_t *mw = user_data;
    (*mw->play_callback)(mw->pb);
    mw->idle_id = 0;
    return FALSE;
}

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
		if (mw->pb != NULL)
		    g_object_unref(mw->pb);
		mw->pb = g_value_peek_pointer(val);
		g_object_ref(mw->pb);
		if (mw->idle_id == 0)
		    mw->idle_id = g_idle_add_full(PRIORITY_MOVIE, callback_display, mw, NULL);
	    }
	}
	break;

    default:	// suppress warnings.
	break;
    }
    
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
    
    return mw;
}

void movie_stop(struct movie_work_t *mw)
{
    if (mw->idle_id != 0)
	g_source_remove(mw->idle_id);
    if (mw->pb != NULL)
	g_object_unref(mw->pb);
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
