#include <gtk/gtk.h>
#include <math.h>
#include "mivselection.h"

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
    g_object_set_data_full(G_OBJECT(vbox), "fullpath", path, (GDestroyNotify) g_free);
    
    GtkWidget *image = gtk_image_new_from_pixbuf(pb);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    g_object_unref(pb_old);
    g_object_unref(pb);
    
    return vbox;
}
#undef SIZE

GtkWidget *image_selection_view_create(const gchar *dirname)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(vbox, "image-selection");
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
    gtk_label_set_xalign(GTK_LABEL(curpath), 0);
    gtk_box_pack_start(GTK_BOX(vbox), curpath, FALSE, TRUE, 0);
    gtk_widget_show(curpath);
    
    return vbox;
}
