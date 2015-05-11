// 7 april 2015
#include "uipriv_unix.h"

struct singleWidget {
	GtkWidget *widget;
	GtkWidget *scrolledWindow;
	GtkWidget *immediate;		// the widget that is added to the parent container; either widget or scrolledWindow
	uiContainer *parent;
	int hidden;
	void (*onDestroy)(void *);
	void *onDestroyData;
};

static void singleDestroy(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	if (s->parent != NULL)
		complain("attempt to destroy a uiControl at %p while it still has a parent", c);
	// first call the widget's own destruction code
	(*(s->onDestroy))(s->onDestroyData);
	// then actually destroy
	g_object_unref(s->immediate);
	// and free ourselves
	uiFree(s);
}

static uintptr_t singleHandle(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	return (uintptr_t) (s->widget);
}

static void singleSetParent(uiControl *c, uiContainer *parent)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);
	uiContainer *oldparent;
	GtkContainer *oldcontainer;
	GtkContainer *newcontainer;

	oldparent = s->parent;
	s->parent = parent;
	if (oldparent != NULL) {
		oldcontainer = GTK_CONTAINER(uiControlHandle(uiControl(oldparent)));
		gtk_container_remove(oldcontainer, s->immediate);
	}
	if (s->parent != NULL) {
		newcontainer = GTK_CONTAINER(uiControlHandle(uiControl(s->parent)));
		gtk_container_add(newcontainer, s->immediate);
	}
}

static void singlePreferredSize(uiControl *c, uiSizing *d, intmax_t *width, intmax_t *height)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);
	GtkRequisition natural;

	// use the natural size as the minimum size is an *absolute* minimum
	// for example, if a label has ellipsizing on, it can be the width of the ellipses, not the text
	// there is a warning about height-for-width sizing, but in my tests this isn't an issue
	gtk_widget_get_preferred_size(s->widget, NULL, &natural);
	*width = natural.width;
	*height = natural.height;
}

static void singleResize(uiControl *c, intmax_t x, intmax_t y, intmax_t width, intmax_t height, uiSizing *d)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);
	GtkAllocation a;

	a.x = x;
	a.y = y;
	a.width = width;
	a.height = height;
	gtk_widget_size_allocate(s->immediate, &a);
}

static int singleVisible(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	return !s->hidden;
}

static void singleShow(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	gtk_widget_show_all(s->immediate);
	s->hidden = 0;
	if (s->parent != NULL)
		uiContainerUpdate(s->parent);
}

static void singleHide(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	gtk_widget_hide(s->immediate);
	s->hidden = 1;
	if (s->parent != NULL)
		uiContainerUpdate(s->parent);
}

static void singleEnable(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	gtk_widget_set_sensitive(s->immediate, TRUE);
}

static void singleDisable(uiControl *c)
{
	struct singleWidget *s = (struct singleWidget *) (c->Internal);

	gtk_widget_set_sensitive(s->immediate, FALSE);
}

void uiUnixMakeControl(uiControl *c, GType type, gboolean inScrolledWindow, gboolean scrolledWindowHasBorder, void (*onDestroy)(void *), void *onDestroyData, const char *firstProperty, ...)
{
	struct singleWidget *s;
	va_list ap;

	s = uiNew(struct singleWidget);

	va_start(ap, firstProperty);
	s->widget = GTK_WIDGET(g_object_new_valist(type, firstProperty, ap));
	va_end(ap);
	s->immediate = s->widget;

	if (inScrolledWindow) {
		s->scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
		if (!GTK_IS_SCROLLABLE(s->widget))
			gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(s->scrolledWindow), s->widget);
		else
			gtk_container_add(GTK_CONTAINER(s->scrolledWindow), s->widget);
		if (scrolledWindowHasBorder)
			gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(s->scrolledWindow), GTK_SHADOW_IN);
		s->immediate = s->scrolledWindow;
	}

	// we need to keep an extra reference on the immediate widget
	// this is so we can reparent the control freely
	g_object_ref_sink(s->immediate);

	s->onDestroy = onDestroy;
	s->onDestroyData = onDestroyData;

	// finally, call gtk_widget_show_all() here to set the initial visibility of the widget
	gtk_widget_show_all(s->immediate);

	uiControl(c)->Internal = s;
	uiControl(c)->Destroy = singleDestroy;
	uiControl(c)->Handle = singleHandle;
	uiControl(c)->SetParent = singleSetParent;
	uiControl(c)->PreferredSize = singlePreferredSize;
	uiControl(c)->Resize = singleResize;
	uiControl(c)->Visible = singleVisible;
	uiControl(c)->Show = singleShow;
	uiControl(c)->Hide = singleHide;
	uiControl(c)->Enable = singleEnable;
	uiControl(c)->Disable = singleDisable;
}
