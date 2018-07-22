#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "mivselection.h"
#include "thumbnail_creator.h"

gboolean miv_display(const gchar *path);

static void move_to_dir(const gchar *path, gboolean display_first);

static GtkWidget *viewport = NULL;
static GtkWidget *hbox = NULL;
static GtkWidget *curpath = NULL;

static GIOChannel *ioch;

G_DEFINE_QUARK(miv-selection-fullpath, miv_selection_fullpath)
G_DEFINE_QUARK(miv-selection-image, miv_selection_image)
G_DEFINE_QUARK(miv-selection-job, miv_selection_job)

static void hover_one(GtkWidget *from, GtkWidget *to, int pos)
{
    if (from != NULL)
	gtk_widget_unset_state_flags(from, GTK_STATE_FLAG_PRELIGHT);
    if (to != NULL) {
	gtk_widget_set_state_flags(to, GTK_STATE_FLAG_PRELIGHT, FALSE);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(to), miv_selection_fullpath_quark());
	gtk_label_set_text(GTK_LABEL(curpath), fullpath);
	
	GtkAllocation alloc;
	gtk_widget_get_allocation(to, &alloc);
	double x1 = alloc.x, x2 = alloc.x + alloc.width;
	
	GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewport));
	double val = gtk_adjustment_get_value(adj);
	double psize = gtk_adjustment_get_page_size(adj);
	if (x1 < val)
	    val = x1;
	if (x2 >= val + psize)
	    val = x2 - psize;
	gtk_adjustment_set_value(adj, val);
    }
}

static void select_one(GtkWidget *from, GtkWidget *to)
{
    if (from != NULL)
	gtk_widget_unset_state_flags(from, GTK_STATE_FLAG_SELECTED);
    if (to != NULL)
	gtk_widget_set_state_flags(to, GTK_STATE_FLAG_SELECTED, FALSE);
}

struct find_selected_t {
    GtkWidget *prev, *cur, *next;
    GtkWidget *sel;
    int prev_pos, cur_pos, next_pos;
    int sel_pos;
    
    int pos;
};

static void find_selected_iter(GtkWidget *w, gpointer user_data)
{
    struct find_selected_t *sel = user_data;
    
    if (sel->cur == NULL) {
	if (!(gtk_widget_get_state_flags(w) & GTK_STATE_FLAG_PRELIGHT)) {
	    sel->prev = w;
	    sel->prev_pos = sel->pos;
	} else {
	    sel->cur = w;
	    sel->cur_pos = sel->pos;
	}
    } else {
	if (sel->next == NULL) {
	    sel->next = w;
	    sel->next_pos = sel->pos;
	}
    }
    
    if ((gtk_widget_get_state_flags(w) & GTK_STATE_FLAG_SELECTED)) {
	sel->sel = w;
	sel->sel_pos = sel->pos;
    }
    
    sel->pos++;
}

static void find_selected(struct find_selected_t *sel)
{
    sel->prev = sel->cur = sel->next = sel->sel = NULL;
    sel->prev_pos = sel->cur_pos = sel->next_pos = sel->sel_pos = -1;
    sel->pos = 0;
    gtk_container_foreach(GTK_CONTAINER(hbox), find_selected_iter, sel);
}

static void select_next(GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(&sel);
    if (sel.cur != NULL && sel.next != NULL)
	hover_one(sel.cur, sel.next, sel.next_pos);
}

static void select_prev(GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(&sel);
    if (sel.cur != NULL && sel.prev != NULL)
	hover_one(sel.cur, sel.prev, sel.prev_pos);
}

static void display_next(GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(&sel);
    
    if (sel.cur != sel.sel) {
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.cur);
		return;
	    }
	}
    }
    
    if (sel.cur != NULL && sel.next != NULL) {
	hover_one(sel.cur, sel.next, sel.next_pos);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.next), miv_selection_fullpath_quark());
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
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.cur);
		return;
	    }
	}
    }
    
    if (sel.cur != NULL && sel.prev != NULL) {
	hover_one(sel.cur, sel.prev, sel.prev_pos);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.prev), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath)) {
		select_one(sel.sel, sel.prev);
		return;
	    }
	}
    }
}

