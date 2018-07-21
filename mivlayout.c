#include <gtk/gtk.h>
#include "mivlayout.h"

typedef struct _MivLayoutChild   MivLayoutChild;

enum {
    CHILD_IMAGE,
    CHILD_LABELS,
    CHILD_SEL,
    NR_CHILDREN
};

enum {
    TRANSLATION_CHANGED,
    LAST_SIGNAL
};

struct _MivLayoutPrivate
{
    struct child_t {
	GtkWidget *w;
	GtkRequisition preferred;
	GtkAllocation alloc;
    } children[NR_CHILDREN];
    int image_tx, image_ty;
    gboolean is_fullscreen;
};

static void miv_layout_finalize           (GObject        *object);
static void miv_layout_realize            (GtkWidget      *widget);
static void miv_layout_unrealize          (GtkWidget      *widget);
static void miv_layout_map                (GtkWidget      *widget);
static void miv_layout_get_preferred_width  (GtkWidget     *widget,
                                             gint          *minimum,
                                             gint          *natural);
static void miv_layout_get_preferred_height (GtkWidget     *widget,
                                             gint          *minimum,
                                             gint          *natural);
static void miv_layout_size_allocate      (GtkWidget      *widget,
                                           GtkAllocation  *allocation);
static void miv_layout_add                (GtkContainer   *container,
					   GtkWidget      *widget);
static void miv_layout_remove             (GtkContainer   *container,
                                           GtkWidget      *widget);
static void miv_layout_forall             (GtkContainer   *container,
                                           gboolean        include_internals,
                                           GtkCallback     callback,
                                           gpointer        callback_data);
static void miv_layout_allocate_child     (MivLayout      *layout,
                                           struct child_t *child);
static void miv_layout_set_child          (MivLayout *layout,
                                           int type,
                                           GtkWidget *w);
static void miv_layout_calc_image_position(MivLayout *layout,
                                           const GtkAllocation *allocation);
static void miv_layout_set_translate(
	MivLayout *layout,
	int tx,
	int ty);
static void miv_layout_translation_changed(MivLayout *layout, gpointer data);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE(MivLayout, miv_layout, GTK_TYPE_CONTAINER)

/* Public interface
 */
/**
 * miv_layout_new:
 * 
 * Creates a new #MivLayout.
 * 
 * Returns: a new #MivLayout
 **/
  
GtkWidget *miv_layout_new(void)
{
  MivLayout *layout;

  layout = g_object_new(MIV_TYPE_LAYOUT, NULL);

  return GTK_WIDGET(layout);
}

static void miv_layout_finalize(GObject *object)
{
    (*G_OBJECT_CLASS(miv_layout_parent_class)->finalize)(object);
}

/**
 * miv_layout_set_image:
 * @layout: a #GtkLayout
 * @child_widget: child widget
 * @x: X position of child widget
 * @y: Y position of child widget
 *
 * Adds @child_widget to @layout, at position (@x,@y).
 * @layout becomes the new parent container of @child_widget.
 * 
 **/
void miv_layout_set_image(
	MivLayout     *layout, 
	GtkWidget     *child_widget)
{
    g_return_if_fail(MIV_IS_LAYOUT(layout));
    g_return_if_fail(GTK_IS_WIDGET(child_widget));
    
    miv_layout_set_child(layout, CHILD_IMAGE, child_widget);
}

void miv_layout_set_selection_view(
	MivLayout     *layout, 
	GtkWidget     *child_widget)
{
    g_return_if_fail(MIV_IS_LAYOUT(layout));
    g_return_if_fail(GTK_IS_WIDGET(child_widget));
    
    miv_layout_set_child(layout, CHILD_SEL, child_widget);
}

void miv_layout_set_labels(
	MivLayout     *layout, 
	GtkWidget     *child_widget)
{
    g_return_if_fail(MIV_IS_LAYOUT(layout));
    g_return_if_fail(GTK_IS_WIDGET(child_widget));
    
    miv_layout_set_child(layout, CHILD_LABELS, child_widget);
}

static void miv_layout_set_child(
	MivLayout *layout,
	int type,
	GtkWidget *w)
{
    MivLayoutPrivate *priv = layout->priv;
    
    if (priv->children[type].w != NULL) {
	gtk_widget_unparent(priv->children[type].w);
	priv->children[type].w = NULL;
    }
    
    priv->children[type].w = w;
    
    if (gtk_widget_get_realized(GTK_WIDGET(layout)))
	gtk_widget_set_parent_window(w, gtk_widget_get_window(GTK_WIDGET(layout)));
    
    gtk_widget_set_parent(w, GTK_WIDGET(layout));
}

