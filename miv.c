#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include "mivlayout.h"
#include "mivselection.h"
#include "miv.h"

enum {
    MODE_NONE,
    MODE_TRANSLATE,
    MODE_ROTATE,
    MODE_SCALE,
};

enum {
    STATUS_TRANSLATE,
    STATUS_SCALE,
    STATUS_ROTATE,
    STATUS_FULLSCREEN,
    STATUS_MAXIMIZE,
    NR_STATUS
};

#define MAX_STATUS_STRINGS	16

#define MAX_SCALE		10
#define DXDY			128

#define ANIM_TIMER_PRIORITY	(G_PRIORITY_DEFAULT_IDLE + 10)	// should be lower than painter.

static GtkWidget *win, *layout = NULL;
static gboolean is_fullscreen = FALSE, is_fullscreened = FALSE;
static GdkPixbuf *pixbuf = NULL;
static GtkWidget *img = NULL;
static GtkWidget *status = NULL, *mode_label;
static gchar *status_strings[NR_STATUS] = { NULL, };
static GtkWidget *labelbox = NULL;
static GtkWidget *image_selection_view = NULL;
static int mode = 0;
static int rotate = 0;	// 0, 90, 180, or 270
static int scale = 0;
static gboolean maximize = FALSE;
static struct miv_selection_t *selw;

/* animation */
static GdkPixbufAnimation *anim = NULL;
static GdkPixbufAnimationIter *anim_iter = NULL;
static guint anim_timer = 0;

static const char css_text[] =
	"box#labelbox label {"
	" color: #ffffff;"
	" background-color: rgba(0,0,0,0.7);"
	"}"
	"#image-selection {"
	" background-color: rgba(1,1,1,0.7);"
	"}"
	"#image-selection .item {"
	" margin: 5px;"
	" padding: 5px;"
	" background-color: rgba(0,0,0,0.7);"
	"}"
	"#image-selection .item:hover {"
	" background-color: rgba(0,128,255,0.7);"
	"}"
	"#image-selection .item:selected:not(:hover) {"
	" background-color: rgba(0,128,255,0.4);"
	"}"
	"#image-selection label {"
	" font-size: x-small;"
	" color: #ffffff;"
	" margin-top: 5px;"
	" margin-left: 5px;"
	"}"
;

static void relayout(void);

static void set_status_string(int idx, gchar *str)
{
    assert((unsigned int) idx < NR_STATUS);
    if (status_strings[idx] != NULL)
	g_free(status_strings[idx]);
    status_strings[idx] = str;
}

static gchar *make_status_string(void)
{
    gchar *strv[NR_STATUS + 1];
    int i, j;
    for (i = j = 0; i < NR_STATUS; i++) {
	if (status_strings[i] != NULL)
	    strv[j++] = status_strings[i];
    }
    strv[j] = NULL;
    return g_strjoinv(", ", strv);
}

static void update_status(void)
{
    gchar *s = make_status_string();
    if (strlen(s) != 0) {
	gtk_label_set_text(GTK_LABEL(status), s);
	gtk_widget_show(status);
    } else
	gtk_widget_hide(status);
    g_free(s);
}

