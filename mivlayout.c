#include <gtk/gtk.h>
#include "mivlayout.h"

typedef struct _MivLayoutChild   MivLayoutChild;

enum {
    CHILD_IMAGE,
    CHILD_LABELS,
    CHILD_SEL,
    NR_CHILDREN
};

struct _MivLayoutPrivate
{
    struct child_t {
	GtkWidget *w;
	GtkRequisition preferred;
	GtkAllocation alloc;
    } children[NR_CHILDREN];
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
static gint miv_layout_draw               (GtkWidget      *widget,
                                           cairo_t        *cr);
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
static void miv_layout_set_child(
	MivLayout *layout,
	int type,
	GtkWidget *w);

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

  layout = g_object_new (MIV_TYPE_LAYOUT, NULL);

  return GTK_WIDGET (layout);
}

static void miv_layout_finalize (GObject *object)
{
  MivLayout *layout = MIV_LAYOUT (object);
  MivLayoutPrivate *priv = layout->priv;

  (*G_OBJECT_CLASS (miv_layout_parent_class)->finalize)(object);
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
    g_return_if_fail (MIV_IS_LAYOUT (layout));
    g_return_if_fail (GTK_IS_WIDGET (child_widget));
    
    miv_layout_set_child(layout, CHILD_IMAGE, child_widget);
}

void miv_layout_set_selection_view(
	MivLayout     *layout, 
	GtkWidget     *child_widget)
{
    g_return_if_fail (MIV_IS_LAYOUT (layout));
    g_return_if_fail (GTK_IS_WIDGET (child_widget));
    
    miv_layout_set_child(layout, CHILD_SEL, child_widget);
}

void miv_layout_set_labels(
	MivLayout     *layout, 
	GtkWidget     *child_widget)
{
    g_return_if_fail (MIV_IS_LAYOUT (layout));
    g_return_if_fail (GTK_IS_WIDGET (child_widget));
    
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

/* Basic Object handling procedures
 */
static void miv_layout_class_init (MivLayoutClass *class)
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
//  widget_class->draw = miv_layout_draw;

  container_class->add = miv_layout_add;
  container_class->remove = miv_layout_remove;
  container_class->forall = miv_layout_forall;
}

static void miv_layout_init (MivLayout *layout)
{
    MivLayoutPrivate *priv;
    
    layout->priv = miv_layout_get_instance_private (layout);
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
    MivLayout *layout = MIV_LAYOUT (widget);
    MivLayoutPrivate *priv = layout->priv;
    GtkAllocation allocation;
    GdkWindow *window;
    GdkWindowAttr attributes;
    gint attributes_mask;
    
    gtk_widget_set_realized (widget, TRUE);
    
    gtk_widget_get_allocation (widget, &allocation);
    
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    // attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;
    
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
    
    window = gdk_window_new (gtk_widget_get_parent_window(widget),
	    &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);
    gtk_widget_register_window (widget, window);
}

static void miv_layout_map (GtkWidget *widget)
{
    MivLayout *layout = MIV_LAYOUT (widget);
    MivLayoutPrivate *priv = layout->priv;
    
    gtk_widget_set_mapped (widget, TRUE);
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	struct child_t *child = &priv->children[i];
	if (child->w != NULL && gtk_widget_get_visible (child->w)) {
	    if (!gtk_widget_get_mapped (child->w))
		gtk_widget_map (child->w);
	}
    }
    
    gdk_window_show(gtk_widget_get_window(widget));
}

static void miv_layout_unrealize (GtkWidget *widget)
{
    MivLayout *layout = MIV_LAYOUT (widget);
    MivLayoutPrivate *priv = layout->priv;
    
    (*GTK_WIDGET_CLASS (miv_layout_parent_class)->unrealize)(widget);
}

static void miv_layout_get_preferred_width (
	GtkWidget *widget,
	gint      *minimum,
	gint      *natural)
{
    MivLayout *layout = MIV_LAYOUT (widget);
    MivLayoutPrivate *priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL)
	    gtk_widget_get_preferred_size(priv->children[i].w, &priv->children[i].preferred, NULL);
    }
    
    *minimum = *natural = 0;
}

static void miv_layout_get_preferred_height (
	GtkWidget *widget,
	gint      *minimum,
	gint      *natural)
{
    MivLayout *layout = MIV_LAYOUT (widget);
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
    
    priv->children[CHILD_IMAGE].alloc.x = 0;
    priv->children[CHILD_IMAGE].alloc.y = 0;
    
    priv->children[CHILD_SEL].alloc.x = 0;
    priv->children[CHILD_SEL].alloc.y = allocation->height - priv->children[CHILD_SEL].preferred.height;
    priv->children[CHILD_SEL].alloc.width = allocation->width;
    
    priv->children[CHILD_LABELS].alloc.x = 0;
    priv->children[CHILD_LABELS].alloc.y = 0;
    
    for (int i = 0; i < NR_CHILDREN; i++)
	miv_layout_allocate_child(layout, &priv->children[i]);
    
    if (gtk_widget_get_realized (widget)) {
	gdk_window_move_resize (gtk_widget_get_window (widget),
		allocation->x, allocation->y,
		allocation->width, allocation->height);
    }
}

static gboolean miv_layout_draw (
	GtkWidget *widget,
	cairo_t   *cr)
{
  MivLayout *layout = MIV_LAYOUT (widget);
  MivLayoutPrivate *priv = layout->priv;

#if 0
  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    GTK_WIDGET_CLASS (miv_layout_parent_class)->draw (widget, cr);
#endif

  return FALSE;
}

/* Container methods
 */
static void miv_layout_add (
	GtkContainer *container,
	GtkWidget    *widget)
{
    miv_layout_set_image(MIV_LAYOUT(container), widget);
}

static void miv_layout_remove (
	GtkContainer *container, 
	GtkWidget    *widget)
{
    MivLayout *layout = MIV_LAYOUT (container);
    MivLayoutPrivate *priv = layout->priv;

    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL && priv->children[i].w == widget) {
	    gtk_widget_unparent(widget);
	    priv->children[i].w = NULL;
	}
    }
}

static void miv_layout_forall (
	GtkContainer *container,
	gboolean      include_internals,
	GtkCallback   callback,
	gpointer      callback_data)
{
    MivLayout *layout = MIV_LAYOUT (container);
    MivLayoutPrivate *priv = layout->priv;
    
    for (int i = 0; i < NR_CHILDREN; i++) {
	if (priv->children[i].w != NULL)
	    (*callback)(priv->children[i].w, callback_data);
    }
}

/* Operations on children
 */

static void miv_layout_allocate_child (
	MivLayout      *layout,
	struct child_t *child)
{
    if (child->w != NULL) {
	printf("alloc: %dx%d+%d+%d.\n", child->alloc.width, child->alloc.height, child->alloc.x, child->alloc.y);
	gtk_widget_size_allocate(child->w, &child->alloc);
    }
}