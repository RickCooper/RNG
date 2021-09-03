#include "xrng.h"
#include "lib_cairox.h"
#include <ctype.h>
#include <cairo-pdf.h>

static GdkPixbuf *oos_box_icon[BOX_TYPE_MAX];

/******************************************************************************/

static void oos_icon_set(BoxType bt, char *file, int w, int h)
{
    GdkPixbuf *tmp = gdk_pixbuf_new_from_file(file, NULL);

    oos_box_icon[bt] = gdk_pixbuf_scale_simple(tmp, w, h, GDK_INTERP_HYPER);
    g_object_unref(tmp);
}

/******************************************************************************/

static void transcode_oos_name(char buffer[], int l, char *name)
{
    int i = 0;

    while ((name[i] != '\0') && (i < l)) {
        if (isalnum(name[i])) {
            buffer[i] = name[i];
        }
        else if (name[i] == ' ') {
            buffer[i] = '\n';
        }
        else {
            buffer[i] = '\0';
        }
        i++;
    }
    if (i < l) {
        buffer[i] = '\0';
    }
    else {
        buffer[l-1] = '\0';
    }
}

static CairoxPoint *transcode_arrow_coordinates(CairoxPoint *coordinates, int points, int width, int height)
{
    CairoxPoint *coords = (CairoxPoint *)malloc(sizeof(CairoxPoint)*points);
    int i;

    for (i = 0; i < points; i++) {
        coords[i].x = coordinates[i].x * width;
        coords[i].y = coordinates[i].y * height;
    }
    return(coords);
}

/*----------------------------------------------------------------------------*/

void diagram_draw_cairo(cairo_t *cr, XGlobals *globals, int width, int height)
{
    CairoxTextParameters p;
    PangoLayout *layout;
    char buffer[64];
    AnnotationList *tmpc;
    ArrowList *tmpa;
    BoxList *tmpb;
    double dashes[2] = {6, 6};
    double dots[2] = {2, 2};

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    layout = pango_cairo_create_layout(cr);
    pangox_layout_set_font_size(layout, 13);
    pangox_layout_set_font_face(layout, PS_FONT_SANS);
    pangox_layout_set_font_style(layout, PANGO_STYLE_NORMAL);

    // Draw the boxes:
    for (tmpb = globals->gv->components; tmpb != NULL; tmpb = tmpb->next) {
        cairox_paint_pixbuf(cr, oos_box_icon[tmpb->bt], (width*tmpb->x-48), (height*tmpb->y-19));
        cairox_text_parameters_set(&p, width*tmpb->x, height*tmpb->y, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_CENTER, 0.0);
        transcode_oos_name(buffer, 64, tmpb->name);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }

    // Draw the arrows/lines:
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    for (tmpa = globals->gv->arrows; tmpa != NULL; tmpa = tmpa->next) {
        CairoxPoint *coordinates = transcode_arrow_coordinates(tmpa->coordinates, tmpa->points, width, height);

        if (tmpa->line_style == LS_DOTTED) {
            cairo_set_dash(cr, dots, 2, 0);
        }
        else if (tmpa->line_style == LS_DASHED) {
            cairo_set_dash(cr, dashes, 2, 0);
        }
        else {
            cairo_set_dash(cr, NULL, 0, 0);
        }

        if (tmpa->vertex_style == VS_SHARP) {
            cairox_paint_straight_arrow(cr, tmpa->head, coordinates, tmpa->points, tmpa->width, 1.0);
        }
        else {
            cairox_paint_curved_arrow(cr, tmpa->head, coordinates, tmpa->points, tmpa->width, 1.0);
        }
        cairo_set_dash(cr, NULL, 0, 0);
        g_free(coordinates);
    }

    // Draw text annotations:
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (tmpc = globals->gv->annotations; tmpc != NULL; tmpc = tmpc->next) {
        pangox_layout_set_font_size(layout, tmpc->fontsize);
        if (tmpc->italic) {
            pangox_layout_set_font_style(layout, PANGO_STYLE_ITALIC);
        }
        else {
            pangox_layout_set_font_style(layout, PANGO_STYLE_NORMAL);
        }
        cairox_text_parameters_set(&p, width*tmpc->x, height*tmpc->y, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_CENTER, tmpc->theta);
        cairox_paint_pango_text(cr, &p, layout, tmpc->text);
    }

    g_object_unref(layout);
}

void diagram_draw_x(XGlobals *globals)
{
    if (globals->diagram != NULL) {
        cairo_t *cr;
        int width, height;

        gtk_widget_realize(globals->diagram);
        cr = gdk_cairo_create(globals->diagram->window);
        width = globals->diagram->allocation.width;
        height = globals->diagram->allocation.height;
        diagram_draw_cairo(cr, globals, width, height);
        cairo_destroy(cr);
    }
}

/******************************************************************************/

static int diagram_canvas_event_expose(GtkWidget *da, GdkEventConfigure *event, XGlobals *globals)
{
    diagram_draw_x(globals);
    return(1);
}

/******************************************************************************/

GtkWidget *diagram_create_notepage(XGlobals *globals)
{
    GtkWidget *page, *tmp;

    page = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(page);

    /* Initialise OOS icons: */
    oos_icon_set(BOX_PROCESS, "icon_process.png", 96, 40);
    oos_icon_set(BOX_BUFFER, "icon_buffer.png", 96, 40);

    /* The drawing panel: */
    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(diagram_canvas_event_expose), globals);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    globals->diagram = tmp;

    return(page);
}

/******************************************************************************/