static void enter_it(GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(&sel);
    
    if (sel.cur != NULL) {
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath))
		select_one(sel.sel, sel.cur);
	} else
	    move_to_dir(fullpath, FALSE);
    }
}

#if 0
void image_selection_view_display_next(GtkWidget *widget)
{
    display_next(widget);
}

void image_selection_view_display_prev(GtkWidget *widget)
{
    display_prev(widget);
}
#endif

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
    case GDK_KEY_Return:
	enter_it(widget);
	break;
    }
}

#define SIZE 100.0

static GtkWidget *create_image_selection_file(const char *dir, const char *name, gchar *fullpath)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_set_qdata_full(G_OBJECT(vbox), miv_selection_fullpath_quark(), fullpath, (GDestroyNotify) g_free);
    
    GtkWidget *image = gtk_image_new_from_icon_name("image-loading", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_widget_set_size_request(image, SIZE, SIZE);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    g_object_set_qdata(G_OBJECT(vbox), miv_selection_image_quark(), image);
    
    struct thumbnail_creator_job_t *job = thumbnail_creator_job_new(fullpath, vbox);
    g_object_set_qdata_full(G_OBJECT(vbox), miv_selection_job_quark(), job, (GDestroyNotify) thumbnail_creator_job_free);
    thumbnail_creator_put(job);
    
    return vbox;
}

static void replace_image_selection_file(struct thumbnail_creator_job_t *job)
{
    GtkWidget *vbox = job->vbox;
    GdkPixbuf *pixbuf = job->pixbuf;
    
    GtkWidget *img = g_object_get_qdata(G_OBJECT(vbox), miv_selection_image_quark());
    gtk_widget_destroy(img);
    
    if (pixbuf != NULL)
	img = gtk_image_new_from_pixbuf(pixbuf);
    else
	img = gtk_image_new_from_icon_name("image-loading", GTK_ICON_SIZE_LARGE_TOOLBAR);
    
    gtk_widget_set_size_request(img, SIZE, SIZE);
    gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(img, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), img, TRUE, TRUE, 0);
    gtk_widget_show(img);
    
    g_object_set_qdata(G_OBJECT(vbox), miv_selection_image_quark(), img);
    g_object_set_qdata(G_OBJECT(vbox), miv_selection_job_quark(), NULL);
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

static gboolean item_draw(GtkWidget *w, cairo_t *cr, gpointer user_data)
{
    struct thumbnail_creator_job_t *job = g_object_get_qdata(G_OBJECT(w), miv_selection_job_quark());
    if (job != NULL)
	thumbnail_creator_prioritize(job);
    
    return FALSE;
}

static GtkWidget *create_image_selection_item(const char *dir, const char *name, gboolean *isimage)
{
    gchar *fullpath = g_strdup_printf("%s/%s", dir, name);
    GtkWidget *w;
    
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	w = create_image_selection_file(dir, name, fullpath);
	*isimage = TRUE;
    } else {
	w = create_image_selection_dir(dir, name, fullpath);
	*isimage = FALSE;
    }
    
    if (w != NULL)
	g_signal_connect(G_OBJECT(w), "draw", G_CALLBACK(item_draw), NULL);
    else
	g_free(fullpath);
    
    return w;
}

#undef SIZE

static void viewport_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer user_data)
{
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewport));
    gtk_adjustment_set_page_size(adj, alloc->width);
}

static void hbox_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer user_data)
{
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(viewport));
    gtk_adjustment_set_upper(adj, alloc->width);
}

struct add_item_t {
    const gchar *dirname;
    GtkBox *hbox;
    GtkWidget *first_item, *first_image;
    int nr;
};

