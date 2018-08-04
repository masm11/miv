#include <gtk/gtk.h>
#include <math.h>
#include <assert.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "mivselection.h"
#include "miv.h"
#include "items_creator.h"

static void move_to_dir(struct miv_selection_t *sw, const gchar *path, gboolean display_first);

struct miv_selection_t {
    GtkWidget *vbox;
    GtkWidget *viewport;
    GtkWidget *hbox;
    GtkWidget *curpath;
    
    GtkWidget *first_item;
    
    struct items_creator_t *cr;
};

G_DEFINE_QUARK(miv-selection-fullpath, miv_selection_fullpath)
G_DEFINE_QUARK(miv-selection-image, miv_selection_image)
G_DEFINE_QUARK(miv-selection-vbox, miv_selection_vbox)
G_DEFINE_QUARK(miv-selection-evbox, miv_selection_evbox)
G_DEFINE_QUARK(miv-selection-gesture, miv_selection_gesture)
G_DEFINE_QUARK(miv-selection-is-movie, miv_selection_is_movie)

static void hover_one(struct miv_selection_t *sw, GtkWidget *from, GtkWidget *to)
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
    GtkWidget *first, *last;
    GtkWidget *prev, *cur, *next;
    GtkWidget *sel;
    GtkWidget *pgup, *pgdn;
    
    gdouble pageup_begin, pageup_end;
    gdouble pagedn_begin, pagedn_end;
};

static void find_selected_iter(GtkWidget *w, gpointer user_data)
{
    struct find_selected_t *sel = user_data;
    
    GtkAllocation alloc;
    gtk_widget_get_allocation(w, &alloc);
    if (alloc.x < 0)
	return;
    
    if (sel->first == NULL)
	sel->first = w;
    
    sel->last = w;
    
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
    
    if (alloc.x < sel->pageup_end)
	sel->pgup = w;
    if (alloc.x + alloc.width > sel->pagedn_begin && sel->pgdn == NULL)
	sel->pgdn = w;
}

static void find_selected(struct miv_selection_t *sw, struct find_selected_t *sel)
{
    sel->first = sel->last = NULL;
    sel->prev = sel->cur = sel->next = sel->sel = NULL;
    sel->pgup = sel->pgdn = NULL;
    
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw->viewport));
    double val = gtk_adjustment_get_value(adj);
    double psize = gtk_adjustment_get_page_size(adj);
    sel->pageup_begin = val - psize - psize / 2;
    sel->pageup_end = sel->pageup_begin + psize;
    sel->pagedn_begin = val + psize + psize / 2;
    sel->pagedn_end = sel->pagedn_begin + psize;
    
    gtk_container_foreach(GTK_CONTAINER(sw->hbox), find_selected_iter, sel);
}

static void select_next(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.next != NULL)
	hover_one(sw, sel.cur, sel.next);
}

static void select_prev(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.prev != NULL)
	hover_one(sw, sel.cur, sel.prev);
}

static void select_first(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.first != NULL)
	hover_one(sw, sel.cur, sel.first);
}

static void select_last(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.last != NULL)
	hover_one(sw, sel.cur, sel.last);
}

static void select_pageup(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.pgup != NULL)
	hover_one(sw, sel.cur, sel.pgup);
}

static void select_pagedn(struct miv_selection_t *sw, GtkWidget *view)
{
    struct find_selected_t sel;
    find_selected(sw, &sel);
    if (sel.cur != NULL && sel.pgdn != NULL)
	hover_one(sw, sel.cur, sel.pgdn);
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
	hover_one(sw, sel.cur, sel.next);
	
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
	hover_one(sw, sel.cur, sel.prev);
	
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.prev), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (miv_display(fullpath, NULL)) {
		select_one(sw, sel.sel, sel.prev);
		return;
	    }
	}
    }
}

static void enter_it(struct miv_selection_t *sw, GtkWidget *view, gboolean with_shift_pressed)
{
    struct find_selected_t sel;
    
    find_selected(sw, &sel);
    
    if (sel.cur != NULL) {
	const gchar *fullpath = g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_fullpath_quark());
	if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	    if (!g_object_get_qdata(G_OBJECT(sel.cur), miv_selection_is_movie_quark()) || !with_shift_pressed) {
		if (miv_display(fullpath, NULL))
		    select_one(sw, sel.sel, sel.cur);
	    } else {
		select_one(sw, sel.sel, sel.cur);
		switch (fork()) {
		case -1:
		    perror("fork");
		    exit(1);
		case 0:
		    execlp("mpv", "mpv", "-fs", fullpath, NULL);
		    perror("execlp: mpv");
		    exit(1);
		}
	    }
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
    case GDK_KEY_Home:
	select_first(sw, widget);
	break;
    case GDK_KEY_End:
	select_last(sw, widget);
	break;
    case GDK_KEY_Page_Up:
	select_pageup(sw, widget);
	break;
    case GDK_KEY_Page_Down:
	select_pagedn(sw, widget);
	break;
    case GDK_KEY_space:
	display_next(sw, widget);
	break;
    case GDK_KEY_BackSpace:
	display_prev(sw, widget);
	break;
    case GDK_KEY_Return:
	enter_it(sw, widget, event->state & GDK_SHIFT_MASK);
	break;
    }
}

#define SIZE 100.0

