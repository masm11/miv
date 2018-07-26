#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "mivselection.h"
#include "thumbnail_creator.h"
#include "miv.h"
#include "mivjobqueue.h"

static void move_to_dir(struct miv_selection_t *sw, const gchar *path, gboolean display_first);

struct miv_selection_t {
    GtkWidget *vbox;
    GtkWidget *viewport;
    GtkWidget *hbox;
    GtkWidget *curpath;
    
    GIOChannel *ioch;
    
    MivJobQueue *queue;
    MivJobQueue *replace_queue;
    
    struct add_item_t {
	gchar *dirname;
	GtkWidget *first_item;
    } add_items_w;
};

G_DEFINE_QUARK(miv-selection-fullpath, miv_selection_fullpath)
G_DEFINE_QUARK(miv-selection-image, miv_selection_image)
G_DEFINE_QUARK(miv-selection-vbox, miv_selection_vbox)
G_DEFINE_QUARK(miv-selection-evbox, miv_selection_evbox)
G_DEFINE_QUARK(miv-selection-gesture, miv_selection_gesture)
G_DEFINE_QUARK(miv-selection-job, miv_selection_job)

static void hover_one(struct miv_selection_t *sw, GtkWidget *from, GtkWidget *to, int pos)
{
    if (from != NULL)
	gtk_widget_unset_state_flags(from, GTK_STATE_FLAG_PRELIGHT);
    if (to != NULL) {
	gtk_widget_set_state_flags(to, GTK_STATE_FLAG_PRELIGHT, FALSE);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(to), miv_selection_fullpath_quark());
	gtk_label_set_text(GTK_LABEL(sw->curpath), fullpath);
	
	GtkAllocation alloc;
	gtk_widget_get_allocation(to, &alloc);
	double x1 = alloc.x, x2 = alloc.x + alloc.width;
	
	GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw->viewport));
	double val = gtk_adjustment_get_value(adj);
	double psize = gtk_adjustment_get_page_size(adj);
	if (x1 < val)
	    val = x1;
	if (x2 >= val + psize)
	    val = x2 - psize;
	gtk_adjustment_set_value(adj, val);
    }
}

static void select_one(struct miv_selection_t *sw, GtkWidget *from, GtkWidget *to)
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

static void find_selected(struct miv_selection_t *sw, struct find_selected_t *sel)
{
    sel->prev = sel->cur = sel->next = sel->sel = NULL;
    sel->prev_pos = sel->cur_pos = sel->next_pos = sel->sel_pos = -1;
    sel->pos = 0;
    gtk_container_foreach(GTK_CONTAINER(sw->hbox), find_selected_iter, sel);
}

static void select_next(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.next != NULL)
	hover_one(sw, sel.cur, sel.next, sel.next_pos);
}

static void select_prev(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.prev != NULL)
	hover_one(sw, sel.cur, sel.prev, sel.prev_pos);
}

static void display_next(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(sw, &sel);
    
    if (sel.cur != sel.sel) {
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath, NULL)) {
		select_one(sw, sel.sel, sel.cur);
		return;
	    }
	}
    }
    
    if (sel.cur != NULL && sel.next != NULL) {
	hover_one(sw, sel.cur, sel.next, sel.next_pos);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.next), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath, NULL)) {
		select_one(sw, sel.sel, sel.next);
		return;
	    }
	}
    }
}

static void display_prev(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(sw, &sel);
    
    if (sel.cur != sel.sel) {
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath, NULL)) {
		select_one(sw, sel.sel, sel.cur);
		return;
	    }
	}
    }
    
    if (sel.cur != NULL && sel.prev != NULL) {
	hover_one(sw, sel.cur, sel.prev, sel.prev_pos);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.prev), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath, NULL)) {
		select_one(sw, sel.sel, sel.prev);
		return;
	    }
	}
    }
}

static void enter_it(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    
    find_selected(sw, &sel);
    
    if (sel.cur != NULL) {
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath, NULL))
		select_one(sw, sel.sel, sel.cur);
	} else
	    move_to_dir(sw, fullpath, FALSE);
    }
}