static void add_items(gpointer data, gpointer user_data)
{
    gchar *name = data;
    struct add_item_t *p = user_data;
    
    if (name[0] == '.' && strcmp(name, "..") != 0)
	return;
    
    gboolean isimage;
    GtkWidget *item = create_image_selection_item(p->dirname, name, &isimage);
    if (item != NULL) {
	gtk_box_pack_start(p->hbox, item, FALSE, FALSE, 0);
	gtk_widget_show(item);
	if (p->first_item == NULL)
	    p->first_item = item;
	if (isimage && p->first_image == NULL)
	    p->first_image = item;
	p->nr++;
    }
}

static int compare_names(gconstpointer a, gconstpointer b)
{
    const char * const *ap = a;
    const char * const *bp = b;
    return strcmp(*ap, *bp);
}

static void move_to_dir(const gchar *path, gboolean display_first)
{
    gchar *dirname = g_strdup(path);
    
    {
	char buf[1024];
	int fd = open(".", O_DIRECTORY | O_RDONLY);
	if (fd != -1) {
	    if (chdir(path) != -1) {
		if (getcwd(buf, sizeof buf) != NULL) {
		    g_free(dirname);
		    dirname = g_strdup(buf);
		}
		fchdir(fd);
	    }
	    close(fd);
	}
    }
    
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
    
    GList *lp = thumbnail_creator_cancel();
    while (lp != NULL) {
	struct thumbnail_creator_job_t *job = lp->data;
	lp = g_list_remove(lp, job);
    }
    
    gtk_container_foreach(GTK_CONTAINER(hbox), (GtkCallback) gtk_widget_destroy, NULL);
    
    struct add_item_t param;
    param.dirname = dirname;
    param.hbox = GTK_BOX(hbox);
    param.first_item = param.first_image = NULL;
    param.nr = 0;
    g_ptr_array_foreach(ary, add_items, &param);
    
    g_ptr_array_free(ary, TRUE);
    
    if (param.first_item != NULL) {
	hover_one(NULL, param.first_item, 0);
	
	if (display_first && param.first_image != NULL) {
	    hover_one(param.first_item, param.first_image, 0);
	    
	    const gchar *fullpath = g_object_get_qdata(G_OBJECT(param.first_image), miv_selection_fullpath_quark());
	    if (miv_display(fullpath))
		select_one(NULL, param.first_image);
	}
    }
}

static gboolean thumbnail_creator_done(GIOChannel *ch, GIOCondition cond, gpointer data)
{
    char buf[1];
    gsize r;
    
    g_io_channel_read_chars(ch, buf, sizeof buf, &r, NULL);
    
    GList *done = thumbnail_creator_get();
    while (done != NULL) {
	struct thumbnail_creator_job_t *job = done->data;
	done = g_list_remove(done, job);
	replace_image_selection_file(job);
    }
    
    return TRUE;
}

GtkWidget *image_selection_view_create(const gchar *dirname, gboolean display_first)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(vbox, "image-selection");
    gtk_widget_show(vbox);
    
    GtkWidget *padding = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), padding, TRUE, TRUE, 0);
    gtk_widget_show(padding);
    
    viewport = gtk_scrolled_window_new(NULL, NULL);
    g_signal_connect(G_OBJECT(viewport), "size-allocate", G_CALLBACK(viewport_size_allocate), NULL);
    gtk_box_pack_end(GTK_BOX(vbox), viewport, FALSE, FALSE, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewport), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_show(viewport);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_signal_connect(G_OBJECT(hbox), "size-allocate", G_CALLBACK(hbox_size_allocate), NULL);
    gtk_container_add(GTK_CONTAINER(viewport), hbox);
    gtk_widget_show(hbox);
    
    curpath = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(curpath), 0);
    gtk_box_pack_start(GTK_BOX(vbox), curpath, FALSE, TRUE, 0);
    gtk_widget_show(curpath);
    
    ioch = thumbnail_creator_init();
    g_io_add_watch(ioch, G_IO_IN, thumbnail_creator_done, NULL);
    
    move_to_dir(dirname, display_first);
    
    return vbox;
}