static void relayout(void)
{
    GdkPixbuf *pb = NULL, *pb_old;
    
    if (is_fullscreen) {
	if (!is_fullscreened) {
	    /* GTK+ 3.22.30:
	     * It seems to need hide and show around fullscreen,
	     * when the image is large.
	     */
	    gtk_widget_hide(win);
	    gtk_window_fullscreen(GTK_WINDOW(win));
	    gtk_widget_show(win);
	    
	    miv_layout_set_fullscreen_mode(MIV_LAYOUT(layout), TRUE);
	    is_fullscreened = TRUE;
	}
    } else {
	if (is_fullscreened) {
	    gtk_widget_hide(win);
	    gtk_window_unfullscreen(GTK_WINDOW(win));
	    gtk_widget_show(win);
	    miv_layout_set_fullscreen_mode(MIV_LAYOUT(layout), FALSE);
	    is_fullscreened = FALSE;
	}
    }
    
    pb_old = gdk_pixbuf_copy(pixbuf);
    switch (rotate) {
    case 0:
	set_status_string(STATUS_ROTATE, NULL);
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_NONE);
	break;
    case 90:
	set_status_string(STATUS_ROTATE, g_strdup("rotate 90"));
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_CLOCKWISE);
	break;
    case 180:
	set_status_string(STATUS_ROTATE, g_strdup("rotate 180"));
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
	break;
    case 270:
	set_status_string(STATUS_ROTATE, g_strdup("rotate 270"));
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
	break;
    }
    g_object_unref(pb_old);
    
    double scale_ratio = 1.0;
    
    pb_old = pb;
    if (maximize && is_fullscreen) {
	set_status_string(STATUS_MAXIMIZE, g_strdup("maximize"));
	
	int w = gdk_pixbuf_get_width(pb_old);
	int h = gdk_pixbuf_get_height(pb_old);
	GtkAllocation alloc;
	gtk_widget_get_allocation(layout, &alloc);
	int lw = alloc.width, lh = alloc.height;	// size of layout widget.
	double wscale = (double) lw / w;
	double hscale = (double) lh / h;
	scale_ratio = fmin(wscale, hscale);
    } else
	set_status_string(STATUS_MAXIMIZE, NULL);
    
    if (scale != 0) {
	set_status_string(STATUS_SCALE, g_strdup_printf("scale %+d", scale));
	double ratio = pow(1.25, scale);
	scale_ratio *= ratio;
    } else
	set_status_string(STATUS_SCALE, NULL);
    
    if (scale_ratio != 1.0) {
	int width = gdk_pixbuf_get_width(pb_old);
	int height = gdk_pixbuf_get_height(pb_old);
	pb = gdk_pixbuf_scale_simple(pb_old, width * scale_ratio, height * scale_ratio, GDK_INTERP_HYPER);
    } else {
	pb = gdk_pixbuf_copy(pb_old);
    }
    
    g_object_unref(pb_old);
    
    gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);
    
    if (is_fullscreen)
	set_status_string(STATUS_FULLSCREEN, g_strdup("fullscreen"));
    else {
	set_status_string(STATUS_FULLSCREEN, NULL);
	gtk_window_resize(GTK_WINDOW(win), 500, 500);
    }
    
    g_object_unref(pb);
    
    if (labelbox != NULL) {
	update_status();
	
	switch (mode) {
	case MODE_NONE:
	    gtk_widget_hide(mode_label);
	    break;
	case MODE_ROTATE:
	    gtk_label_set_text(GTK_LABEL(mode_label), "rotating");
	    gtk_widget_show(mode_label);
	    break;
	case MODE_SCALE:
	    gtk_label_set_text(GTK_LABEL(mode_label), "scaling");
	    gtk_widget_show(mode_label);
	    break;
	case MODE_TRANSLATE:
	    gtk_label_set_text(GTK_LABEL(mode_label), "translating");
	    gtk_widget_show(mode_label);
	    break;
	}
    }
}

static void layout_translation_changed(GtkWidget *layout, gpointer data, gpointer user_data)
{
    GdkPoint *ptr = data;
    if (ptr->x != 0 || ptr->y != 0)
	set_status_string(STATUS_TRANSLATE, g_strdup_printf("translate %+d%+d ", ptr->x, ptr->y));
    else
	set_status_string(STATUS_TRANSLATE, NULL);
}

