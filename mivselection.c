#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include "mivselection.h"

gboolean miv_display(const gchar *path);

static GtkWidget *hbox = NULL;
static GtkWidget *curpath = NULL;

G_DEFINE_QUARK(miv-selection-fullpath, miv_selection_fullpath)

static void hover_one(GtkWidget *from, GtkWidget *to)
{
    printf("hover: %p -> %p\n", from, to);
    if (from != NULL)
	gtk_widget_unset_state_flags(from, GTK_STATE_FLAG_PRELIGHT);
    if (to != NULL)
	gtk_widget_set_state_flags(to, GTK_STATE_FLAG_PRELIGHT, FALSE);
}

static void select_one(GtkWidget *from, GtkWidget *to)
{
    printf("select: %p -> %p\n", from, to);
    if (from != NULL)
	gtk_widget_unset_state_flags(from, GTK_STATE_FLAG_SELECTED);
    if (to != NULL)
	gtk_widget_set_state_flags(to, GTK_STATE_FLAG_SELECTED, FALSE);
}

struct find_selected_t {
    GtkWidget *prev, *cur, *next;
    GtkWidget *sel;
};

static void find_selected_iter(GtkWidget *w, gpointer user_data)
{
    struct find_selected_t *sel = user_data;
    if (sel->cur == NULL) {
	if (!(gtk_widget_get_state_flags(w) & GTK_STATE_FLAG_PRELIGHT))
	    sel->prev = w;
	else
	    sel->cur = w;
    } else {
	if (sel->next == NULL)
	    sel->next = w;
    }
    if ((gtk_widget_get_state_flags(w) & GTK_STATE_FLAG_SELECTED))
	sel->sel = w;
}

static void find_selected(struct find_selected_t *sel)
{
    sel->prev = sel->cur = sel->next = sel->sel = NULL;
    gtk_container_foreach(GTK_CONTAINER(hbox), find_selected_iter, sel);
}

static void select_next(GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(&sel);
    if (sel.cur != NULL && sel.next != NULL)
	hover_one(sel.cur, sel.next);
}

static void select_prev(GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(&sel);
    if (sel.cur != NULL && sel.prev != NULL)
	hover_one(sel.cur, sel.prev);
}

static void display_next(GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(&sel);
    
    if (sel.cur != sel.sel) {
	gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.cur);
		return;
	    }
	}
    }
    
    if (sel.cur != NULL && sel.next != NULL) {
	hover_one(sel.cur, sel.next);
	
	gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.next), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.next);
		return;
	    }
	}
    }
}

static void display_prev(GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(&sel);
    
    if (sel.cur != sel.sel) {
	gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.cur);
		return;
	    }
	}
    }
    
    if (sel.cur != NULL && sel.prev != NULL) {
	hover_one(sel.cur, sel.prev);
	
	gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.prev), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.prev);
		return;
	    }
	}
    }
}

void image_selection_view_key_event(GtkWidget *widget, GdkEventKey *event)
{
    assert(GTK_IS_BOX(widget));
    
    switch (event->keyval) {
    case GDK_KEY_v:
    case GDK_KEY_V:
	gtk_widget_hide(widget);
	break;
    case GDK_KEY_Right:
	select_next(widget);
	break;
    case GDK_KEY_Left:
	select_prev(widget);
	break;
    case GDK_KEY_space:
	display_next(widget);
	break;
    case GDK_KEY_BackSpace:
	display_prev(widget);
	break;
    }
}

#define SIZE 100.0

static GtkWidget *create_image_selection_file(const char *dir, const char *name, gchar *fullpath)
{
    GError *err = NULL;
    GdkPixbuf *pb_old = gdk_pixbuf_new_from_file(fullpath, &err);
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
    g_object_set_qdata_full(G_OBJECT(vbox), miv_selection_fullpath_quark(), fullpath, (GDestroyNotify) g_free);
    
    GtkWidget *image = gtk_image_new_from_pixbuf(pb);
    gtk_widget_set_size_request(image, SIZE, SIZE);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    g_object_unref(pb_old);
    g_object_unref(pb);
    
    return vbox;
}

static GtkWidget *create_image_selection_dir(const char *dir, const char *name, gchar *fullpath)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_set_qdata_full(G_OBJECT(vbox), miv_selection_fullpath_quark(), fullpath, (GDestroyNotify) g_free);
    
    GtkWidget *image = gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_DND);
    gtk_widget_set_size_request(image, SIZE, SIZE);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    return vbox;
}

static GtkWidget *create_image_selection_item(const char *dir, const char *name)
{
    gchar *fullpath = g_strdup_printf("%s/%s", dir, name);
    GtkWidget *w;
    
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	w = create_image_selection_file(dir, name, fullpath);
    } else {
	w = create_image_selection_dir(dir, name, fullpath);
    }
    
    if (w == NULL)
	g_free(fullpath);
    
    return w;
}

#undef SIZE

struct add_item_t {
    const gchar *dirname;
    GtkBox *hbox;
    GtkWidget *first_item;
};

static void add_items(gpointer data, gpointer user_data)
{
    gchar *name = data;
    struct add_item_t *p = user_data;
    
    if (name[0] == '.' && strcmp(name, "..") != 0)
	return;
    
    GtkWidget *item = create_image_selection_item(p->dirname, name);
    if (item != NULL) {
	gtk_box_pack_start(p->hbox, item, FALSE, FALSE, 0);
	gtk_widget_show(item);
	if (p->first_item == NULL)
	    p->first_item = item;
    }
}

static int compare_names(gconstpointer a, gconstpointer b)
{
    const char * const *ap = a;
    const char * const *bp = b;
    return strcmp(*ap, *bp);
}

static void move_to_dir(const gchar *dirname)
{
    GPtrArray *ary = g_ptr_array_new_with_free_func(g_free);
    
    GError *err = NULL;
    GDir *dir = g_dir_open(dirname, 0, &err);
    if (err == NULL) {
	while (TRUE) {
	    const gchar *name = g_dir_read_name(dir);
	    if (name == NULL)
		break;
	    g_ptr_array_add(ary, g_strdup(name));
	}
	g_dir_close(dir);
	
	g_ptr_array_sort(ary, compare_names);
    }
    
    g_ptr_array_insert(ary, 0, g_strdup(".."));
    
    struct add_item_t param;
    param.dirname = dirname;
    param.hbox = GTK_BOX(hbox);
    param.first_item = NULL;
    g_ptr_array_foreach(ary, add_items, &param);
    
    g_ptr_array_free(ary, TRUE);
    
    if (param.first_item != NULL) {
	hover_one(NULL, param.first_item);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(param.first_item), miv_selection_fullpath_quark());
	gtk_label_set_text(GTK_LABEL(curpath), fullpath);
    }
}

GtkWidget *image_selection_view_create(const gchar *dirname)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(vbox, "image-selection");
    gtk_widget_show(vbox);
    
    GtkWidget *padding = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), padding, TRUE, TRUE, 0);
    gtk_widget_show(padding);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    
    curpath = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(curpath), 0);
    gtk_box_pack_start(GTK_BOX(vbox), curpath, FALSE, TRUE, 0);
    gtk_widget_show(curpath);
    
    move_to_dir(dirname);
    
    return vbox;
}
