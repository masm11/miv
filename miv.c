#include <gtk/gtk.h>
#include <math.h>

enum {
    MODE_NONE,
    MODE_ROTATE,
    MODE_SCALE,
};

static GtkWidget *win, *layout = NULL;
static gboolean is_fullscreen = FALSE;
static GdkPixbuf *pixbuf;
static GtkWidget *img;
static int mode = 0;
static int rotate = 0;	// 0, 90, 180, or 270
static int scale = 0;
static int tx = 0, ty = 0;
static gboolean maximize = FALSE;

static void relayout(void);

static void layout_size_allocate(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
    relayout();
}

static void relayout(void)
{
    GdkPixbuf *pb = NULL, *pb_old;
    
    if (is_fullscreen) {
	if (layout == NULL) {
	    layout = gtk_layout_new(NULL, NULL);
	    g_signal_connect_after(G_OBJECT(layout), "size-allocate", G_CALLBACK(layout_size_allocate), NULL);
	    gtk_widget_show(layout);
	    g_object_ref(img);
	    gtk_container_remove(GTK_CONTAINER(win), img);
	    gtk_container_add(GTK_CONTAINER(win), layout);
	    gtk_layout_put(GTK_LAYOUT(layout), img, 0, 0);
	    g_object_unref(img);
	}
    } else {
	if (layout != NULL) {
	    g_object_ref(img);
	    gtk_container_remove(GTK_CONTAINER(layout), img);
	    gtk_container_remove(GTK_CONTAINER(win), layout);
	    layout = NULL;
	    gtk_container_add(GTK_CONTAINER(win), img);
	    g_object_unref(img);
	}
    }
    
    pb_old = gdk_pixbuf_copy(pixbuf);
    switch (rotate) {
    case 0:
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_NONE);
	break;
    case 90:
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_CLOCKWISE);
	break;
    case 180:
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
	break;
    case 270:
	pb = gdk_pixbuf_rotate_simple(pb_old, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
	break;
    }
    g_object_unref(pb_old);
    
    pb_old = pb;
    if (maximize && is_fullscreen) {
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
	pb = gdk_pixbuf_copy(pb_old);
    }
    g_object_unref(pb_old);
    
    pb_old = pb;
    {
	int width = gdk_pixbuf_get_width(pb_old);
	int height = gdk_pixbuf_get_height(pb_old);
	double ratio = pow(1.25, scale);
	width *= ratio;
	height *= ratio;
	pb = gdk_pixbuf_scale_simple(pb_old, width, height, GDK_INTERP_BILINEAR);
    }
    g_object_unref(pb_old);
    
    gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);
    
    if (is_fullscreen) {
	GtkAllocation alloc;
	int w = gdk_pixbuf_get_width(pb);
	int h = gdk_pixbuf_get_height(pb);
	gtk_widget_get_allocation(layout, &alloc);
	int lw = alloc.width, lh = alloc.height;	// size of layout widget.
	
	printf("----\n");
	printf("layout: %dx%d+%d+%d\n", alloc.width, alloc.height, alloc.x, alloc.y);
	gtk_widget_get_allocation(win, &alloc);
	printf("window: %dx%d+%d+%d\n", alloc.width, alloc.height, alloc.x, alloc.y);
	
	int x, y;	// where to place center of the image in layout widget.
	x = lw / 2;
	y = lh / 2;
	
	if (w > lw) {
	    x += tx;
	    if (x - w / 2 > 0)
		x = w / 2;
	    if (x + w / 2 < lw)
		x = lw - w / 2;
	}
	if (h > lh) {
	    y += ty;
	    if (y - h / 2 > 0)
		y = h / 2;
	    if (y + h / 2 < lh)
		y = lh - h / 2;
	}
	
	gtk_widget_get_allocation(win, &alloc);
	// gtk_widget_set_size_request(layout, alloc.width, alloc.height);
	printf("img: +%d+%d\n", x - w / 2, y - h / 2);
	gtk_layout_move(GTK_LAYOUT(layout), img, x - w / 2, y - h / 2);
    } else {
	GtkAllocation alloc;
	int w = gdk_pixbuf_get_width(pb);
	int h = gdk_pixbuf_get_height(pb);
	// gtk_widget_set_size_request(layout, w, h);
	// gtk_layout_move(GTK_LAYOUT(layout), img, 0, 0);
    }
    
    gtk_window_resize(GTK_WINDOW(win), 100, 100);
    
    g_object_unref(pb);
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
	if (is_fullscreen) {
	    gtk_window_unfullscreen(GTK_WINDOW(win));
	    is_fullscreen = FALSE;
	} else {
	    gtk_window_fullscreen(GTK_WINDOW(win));
	    is_fullscreen = TRUE;
	}
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
	    tx += 128;
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
	    tx -= 128;
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
	    ty += 128;
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
	    ty -= 128;
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
	tx = ty = 0;
	break;
	
    case GDK_KEY_Escape:
	mode = MODE_NONE;
	break;
    }
    
    relayout();
    
    return TRUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    if (argc < 2) {
	printf("no file specified.\n");
	exit(1);
    }
    
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 100, 100);
    gtk_widget_show(win);
    g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK(key_press_event), NULL);
    
    pixbuf = gdk_pixbuf_new_from_file(argv[1], NULL);
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    
    img = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(img);
    gtk_container_add(GTK_CONTAINER(win), img);
    
    gtk_main();
    
    return 0;
}