static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (event->keyval == GDK_KEY_Q || event->keyval == GDK_KEY_q) {
	gtk_main_quit();
	return TRUE;
    }
    
    switch (event->keyval) {
    case GDK_KEY_F:
    case GDK_KEY_f:
	is_fullscreen = !is_fullscreen;
	break;
	
    case GDK_KEY_M:
    case GDK_KEY_m:
	maximize = !maximize;
	break;
	
    case GDK_KEY_R:
    case GDK_KEY_r:
	mode = MODE_ROTATE;
	break;
	
    case GDK_KEY_S:
    case GDK_KEY_s:
	mode = MODE_SCALE;
	break;
	
    case GDK_KEY_T:
    case GDK_KEY_t:
	mode = MODE_TRANSLATE;
	break;
	
    case GDK_KEY_Escape:
	mode = MODE_NONE;
	break;
	
    case GDK_KEY_Left:
	switch (mode) {
	case MODE_NONE:
	    if (gtk_widget_get_mapped(image_selection_view)) {
		image_selection_view_key_event(image_selection_view, event, selw);
		break;
	    }
	    /* fall through */
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), DXDY, 0);
	    break;
	case MODE_ROTATE:
	    if ((rotate -= 90) < 0)
		rotate += 360;
	    break;
	}
	break;
	
    case GDK_KEY_Right:
	switch (mode) {
	case MODE_NONE:
	    if (gtk_widget_get_mapped(image_selection_view)) {
		image_selection_view_key_event(image_selection_view, event, selw);
		break;
	    }
	    /* fall through */
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), -DXDY, 0);
	    break;
	case MODE_ROTATE:
	    if ((rotate += 90) >= 360)
		rotate -= 360;
	    break;
	}
	break;
	
    case GDK_KEY_Up:
	switch (mode) {
	case MODE_NONE:
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), 0, DXDY);
	    break;
	case MODE_SCALE:
	    if ((scale += 1) >= MAX_SCALE)
		scale = MAX_SCALE;
	    break;
	}
	break;
	
    case GDK_KEY_Down:
	switch (mode) {
	case MODE_NONE:
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), 0, -DXDY);
	    break;
	case MODE_SCALE:
	    if ((scale -= 1) <= -MAX_SCALE)
		scale = -MAX_SCALE;
	    break;
	}
	break;
	
    case GDK_KEY_1:
	scale = 0;
	break;
	
    case GDK_KEY_0:
	miv_layout_reset_translation(MIV_LAYOUT(layout));
	break;
	
    case GDK_KEY_v:
    case GDK_KEY_V:
	if (gtk_widget_get_mapped(image_selection_view))
	    image_selection_view_key_event(image_selection_view, event, selw);
	else
	    gtk_widget_show(image_selection_view);
	mode = MODE_NONE;
	break;
	
    case GDK_KEY_Return:
	if (mode == MODE_NONE) {
	    if (gtk_widget_get_mapped(image_selection_view))
		image_selection_view_key_event(image_selection_view, event, selw);
	} else
	    mode = MODE_NONE;
	break;
	
    case GDK_KEY_space:
	image_selection_view_key_event(image_selection_view, event, selw);
	break;
	
    case GDK_KEY_BackSpace:
	image_selection_view_key_event(image_selection_view, event, selw);
	break;
    }
    
    relayout();
    
    return TRUE;
}

static gboolean anim_advance(gpointer user_data)
{
    anim_timer = 0;
    
    gdk_pixbuf_animation_iter_advance(anim_iter, NULL);
    
    if (pixbuf != NULL) {
	g_object_unref(pixbuf);
	pixbuf = NULL;
    }
    GdkPixbuf *pb = gdk_pixbuf_animation_iter_get_pixbuf(anim_iter);
    pixbuf = gdk_pixbuf_copy(pb);
    
    int msec = gdk_pixbuf_animation_iter_get_delay_time(anim_iter);
    if (msec != -1)
	anim_timer = g_timeout_add_full(ANIM_TIMER_PRIORITY, msec, (GSourceFunc) anim_advance, NULL, NULL);
    
    relayout();
    
    return FALSE;
}