void miv_layout_translate_image(
	MivLayout *layout,
	int dx,
	int dy)
{
    MivLayoutPrivate *priv;
    
    g_return_if_fail(MIV_IS_LAYOUT(layout));
    
    priv = layout->priv;
    
    miv_layout_set_translate(layout, priv->image_tx + dx, priv->image_ty + dy);
    
    if (priv->children[CHILD_IMAGE].w != NULL) {
	if (gtk_widget_get_visible(priv->children[CHILD_IMAGE].w) &&
		gtk_widget_get_visible(GTK_WIDGET(layout)))
	    gtk_widget_queue_resize(priv->children[CHILD_IMAGE].w);
    }
}

void miv_layout_reset_translation(MivLayout *layout)
{
    MivLayoutPrivate *priv;
    
    g_return_if_fail(MIV_IS_LAYOUT(layout));
    
    priv = layout->priv;
    
    miv_layout_set_translate(layout, 0, 0);
    
    if (priv->children[CHILD_IMAGE].w != NULL) {
	if (gtk_widget_get_visible(priv->children[CHILD_IMAGE].w) &&
		gtk_widget_get_visible(GTK_WIDGET(layout)))
	    gtk_widget_queue_resize(priv->children[CHILD_IMAGE].w);
    }
}

void miv_layout_set_fullscreen_mode(
	MivLayout *layout,
	gboolean is_fullscreen)
{
    MivLayoutPrivate *priv;
    
    g_return_if_fail(MIV_IS_LAYOUT(layout));
    
    priv = layout->priv;
    
    if (priv->is_fullscreen != is_fullscreen) {
	priv->is_fullscreen = is_fullscreen;
	gtk_widget_queue_resize(GTK_WIDGET(layout));
    }
}

/* Basic Object handling procedures
 */
static void miv_layout_class_init(MivLayoutClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->finalize = miv_layout_finalize;

  widget_class->realize = miv_layout_realize;
  widget_class->unrealize = miv_layout_unrealize;
  widget_class->map = miv_layout_map;
  widget_class->get_preferred_width = miv_layout_get_preferred_width;
  widget_class->get_preferred_height = miv_layout_get_preferred_height;
  widget_class->size_allocate = miv_layout_size_allocate;

  container_class->add = miv_layout_add;
  container_class->remove = miv_layout_remove;
  container_class->forall = miv_layout_forall;
  
  class->translation_changed = miv_layout_translation_changed;
  
  signals[TRANSLATION_CHANGED] =
	  g_signal_new("translation-changed",
		  G_OBJECT_CLASS_TYPE(gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET(MivLayoutClass, translation_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void miv_layout_init(MivLayout *layout)
{
    MivLayoutPrivate *priv;
    
    layout->priv = miv_layout_get_instance_private(layout);
    priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	priv->children[i].w = NULL;
	priv->children[i].preferred.width = 1;
	priv->children[i].preferred.height = 1;
	priv->children[i].alloc.x = -1;
	priv->children[i].alloc.y = -1;
	priv->children[i].alloc.width = 1;
	priv->children[i].alloc.height = 1;
    }
}

/* Widget methods
 */

static void miv_layout_realize(GtkWidget *widget)
{
    GtkAllocation allocation;
    GdkWindow *window;
    GdkWindowAttr attributes;
    gint attributes_mask;
    
    gtk_widget_set_realized(widget, TRUE);
    
    gtk_widget_get_allocation(widget, &allocation);
    
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    // attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;
    
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
    
    window = gdk_window_new(gtk_widget_get_parent_window(widget),
	    &attributes, attributes_mask);
    gtk_widget_set_window(widget, window);
    gtk_widget_register_window(widget, window);
}

static void miv_layout_map(GtkWidget *widget)
{
    MivLayout *layout = MIV_LAYOUT(widget);
    MivLayoutPrivate *priv = layout->priv;
    
    gtk_widget_set_mapped(widget, TRUE);
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	struct child_t *child = &priv->children[i];
	if (child->w != NULL && gtk_widget_get_visible(child->w)) {
	    if (!gtk_widget_get_mapped(child->w))
		gtk_widget_map(child->w);
	}
    }
    
    gdk_window_show(gtk_widget_get_window(widget));
}

static void miv_layout_unrealize(GtkWidget *widget)
{
    (*GTK_WIDGET_CLASS(miv_layout_parent_class)->unrealize)(widget);
}

static void miv_layout_get_preferred_width(
	GtkWidget *widget,
	gint      *minimum,
	gint      *natural)
{
    MivLayout *layout = MIV_LAYOUT(widget);
    MivLayoutPrivate *priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL)
	    gtk_widget_get_preferred_size(priv->children[i].w, &priv->children[i].preferred, NULL);
    }
    
    *minimum = *natural = 0;
}