void image_selection_view_key_event(GtkWidget *widget, GdkEventKey *event, struct miv_selection_t *sw)
{
    assert(GTK_IS_BOX(widget));
    
    switch (event->keyval) {
    case GDK_KEY_v:
    case GDK_KEY_V:
	gtk_widget_hide(widget);
	break;
    case GDK_KEY_Right:
	if (event->state & GDK_SHIFT_MASK)
	    display_next(sw, widget);
	else
	    select_next(sw, widget);
	break;
    case GDK_KEY_Left:
	if (event->state & GDK_SHIFT_MASK)
	    display_prev(sw, widget);
	else
	    select_prev(sw, widget);
	break;
    case GDK_KEY_space:
	display_next(sw, widget);
	break;
    case GDK_KEY_BackSpace:
	display_prev(sw, widget);
	break;
    case GDK_KEY_Return:
	enter_it(sw, widget);
	break;
    }
}

#define SIZE 100.0

static GtkWidget *create_image_selection_file(const char *dir, const char *name, gchar *fullpath)
{
    // box: for margin/padding. eventbox doesn't supports event selection.
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *style_context = gtk_widget_get_style_context(outer);
    gtk_style_context_add_class(style_context, "item");
    
    // eventbox: for event selection. box doesn't supports margin/padding.
    GtkWidget *evbox = gtk_event_box_new();
    g_object_set_qdata_full(G_OBJECT(outer), miv_selection_fullpath_quark(), fullpath, (GDestroyNotify) g_free);
    gtk_box_pack_start(GTK_BOX(outer), evbox, TRUE, TRUE, 0);
    gtk_widget_show(evbox);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(evbox), vbox);
    gtk_widget_show(vbox);
    
    GtkWidget *image = gtk_image_new_from_icon_name("image-loading", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_widget_set_size_request(image, SIZE, SIZE);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    g_object_set_qdata(G_OBJECT(outer), miv_selection_evbox_quark(), evbox);
    g_object_set_qdata(G_OBJECT(outer), miv_selection_vbox_quark(), vbox);
    g_object_set_qdata(G_OBJECT(outer), miv_selection_image_quark(), image);
    
    struct thumbnail_creator_job_t *job = thumbnail_creator_job_new(fullpath, outer);
    g_object_set_qdata_full(G_OBJECT(outer), miv_selection_job_quark(), job, NULL);
    thumbnail_creator_put_job(job);
    
    return outer;
}

static void replace_image_selection_file(gpointer data, gpointer user_data)
{
    struct thumbnail_creator_job_t *job = data;
    GtkWidget *evbox = job->vbox;
    GdkPixbuf *pixbuf = job->pixbuf;
    
    GtkWidget *vbox = g_object_get_qdata(G_OBJECT(evbox), miv_selection_vbox_quark());
    assert(GTK_IS_BOX(vbox));
    GtkWidget *img = g_object_get_qdata(G_OBJECT(evbox), miv_selection_image_quark());
    assert(GTK_IS_IMAGE(img));
    gtk_widget_destroy(img);
    
    if (pixbuf != NULL)
	img = gtk_image_new_from_pixbuf(pixbuf);
    else
	img = gtk_image_new_from_icon_name("image-loading", GTK_ICON_SIZE_LARGE_TOOLBAR);
    assert(GTK_IS_IMAGE(img));
    
    gtk_widget_set_size_request(img, SIZE, SIZE);
    gtk_widget_set_halign(img, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(img, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), img, TRUE, TRUE, 0);
    gtk_widget_show(img);
    
    g_object_set_qdata(G_OBJECT(evbox), miv_selection_image_quark(), img);
    g_object_set_qdata(G_OBJECT(evbox), miv_selection_job_quark(), NULL);
}

static GtkWidget *create_image_selection_dir(const char *dir, const char *name, gchar *fullpath)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *style_context = gtk_widget_get_style_context(outer);
    gtk_style_context_add_class(style_context, "item");
    
    GtkWidget *evbox = gtk_event_box_new();
    g_object_set_qdata_full(G_OBJECT(outer), miv_selection_fullpath_quark(), fullpath, (GDestroyNotify) g_free);
    gtk_box_pack_start(GTK_BOX(outer), evbox, TRUE, TRUE, 0);
    gtk_widget_show(evbox);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(evbox), vbox);
    gtk_widget_show(vbox);
    
    GtkWidget *image = gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_DND);
    gtk_widget_set_size_request(image, SIZE, SIZE);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);
    gtk_widget_show(image);
    
    g_object_set_qdata(G_OBJECT(outer), miv_selection_evbox_quark(), evbox);
    g_object_set_qdata(G_OBJECT(outer), miv_selection_vbox_quark(), vbox);
    
    return outer;
}

