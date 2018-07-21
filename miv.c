#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include "mivlayout.h"

enum {
    MODE_NONE,
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

static GtkCssProvider *css_provider;
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
	"box#image-selection {"
	" background-color: rgba(1,1,1,0.7);"
	"}"
	"box#image-selection box {"
	" margin: 5px;"
	" padding: 5px;"
	" background-color: rgba(0,0,0,0.7);"
	"}"
	"box#image-selection label {"
	" font-size: x-small;"
	" color: #ffffff;"
	" margin-top: 5px;"
	" margin-left: 5px;"
	"}"
;

static void relayout(void);

static void add_css_provider(GtkWidget *w)
{
    GtkStyleContext *style_context;
    style_context = gtk_widget_get_style_context(w);
    gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

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
	    gtk_window_fullscreen(GTK_WINDOW(win));
	    miv_layout_set_fullscreen_mode(MIV_LAYOUT(layout), TRUE);
	    is_fullscreened = TRUE;
	}
    } else {
	if (is_fullscreened) {
	    gtk_window_unfullscreen(GTK_WINDOW(win));
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
	double scale = fmin(wscale, hscale);
	pb = gdk_pixbuf_scale_simple(pb_old, w * scale, h * scale, GDK_INTERP_BILINEAR);
    } else {
	set_status_string(STATUS_MAXIMIZE, NULL);
	pb = gdk_pixbuf_copy(pb_old);
    }
    g_object_unref(pb_old);
    
    pb_old = pb;
    if (scale != 0) {
	set_status_string(STATUS_SCALE, g_strdup_printf("scale %+d", scale));
	int width = gdk_pixbuf_get_width(pb_old);
	int height = gdk_pixbuf_get_height(pb_old);
	double ratio = pow(1.25, scale);
	width *= ratio;
	height *= ratio;
	pb = gdk_pixbuf_scale_simple(pb_old, width, height, GDK_INTERP_BILINEAR);
    } else {
	set_status_string(STATUS_SCALE, NULL);
	pb = gdk_pixbuf_copy(pb_old);
    }
    g_object_unref(pb_old);
    
    gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);
    
    if (is_fullscreen) {
	set_status_string(STATUS_FULLSCREEN, g_strdup("fullscreen"));
	
	gtk_widget_set_size_request(layout, -1, -1);
    } else {
	set_status_string(STATUS_FULLSCREEN, NULL);
	
	int w = gdk_pixbuf_get_width(pb);
	int h = gdk_pixbuf_get_height(pb);
	gtk_widget_set_size_request(layout, w, h);
	set_status_string(STATUS_FULLSCREEN, NULL);
    }
    
    gtk_window_resize(GTK_WINDOW(win), 100, 100);
    
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
	}
	
#if 0
	/* In order to paint labels at the last,
	 * remove and re-put labelbox.
	 */
	g_object_ref(labelbox);
	gtk_container_remove(GTK_CONTAINER(layout), labelbox);
	gtk_layout_put(GTK_LAYOUT(layout), labelbox, 0, 0);
	g_object_unref(labelbox);
#endif
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
    printf("key_press:\n");
    
    switch (event->keyval) {
    case GDK_KEY_Q:
    case GDK_KEY_q:
	gtk_main_quit();
	break;
	
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
	
    case GDK_KEY_Left:
	switch (mode) {
	case MODE_NONE:
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
	    miv_layout_translate_image(MIV_LAYOUT(layout), 0, 128);
	    break;
	case MODE_SCALE:
	    if ((scale -= 1) <= -5)
		scale = -5;
	    break;
	}
	break;
	
    case GDK_KEY_Down:
	switch (mode) {
	case MODE_NONE:
	    miv_layout_translate_image(MIV_LAYOUT(layout), 0, -128);
	    break;
	case MODE_SCALE:
	    if ((scale += 1) >= 5)
		scale = 5;
	    break;
	}
	break;
	
    case GDK_KEY_1:
	scale = 0;
	break;
	
    case GDK_KEY_0:
	miv_layout_reset_translation(MIV_LAYOUT(layout));
	break;
	
    case GDK_KEY_Escape:
	mode = MODE_NONE;
	break;
    }
    
    relayout();
    
    return TRUE;
}

