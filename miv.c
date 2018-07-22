#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include "mivlayout.h"
#include "mivselection.h"

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

static GtkWidget *win, *layout = NULL;
static gboolean is_fullscreen = FALSE, is_fullscreened = FALSE;
static GdkPixbuf *pixbuf;
static GtkWidget *img;
static GtkWidget *status = NULL, *mode_label;
static gchar *status_strings[NR_STATUS] = { NULL, };
static GtkWidget *labelbox = NULL;
static GtkWidget *image_selection_view = NULL;
static int mode = 0;
static int rotate = 0;	// 0, 90, 180, or 270
static int scale = 0;
static gboolean maximize = FALSE;

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
		image_selection_view_key_event(image_selection_view, event);
		break;
	    }
	    /* fall through */
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), 128, 0);
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
		image_selection_view_key_event(image_selection_view, event);
		break;
	    }
	    /* fall through */
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), -128, 0);
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
	    miv_layout_translate_image(MIV_LAYOUT(layout), 0, 128);
	    break;
	case MODE_SCALE:
	    if ((scale += 1) >= 5)
		scale = 5;
	    break;
	}
	break;
	
    case GDK_KEY_Down:
	switch (mode) {
	case MODE_NONE:
	case MODE_TRANSLATE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), 0, -128);
	    break;
	case MODE_SCALE:
	    if ((scale -= 1) <= -5)
		scale = -5;
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
	    image_selection_view_key_event(image_selection_view, event);
	else
	    gtk_widget_show(image_selection_view);
	mode = MODE_NONE;
	break;
	
    case GDK_KEY_Return:
	if (mode == MODE_NONE) {
	    if (gtk_widget_get_mapped(image_selection_view))
		image_selection_view_key_event(image_selection_view, event);
	} else
	    mode = MODE_NONE;
	break;
	
    case GDK_KEY_space:
	image_selection_view_key_event(image_selection_view, event);
	break;
	
    case GDK_KEY_BackSpace:
	image_selection_view_key_event(image_selection_view, event);
	break;
    }
    
    relayout();
    
    return TRUE;
}

gboolean miv_display(const gchar *path)
{
    GdkPixbuf *pb;
    GError *err = NULL;
    pb = gdk_pixbuf_new_from_file(path, &err);
    if (err != NULL)
	return FALSE;
    
    pixbuf = pb;
    relayout();
    return TRUE;
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
    
    if (filepath != NULL) {
	GError *err = NULL;
	pixbuf = gdk_pixbuf_new_from_file(filepath, &err);
	if (err != NULL) {
	    fprintf(stderr, "%s\n", err->message);
	    exit(1);
	}
	img = gtk_image_new_from_pixbuf(pixbuf);
	display_first = FALSE;
    } else {
	img = gtk_image_new_from_icon_name("image-missing", GTK_ICON_SIZE_LARGE_TOOLBAR);
	display_first = TRUE;
    }
    gtk_widget_show(img);
    miv_layout_set_image(MIV_LAYOUT(layout), img);
    
    image_selection_view = image_selection_view_create(dirname, display_first);
    if (image_selection_view != NULL)
	miv_layout_set_selection_view(MIV_LAYOUT(layout), image_selection_view);
    
    gtk_main();
    
    return 0;
}