static gboolean item_draw(GtkWidget *w, cairo_t *cr, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    struct thumbnail_creator_job_t *job = g_object_get_qdata(G_OBJECT(w), miv_selection_job_quark());
    if (job != NULL) {
	GtkAllocation alloc;
	gtk_widget_get_allocation(w, &alloc);
	
	GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw->viewport));
	double beg = gtk_adjustment_get_value(adj);
	double end = beg + gtk_adjustment_get_page_size(adj);
	
	if (!(alloc.x + alloc.width < beg || alloc.x >= end))
	    thumbnail_creator_prioritize(job);
    }
    
    return FALSE;
}

static void item_pressed(GtkGestureMultiPress *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    
    GtkWidget *item = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    while (item != NULL) {
	GtkStyleContext *style_context = gtk_widget_get_style_context(item);
	if (gtk_style_context_has_class(style_context, "item"))
	    break;
	item = gtk_widget_get_parent(item);
    }
    assert(item != NULL);
    
    struct find_selected_t sel;
    find_selected(sw, &sel);
    hover_one(sw, sel.cur, item, 0);
    const gchar *fullpath = g_object_get_qdata(G_OBJECT(item), miv_selection_fullpath_quark());
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	if (miv_display(fullpath, NULL))
	    select_one(sw, sel.sel, item);
    } else
	move_to_dir(sw, fullpath, FALSE);
}

static gboolean item_enter_notify_event(GtkWidget *w, GdkEvent *ev, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    
    GtkWidget *item = w;
    while (item != NULL) {
	GtkStyleContext *style_context = gtk_widget_get_style_context(item);
	if (gtk_style_context_has_class(style_context, "item"))
	    break;
	item = gtk_widget_get_parent(item);
    }
    assert(item != NULL);
    
    struct find_selected_t sel;
    find_selected(sw, &sel);
    hover_one(sw, sel.cur, item, 0);
    
    return TRUE;
}

static GtkWidget *create_image_selection_item(struct miv_selection_t *sw, const char *dir, const char *name, gboolean *isimage)
{
    gchar *fullpath;
    GtkWidget *w;
    
    if (strcmp(dir, "/") != 0)
	fullpath = g_strdup_printf("%s/%s", dir, name);
    else
	fullpath = g_strdup_printf("/%s", name);
    
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	w = create_image_selection_file(dir, name, fullpath);
	*isimage = TRUE;
    } else {
	w = create_image_selection_dir(dir, name, fullpath);
	*isimage = FALSE;
    }
    
    if (w != NULL) {
	g_signal_connect(G_OBJECT(w), "draw", G_CALLBACK(item_draw), sw);
	
	GtkWidget *evbox = g_object_get_qdata(G_OBJECT(w), miv_selection_evbox_quark());
	GtkGesture *gesture = gtk_gesture_multi_press_new(evbox);
	gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(gesture), GTK_PHASE_TARGET);
	g_signal_connect(G_OBJECT(gesture), "pressed", G_CALLBACK(item_pressed), sw);
	g_object_set_qdata_full(G_OBJECT(w), miv_selection_gesture_quark(), gesture, g_object_unref);  // to unref gesture on widget destruction.
	g_signal_connect(G_OBJECT(evbox), "enter-notify-event", G_CALLBACK(item_enter_notify_event), sw);
    } else
	g_free(fullpath);
    
    return w;
}

#undef SIZE

static void viewport_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw->viewport));
    gtk_adjustment_set_page_size(adj, alloc->width);
}

static void hbox_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw->viewport));
    gtk_adjustment_set_upper(adj, alloc->width);
}

static void add_items_iter(gpointer data, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    const gchar *name = data;
    struct add_item_t *p = &sw->add_items_w;
    
    if (name[0] == '.' && strcmp(name, "..") != 0)
	return;
    
    gboolean isimage;
    GtkWidget *item = create_image_selection_item(sw, p->dirname, name, &isimage);
    if (item != NULL) {
	gtk_box_pack_start(GTK_BOX(sw->hbox), item, FALSE, FALSE, 0);
	gtk_widget_show(item);
	if (p->first_item == NULL) {
	    p->first_item = item;
	    hover_one(sw, NULL, item, 0);
	}
    }
}