static void anim_start(GdkPixbufAnimation *an)
{
    if (anim_timer != 0) {
	g_source_remove(anim_timer);
	anim_timer = 0;
    }
    if (anim_iter != NULL) {
	g_object_unref(anim_iter);
	anim_iter = NULL;
    }
    if (anim != NULL) {
	g_object_unref(anim);
	anim = NULL;
    }
    if (pixbuf != NULL) {
	g_object_unref(pixbuf);
	pixbuf = NULL;
    }
    
    anim = an;
    anim_iter = gdk_pixbuf_animation_get_iter(an, NULL);
    GdkPixbuf *pb = gdk_pixbuf_animation_iter_get_pixbuf(anim_iter);
    pixbuf = gdk_pixbuf_copy(pb);
    int msec = gdk_pixbuf_animation_iter_get_delay_time(anim_iter);
    anim_timer = g_timeout_add_full(ANIM_TIMER_PRIORITY, msec, (GSourceFunc) anim_advance, NULL, NULL);
    
    relayout();
}

gboolean miv_display(const gchar *path, GError **err)
{
    GError *err_dummy;
    if (err == NULL)
	err = &err_dummy;
    
    *err = NULL;
    GdkPixbufAnimation *an = gdk_pixbuf_animation_new_from_file(path, err);
    if (*err == NULL) {
	if (!gdk_pixbuf_animation_is_static_image(an)) {
	    /* This is really an animation. */
	    anim_start(an);
	    return TRUE;
	}
	g_object_unref(an);
    }
    
    *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, err);
    if (*err == NULL) {
	if (pixbuf != NULL) {
	    g_object_unref(pixbuf);
	    pixbuf = NULL;
	}
	pixbuf = pb;
	relayout();
	return TRUE;
    }
    
    return FALSE;
}

static void init_style(void)
{
    GdkScreen *scr = gdk_screen_get_default();
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css_text, -1, NULL);
    gtk_style_context_add_provider_for_screen(scr, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    if (argc < 2) {
	fprintf(stderr, "No file specified.\n");
	exit(1);
    }
    
    init_style();
    
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK(key_press_event), NULL);
    gtk_window_set_default_size(GTK_WINDOW(win), 500, 500);
    gtk_widget_show(win);
    
    layout = miv_layout_new();
    g_signal_connect(G_OBJECT(layout), "translation-changed", G_CALLBACK(layout_translation_changed), NULL);
    gtk_widget_show(layout);
    gtk_container_add(GTK_CONTAINER(win), layout);
    
    labelbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(labelbox, "labelbox");
    gtk_widget_show(labelbox);
    miv_layout_set_labels(MIV_LAYOUT(layout), labelbox);
    
    status = gtk_label_new("");
    gtk_widget_show(status);
    gtk_box_pack_start(GTK_BOX(labelbox), status, FALSE, FALSE, 0);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(labelbox), hbox, FALSE, FALSE, 0);
    
    mode_label = gtk_label_new("");
    gtk_widget_show(mode_label);
    gtk_box_pack_start(GTK_BOX(hbox), mode_label, FALSE, FALSE, 0);
    
    gchar *path = argv[1];
    gchar *filepath = NULL, *dirname = NULL;
    gboolean display_first = FALSE;
    
    if (g_file_test(path, G_FILE_TEST_IS_DIR))
	dirname = path;
    else {
	filepath = path;
	dirname = g_path_get_dirname(path);
    }
    
    img = gtk_image_new_from_icon_name("image-missing", GTK_ICON_SIZE_LARGE_TOOLBAR);
    if (filepath != NULL) {
	GError *err = NULL;
	if (!miv_display(filepath, &err)) {
	    if (err != NULL)
		fprintf(stderr, "%s", err->message);
	    else
		fprintf(stderr, "Can't display for some reasons.\n");
	    exit(1);
	}
	display_first = FALSE;
    } else {
	display_first = TRUE;
    }
    gtk_widget_show(img);
    miv_layout_set_image(MIV_LAYOUT(layout), img);
    
    selw = miv_selection_create(dirname, display_first);
    image_selection_view = miv_selection_get_widget(selw);
    miv_layout_set_selection_view(MIV_LAYOUT(layout), image_selection_view);
    
    gtk_main();
    
    return 0;
}