static GtkWidget *create_image_selection_file(gchar *fullpath)
{
    // box: for margin/padding. eventbox doesn't supports event selection.
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *style_context = gtk_widget_get_style_context(outer);
    gtk_style_context_add_class(style_context, "item");
    
    // eventbox: for event selection. box doesn't supports margin/padding.
    GtkWidget *evbox = gtk_event_box_new();
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
    
    GObject *gobj = G_OBJECT(outer);
    g_object_set_qdata(gobj, miv_selection_evbox_quark(), evbox);
    g_object_set_qdata(gobj, miv_selection_vbox_quark(), vbox);
    g_object_set_qdata(gobj, miv_selection_image_quark(), image);
    
    return outer;
}

static void replace_item_image(GtkWidget *item, GdkPixbuf *pixbuf, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    
    (void) sw;
    
    GtkWidget *vbox = g_object_get_qdata(G_OBJECT(item), miv_selection_vbox_quark());
    assert(GTK_IS_BOX(vbox));
    GtkWidget *img = g_object_get_qdata(G_OBJECT(item), miv_selection_image_quark());
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
    
    GObject *gobj = G_OBJECT(item);
    g_object_set_qdata(gobj, miv_selection_image_quark(), img);
    g_object_set_qdata(gobj, miv_selection_is_movie_quark(), g_object_get_data(G_OBJECT(pixbuf), "miv-is-movie"));
}

static GtkWidget *create_image_selection_dir(gchar *fullpath)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *style_context = gtk_widget_get_style_context(outer);
    gtk_style_context_add_class(style_context, "item");
    
    GtkWidget *evbox = gtk_event_box_new();
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
    
    GObject *gobj = G_OBJECT(outer);
    g_object_set_qdata(gobj, miv_selection_evbox_quark(), evbox);
    g_object_set_qdata(gobj, miv_selection_vbox_quark(), vbox);
    
    return outer;
}

static gboolean item_draw(GtkWidget *w, cairo_t *cr, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(w, &alloc);
    
    GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sw->viewport));
    double beg = gtk_adjustment_get_value(adj);
    double end = beg + gtk_adjustment_get_page_size(adj);
    
    if (!(alloc.x + alloc.width < beg || alloc.x >= end))
	items_creator_prioritize(sw->cr, w);
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
    hover_one(sw, sel.cur, item);
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
    hover_one(sw, sel.cur, item);
    
    return TRUE;
}

static GtkWidget *create_image_selection_item(struct miv_selection_t *sw, const char *name, gboolean *isimage)
{
    gchar *fullpath = g_strdup(name);
    GtkWidget *w;
    
    if (!g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
	w = create_image_selection_file(fullpath);
	*isimage = TRUE;
    } else {
	w = create_image_selection_dir(fullpath);
	*isimage = FALSE;
    }
    
    if (w != NULL) {
	GObject *gobj = G_OBJECT(w);
	g_object_set_qdata_full(gobj, miv_selection_fullpath_quark(), fullpath, (GDestroyNotify) g_free);
	
	g_signal_connect(gobj, "draw", G_CALLBACK(item_draw), sw);
	
	GtkWidget *evbox = g_object_get_qdata(gobj, miv_selection_evbox_quark());
	GtkGesture *gesture = gtk_gesture_multi_press_new(evbox);
	gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(gesture), GTK_PHASE_TARGET);
	g_signal_connect(G_OBJECT(gesture), "pressed", G_CALLBACK(item_pressed), sw);
	g_object_set_qdata_full(gobj, miv_selection_gesture_quark(), gesture, g_object_unref);  // to unref gesture on widget destruction.
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

static GtkWidget *add_item(const gchar *fullpath, gpointer user_data)
{
    struct miv_selection_t *sw = user_data;
    
    gboolean isimage;
    GtkWidget *item = create_image_selection_item(sw, fullpath, &isimage);
    if (item != NULL) {
	gtk_box_pack_start(GTK_BOX(sw->hbox), item, FALSE, FALSE, 0);
	gtk_widget_show(item);
	if (sw->first_item == NULL) {
	    sw->first_item = item;
	    hover_one(sw, NULL, item);
	}
    }
    
    return isimage ? item : NULL;
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
    
    inline gchar *make_fullpath(const gchar *name) {
	if (strcmp(dirname, "/") != 0)
	    return g_strdup_printf("%s/%s", dirname, name);
	else
	    return g_strdup_printf("/%s", name);
    }
    
    int compare_names(gconstpointer a, gconstpointer b) {
	return strcmp(a, b);
    }
    
    GList *list = NULL;
    GError *err = NULL;
    GDir *dir = g_dir_open(dirname, 0, &err);
    if (err == NULL) {
	while (TRUE) {
	    const gchar *name = g_dir_read_name(dir);
	    if (name == NULL)
		break;
	    if (name[0] == '.')
		continue;
	    list = g_list_prepend(list, make_fullpath(name));
	}
	g_dir_close(dir);
	
	list = g_list_sort(list, compare_names);
    }
    
    list = g_list_prepend(list, make_fullpath(".."));
    
    
    if (sw->cr != NULL) {
	items_creator_destroy(sw->cr);
	sw->cr = NULL;
    }
    
    gtk_container_foreach(GTK_CONTAINER(sw->hbox), (GtkCallback) gtk_widget_destroy, NULL);
    
    
    sw->first_item = NULL;
    
    sw->cr = items_creator_new(list, sw);
    items_creator_set_add_handler(sw->cr, add_item);
    items_creator_set_replace_handler(sw->cr, replace_item_image);
    
    
    g_free(dirname);
}

struct miv_selection_t *miv_selection_create(const gchar *dirname, gboolean display_first)
{
    struct miv_selection_t *sw = g_new0(struct miv_selection_t, 1);
    
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
    
    move_to_dir(sw, dirname, display_first);
    
    return sw;
}

GtkWidget *miv_selection_get_widget(struct miv_selection_t *sw)
{
    return sw->vbox;
}