#define SIZE 100.0
static GtkWidget *create_image_selection_item(const char *dir, const char *name)
{
    gchar *path = g_strdup_printf("%s/%s", dir, name);
    GError *err = NULL;
    GdkPixbuf *pb_old = gdk_pixbuf_new_from_file(path, &err);
    if (err != NULL)
	return NULL;
    int orig_w = gdk_pixbuf_get_width(pb_old);
    int orig_h = gdk_pixbuf_get_height(pb_old);
    double scalew = SIZE / orig_w;
    double scaleh = SIZE / orig_h;
    double scale = fmin(scalew, scaleh);
    int w = orig_w * scale;
    int h = orig_h * scale;
    GdkPixbuf *pb = gdk_pixbuf_scale_simple(pb_old, w, h, GDK_INTERP_NEAREST);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_css_provider(vbox);
    g_object_set_data_full(G_OBJECT(vbox), "fullpath", path, (GDestroyNotify) g_free);
    
    GtkWidget *image = gtk_image_new_from_pixbuf(pb);
    add_css_provider(image);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_widget_set_size_request(image, 100, 100);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    g_object_unref(pb_old);
    g_object_unref(pb);
    
    return vbox;
}
#undef SIZE

static GtkWidget *create_image_selection_view(const gchar *dirname)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(vbox, "image-selection");
    add_css_provider(vbox);
    gtk_widget_show(vbox);
    
    GtkWidget *padding = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), padding, TRUE, TRUE, 0);
    gtk_widget_show(padding);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    
#if 0
    {
	GtkWidget *item = create_image_selection_item(dirname, "..");
	if (item != NULL) {
	    gtk_box_pack_start(GTK_BOX(hbox), item, FALSE, FALSE, 0);
	    gtk_widget_show(item);
	}
    }
#endif
    
    GError *err = NULL;
    GDir *dir = g_dir_open(dirname, 0, &err);
    if (err != NULL) {
	gtk_widget_destroy(vbox);
	return NULL;
    }
    while (TRUE) {
	const gchar *name = g_dir_read_name(dir);
	if (name == NULL)
	    break;
	
	GtkWidget *item = create_image_selection_item(dirname, name);
	if (item != NULL) {
	    gtk_box_pack_start(GTK_BOX(hbox), item, FALSE, FALSE, 0);
	    gtk_widget_show(item);
	}
    }
    g_dir_close(dir);
    
    GtkWidget *curpath = gtk_label_new("foobar.txt");
    add_css_provider(curpath);
    gtk_label_set_xalign(GTK_LABEL(curpath), 0);
    gtk_box_pack_start(GTK_BOX(vbox), curpath, FALSE, TRUE, 0);
    gtk_widget_show(curpath);
    
    return vbox;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    if (argc < 2) {
	printf("no file specified.\n");
	exit(1);
    }
    
    css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider, css_text, -1, NULL);
    
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 100, 100);
    gtk_widget_show(win);
    g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK(key_press_event), NULL);
    
    layout = miv_layout_new();
    g_signal_connect(G_OBJECT(layout), "translation-changed", G_CALLBACK(layout_translation_changed), NULL);
    gtk_widget_show(layout);
    gtk_container_add(GTK_CONTAINER(win), layout);
    
    labelbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(labelbox, "labelbox");
    gtk_widget_show(labelbox);
    miv_layout_set_labels(MIV_LAYOUT(layout), labelbox);
    
    status = gtk_label_new("");
    add_css_provider(status);
    gtk_widget_show(status);
    gtk_box_pack_start(GTK_BOX(labelbox), status, FALSE, FALSE, 0);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(labelbox), hbox, FALSE, FALSE, 0);
    
    mode_label = gtk_label_new("");
    add_css_provider(mode_label);
    gtk_widget_show(mode_label);
    gtk_box_pack_start(GTK_BOX(hbox), mode_label, FALSE, FALSE, 0);
    
    pixbuf = gdk_pixbuf_new_from_file(argv[1], NULL);
    
    img = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(img);
    miv_layout_set_image(MIV_LAYOUT(layout), img);
    
    image_selection_view = create_image_selection_view("/home/masm");
    if (image_selection_view != NULL)
	miv_layout_set_selection_view(MIV_LAYOUT(layout), image_selection_view);
    
    gtk_main();
    
    return 0;
}