static int compare_names(gconstpointer a, gconstpointer b)
{
    const char * const *ap = a;
    const char * const *bp = b;
    return strcmp(*ap, *bp);
}

static gchar *getcanonpath(const gchar *path)
{
    gchar *p = NULL;
    char buf[1024];
    int fd = open(".", O_DIRECTORY | O_RDONLY);
    if (fd != -1) {
	if (chdir(path) != -1) {
	    if (getcwd(buf, sizeof buf) != NULL)
		p = g_strdup(buf);
	    else
		perror("getcwd");
	    fchdir(fd);
	} else
	    perror(path);
	close(fd);
    } else
	perror(".");
    return p;
}

static void move_to_dir(struct miv_selection_t *sw, const gchar *path, gboolean display_first)
{
    gchar *dirname = getcanonpath(path);
    if (dirname == NULL)
	dirname = g_strdup(path);
    
    GPtrArray *ary = g_ptr_array_new();
    
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
    
    
    miv_job_queue_cancel_all(sw->queue);
    miv_job_queue_cancel_all(sw->replace_queue);
    
    GList *lp = thumbnail_creator_cancel();
    while (lp != NULL) {
	struct thumbnail_creator_job_t *job = lp->data;
	lp = g_list_remove(lp, job);
    }
    
    printf("destroying items.\n");
    gtk_container_foreach(GTK_CONTAINER(sw->hbox), (GtkCallback) gtk_widget_destroy, NULL);
    
    
    printf("adding items.\n");
    
    sw->add_items_w.dirname = dirname;
    sw->add_items_w.first_item = NULL;
    for (int i = 0; i < ary->len; i++)
	miv_job_queue_enqueue(sw->queue, g_ptr_array_index(ary, i), sw, g_free);
    
    g_ptr_array_free(ary, TRUE);
    
    printf("done.\n");
}

static gboolean thumbnail_creator_done(GIOChannel *ch, GIOCondition cond, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    char buf[1];
    gsize r;
    
    g_io_channel_read_chars(ch, buf, sizeof buf, &r, NULL);
    
    GList *done = thumbnail_creator_get_done();
    while (done != NULL) {
	struct thumbnail_creator_job_t *job = done->data;
	done = g_list_remove(done, job);
	miv_job_queue_enqueue(sw->replace_queue, job, sw, (GDestroyNotify) thumbnail_creator_job_free);
    }
    
    return TRUE;
}

struct miv_selection_t *miv_selection_create(const gchar *dirname, gboolean display_first)
{
    struct miv_selection_t *sw = g_new0(struct miv_selection_t, 1);
    
    sw->queue = miv_job_queue_new(G_PRIORITY_DEFAULT_IDLE, add_items_iter);
    sw->replace_queue = miv_job_queue_new(G_PRIORITY_DEFAULT_IDLE + 20, replace_image_selection_file);
    
    sw->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(sw->vbox, "image-selection");
    
    GtkWidget *padding = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(sw->vbox), padding, TRUE, TRUE, 0);
    gtk_widget_show(padding);
    
    sw->viewport = gtk_scrolled_window_new(NULL, NULL);
    g_signal_connect(G_OBJECT(sw->viewport), "size-allocate", G_CALLBACK(viewport_size_allocate), sw);
    gtk_box_pack_end(GTK_BOX(sw->vbox), sw->viewport, FALSE, FALSE, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw->viewport), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_show(sw->viewport);
    
    sw->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_signal_connect(G_OBJECT(sw->hbox), "size-allocate", G_CALLBACK(hbox_size_allocate), sw);
    gtk_container_add(GTK_CONTAINER(sw->viewport), sw->hbox);
    gtk_widget_show(sw->hbox);
    
    sw->curpath = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(sw->curpath), 0);
    gtk_box_pack_start(GTK_BOX(sw->vbox), sw->curpath, FALSE, TRUE, 0);
    gtk_widget_show(sw->curpath);
    
    sw->ioch = thumbnail_creator_init();
    g_io_add_watch(sw->ioch, G_IO_IN, thumbnail_creator_done, sw);
    
    move_to_dir(sw, dirname, display_first);
    
    return sw;
}

GtkWidget *miv_selection_get_widget(struct miv_selection_t *sw)
{
    return sw->vbox;
}