static void miv_layout_get_preferred_height(
	GtkWidget *widget,
	gint      *minimum,
	gint      *natural)
{
    MivLayout *layout = MIV_LAYOUT(widget);
    MivLayoutPrivate *priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL)
	    gtk_widget_get_preferred_size(priv->children[i].w, &priv->children[i].preferred, NULL);
    }
    
    *minimum = *natural = 0;
}

static void miv_layout_size_allocate(
	GtkWidget     *widget,
	GtkAllocation *allocation)
{
    MivLayout *layout = MIV_LAYOUT(widget);
    MivLayoutPrivate *priv = layout->priv;
    
    gtk_widget_set_allocation(widget, allocation);
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	priv->children[i].alloc.width = priv->children[i].preferred.width;
	priv->children[i].alloc.height = priv->children[i].preferred.height;
    }
    
    miv_layout_calc_image_position(layout, allocation);
    
    priv->children[CHILD_SEL].alloc.x = 0;
    priv->children[CHILD_SEL].alloc.y = allocation->height - priv->children[CHILD_SEL].preferred.height;
    priv->children[CHILD_SEL].alloc.width = allocation->width;
    
    priv->children[CHILD_LABELS].alloc.x = 0;
    priv->children[CHILD_LABELS].alloc.y = 0;
    
    for (int i = 0; i < NR_CHILDREN; i++)
	miv_layout_allocate_child(layout, &priv->children[i]);
    
    if (gtk_widget_get_realized(widget)) {
	gdk_window_move_resize(gtk_widget_get_window(widget),
		allocation->x, allocation->y,
		allocation->width, allocation->height);
    }
}

static void miv_layout_calc_image_position(MivLayout *layout, const GtkAllocation *allocation)
{
    MivLayoutPrivate *priv = layout->priv;
    int lw = allocation->width, lh = allocation->height;
    int x, y;	// where to place center of the image in layout widget.
    int tx = priv->image_tx, ty = priv->image_ty;
    int w = priv->children[CHILD_IMAGE].preferred.width;
    int h = priv->children[CHILD_IMAGE].preferred.height;
    
    x = lw / 2;
    y = lh / 2;
    
    if (w > lw) {
	x += tx;
	if (x - w / 2 > 0)
	    x = w / 2;
	if (x + w / 2 < lw)
	    x = lw - w / 2;
	tx = x - lw / 2;
    } else
	tx = 0;
    if (h > lh) {
	y += ty;
	if (y - h / 2 > 0)
	    y = h / 2;
	if (y + h / 2 < lh)
	    y = lh - h / 2;
	ty = y - lh / 2;
    } else
	ty = 0;
    
    priv->children[CHILD_IMAGE].alloc.x = x - w / 2;
    priv->children[CHILD_IMAGE].alloc.y = y - h / 2;
    miv_layout_set_translate(layout, tx, ty);
}

static void miv_layout_set_translate(
	MivLayout *layout,
	int tx,
	int ty)
{
    MivLayoutPrivate *priv = layout->priv;
    
    if (priv->image_tx != tx || priv->image_ty != ty) {
	priv->image_tx = tx;
	priv->image_ty = ty;
	GdkPoint ptr;
	ptr.x = tx;
	ptr.y = ty;
	g_signal_emit(layout, signals[TRANSLATION_CHANGED], 0, &ptr);
    }
}

static void miv_layout_translation_changed(MivLayout *layout, gpointer data)
{
}

/* Container methods
 */
static void miv_layout_add(
	GtkContainer *container,
	GtkWidget    *widget)
{
    miv_layout_set_image(MIV_LAYOUT(container), widget);
}

static void miv_layout_remove(
	GtkContainer *container, 
	GtkWidget    *widget)
{
    MivLayout *layout = MIV_LAYOUT(container);
    MivLayoutPrivate *priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL && priv->children[i].w == widget) {
	    gtk_widget_unparent(widget);
	    priv->children[i].w = NULL;
	}
    }
}

static void miv_layout_forall(
	GtkContainer *container,
	gboolean      include_internals,
	GtkCallback   callback,
	gpointer      callback_data)
{
    MivLayout *layout = MIV_LAYOUT(container);
    MivLayoutPrivate *priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL)
	    (*callback)(priv->children[i].w, callback_data);
    }
}

/* Operations on children
 */

static void miv_layout_allocate_child(
	MivLayout      *layout,
	struct child_t *child)
{
    if (child->w != NULL) {
	printf("%s %dx%d+%d+%d\n",
		G_OBJECT_TYPE_NAME(child->w),
		child->alloc.width,
		child->alloc.height,
		child->alloc.x,
		child->alloc.y);
	gtk_widget_size_allocate(child->w, &child->alloc);
    }
}
