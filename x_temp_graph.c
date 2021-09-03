/*******************************************************************************

    File:     x_temp_graph.c
    Author:   Rick Cooper
    Created:  18/09/13
    Contents: Draw a graph showing p(selection) as a function of temperature

    Public procedures:

*******************************************************************************/

#include "xrng.h"
#include "lib_cairox.h"
#include <ctype.h>
#include <cairo-pdf.h>

#define MARGIN_LEFT 40
#define MARGIN_RIGHT 15
#define MARGIN_TOP 10
#define MARGIN_BOTTOM 40

#define T_MIN 0.00
#define T_MAX 1.00
#define J_MAX 400

extern double strength[SCHEMA_SET_SIZE];
extern char *slabels[SCHEMA_SET_SIZE];

static double value[SCHEMA_SET_SIZE][J_MAX] = {};
static GtkWidget *temp_graph_canvas = NULL;

static double rng_selection_probability(int s, double temp)
{
    if (temp < 0) {
        return(0.0);
    }
    else if (temp > 0) {
        double weighted_sum = 0.0;
        int i;

        for (i = 0; i < SCHEMA_SET_SIZE; i++) {
            weighted_sum += exp(log(strength[i])/temp);
        }
        return(exp(log(strength[s])/temp) / weighted_sum);
    }
    else {
        /* Check if this schema is maximal strength, in which case its  */
        /* selection probability is the reciprocal of the number of     */
        /* maximal strength schemas. Otherwise it is zero.              */

        double maximal_value = -1.0;
        int maximal_count = 0;
        Boolean is_maximal = FALSE;
        int i;

        for (i = 0; i < SCHEMA_SET_SIZE; i++) {
            if (strength[i] > maximal_value) {
                maximal_value = strength[i];
                maximal_count = 1;
                is_maximal = (s == i);
            }
            else if (strength[i] == maximal_value) {
                maximal_count++;
                is_maximal = is_maximal || (s == i);
            }
            else if (s == i) {
                is_maximal = FALSE;
            }
        }
        return(is_maximal ? 1.0 / (double) maximal_count : 0.0);
    }
}

/*----------------------------------------------------------------------------*/

void cairo_draw_temperature_graph(cairo_t *cr, double width, double height)
{
    CairoxTextParameters p;
    PangoLayout *layout;
    char buffer[64];
    double x, y, x0, y0, x1, y1, t, pr;
    double t0 = 1.0; /* Temperature used to calculate probabilities in the legend */
    int i, j;

    x0 = MARGIN_LEFT;
    x1 = width - MARGIN_RIGHT;
    y0 = MARGIN_TOP;
    y1 = height - MARGIN_BOTTOM;

    /* Clear the background: */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    layout = pango_cairo_create_layout(cr);
    pangox_layout_set_font_size(layout, 14);

    /* Draw the axes: */

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    for (i = 0; i < 11; i++) {
        t = T_MIN + i * (T_MAX - T_MIN) / 10.0;
        x = x0 + (x1 - x0) *  i / 10.0;
        if ((i == 0) || (i == 10)) {
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        }
        else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairox_paint_line(cr, 1.0, x, y0, x, y1-3);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        g_snprintf(buffer, 64, "%5.3f", t);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    cairox_text_parameters_set(&p, (x0+x1)/2.0, y1+24, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "Temperature");

    // Vetical axis
    pangox_layout_set_font_size(layout, 12);
    for (i = 0; i < 11; i++) {
        pr = i / 10.0;
        y = y1 + (y0 - y1) *  i / 10.0;
        if ((i == 0) || (i == 10)) {
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        }
        else {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        }
        cairox_paint_line(cr, 1.0, x0+3, y, x1, y);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairox_paint_line(cr, 1.0, x0-3, y, x0+3, y);
        cairox_text_parameters_set(&p, x0-4, y, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_CENTER, 0.0);
        g_snprintf(buffer, 64, "%3.1f", pr);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    cairox_text_parameters_set(&p, x0-24, (y0+y1)/2.0, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 90.0);
    cairox_paint_pango_text(cr, &p, layout, "P(selection)");

    // Now draw the graph
    for (i = 0; i < SCHEMA_SET_SIZE; i++) {
        j = 0;
        x = x0 + (x1 - x0) * j / (J_MAX - 1.0);
        y = y1 - (y1 - y0) * value[i][j];
        cairox_select_colour_scale(cr, i / 9.0);
        cairo_move_to(cr, x, y);
        for (j = 1; j < J_MAX; j++) {
            x = x0 + (x1 - x0) * j / (J_MAX - 1.0);
            y = y1 - (y1 - y0) * value[i][j];
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
    }

    // And the legend:
    /* The bounding box: */
    cairo_rectangle(cr, x1-220-MARGIN_RIGHT, y0 + 15, 220, 200);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    /* The title: */
    cairox_text_parameters_set(&p, x1-110-MARGIN_RIGHT, y0 + 24, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_CENTER, 0.0);
    g_snprintf(buffer, 64, "Selection probability at T = %4.2f", t0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    for (i = 0; i < SCHEMA_SET_SIZE; i++) {
        /* A legend entry: */
        y = y0 + 40 + i*16;
        cairox_select_colour_scale(cr, i / 9.0);
        cairo_move_to(cr, x1-220, y);
        cairo_line_to(cr, x1-170, y);
        cairo_stroke(cr);
        cairox_text_parameters_set(&p, x1-160, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
        g_snprintf(buffer, 64, "%s", slabels[i]);
        cairox_paint_pango_text(cr, &p, layout, buffer);

        cairox_text_parameters_set(&p, x1-70, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
        g_snprintf(buffer, 64, "[%4.3f]", rng_selection_probability(i, t0));
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }

    g_object_unref(layout);
}

void temperature_graph_canvas_draw()
{
    if (temp_graph_canvas != NULL) {
        cairo_surface_t *s;
        cairo_t *cr;
        int width, height;

        double temp;
        int i, j;

        /* Calculate the values to be graphed: */
        for (j = 0; j < J_MAX; j++) {
            temp = T_MIN + (T_MAX - T_MIN) * j / (double) (J_MAX - 1);
            for (i = 0; i < SCHEMA_SET_SIZE; i++) {
                value[i][j] = rng_selection_probability(i, temp);
            }
        }

        /* Double buffer: First create and draw to a surface: */
        gtk_widget_realize(temp_graph_canvas);
        width = temp_graph_canvas->allocation.width;
        height = temp_graph_canvas->allocation.height;
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create(s);
        cairo_draw_temperature_graph(cr, width, height);
        cairo_destroy(cr);

        /* Now copy the surface to the window: */
        cr = gdk_cairo_create(temp_graph_canvas->window);
        cairo_set_source_surface(cr, s, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);

        /* And destroy the surface: */
        cairo_surface_destroy(s);
    }
}

static int temperature_graph_event_expose(GtkWidget *da, GdkEventConfigure *event, void *dummy)
{
    if (temp_graph_canvas != NULL) {
        temperature_graph_canvas_draw();
    }
    return(1);
}

GtkWidget *temperature_graph_create_notepage()
{
    GtkWidget *tmp, *page;

    page = gtk_vbox_new(FALSE, 0);

    /* The output panel: */

    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(temperature_graph_event_expose), NULL);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    temp_graph_canvas = tmp;

    gtk_widget_show(page);
    return(page);
}

/******************************************************************************/
