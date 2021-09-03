/*******************************************************************************

    File:     x_widgets.c
    Author:   Rick Cooper
    Created:  22/06/12
    Edited:   22/06/12
    Contents: An X interface to the RNG model

    Public procedures:
        Boolean rng_widgets_create(XGlobals *globals, OosVars *gv);
        void rng_widgets_initialise(XGlobals *globals);

*******************************************************************************/

#include "xrng.h"
#include "lib_cairox.h"
#include <ctype.h>
#include <cairo-pdf.h>

extern GtkWidget *temperature_graph_create_notepage();
extern void cairo_draw_temperature_graph(cairo_t *cr, double width, double height);
extern void temperature_graph_canvas_draw();

/* How many sets of data on the various bar charts: */
#define DATASETS 6

char *stat_label[DATASETS] = {"R", "RNG", "RR", "AA", "OA", "TPI"};
// char *stat_label[9] = {"R", "R_2", "RNG", "RR", "AA", "OA", "TPI", "RG (mean)", "RG (median)"};

// The range is X_LEFT to X_LEFT + X_DELTA * MAX_GROUPS

#define DECAY_LEFT        0
#define DECAY_DELTA       2
#define TEMP_LEFT      0.10
#define TEMP_DELTA     0.10
#define UPDATE_LEFT    1.00
#define UPDATE_DELTA  -0.05
#define MONITOR_LEFT   1.00
#define MONITOR_DELTA -0.05
#define SWITCH_LEFT    1.00
#define SWITCH_DELTA  -0.05

extern double strength[SCHEMA_SET_SIZE];
extern char *slabels[SCHEMA_SET_SIZE];

#define GR_MARGIN_UPPER 10
#define GR_MARGIN_LOWER 60

/******************************************************************************/

#define LINE_SEP 20
#define MARGIN  100

static void cairox_draw_statistics(cairo_t *cr, PangoLayout *layout, double y, double width, RngScores *scores)
{
    CairoxTextParameters p;
    char buffer[32];
    int i;
    double col_size;

    cairox_text_parameters_set(&p, 80, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "R1:");
    cairox_text_parameters_set(&p, 200, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "R2:");
    cairox_text_parameters_set(&p, 320, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "RNG:");
    cairox_text_parameters_set(&p, 440, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "TPI:");
    cairox_text_parameters_set(&p, 560, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "RG 1:");
    cairox_text_parameters_set(&p, 680, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "RG 2:");

    g_snprintf(buffer, 32, "%5.3f", scores->r1);
    cairox_text_parameters_set(&p, 120, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    g_snprintf(buffer, 32, "%5.3f", scores->r2);
    cairox_text_parameters_set(&p, 240, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    g_snprintf(buffer, 32, "%5.3f", scores->rng);
    cairox_text_parameters_set(&p, 360, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    g_snprintf(buffer, 32, "%5.3f", scores->tpi);
    cairox_text_parameters_set(&p, 480, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    g_snprintf(buffer, 32, "%5.3f", scores->rg1);
    cairox_text_parameters_set(&p, 600, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    g_snprintf(buffer, 32, "%5.3f", scores->rg2);
    cairox_text_parameters_set(&p, 720, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    cairox_text_parameters_set(&p, 80, y+LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "Associates:");
    col_size = (width - (MARGIN * 3.0)) / 10.0;
    for (i = 0; i < 11; i++) {
        g_snprintf(buffer, 32, "%5.3f", scores->associates[(i+5)%10]);
        cairox_text_parameters_set(&p, 2*MARGIN+i*col_size, y+LINE_SEP, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, buffer);
        g_snprintf(buffer, 32, "[%+d]", i - 5);
        cairox_text_parameters_set(&p, 2*MARGIN+i*col_size, y+2*LINE_SEP, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
}

/*----------------------------------------------------------------------------*/

static void cairo_draw_one_sequence(cairo_t *cr, XGlobals *globals, int width, int height)
{
    RngSubjectData *subject;
    CairoxTextParameters p;
    OosVars *gv = globals->gv;
    char buffer[32];
    PangoLayout *layout;
    double col_size;
    int i;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    layout = pango_cairo_create_layout(cr);
    pangox_layout_set_font_size(layout, 14);

    subject = &(((RngData *)gv->task_data)->subject[0]);

    g_snprintf(buffer, 32, "Responses (%d trial(s)):", subject->n);
    cairox_text_parameters_set(&p, 40,  LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    g_snprintf(buffer, 32, "Cycles: %d", gv->cycle);
    cairox_text_parameters_set(&p, width-20, LINE_SEP, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    if (subject->n > 0) {
        col_size = (width - (MARGIN * 2.0)) / 25.0;

        for (i = 0; i < subject->n; i++) {
            double x = MARGIN + (i % 25) * col_size;
            double y = 40 + LINE_SEP * (int) (i / 25);

            g_snprintf(buffer, 32, "%d", subject->response[i]);
            cairox_text_parameters_set(&p, x, y, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
            cairox_paint_pango_text(cr, &p, layout, buffer);
        }
    }

    if (subject->n == gv->trials_per_subject) {
        g_snprintf(buffer, 32, "Model Statistics:");
        cairox_text_parameters_set(&p, 40,  8*LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, buffer);
        cairox_draw_statistics(cr, layout, 9*LINE_SEP, width, &(subject->scores));
    }

    g_snprintf(buffer, 32, "Control Data:");
    cairox_text_parameters_set(&p, 40,  12*LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairox_draw_statistics(cr, layout, 13*LINE_SEP, width, &subject_ctl.mean);

    g_object_unref(layout);
}

static void one_sequence_canvas_draw(XGlobals *globals)
{
    cairo_surface_t *s;
    cairo_t *cr;
    int width, height;

    width = globals->seq_canvas->allocation.width;
    height = globals->seq_canvas->allocation.height;
    s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(s);
    cairo_draw_one_sequence(cr, globals, width, height);
    cairo_destroy(cr);

    /* Now copy the surface to the window: */
    cr = gdk_cairo_create(globals->seq_canvas->window);
    cairo_set_source_surface(cr, s, 0.0, 0.0);
    cairo_paint(cr);
    cairo_destroy(cr);

    /* And destroy the surface: */
    cairo_surface_destroy(s);
}

static void x_task_run_one_sequence(GtkWidget *caller, XGlobals *globals)
{
    RngSubjectData *subject;

    rng_create(globals->gv, &(globals->params));
    rng_initialise_subject(globals->gv);
    globals->running = TRUE;
    while ((globals->running) && (oos_step(globals->gv))) {
        gtk_main_iteration_do(FALSE);
    }
    subject = &(((RngData *)globals->gv->task_data)->subject[globals->gv->block]);
    rng_analyse_subject_responses(NULL, subject, globals->gv->trials_per_subject);
    one_sequence_canvas_draw(globals);
    globals->running = FALSE;
}

static int one_sequence_canvas_event_expose(GtkWidget *da, GdkEventConfigure *event, XGlobals *globals)
{
    if (globals->seq_canvas != NULL) {
        one_sequence_canvas_draw(globals);
    }
    return(1);
}

static GtkWidget *notepage_one_sequence_create(XGlobals *globals)
{
    GtkWidget *page, *hbox, *tmp;
    GtkToolItem *tool_item;
    GtkTooltips *tooltips;

    page = gtk_vbox_new(FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    tooltips = gtk_tooltips_new();

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD); // Run study
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_run_one_sequence), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Generate and score one sequence", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    /* The output panel: */

    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(one_sequence_canvas_event_expose), globals);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    globals->seq_canvas = tmp;

    gtk_widget_show(page);
    return(page);
}

/******************************************************************************/

static void cairo_table_row_headings(cairo_t *cr, PangoLayout *layout, double x0, double y0, double dy)
{
    CairoxTextParameters p;
    char buffer[32];
    double y;
    int i;

    y = y0;
    for (i = 0; i < DATASETS; i++) {
        cairox_text_parameters_set(&p, x0+5, y+dy-2, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, stat_label[i]);
        cairo_move_to(cr, x0, y);
        cairo_line_to(cr, x0, y+dy);
        cairo_line_to(cr, x0+100, y+dy);
        cairo_line_to(cr, x0+100, y);
        cairo_line_to(cr, x0, y);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_stroke(cr);
        y += dy;
    }
    for (i = 0; i < 10; i++) {
        g_snprintf(buffer, 32, "Associate[+%d]", i);
        cairox_text_parameters_set(&p, x0+5, y+dy-2, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, buffer);
        cairo_move_to(cr, x0, y);
        cairo_line_to(cr, x0, y+dy);
        cairo_line_to(cr, x0+100, y+dy);
        cairo_line_to(cr, x0+100, y);
        cairo_line_to(cr, x0, y);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_stroke(cr);
        y += dy;
    }
}

static void cairo_table_column(cairo_t *cr, PangoLayout *layout, double x0, double y0, double dy, char *heading, RngScores *means)
{
    CairoxTextParameters p;
    char buffer[32];
    double y;
    int i;

    y = y0;

    cairox_text_parameters_set(&p, x0+40, y-2, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, heading);

    g_snprintf(buffer, 32, "%5.3f", means->r1);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

#if FALSE
    g_snprintf(buffer, 32, "%5.3f", means->r2);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;
#endif

    g_snprintf(buffer, 32, "%5.3f", means->rng);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

    g_snprintf(buffer, 32, "%5.3f", means->rr);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

    g_snprintf(buffer, 32, "%5.3f", means->aa);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

    g_snprintf(buffer, 32, "%5.3f", means->oa);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

    g_snprintf(buffer, 32, "%5.3f", means->tpi);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

#if FALSE
    g_snprintf(buffer, 32, "%5.3f", means->rg1);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;

    g_snprintf(buffer, 32, "%5.3f", means->rg2);
    cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    cairo_move_to(cr, x0, y);
    cairo_line_to(cr, x0, y+dy);
    cairo_line_to(cr, x0+80, y+dy);
    cairo_line_to(cr, x0+80, y);
    cairo_line_to(cr, x0, y);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);
    y += dy;
#endif

    for (i = 0; i < 10; i++) {
        g_snprintf(buffer, 32, "%5.3f", means->associates[i]);
        cairox_text_parameters_set(&p, x0+60, y+dy-2, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, buffer);
        cairo_move_to(cr, x0, y);
        cairo_line_to(cr, x0, y+dy);
        cairo_line_to(cr, x0+80, y+dy);
        cairo_line_to(cr, x0+80, y);
        cairo_line_to(cr, x0, y);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_stroke(cr);
        y += dy;
    }
}

static void cairo_draw_table(cairo_t *cr, PangoLayout *layout, double width, double y0, double y1, OosVars *gv)
{
    RngData *task_data = (RngData *)gv->task_data;
    double dy = (y1 - y0) / 19.0;
    double x0 = (width - 500) / 2.0;

    pangox_layout_set_font_size(layout, 12);
    cairo_table_row_headings(cr, layout, x0, y0, dy);

    cairo_table_column(cr, layout, x0+100, y0, dy, "Ctrl", &subject_ctl.mean);
    cairo_table_column(cr, layout, x0+180, y0, dy, "DS", &subject_ds.mean);
    cairo_table_column(cr, layout, x0+260, y0, dy, "2B", &subject_2b.mean);
    cairo_table_column(cr, layout, x0+340, y0, dy, "GnG", &subject_gng.mean);
    if (task_data->group.n > 0) {
        cairo_table_column(cr, layout, x0+420, y0, dy, "Model", &(task_data->group.mean));
    }
}

/*----------------------------------------------------------------------------*/

static void draw_bar(cairo_t *cr, double x, double w, double y0, double y, double dy, int colour)
{
    cairo_move_to(cr, x-w*0.5, y0);
    cairo_line_to(cr, x-w*0.5, y);
    cairo_line_to(cr, x+w*0.5, y);
    cairo_line_to(cr, x+w*0.5, y0);

    cairox_select_colour(cr, colour, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);

    if (dy > 0) {
        cairox_paint_line(cr, 1.0,  x, y-dy, x, y+dy);
        cairox_paint_line(cr, 1.0,  x-3, y+dy, x+3, y+dy);
        cairox_paint_line(cr, 1.0,  x-3, y-dy, x+3, y-dy);
    }
}

static void global_analysis_draw_data(cairo_t *cr, double x0, double x1, double y0, double y1, double offset, RngGroupData *scores, int colour)
{
    double x, y, dy;
    int w = 16;
    int i = 0;

    /* R1: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.r1 * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.r1 * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

#if FALSE
    /* R2: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.r2 * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.r2 * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;
#endif

    /* RNG: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.rng * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.rng * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

    /* RR: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.rr * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.rr * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

    /* AA: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.aa * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.aa * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

    /* OA: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.oa * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.oa * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

    /* TPI: */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.tpi * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.tpi * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

#if FALSE
    /* RG (mean): */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.rg1 * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.rg1 * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;

    /* RG (median): */
    x = x0 + (x1-x0) * (i + 0.5) / (double) DATASETS + offset * w;
    y = (y0+y1)*0.5 + scores->mean.rg2 * (y0-y1) * 0.2;
    if (scores->n > 1) {
        dy = scores->sd.rg2 * (y1-y0) * 0.2 / sqrt(scores->n);
    }
    else {
        dy = 0.0;
    }
    draw_bar(cr, x, w, (y0+y1)*0.5, y, dy, colour); i++;
#endif
}

static void global_analysis_draw_key(cairo_t *cr, PangoLayout *layout, double x, double y, char *label, int n, int colour)
{
    CairoxTextParameters p;
    char buffer[32];

    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x-18, y-6, 13, 13);
    cairox_select_colour(cr, colour, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);

    cairox_text_parameters_set(&p, x, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
    g_snprintf(buffer, 32, "%s (N = %d)", label, n);
    cairox_paint_pango_text(cr, &p, layout, buffer);
}

static void cairo_draw_global_analysis(cairo_t *cr, PangoLayout *layout, double x0, double x1, double y0, double y1, OosVars *gv)
{
    CairoxTextParameters p;
    RngData *task_data = (RngData *)gv->task_data;
    RngGroupData ds_z, tb_z, gng_z, z_scores;
    char buffer[32];
    double x, y, fit;
    int i;

    // Vertical axis:
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    for (i = 1; i < 10; i++) {
        y = y1 + (y0 - y1) * i / 10.0;
        cairox_paint_line(cr, 1.0, x0-3, y, x1, y);
        cairox_text_parameters_set(&p, x0-4, y, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_CENTER, 0.0);
        if (i % 2 == 1) {
            g_snprintf(buffer, 32, "%4.2f", (i - 5) * 0.5);
            cairox_paint_pango_text(cr, &p, layout, buffer);
        }
    }

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    for (i = 0; i < DATASETS; i++) {
        x = x0 + (x1 - x0) * (i + 0.5) / (double) DATASETS;
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        cairox_paint_pango_text(cr, &p, layout, stat_label[i]);
    }

    rng_scores_convert_to_z(&subject_ds, &subject_ctl, &ds_z);
    global_analysis_draw_data(cr, x0, x1, y0, y1, -1.5, &ds_z, 2);
    global_analysis_draw_key(cr, layout, x1-95, y0+10, "DS", subject_ds.n, 2);

    rng_scores_convert_to_z(&subject_2b, &subject_ctl, &tb_z);
    global_analysis_draw_data(cr, x0, x1, y0, y1, -0.5, &tb_z, 4);
    global_analysis_draw_key(cr, layout, x1-95, y0+30, "2B", subject_2b.n, 4);

    rng_scores_convert_to_z(&subject_gng, &subject_ctl, &gng_z);
    global_analysis_draw_data(cr, x0, x1, y0, y1,  0.5, &gng_z, 3);
    global_analysis_draw_key(cr, layout, x1-95, y0+50, "GnG", subject_gng.n, 3);

    /* Only draw the model data and fits if it has been analysed: */
    if (task_data->group.n > 0) {
        rng_scores_convert_to_z(&(task_data->group), &subject_ctl, &z_scores);
        global_analysis_draw_data(cr, x0, x1, y0, y1,  1.5, &z_scores, 6);
        global_analysis_draw_key(cr, layout, x1-95, y0+70, "Model", task_data->group.n, 6);

        y = y1 - 100;
        fit = rng_data_calculate_fit(&(task_data->group), &subject_ctl);
        cairox_text_parameters_set(&p, x0+10, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        g_snprintf(buffer, 32, "Fit: %5.3f (ctrl)", fit);
        cairox_paint_pango_text(cr, &p, layout, buffer);

        y += 20;
        fit = rng_data_calculate_fit(&(task_data->group), &subject_ds);
        cairox_text_parameters_set(&p, x0+10, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        g_snprintf(buffer, 32, "Fit: %5.3f (DS)", fit);
        cairox_paint_pango_text(cr, &p, layout, buffer);

        y += 20;
        fit = rng_data_calculate_fit(&(task_data->group), &subject_2b);
        cairox_text_parameters_set(&p, x0+10, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        g_snprintf(buffer, 32, "Fit: %5.3f (2B)", fit);
        cairox_paint_pango_text(cr, &p, layout, buffer);

        y += 20;
        fit = rng_data_calculate_fit(&(task_data->group), &subject_gng);
        cairox_text_parameters_set(&p, x0+10, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
        g_snprintf(buffer, 32, "Fit: %5.3f (GNG)", fit);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }

    /* Outside box, and zero horizontal axis: */
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairox_paint_line(cr, 1.0, x0, y0, x1, y0);
    cairox_paint_line(cr, 1.0, x0, y0, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x1, y0);
    cairox_paint_line(cr, 1.0, x0, (y1+y0)*0.5, x1, (y1+y0)*0.5);
}

/*----------------------------------------------------------------------------*/

static void cairo_draw_second_order_bars(cairo_t *cr, double x0, double x1, double y0, double y1, RngGroupData *grp_data, double offset, int colour)
{
    double x, y, dy;
    int k;
    int w = 9;

    for (k = 0; k < 11; k++) {
        x = x0 + (x1 - x0) * (k + 0.5) / 11.0;
        y = y1 + (y0 - y1) * (grp_data->mean.associates[(k+5) % 10] / 0.30);
        if (grp_data->n > 1) {
            dy = (y1 - y0) * (grp_data->sd.associates[(k+5) % 10] / 0.30) / sqrt(grp_data->n); // Convert SD to SE
        }
        else {
            dy = 0;
        }
        draw_bar(cr, x+offset*w, w, y1, y, dy, colour);
    }
}

static void cairo_draw_second_order_analysis(cairo_t *cr, PangoLayout *layout, double x0, double x1, double y0, double y1, OosVars *gv)
{
    CairoxTextParameters p;
    RngData *task_data = (RngData *)gv->task_data;
    char buffer[16];
    double x, y;
    int i;

    cairox_paint_line(cr, 1.0, x0, y0, x1, y0);
    cairox_paint_line(cr, 1.0, x0, y0, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x1, y0);

    // Vertical axis
    pangox_layout_set_font_size(layout, 12);
    for (i = 0; i < 7; i++) {
        y = y1 + (y0 - y1) * i / 6.0;
        cairox_paint_line(cr, 1.0, x0-3, y, x0+3, y);
        cairox_text_parameters_set(&p, x0-4, y, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_CENTER, 0.0);
        g_snprintf(buffer, 16, "%4.2f", i * 0.05);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    y = y1 + (y0 - y1) * 2 / 6.0; // Horizontal line at 0.10
    cairox_paint_line(cr, 1.0, x0, y, x1, y);
    y = y1 + (y0 - y1) * 4 / 6.0; // Horizontal line at 0.20
    cairox_paint_line(cr, 1.0, x0, y, x1, y);

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    for (i = 0; i < 11; i++) {
        x = x0 + (x1 - x0) * (i + 0.5) / 11.0;
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        if (i < 5) {
            g_snprintf(buffer, 16, "-%d", 5 - i);
        }
        else if (i > 5) {
            g_snprintf(buffer, 16, "+%d", i - 5);
        }
        else {
            g_snprintf(buffer, 16, "0");
        }
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }

    cairo_draw_second_order_bars(cr, x0, x1, y0, y1, &subject_ctl, -2, 0);
    global_analysis_draw_key(cr, layout, x1-100, y0+10, "CTRL", subject_ctl.n, 0);
    cairo_draw_second_order_bars(cr, x0, x1, y0, y1, &subject_ds, -1, 2);
    global_analysis_draw_key(cr, layout, x1-100, y0+30, "DS", subject_ds.n, 2);
    cairo_draw_second_order_bars(cr, x0, x1, y0, y1, &subject_2b, 0, 3);
    global_analysis_draw_key(cr, layout, x1-100, y0+50, "2B", subject_2b.n, 3);
    cairo_draw_second_order_bars(cr, x0, x1, y0, y1, &subject_gng, 1, 4);
    global_analysis_draw_key(cr, layout, x1-100, y0+70, "GnG", subject_gng.n, 4);

    if (task_data->group.n > 0) {
        cairo_draw_second_order_bars(cr, x0, x1, y0, y1, &(task_data->group), 2, 6);
        global_analysis_draw_key(cr, layout, x1-100, y0+90, "Model", task_data->group.n, 6);
    }
}

/*----------------------------------------------------------------------------*/

static void cairo_draw_second_order_star(cairo_t *cr, double xc, double yc, double size, RngScores *scores, int j)
{
    double x, y, theta;
    int i;

    i = 0;
    cairox_select_colour(cr, j, 1.0);
    cairo_set_line_width(cr, 2);
    theta = 0;

    x = xc - size * cos(theta) * scores->associates[i] / 0.25;
    y = yc - size * sin(theta) * scores->associates[i] / 0.25;
    cairo_move_to(cr, x, y);
    for (i = 1; i < 10; i++) {
        theta = 2.0 * M_PI * (i / 10.0);
        x = xc - size * cos(theta) * scores->associates[i] / 0.25;
        y = yc - size * sin(theta) * scores->associates[i] / 0.25;
        cairo_line_to(cr, x, y);
    }
    cairo_close_path(cr);
    if (j == 0) {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
        cairo_fill(cr);
    }
    else {
        cairo_stroke(cr);
    }
}

static void cairo_draw_star_chart(cairo_t *cr, PangoLayout *layout, double x0, double x1, double y0, double y1, OosVars *gv)
{
    RngData *task_data = (RngData *)gv->task_data;
    CairoxTextParameters p;
    double xc, yc, x, y;
    double theta, size;
    char buffer[16];
    int i;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_width(cr, 1);

    xc = (x0 + x1) * 0.5;
    yc = (y0 + y1) * 0.5;
    size = MIN(xc, yc);

    /* Draw the axes (i.e., the arms of the star): */
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    for (i = 0; i < 10; i++) {
        theta = 2.0 * M_PI * (i / 10.0);
        x = xc - size * cos(theta);
        y = yc - size * sin(theta);
        cairo_move_to(cr, xc, yc);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
    }

    /* Draw circles to show 0.1, 0.2 from the centre: */

    pangox_layout_set_font_size(layout, 12);
    for (i = 1; i < 3; i++) {
        x = xc + size * i / 2.5;
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        g_snprintf(buffer, 16, "%3.1f", i * 0.1);
        cairox_text_parameters_set(&p, xc - size * i /2.5, yc-1, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
        cairox_paint_pango_text(cr, &p, layout, buffer);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_move_to(cr, x, yc+3);
        cairo_line_to(cr, x, yc-2);
        cairo_stroke(cr);
        cairo_arc(cr, xc, yc, size * i / 2.5, 0, 2*M_PI);
        cairo_stroke(cr);
    }

    /* For each data set, draw the data: */

    pangox_layout_set_font_size(layout, 12);
    cairo_draw_second_order_star(cr, xc, yc, size, &subject_ctl.mean, 0);
    global_analysis_draw_key(cr, layout, x1-120, y0+10, "Control", subject_ctl.n, 1);
// COMMENT OUT THE FOLLOWING SIX LINES TO JUST DRAW CONTROL CONDITION:
    cairo_draw_second_order_star(cr, xc, yc, size, &subject_ds.mean, 2);
    global_analysis_draw_key(cr, layout, x1-120, y0+35, "Digit Switching", subject_ds.n, 2);
    cairo_draw_second_order_star(cr, xc, yc, size, &subject_2b.mean, 4);
    global_analysis_draw_key(cr, layout, x1-120, y0+60, "2-Back", subject_2b.n, 4);
    cairo_draw_second_order_star(cr, xc, yc, size, &subject_gng.mean, 3);
    global_analysis_draw_key(cr, layout, x1-120, y0+85, "Go/No-Go", subject_gng.n, 3);

    if (task_data->group.n > 0) {
        cairo_draw_second_order_star(cr, xc, yc, size, &(task_data->group.mean), 6);
        global_analysis_draw_key(cr, layout, x1-120, y0+110, "Model", task_data->group.n, 6);
    }
}

/*----------------------------------------------------------------------------*/

#define LG_WIDTH 1.0

static void cairo_draw_1d_legend_entry(cairo_t *cr, PangoLayout *layout, int x0, int y0, char *label, int colour)
{
    CairoxTextParameters p;

    cairox_text_parameters_set(&p, x0, y0, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, label);
    cairox_select_colour(cr, colour, 1.0);
    cairox_paint_line(cr, LG_WIDTH, x0-25, y0-5, x0-5, y0-5);
}

static void cairo_draw_varied_data(cairo_t *cr, PangoLayout *layout, double width, double height, XGlobals *globals)
{
    CairoxTextParameters p;
    char buffer[16];
    double x, y, dy;
    int i;

    double x0 = width*0.08;
    double x1 = width*0.95;
    double y0 = GR_MARGIN_UPPER;
    double y1 = height-GR_MARGIN_LOWER;

    // Vertical axis
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    for (i = 1; i < 10; i++) {
        y = y1 + (y0 - y1) * i / 10.0;
        cairox_paint_line(cr, 1.0, x0-3, y, x1, y);
        cairox_text_parameters_set(&p, x0-4, y, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_CENTER, 0.0);
        if (i % 2 == 1) {
            g_snprintf(buffer, 32, "%4.2f", (i - 5) * 0.5);
            cairox_paint_pango_text(cr, &p, layout, buffer);
        }
    }

    // Draw the data for R1: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-600, y0+20, "R", 2);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 2, 1.0);
        // Draw the data for R1: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].r1 / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].r1 / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for R1: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].r1 / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].r1 / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }

#if FALSE
    // Draw the data for R2: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-500, y0+20, "R2", 3);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 3, 1.0);
        // Draw the data for R2: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].r2 / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].r2 / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for R1: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].r2 / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].r2 / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }
#endif

    // Draw the data for RNG: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-500, y0+20, "RNG", 3);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 3, 1.0);
        // Draw the data for RNG: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].rng / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rng / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for RNG: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rng / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].rng / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }

    // Draw the data for RR: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-400, y0+20, "RR", 4);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 4, 1.0);
        // Draw the data for RR: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].rr / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rr / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for RR: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rr / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].rr / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }

    // Draw the data for AA: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-300, y0+20, "AA", 5);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 5, 1.0);
        // Draw the data for AA: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].aa / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].aa / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for AA: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].aa / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].aa / sqrt(6.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }

    // Draw the data for OA: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-200, y0+20, "OA", 6);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 6, 1.0);
        // Draw the data for OA: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].oa / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].oa / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for OA: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].oa / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].oa / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }

    // Draw the data for TPI: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-100, y0+20, "TPI", 7);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 7, 1.0);
        // Draw the data for TPI: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].tpi / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].tpi / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for TPI: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].tpi / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].tpi / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }
#if FALSE
    // Draw the data for RG: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-200, y0+20, "RG (mean)", 4);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 4, 1.0);
        // Draw the data for RG: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].rg1 / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rg1 / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for RG: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rg1 / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].rg1 / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }

    // Draw the data for RG: First the legend...
    cairo_draw_1d_legend_entry(cr, layout, x1-100, y0+20, "RG (median)", 5);
    if (globals->group_index >= 0) {
        cairox_select_colour(cr, 5, 1.0);
        // Draw the data for RG: Then the line...
        x = x0 + 10;
        y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[0].rg2 / 5.00;
        cairo_set_line_width(cr, LG_WIDTH);
        cairo_move_to(cr, x, y);
        for (i = 1; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rg2 / 5.00;
            cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);
        cairo_set_line_width(cr, 1.0);
        // Draw the data for RG: Then the error bars...
        for (i = 0; i < globals->group_index; i++) {
            x = x0 + 10 + (x1 - x0 - 20) *  i / (MAX_GROUPS - 1.0);
            y = (y0 + y1) * 0.5 + (y0 - y1) * globals->group_mean[i].rg2 / 5.00;
            dy = ((y0 - y1) * globals->group_sd[i].rg2 / sqrt(36.0)) / 5.00;
            cairox_paint_line(cr, 1.0, x, y-dy, x, y+dy);
            cairox_paint_line(cr, 1.0, x-3, y-dy, x+3, y-dy);
            cairox_paint_line(cr, 1.0, x-3, y+dy, x+3, y+dy);
        }
    }
#endif

    // White space above and below the graph to crop data within the graph
    if (!globals->running) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_rectangle(cr, 0, 0, width, y0-1);
        cairo_fill(cr);
        cairo_rectangle(cr, 0, y1+1, width, height-y1);
        cairo_fill(cr);
    }

    // Outside box, and zero horizontal axis:
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairox_paint_line(cr, 1.0, x0, y0, x1, y0);
    cairox_paint_line(cr, 1.0, x0, y0, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x1, y0);
    cairox_paint_line(cr, 1.0, x0, (y1+y0)*0.5, x1, (y1+y0)*0.5);

}

/*----------------------------------------------------------------------------*/

static void cairo_draw_vary_decay_axis(cairo_t *cr, PangoLayout *layout, double width, double height, XGlobals *globals)
{
    CairoxTextParameters p;
    char buffer[16];
    double x;
    int i;

    double x0 = width*0.08;
    double x1 = width*0.95;
    double y1 = height-GR_MARGIN_LOWER;

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (i = 0; i < MAX_GROUPS; i += 2) {
        x = x0 + 10 + (x1 - x0 - 20) *  i / (double) (MAX_GROUPS - 1);
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        g_snprintf(buffer, 16, "%d", (int) (DECAY_LEFT + i * DECAY_DELTA));
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, width*0.5, height-GR_MARGIN_LOWER+20, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "WM Decay Time (cycles)");
}

static void cairo_draw_vary_temperature_axis(cairo_t *cr, PangoLayout *layout, double width, double height, XGlobals *globals)
{
    CairoxTextParameters p;
    char buffer[16];
    double x;
    int i;

    double x0 = width*0.08;
    double x1 = width*0.95;
    double y1 = height-GR_MARGIN_LOWER;

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (i = 0; i < MAX_GROUPS; i += 2) {
        x = x0 + 10 + (x1 - x0 - 20) *  i / (double) (MAX_GROUPS - 1);
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        g_snprintf(buffer, 16, "%4.2f", TEMP_LEFT + i * TEMP_DELTA);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, width*0.5, height-GR_MARGIN_LOWER+20, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "Temperature");
}

static void cairo_draw_vary_update_axis(cairo_t *cr, PangoLayout *layout, double width, double height, XGlobals *globals)
{
    CairoxTextParameters p;
    char buffer[16];
    double x;
    int i;

    double x0 = width*0.08;
    double x1 = width*0.95;
    double y1 = height-GR_MARGIN_LOWER;

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (i = 0; i < MAX_GROUPS; i += 2) {
        x = x0 + 10 + (x1 - x0 - 20) *  i / (double) (MAX_GROUPS - 1);
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        g_snprintf(buffer, 16, "%4.2f", UPDATE_LEFT + i * UPDATE_DELTA);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, width*0.5, height-GR_MARGIN_LOWER+20, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "WM Update Efficiency (probability of update)");
}

static void cairo_draw_vary_switch_axis(cairo_t *cr, PangoLayout *layout, double width, double height, XGlobals *globals)
{
    CairoxTextParameters p;
    char buffer[16];
    double x;
    int i;

    double x0 = width*0.08;
    double x1 = width*0.95;
    double y1 = height-GR_MARGIN_LOWER;

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (i = 0; i < MAX_GROUPS; i += 2) {
        x = x0 + 10 + (x1 - x0 - 20) *  i / (double) (MAX_GROUPS - 1);
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        g_snprintf(buffer, 16, "%4.2f", SWITCH_LEFT + i * SWITCH_DELTA);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, width*0.5, height-GR_MARGIN_LOWER+20, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "Switch Rate (probability of switch)");
}

static void cairo_draw_vary_monitoring_axis(cairo_t *cr, PangoLayout *layout, double width, double height, XGlobals *globals)
{
    CairoxTextParameters p;
    char buffer[16];
    double x;
    int i;

    double x0 = width*0.08;
    double x1 = width*0.95;
    double y1 = height-GR_MARGIN_LOWER;

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    for (i = 0; i < MAX_GROUPS; i += 2) {
        x = x0 + 10 + (x1 - x0 - 20) *  i / (double) (MAX_GROUPS - 1);
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        g_snprintf(buffer, 16, "%4.2f", MONITOR_LEFT + i * MONITOR_DELTA);
        cairox_paint_pango_text(cr, &p, layout, buffer);
    }
    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, width*0.5, height-GR_MARGIN_LOWER+20, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "Monitoring Efficiency (probability of monitoring)");
}

/*----------------------------------------------------------------------------*/

static void cairo_draw_results_analysis(cairo_t *cr, XGlobals *globals, int width, int height)
{
    PangoLayout *layout;

    if (globals->running) {
        cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    }
    else {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    }
    cairo_paint(cr);

    layout = pango_cairo_create_layout(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    switch (globals->canvas1_selection) {
        case CANVAS_TABLE: {
            cairo_draw_table(cr, layout, width, height*0.05, height*0.95, globals->gv);
            break;
        }
        case CANVAS_GLOBAL: {
            cairo_draw_global_analysis(cr, layout, width*0.08, width*0.95, GR_MARGIN_UPPER, height-GR_MARGIN_LOWER, globals->gv);
            break;
        }
        case CANVAS_SECOND_ORDER: {
            cairo_draw_second_order_analysis(cr, layout, width*0.08, width*0.95, GR_MARGIN_UPPER, height-GR_MARGIN_LOWER, globals->gv);
            break;
        }
        case CANVAS_STAR: {
            cairo_draw_star_chart(cr, layout, width*0.08, width*0.95, GR_MARGIN_UPPER, height-GR_MARGIN_LOWER, globals->gv);
            break;
        }
    }
    g_object_unref(layout);
}

static void basic_results_canvas_draw(XGlobals *globals)
{
    if (globals->output_panel1 != NULL) {
        cairo_surface_t *s;
        cairo_t *cr;
        int width, height;

        /* Double buffer: First create and draw to a surface: */
        gtk_widget_realize(globals->output_panel1);
        width = globals->output_panel1->allocation.width;
        height = globals->output_panel1->allocation.height;
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create(s);
        cairo_draw_results_analysis(cr, globals, width, height);
        cairo_destroy(cr);

        /* Now copy the surface to the window: */
        cr = gdk_cairo_create(globals->output_panel1->window);
        cairo_set_source_surface(cr, s, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);

        /* And destroy the surface: */
        cairo_surface_destroy(s);
    }
}

static void cairo_draw_1d_parameter_analysis(cairo_t *cr, XGlobals *globals, int width, int height)
{
    PangoLayout *layout;
    CairoxTextParameters p;
    char buffer[256];

    if (globals->running) {
        cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    }
    else {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    }
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    layout = pango_cairo_create_layout(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    cairo_draw_varied_data(cr, layout, width, height, globals);

    switch (globals->canvas2_selection) {
        case CANVAS_VARY_DECAY: {
            cairo_draw_vary_decay_axis(cr, layout, width, height, globals);
            g_snprintf(buffer, 256, "a) Temperature: %4.2f; Switch Rate: %4.2f; Monitoring Efficiency: %4.2f; WM Update Efficiency: %4.2f", globals->params.selection_temperature, globals->params.switch_rate, globals->params.monitoring_efficiency, globals->params.wm_update_efficiency);
            break;
        }
        case CANVAS_VARY_UPDATE: {
            cairo_draw_vary_update_axis(cr, layout, width, height, globals);
            g_snprintf(buffer, 256, "b) WM Decay Time: %d; Temperature: %4.2f; Switch Rate: %4.2f; Monitoring Efficiency: %4.2f", globals->params.wm_decay_rate, globals->params.selection_temperature, globals->params.switch_rate, globals->params.monitoring_efficiency);
            break;
        }
        case CANVAS_VARY_MONITORING: {
            cairo_draw_vary_monitoring_axis(cr, layout, width, height, globals);
            g_snprintf(buffer, 256, "c) WM Decay Time: %d; Temperature: %4.2f; Switch Rate: %4.2f; WM Update Efficiency: %4.2f", globals->params.wm_decay_rate, globals->params.selection_temperature, globals->params.switch_rate, globals->params.wm_update_efficiency);
            break;
        }
        case CANVAS_VARY_SWITCH: {
            cairo_draw_vary_switch_axis(cr, layout, width, height, globals);
            g_snprintf(buffer, 256, "d) WM Decay Time: %d; Temperature: %4.2f; Monitoring Efficiency: %4.2f; WM Update Efficiency: %4.2f", globals->params.wm_decay_rate, globals->params.selection_temperature, globals->params.monitoring_efficiency, globals->params.wm_update_efficiency);
            break;
        }
        case CANVAS_VARY_TEMPERATURE: {
            cairo_draw_vary_temperature_axis(cr, layout, width, height, globals);
            g_snprintf(buffer, 256, "e) WM Decay Time: %d; Switch Rate: %4.2f; Monitoring Efficiency: %4.2f; WM Update Efficiency: %4.2f", globals->params.wm_decay_rate, globals->params.switch_rate, globals->params.monitoring_efficiency, globals->params.wm_update_efficiency);
            break;
        }
    }

    /* And finally place the title on the graph: */
    pangox_layout_set_font_size(layout, 12);
    cairox_text_parameters_set(&p, width*0.5, height-10, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
    g_object_unref(layout);
}

static void x_draw_ps1d_canvas(XGlobals *globals)
{
    if (globals->output_panel2 != NULL) {
        cairo_surface_t *s;
        cairo_t *cr;
        int width, height;

        /* Double buffer: First create and draw to a surface: */
        gtk_widget_realize(globals->output_panel2);
        width = globals->output_panel2->allocation.width;
        height = globals->output_panel2->allocation.height;
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create(s);
        cairo_draw_1d_parameter_analysis(cr, globals, width, height);
        cairo_destroy(cr);

        /* Now copy the surface to the window: */
        cr = gdk_cairo_create(globals->output_panel2->window);
        cairo_set_source_surface(cr, s, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);

        /* And destroy the surface: */
        cairo_surface_destroy(s);
    }
}

static void x_draw_ps1d_canvas_to_pdf(cairo_t *cr, XGlobals *globals, double width, double height, int pos)
{
    cairo_surface_t *s;
    cairo_t *cr2;

    // Create a temporary cairo surface/canvas and draw onto it:
    s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height / 5.0);
    cr2 = cairo_create(s);
    cairo_draw_1d_parameter_analysis(cr2, globals, width, height / 5.0);
    cairo_destroy(cr2);

    // Now stick the drawing in the right bit of the page:
    cairo_set_source_surface(cr, s, 0, (pos - 1) * height / 5.0);
    cairo_paint(cr);

    // And dispose of the surface:
    cairo_surface_destroy(s);
}

/******************************************************************************/

static void x_task_initialise(GtkWidget *caller, XGlobals *globals)
{
    rng_create(globals->gv, &(globals->params));
    rng_initialise_subject(globals->gv);
    basic_results_canvas_draw(globals);
}

static void x_task_run_block(GtkWidget *caller, XGlobals *globals)
{
    globals->running = TRUE;
    basic_results_canvas_draw(globals);
    gtk_main_iteration_do(FALSE);
    rng_create(globals->gv, &(globals->params));
    rng_initialise_subject(globals->gv);
    rng_run(globals->gv);
    rng_analyse_group_data((RngData *)globals->gv->task_data);
    globals->running = FALSE;
    basic_results_canvas_draw(globals);
}

static void set_variable_parameter(XGlobals *globals)
{
    // Note: group_index varies from 0 up to MAX_GROUPS (exclusive)

    if (globals->canvas2_selection == CANVAS_VARY_DECAY) {
        globals->params.wm_decay_rate = DECAY_LEFT + globals->group_index * DECAY_DELTA;
    }
    else if (globals->canvas2_selection == CANVAS_VARY_TEMPERATURE) {
        globals->params.selection_temperature = TEMP_LEFT + globals->group_index * TEMP_DELTA;
    }
    else if (globals->canvas2_selection == CANVAS_VARY_MONITORING) {
        globals->params.monitoring_efficiency = MONITOR_LEFT + globals->group_index * MONITOR_DELTA;
    }
    else if (globals->canvas2_selection == CANVAS_VARY_SWITCH) {
        globals->params.switch_rate = SWITCH_LEFT + globals->group_index * SWITCH_DELTA;
    }
    else if (globals->canvas2_selection == CANVAS_VARY_UPDATE) {
        globals->params.wm_update_efficiency = UPDATE_LEFT + globals->group_index * UPDATE_DELTA;
    }
}

static void rng_parameters_save(RngParameters *source, RngParameters *tmp)
{
    tmp->wm_decay_rate = source->wm_decay_rate;
    tmp->selection_temperature = source->selection_temperature;
    tmp->switch_rate = source->switch_rate;
    tmp->monitoring_efficiency = source->monitoring_efficiency;
    tmp->monitoring_method = source->monitoring_method;
    tmp->wm_update_efficiency = source->wm_update_efficiency;
}

static void rng_parameters_restore(RngParameters *dest, RngParameters *tmp)
{
    dest->wm_decay_rate = tmp->wm_decay_rate;
    dest->selection_temperature = tmp->selection_temperature;
    dest->switch_rate = tmp->switch_rate;
    dest->monitoring_efficiency = tmp->monitoring_efficiency;
    dest->monitoring_method = tmp->monitoring_method;
    dest->wm_update_efficiency = tmp->wm_update_efficiency;
}

static void run_parameter_study(XGlobals *globals)
{
    RngData *task_data;
    RngParameters tmp;

    rng_parameters_save(&(globals->params), &tmp);

    for (globals->group_index = 0; globals->group_index < MAX_GROUPS; globals->group_index++) {
        set_variable_parameter(globals);
        rng_create(globals->gv, &(globals->params));
        rng_initialise_subject(globals->gv);
        rng_run(globals->gv);
        task_data = (RngData *)globals->gv->task_data;
        rng_analyse_group_data(task_data);
        globals->group_mean[globals->group_index].r1 = (task_data->group.mean.r1 - subject_ctl.mean.r1) / subject_ctl.sd.r1;
        globals->group_sd[globals->group_index].r1 = task_data->group.sd.r1 / subject_ctl.sd.r1;
        globals->group_mean[globals->group_index].r2 = (task_data->group.mean.r2 - subject_ctl.mean.r2) / subject_ctl.sd.r2;
        globals->group_sd[globals->group_index].r2 = task_data->group.sd.r2 / subject_ctl.sd.r2;
        globals->group_mean[globals->group_index].rng = (task_data->group.mean.rng - subject_ctl.mean.rng) / subject_ctl.sd.rng;
        globals->group_sd[globals->group_index].rng = task_data->group.sd.rng / subject_ctl.sd.rng;
        globals->group_mean[globals->group_index].rr = (task_data->group.mean.rr - subject_ctl.mean.rr) / subject_ctl.sd.rr;
        globals->group_sd[globals->group_index].rr = task_data->group.sd.rr / subject_ctl.sd.rr;
        globals->group_mean[globals->group_index].aa = (task_data->group.mean.aa - subject_ctl.mean.aa) / subject_ctl.sd.aa;
        globals->group_sd[globals->group_index].aa = task_data->group.sd.aa / subject_ctl.sd.aa;
        globals->group_mean[globals->group_index].oa = (task_data->group.mean.oa - subject_ctl.mean.oa) / subject_ctl.sd.oa;
        globals->group_sd[globals->group_index].oa = task_data->group.sd.oa / subject_ctl.sd.oa;
        globals->group_mean[globals->group_index].tpi = (task_data->group.mean.tpi - subject_ctl.mean.tpi) / subject_ctl.sd.tpi;
        globals->group_sd[globals->group_index].tpi = task_data->group.sd.tpi / subject_ctl.sd.tpi;
        globals->group_mean[globals->group_index].rg1 = (task_data->group.mean.rg1 - subject_ctl.mean.rg1) / subject_ctl.sd.rg1;
        globals->group_sd[globals->group_index].rg1 = task_data->group.sd.rg1 / subject_ctl.sd.rg1;
        globals->group_mean[globals->group_index].rg2 = (task_data->group.mean.rg2 - subject_ctl.mean.rg2) / subject_ctl.sd.rg2;
        globals->group_sd[globals->group_index].rg2 = task_data->group.sd.rg2 / subject_ctl.sd.rg2;
        if (globals->dynamic_canvases) {
            x_draw_ps1d_canvas(globals);
        }
        gtk_main_iteration_do(FALSE);
    }

    rng_parameters_restore(&(globals->params), &tmp);
}

static void x_task_run_parameter_study(GtkWidget *caller, XGlobals *globals)
{
    globals->running = TRUE;
    x_draw_ps1d_canvas(globals);
    gtk_main_iteration_do(FALSE);
    run_parameter_study(globals);
    globals->running = FALSE;
    x_draw_ps1d_canvas(globals);
}

static void x_task_run_all_parameter_studies(GtkWidget *caller, XGlobals *globals)
{
    Canvas2View tmp_v = globals->canvas2_selection;
    Boolean tmp_dc = globals->dynamic_canvases;

    cairo_surface_t *surface;
    cairo_t *cr;
    double width, height;
    char *filename;

    filename = "ScreenDumps/sim2_graphs.pdf";
    width = 1200; height = width * sqrt(2);

    fprintf(stdout, "Saving image to %s ...", filename); fflush(stdout);

    surface = cairo_pdf_surface_create(filename, width, height);
    cr = cairo_create(surface);

    globals->dynamic_canvases = FALSE;
    globals->canvas2_selection = CANVAS_VARY_DECAY;
    run_parameter_study(globals);
    x_draw_ps1d_canvas_to_pdf(cr, globals, width, height, 1);

    globals->canvas2_selection = CANVAS_VARY_UPDATE;
    run_parameter_study(globals);
    x_draw_ps1d_canvas_to_pdf(cr, globals, width, height, 2);

    globals->canvas2_selection = CANVAS_VARY_MONITORING;
    run_parameter_study(globals);
    x_draw_ps1d_canvas_to_pdf(cr, globals, width, height, 3);

    globals->canvas2_selection = CANVAS_VARY_SWITCH;
    run_parameter_study(globals);
    x_draw_ps1d_canvas_to_pdf(cr, globals, width, height, 4);

    globals->canvas2_selection = CANVAS_VARY_TEMPERATURE;
    run_parameter_study(globals);
    x_draw_ps1d_canvas_to_pdf(cr, globals, width, height, 5);

    fprintf(stdout, " Done\n");

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    globals->dynamic_canvases = tmp_dc;
    globals->canvas2_selection = tmp_v;
}

/******************************************************************************/

static int analysis_canvas1_event_expose(GtkWidget *da, GdkEventConfigure *event, XGlobals *globals)
{
    basic_results_canvas_draw(globals);
    return(1);
}

static int analysis_canvas2_event_expose(GtkWidget *da, GdkEventConfigure *event, XGlobals *globals)
{
    x_draw_ps1d_canvas(globals);
    return(1);
}

/*----------------------------------------------------------------------------*/

static void x_spin_strength(GtkWidget *button, double *strength)
{
    *strength = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void x_spin_individual_variability(GtkWidget *button, RngParameters *params)
{
    params->individual_variability = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static gint weight_win_delete(GtkWidget *caller, GdkEvent *event, GtkWidget *weight_win)
{
    gtk_widget_hide(weight_win);
    return(1);    // Return 1 to abort deletion
}

static void x_save_dvs(GtkWidget *caller, XGlobals *globals)
{
    RngData *task_data;
    FILE *fp = stdout; 
    int i;

    task_data = (RngData *)globals->gv->task_data;

    fprintf(fp, "ID\tR\tRNG\tRR\tAR\tOR\tTPI\n");
    for (i = 0; i < globals->params.sample_size; i++) {
        fprintf(fp, "%d\t", i+1);
        fprintf(fp, "%6.4f\t", task_data->subject[i].scores.r1);
        fprintf(fp, "%6.4f\t", task_data->subject[i].scores.rng);
        fprintf(fp, "%6.4f\t", task_data->subject[i].scores.rr);
        fprintf(fp, "%6.4f\t", task_data->subject[i].scores.aa);
        fprintf(fp, "%6.4f\t", task_data->subject[i].scores.oa);
        fprintf(fp, "%6.4f\n", task_data->subject[i].scores.tpi);
    }
}

static void x_show_weights(GtkWidget *caller, XGlobals *globals)
{
    static GtkWidget *weight_win = NULL;

    if (weight_win != NULL) {
        gtk_widget_hide(weight_win);
        gtk_widget_show(weight_win);
    }
    else {
        GtkWidget *page, *hbox, *tmp;
        int i;
    
        weight_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        g_signal_connect(G_OBJECT(weight_win), "delete_event", G_CALLBACK(weight_win_delete), weight_win);

        page = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(weight_win), page);
        gtk_widget_show(page);        

        for (i = 0; i < SCHEMA_SET_SIZE; i++) {
            hbox = gtk_hbox_new(TRUE, 5);
            gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
            gtk_widget_show(hbox);

            tmp = gtk_label_new(slabels[i]);
            gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 0);
            gtk_widget_show(tmp);

            tmp = gtk_spin_button_new_with_range(0.0, 50.0, 0.05);
            gtk_spin_button_set_digits(GTK_SPIN_BUTTON(tmp), 2);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), strength[i]);
            g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_strength), &strength[i]);
            gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 0);
            gtk_widget_show(tmp);
	}

        hbox = gtk_hbox_new(TRUE, 5);
        gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
        gtk_widget_show(hbox);

        tmp = gtk_label_new("I.D.:");
        gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);

        tmp = gtk_spin_button_new_with_range(0.0, 10.0, 0.10);
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(tmp), 2);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), globals->params.individual_variability);
        g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_individual_variability), &(globals->params));
        gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);

        gtk_widget_show(weight_win);
    }
}

#if FALSE

static void x_toggle_dynamic_display(GtkWidget *caller, XGlobals *globals)
{
    globals->dynamic_canvases = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller));
}

static void x_toggle_colour(GtkWidget *caller, XGlobals *globals)
{
    globals->colour = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller));
    basic_results_canvas_draw(globals);
}

#endif

static void radio_select_table(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        globals->canvas1_selection = CANVAS_TABLE;
        basic_results_canvas_draw(globals);
    }
}

static void radio_select_global_graph(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        globals->canvas1_selection = CANVAS_GLOBAL;
        basic_results_canvas_draw(globals);
    }
}

static void radio_select_2nd_order_graph(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        globals->canvas1_selection = CANVAS_SECOND_ORDER;
        basic_results_canvas_draw(globals);
    }
}

static void radio_select_2nd_order_star(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        globals->canvas1_selection = CANVAS_STAR;
        basic_results_canvas_draw(globals);
    }
}

static void x_spin_wm_decay(GtkWidget *button, RngParameters *params)
{
    params->wm_decay_rate = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void x_spin_selection_temperature(GtkWidget *button, RngParameters *params)
{
    params->selection_temperature = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void x_spin_switch_rate(GtkWidget *button, RngParameters *params)
{
    params->switch_rate = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void x_spin_monitoring_efficiency(GtkWidget *button, RngParameters *params)
{
    params->monitoring_efficiency = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void x_spin_update_efficiency(GtkWidget *button, RngParameters *params)
{
    params->wm_update_efficiency = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void x_set_monitoring_method(GtkWidget *button, RngParameters *params)
{
    params->monitoring_method = gtk_combo_box_get_active(GTK_COMBO_BOX(button));
}

static void x_spin_sample_size(GtkWidget *button, RngParameters *params)
{
    params->sample_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

/*----------------------------------------------------------------------------*/

static gint event_delete(GtkWidget *caller, GdkEvent *event, XGlobals *globals)
{
    return(0);    // Return 1 if we need to abort
}

/*----------------------------------------------------------------------------*/

static void callback_destroy(GtkWidget *caller, XGlobals *globals)
{
    gtk_main_quit();
}

/*----------------------------------------------------------------------------*/

static GtkWidget *notepage_basic_results_create(XGlobals *globals)
{
    GtkWidget *page, *hbox, *tmp, *vbox, *align;
    GtkToolItem *tool_item;
    GtkTooltips *tooltips = gtk_tooltips_new();
    GSList *radio_group = NULL;

    page = gtk_vbox_new(FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Parameters:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST); // Run Block
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_run_block), globals);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run 36 virtual subjects", NULL);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_FIRST); // Initialise
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_initialise), globals);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Re-initialise", NULL);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tmp = gtk_spin_button_new_with_range(0, MAX_SUBJECTS, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), 36);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_sample_size), &(globals->params));
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new("Sample Size:");
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    /* Padding on the left: */

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    /* Column 1: */

    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("WM decay rate:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("WM update efficiency:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Selection temperature:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    /* Column 2: */
    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    tmp = gtk_spin_button_new_with_range(1.0, 500.0, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), globals->params.wm_decay_rate);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_wm_decay), &(globals->params));
    gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), globals->params.wm_update_efficiency);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_update_efficiency), &(globals->params));
    gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.0, 10.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), globals->params.selection_temperature);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_selection_temperature), &(globals->params));
    gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    /* Central padding: */

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    /* Column 3: */

    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Switch rate:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Monitoring efficiency:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Monitoring method:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    /* Column 4: */
    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), globals->params.switch_rate);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_switch_rate), &(globals->params));
    gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), globals->params.monitoring_efficiency);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_monitoring_efficiency), &(globals->params));
    gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    tmp = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Simple");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Check Gaps");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Alternate Direction");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), globals->params.monitoring_method);
    g_signal_connect(G_OBJECT(tmp), "changed", G_CALLBACK(x_set_monitoring_method), &(globals->params));
    gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    /* Padding on the right: */

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    /* The control panel: */

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("View:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_radio_button_new_with_label(radio_group, "Tablulated DVs");
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(radio_select_table), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (globals->canvas1_selection == CANVAS_TABLE));
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(tmp));
    gtk_widget_show(tmp);

    tmp = gtk_radio_button_new_with_label(radio_group, "Primary DV Graph");
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(radio_select_global_graph), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (globals->canvas1_selection == CANVAS_GLOBAL));
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(tmp));
    gtk_widget_show(tmp);

    tmp = gtk_radio_button_new_with_label(radio_group, "2nd Order Bar Chart");
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(radio_select_2nd_order_graph), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (globals->canvas1_selection == CANVAS_SECOND_ORDER));
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(tmp));
    gtk_widget_show(tmp);

    tmp = gtk_radio_button_new_with_label(radio_group, "2nd Order Star Chart");
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(radio_select_2nd_order_star), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (globals->canvas1_selection == CANVAS_STAR));
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(tmp));
    gtk_widget_show(tmp);
#if FALSE
    tmp = gtk_check_button_new_with_label("Dynamic display");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), globals->dynamic_canvases);
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(x_toggle_dynamic_display), globals);
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_check_button_new_with_label("Colour");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), globals->colour);
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(x_toggle_colour), globals);
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);
#endif

    tmp = gtk_button_new_with_label(" Weights ");
    g_signal_connect(G_OBJECT(tmp), "clicked", G_CALLBACK(x_show_weights), globals);
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_button_new_with_label(" Dump DVs ");
    g_signal_connect(G_OBJECT(tmp), "clicked", G_CALLBACK(x_save_dvs), globals);
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    /* A separator for aesthetic reasons: */

    tmp = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(page), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* The output panel: */

    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(analysis_canvas1_event_expose), globals);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    globals->output_panel1 = tmp;

    gtk_widget_show(page);
    return(page);
}

/******************************************************************************/

static void set_canvas2_selection(XGlobals *globals, Canvas2View view)
{
    if (globals->canvas2_selection != view) {
        globals->canvas2_selection = view;
        globals->group_index = -1;
        x_draw_ps1d_canvas(globals);
    }
}

static void radio_select_wm_decay_rate(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        set_canvas2_selection(globals, CANVAS_VARY_DECAY);
    }
}

static void radio_select_temperature(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        set_canvas2_selection(globals, CANVAS_VARY_TEMPERATURE);
    }
}

static void radio_select_switch_rate(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        set_canvas2_selection(globals, CANVAS_VARY_SWITCH);
    }
}

static void radio_select_monitoring_efficiency(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        set_canvas2_selection(globals, CANVAS_VARY_MONITORING);
    }
}

static void radio_select_wm_update_efficiency(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        set_canvas2_selection(globals, CANVAS_VARY_UPDATE);
    }
}

static GtkWidget *notepage_parameter_studies_1d_create(XGlobals *globals)
{
    GtkWidget *page, *button, *hbox, *tmp;
    GtkToolItem *tool_item;
    GtkTooltips *tooltips;
    GSList *radio_group = NULL;

    page = gtk_vbox_new(FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Uni-dimensional Parameter study:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tooltips = gtk_tooltips_new();

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST); // Run study
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_run_parameter_study), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run full parameter study (36 subjects at each value of the parameter)", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_PRINT); // Generate PDF for all studies
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_run_all_parameter_studies), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run all parameter studies (36 subjects at each value of each parameter)", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    button = gtk_radio_button_new_with_label(radio_group, "WM decay rate");
    g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(radio_select_wm_decay_rate), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), (globals->canvas2_selection == CANVAS_VARY_DECAY));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    gtk_widget_show(button);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    button = gtk_radio_button_new_with_label(radio_group, "WM update efficiency");
    g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(radio_select_wm_update_efficiency), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), (globals->canvas2_selection == CANVAS_VARY_UPDATE));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    gtk_widget_show(button);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    button = gtk_radio_button_new_with_label(radio_group, "Monitoring efficiency");
    g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(radio_select_monitoring_efficiency), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), (globals->canvas2_selection == CANVAS_VARY_MONITORING));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    gtk_widget_show(button);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    button = gtk_radio_button_new_with_label(radio_group, "Selection temperature");
    g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(radio_select_temperature), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), (globals->canvas2_selection == CANVAS_VARY_TEMPERATURE));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    gtk_widget_show(button);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    button = gtk_radio_button_new_with_label(radio_group, "Switch rate");
    g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(radio_select_switch_rate), globals);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), (globals->canvas2_selection == CANVAS_VARY_SWITCH));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);
    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    gtk_widget_show(button);

    /* A separator for aesthetic reasons: */

    tmp = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(page), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* The output panel: */

    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(analysis_canvas2_event_expose), globals);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    globals->output_panel2 = tmp;

    gtk_widget_show(page);
    return(page);
}

/******************************************************************************/

#define PS_STEPS 10

typedef struct ps_2d_data {
    int variable_1;
    int variable_2;
    RngParameters parameters;
    OosVars *gv;
    GtkWidget *canvas;
    double scale;
    double pixels[DATASETS][PS_STEPS][PS_STEPS];
} PS_2D_Data;

static PS_2D_Data ps_2d = {0, 1, {20, 1.00, 10.0, 0.50, 0, 1.00, 0.50, 36}, NULL, NULL, 0.5, {}};

static void ps_2d_initialise_data(PS_2D_Data *ps_2d)
{
    int i, j, k;

    for (i = 0; i < PS_STEPS; i++) {
        for (j = 0; j < PS_STEPS; j++) {
            for (k = 0; k < DATASETS; k++) {
                ps_2d->pixels[k][i][j] = 0.0;
            }
        }
    }
    if (ps_2d->gv == NULL) {
        ps_2d->gv = oos_globals_create();
    }
}

static void ps_2d_parameters_set(RngParameters *parameters, int pid, double val)
{
    if (pid == 0) {
        parameters->wm_decay_rate = 10 + val * 80; /* Range 10 to 90 */
    }
    else if (pid == 1) {
        parameters->switch_rate = val; /* Range 0 to 1 */
    }
    else if (pid == 2) {
        parameters->monitoring_efficiency = val; /* Range 0 to 1 */
    }
    else if (pid == 3) {
        parameters->wm_update_efficiency = val; /* Range 0 to 1 */
    }
}

#define PS2D_SCALE 2.5

char *ps_2d_axis_label[4] = {
    "WM Decay Rate",
    "Switch Rate",
    "Monitoring Efficiency",
    "WM Update Efficiency"
};

static void cairo_draw_ps_2d_fit(cairo_t *cr, PangoLayout *layout, double x0, double y0, PS_2D_Data *ps_2d, char *label, double pixels[PS_STEPS][PS_STEPS])
{
    CairoxTextParameters p;
    double x, y, dx, dy, c;
    char buffer[64];
    int i, j;

    dx = 100 / (double) PS_STEPS;
    dy = -100 / (double) PS_STEPS;

    for (i = 0; i < PS_STEPS; i++) {
        x = x0 + 100 * i / (double) PS_STEPS;
        for (j = 0; j < PS_STEPS; j++) {
            y = y0 + 100 - 100 * j / (double) PS_STEPS;

            // Pixels code difference in z scores, so a zero pixel should map to colour 0.5
            // Assume that z-score differences range from -2.5 to +2.5;
            c = pixels[i][j] / (2.0 * ps_2d->scale) + 0.5;

            cairox_select_colour_scale(cr, c);
            cairo_rectangle(cr, x, y, dx, dy);
            cairo_fill(cr);
        }
    }

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, x0, y0, 100, 100);
    cairo_stroke(cr);

    pangox_layout_set_font_size(layout, 11);
    cairox_text_parameters_set(&p, x0+50, y0+103, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
    g_snprintf(buffer, 64, "%s", ps_2d_axis_label[ps_2d->variable_1]);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    pangox_layout_set_font_size(layout, 11);
    cairox_text_parameters_set(&p, x0-3, y0+50, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 90.0);
    g_snprintf(buffer, 64, "%s", ps_2d_axis_label[ps_2d->variable_2]);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, x0+50, y0-4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, label);
}

#if FALSE
static void cairo_draw_ps_2d_legend(cairo_t *cr, PangoLayout *layout, PS_2D_Data *ps_2d, double x0, double y0)
{
    CairoxTextParameters p;
    char buffer[64];
    double x, y;
    int i;

    for (i = 0; i < 21; i++) {
        x = x0 + (140 * i / 21.0);
        y = y0 + 2;

        cairox_select_colour_scale(cr, i / 20.0);
        cairo_rectangle(cr, x, y, 7, 10);
        cairo_fill(cr);
    }

    y = y0;
    x = x0 + (140 *  0 / 21.0);
    cairox_text_parameters_set(&p, x, y, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
    g_snprintf(buffer, 64, "-%3.1f", ps_2d->scale);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    x = x0 + (140 * 10.5 / 21.0);
    cairox_text_parameters_set(&p, x, y, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, " 0.0");

    x = x0 + (140 * 21 / 21.0);
    cairox_text_parameters_set(&p, x, y, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_BOTTOM, 0.0);
    g_snprintf(buffer, 64, "+%3.1f", ps_2d->scale);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    y = y0 + 30;
    cairox_text_parameters_set(&p, x0, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_TOP, 0.0);
    g_snprintf(buffer, 64, "%s: %d - %d", ps_2d_axis_label[0], 10, 90);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    y += 20;
    cairox_text_parameters_set(&p, x0, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_TOP, 0.0);
    g_snprintf(buffer, 64, "%s: %d - %d", ps_2d_axis_label[1], 0, 50);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    y += 20;
    cairox_text_parameters_set(&p, x0, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_TOP, 0.0);
    g_snprintf(buffer, 64, "%s: %3.1f - %3.1f", ps_2d_axis_label[2], 0.0, 1.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);

    y += 20;
    cairox_text_parameters_set(&p, x0, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_TOP, 0.0);
    g_snprintf(buffer, 64, "%s: %3.1f - %3.1f", ps_2d_axis_label[3], 0.0, 1.0);
    cairox_paint_pango_text(cr, &p, layout, buffer);
}
#endif

static void cairo_draw_ps_2d(cairo_t *cr, PS_2D_Data *ps_2d, double width, double height)
{
    PangoLayout *layout;
    double x0 = (width - 700) / 2.0;
    CairoxTextParameters p;

    /* Clear the canvas: */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    layout = pango_cairo_create_layout(cr);
    pangox_layout_set_font_size(layout, 14);

    /* Draw each surface: */

    cairo_draw_ps_2d_fit(cr, layout,  x0, 50, ps_2d, stat_label[0], ps_2d->pixels[0]);
    cairo_draw_ps_2d_fit(cr, layout, 200+x0, 50, ps_2d, stat_label[1], ps_2d->pixels[1]);
    cairo_draw_ps_2d_fit(cr, layout, 400+x0, 50, ps_2d, stat_label[2], ps_2d->pixels[2]);
    cairo_draw_ps_2d_fit(cr, layout, 600+x0, 50, ps_2d, stat_label[3], ps_2d->pixels[3]);
    cairo_draw_ps_2d_fit(cr, layout, 0+x0, 200, ps_2d, stat_label[4], ps_2d->pixels[4]);
    cairo_draw_ps_2d_fit(cr, layout, 200+x0, 200, ps_2d, stat_label[5], ps_2d->pixels[5]);
//    cairo_draw_ps_2d_fit(cr, layout, 400+x0, 200, ps_2d, stat_label[6], ps_2d->pixels[6]);
//    cairo_draw_ps_2d_fit(cr, layout, 600+x0, 200, ps_2d, stat_label[7], ps_2d->pixels[7]);

//    pangox_layout_set_font_size(layout, 12);
//    cairo_draw_ps_2d_legend(cr, layout, ps_2d, x0, 200);

    pangox_layout_set_font_size(layout, 14);
    cairox_text_parameters_set(&p, width-5, height-5, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_BOTTOM, 0.0);
    cairox_paint_pango_text(cr, &p, layout, "36 repetitions per point");

    g_object_unref(layout);
}

static void ps_2d_canvas_draw(PS_2D_Data *ps_2d)
{
    cairo_surface_t *s;
    cairo_t *cr;
    int width, height;

    width = ps_2d->canvas->allocation.width;
    height = ps_2d->canvas->allocation.height;
    s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(s);
    cairo_draw_ps_2d(cr, ps_2d, width, height);
    cairo_destroy(cr);

    /* Now copy the surface to the window: */
    cr = gdk_cairo_create(ps_2d->canvas->window);
    cairo_set_source_surface(cr, s, 0.0, 0.0);
    cairo_paint(cr);
    cairo_destroy(cr);

    /* And destroy the surface: */
    cairo_surface_destroy(s);
}

static void x_task_ps_2d_initialise(GtkWidget *caller, PS_2D_Data *ps_2d)
{
    ps_2d_initialise_data(ps_2d);
    ps_2d_canvas_draw(ps_2d);
}

static void x_task_ps_2d_run(GtkWidget *caller, PS_2D_Data *ps_2d)
{
    RngParameters tmp;
    RngData *task_data;
    int i, j;

    rng_parameters_save(&(ps_2d->parameters), &tmp);
    ps_2d_initialise_data(ps_2d);

    for (i = 0; i < PS_STEPS; i++) {
        ps_2d_parameters_set(&(ps_2d->parameters), ps_2d->variable_1, (i + 0.5) / (double) PS_STEPS);
        for (j = 0; j < PS_STEPS; j++) {
            ps_2d_parameters_set(&(ps_2d->parameters), ps_2d->variable_2, (j + 0.5) / (double) PS_STEPS);
            rng_create(ps_2d->gv, &(ps_2d->parameters));
            rng_initialise_subject(ps_2d->gv);
            rng_run(ps_2d->gv);
            rng_analyse_group_data((RngData *)ps_2d->gv->task_data);

            task_data = (RngData *)ps_2d->gv->task_data;

            ps_2d->pixels[0][i][j] = (task_data->group.mean.r1 - subject_ctl.mean.r1) / subject_ctl.sd.r1;
//            ps_2d->pixels[1][i][j] = (task_data->group.mean.r2 - subject_ctl.mean.r2) / subject_ctl.sd.r2;
            ps_2d->pixels[1][i][j] = (task_data->group.mean.rng - subject_ctl.mean.rng) / subject_ctl.sd.rng;
            ps_2d->pixels[2][i][j] = (task_data->group.mean.rr - subject_ctl.mean.rr) / subject_ctl.sd.rr;
            ps_2d->pixels[3][i][j] = (task_data->group.mean.aa - subject_ctl.mean.aa) / subject_ctl.sd.aa;
            ps_2d->pixels[4][i][j] = (task_data->group.mean.oa - subject_ctl.mean.oa) / subject_ctl.sd.oa;
            ps_2d->pixels[5][i][j] = (task_data->group.mean.tpi - subject_ctl.mean.tpi) / subject_ctl.sd.tpi;
//            ps_2d->pixels[7][i][j] = (task_data->group.mean.rg1 - subject_ctl.mean.rg1) / subject_ctl.sd.rg1;
            ps_2d_canvas_draw(ps_2d);
        }
    }
    rng_parameters_restore(&(ps_2d->parameters), &tmp);
}

static int canvas_2d_event_expose(GtkWidget *da, GdkEventConfigure *event, PS_2D_Data *ps_2d)
{
    if (ps_2d->canvas != NULL) {
        ps_2d_canvas_draw(ps_2d);
    }
    return(1);
}

static void x_set_2d_variable_1(GtkWidget *button, PS_2D_Data *ps_2d)
{
    ps_2d->variable_1 = gtk_combo_box_get_active(GTK_COMBO_BOX(button));
    ps_2d_canvas_draw(ps_2d);
}

static void x_set_2d_variable_2(GtkWidget *button, PS_2D_Data *ps_2d)
{
    ps_2d->variable_2 = gtk_combo_box_get_active(GTK_COMBO_BOX(button));
    ps_2d_canvas_draw(ps_2d);
}

static void ps2d_spin_scale(GtkWidget *button, PS_2D_Data *ps_2d)
{
    ps_2d->scale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
    ps_2d_canvas_draw(ps_2d);
}

static GtkWidget *notepage_parameter_studies_2d_create(XGlobals *globals)
{
    GtkWidget *page, *hbox, *tmp;
    GtkToolItem *tool_item;
    GtkTooltips *tooltips;

    page = gtk_vbox_new(FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Bi-dimensional Parameter study:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tooltips = gtk_tooltips_new();

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST); // Run study
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_ps_2d_run), &ps_2d);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run full parameter study (36 subjects at each value of each parameter)", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_FIRST); // Initialise
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_ps_2d_initialise), &ps_2d);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Re-initialise", NULL);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_widget_show(GTK_WIDGET(tool_item));

    /* Defaults: */

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("WM decay rate:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(1.0, 500.0, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), ps_2d.parameters.wm_decay_rate);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_wm_decay), &ps_2d.parameters);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new(" "); // Padding
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new("Switch selection direction:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Simple");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Check Gaps");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Alternate Direction");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), ps_2d.parameters.monitoring_method);
    g_signal_connect(G_OBJECT(tmp), "changed", G_CALLBACK(x_set_monitoring_method), &(ps_2d.parameters));
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Switch rate:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), ps_2d.parameters.switch_rate);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_switch_rate), &ps_2d.parameters);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Monitoring efficiency:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), ps_2d.parameters.monitoring_efficiency);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_monitoring_efficiency), &ps_2d.parameters);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new(" "); // Padding
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new("WM update efficiency:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), ps_2d.parameters.wm_update_efficiency);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_update_efficiency), &ps_2d.parameters);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    /* Parameters to be varied: */

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Horizontal axis:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "WM decay rate");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Switch rate");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Monitoring efficiency");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "WM update efficiency");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), ps_2d.variable_1);
    g_signal_connect(G_OBJECT(tmp), "changed", G_CALLBACK(x_set_2d_variable_1), &ps_2d);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new(" "); // Padding
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new("Vertical axis:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "WM decay rate");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Switch rate");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "Monitoring efficiency");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tmp), "WM update efficiency");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), ps_2d.variable_2);
    g_signal_connect(G_OBJECT(tmp), "changed", G_CALLBACK(x_set_2d_variable_2), &ps_2d);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new(" "); // Padding
    gtk_box_pack_start(GTK_BOX(hbox), tmp, TRUE, TRUE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_label_new("Scale:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tmp = gtk_spin_button_new_with_range(0.1, 5.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), ps_2d.scale);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(ps2d_spin_scale), &ps_2d);
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    /* A separator for aesthetic reasons: */

    tmp = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(page), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* The output panel: */

    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(canvas_2d_event_expose), &ps_2d);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    ps_2d.canvas = tmp;
    ps_2d_initialise_data(&ps_2d);

    gtk_widget_show(page);
    return(page);
}

/******************************************************************************/

static int qjep_count = 0;
static int qjep_subjects = 36;
static GtkWidget *qjep_canvas = NULL;
static RngData qjep_data[4];
static RngGroupData human_z[4];
static int qjep_view = 0; // Initially show observed data

static void qjep_load_human_data()
{
    rng_scores_convert_to_z(&subject_ctl, &subject_ctl, &human_z[0]);
    rng_scores_convert_to_z(&subject_ds, &subject_ctl, &human_z[1]);
    rng_scores_convert_to_z(&subject_2b, &subject_ctl, &human_z[2]);
    rng_scores_convert_to_z(&subject_gng, &subject_ctl, &human_z[3]);
}

static void cairo_paint_parameter_values(cairo_t *cr, PangoLayout *layout, char *label, RngParameters *params, double x, double y)
{
    CairoxTextParameters p;
    char buffer[128];

    cairox_text_parameters_set(&p, x, y, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
    g_snprintf(buffer, 128, "%s: %d, %4.2f, %4.2f, %4.2f, %4.2f", label, params->wm_decay_rate, params->selection_temperature, params->switch_rate, params->monitoring_efficiency, params->wm_update_efficiency);
    cairox_paint_pango_text(cr, &p, layout, buffer);
}

void rng_set_parameters(RngData *data, int wm_decay, double wm_update_efficiency, double selection_temperature, double switch_rate, int monitoring_method, double monitoring_efficiency, double individual_variability)
{
    data->params.wm_decay_rate = wm_decay;
    data->params.wm_update_efficiency = wm_update_efficiency;
    data->params.selection_temperature = selection_temperature;
    data->params.switch_rate = switch_rate;
    data->params.monitoring_method = monitoring_method;
    data->params.monitoring_efficiency = monitoring_efficiency;
    data->params.individual_variability = individual_variability;
    data->params.sample_size = 36;
}

static void cairo_draw_qjep_analysis(cairo_t *cr, XGlobals *globals, double width, double height)
{
    RngGroupData qjep_z[4];
    CairoxTextParameters p;
    double x0, x1, y0, y1;
    PangoLayout *layout;
    char buffer[32];
    double x, y;
    int i;

    x0 = width*0.08;
    x1 = width*0.95;
    y0 = GR_MARGIN_UPPER;
    y1 = height-GR_MARGIN_LOWER;

    if (globals->running) {
        cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    }
    else {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    }
    cairo_paint(cr);

    layout = pango_cairo_create_layout(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    // Vertical axis:
    pangox_layout_set_font_size(layout, 12);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    for (i = 1; i < 10; i++) {
        y = y1 + (y0 - y1) * i / 10.0;
        cairox_paint_line(cr, 1.0, x0-3, y, x1, y);
        cairox_text_parameters_set(&p, x0-4, y, PANGOX_XALIGN_RIGHT, PANGOX_YALIGN_CENTER, 0.0);
        if (i % 2 == 1) {
            g_snprintf(buffer, 32, "%4.2f", (i - 5) * 0.5);
            cairox_paint_pango_text(cr, &p, layout, buffer);
        }
    }

    // Horizontal axis
    pangox_layout_set_font_size(layout, 12);
    for (i = 0; i < DATASETS; i++) {
        x = x0 + (x1 - x0) * (i + 0.5) / (double) DATASETS;
        cairox_paint_line(cr, 1.0, x, y1-3, x, y1+3);
        cairox_text_parameters_set(&p, x, y1+4, PANGOX_XALIGN_CENTER, PANGOX_YALIGN_TOP, 0.0);
        cairox_paint_pango_text(cr, &p, layout, stat_label[i]);
    }

    if (qjep_view == 0) {
        /* Draw the data from the real human subjects: */
        global_analysis_draw_data(cr, x0, x1, y0, y1, -1.5, &human_z[1], 2);
        global_analysis_draw_key(cr, layout, x1-100, y0+10, "DS", human_z[1].n, 2);

        global_analysis_draw_data(cr, x0, x1, y0, y1, -0.5, &human_z[2], 4);
        global_analysis_draw_key(cr, layout, x1-100, y0+30, "2B", human_z[2].n, 4);

        global_analysis_draw_data(cr, x0, x1, y0, y1,  0.5, &human_z[3], 3);
        global_analysis_draw_key(cr, layout, x1-100, y0+50, "GnG", human_z[3].n, 3);
    }
    else if (qjep_count > 1) {
        /* Only draw the task data if we have enough subjects to calculate SD: */
        rng_scores_convert_to_z(&(qjep_data[1].group), &subject_ctl, &qjep_z[1]);
//        rng_scores_convert_to_z(&(qjep_data[1].group), &(qjep_data[0].group), &qjep_z[1]);
        global_analysis_draw_data(cr, x0, x1, y0, y1, -1.5, &qjep_z[1], 2);
        global_analysis_draw_key(cr, layout, x1-100, y0+10, "DS", qjep_z[1].n, 2);

        rng_scores_convert_to_z(&(qjep_data[2].group), &subject_ctl, &qjep_z[2]);
//        rng_scores_convert_to_z(&(qjep_data[2].group), &(qjep_data[0].group), &qjep_z[2]);
        global_analysis_draw_data(cr, x0, x1, y0, y1, -0.5, &qjep_z[2], 4);
        global_analysis_draw_key(cr, layout, x1-100, y0+30, "2B", qjep_z[2].n, 4);

        rng_scores_convert_to_z(&(qjep_data[3].group), &subject_ctl, &qjep_z[3]);
//        rng_scores_convert_to_z(&(qjep_data[3].group), &(qjep_data[0].group), &qjep_z[3]);
        global_analysis_draw_data(cr, x0, x1, y0, y1,  0.5, &qjep_z[3], 3);
        global_analysis_draw_key(cr, layout, x1-100, y0+50, "GnG", qjep_z[3].n, 3);
    }

    if (qjep_view == 1) {
        /* Draw the parameter values: */
        cairo_paint_parameter_values(cr, layout, "DS", &(qjep_data[1].params), x0 + 10, y1-46);
        cairo_paint_parameter_values(cr, layout, "2B", &(qjep_data[2].params), x0 + 10, y1-28);
        cairo_paint_parameter_values(cr, layout, "GnG", &(qjep_data[3].params), x0 + 10, y1-10);
    }

    /* Outside box, and zero horizontal axis: */
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairox_paint_line(cr, 1.0, x0, y0, x1, y0);
    cairox_paint_line(cr, 1.0, x0, y0, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x0, y1);
    cairox_paint_line(cr, 1.0, x1, y1, x1, y0);
    cairox_paint_line(cr, 1.0, x0, (y1+y0)*0.5, x1, (y1+y0)*0.5);

    g_object_unref(layout);
}

static void qjep_spin_subjects(GtkWidget *button, void *dummy)
{
    qjep_subjects = (int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
}

static void rng_save_dvs(RngSubjectData *source, RngSubjectData *target)
{
    int i;

    target->n = source->n;
    for (i = 0; i < MAX_TRIALS; i++) {
        target->response[i] = source->response[i];
    }
    target->scores.r1 = source->scores.r1;
    target->scores.r2 = source->scores.r2;
    target->scores.rng = source->scores.rng;
    target->scores.rr = source->scores.rr;
    target->scores.aa = source->scores.aa;
    target->scores.oa = source->scores.oa;
    target->scores.tpi = source->scores.tpi;
    target->scores.rg1 = source->scores.rg1;
    target->scores.rg2 = source->scores.rg2;
    for (i = 0; i < 10; i++) {
        target->scores.associates[i] = source->scores.associates[i];
    }
}

static void qjep_draw_canvas(XGlobals *globals)
{
    if (qjep_canvas != NULL) {
        cairo_surface_t *s;
        cairo_t *cr;
        int width, height;

        /* Double buffer: First create and draw to a surface: */
        gtk_widget_realize(qjep_canvas);
        width = qjep_canvas->allocation.width;
        height = qjep_canvas->allocation.height;
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create(s);
        cairo_draw_qjep_analysis(cr, globals, width, height);
        cairo_destroy(cr);

        /* Now copy the surface to the window: */
        cr = gdk_cairo_create(qjep_canvas->window);
        cairo_set_source_surface(cr, s, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);

        /* And destroy the surface: */
        cairo_surface_destroy(s);
    }
}

static void qjep_initialise(GtkWidget *caller, XGlobals *globals)
{
    qjep_count = 0;
    qjep_draw_canvas(globals);
}

static void qjep_run_block(GtkWidget *caller, XGlobals *globals)
{
    int i, tmp;

    tmp = globals->gv->subjects_per_experiment;
    globals->gv->subjects_per_experiment = 1;

    globals->running = TRUE;
    qjep_draw_canvas(globals);
    gtk_main_iteration_do(FALSE);

    while (qjep_count < qjep_subjects) {
        // Note qjep_data[0] was for simulated control data - use real control
        // data instead
        for (i = 1; i < 4; i++) {
            rng_create(globals->gv, &(qjep_data[i].params));
            rng_initialise_subject(globals->gv);
            rng_run(globals->gv);
            // Subjects responses will already be scored, so we just need to record them:
            rng_save_dvs(&(((RngData *)globals->gv->task_data)->subject[0]), &qjep_data[i].subject[qjep_count]);
            // Now (re-)calculate the group statistics
            qjep_data[i].group.n = (qjep_count + 1);
            rng_analyse_group_data(&qjep_data[i]);
	}
        qjep_count++;
        qjep_draw_canvas(globals);
    }
    globals->running = FALSE;
    qjep_draw_canvas(globals);

    globals->gv->subjects_per_experiment = tmp;
}

static int analysis_qjep_event_expose(GtkWidget *da, GdkEventConfigure *event, XGlobals *globals)
{
    qjep_draw_canvas(globals);
    return(1);
}


static void qjep_radio_select_observed(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        qjep_view = 0;
        qjep_draw_canvas(globals);
    }
}

static void qjep_radio_select_simulated(GtkWidget *caller, XGlobals *globals)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(caller))) {
        qjep_view = 1;
        qjep_draw_canvas(globals);
    }
}

static GtkWidget *notepage_qjep_simulation_create(XGlobals *globals)
{
    GtkWidget *page, *hbox, *tmp, *vbox, *align;
    GtkToolItem *tool_item;
    GtkTooltips *tooltips;
    int i;

// Parameters are:
//    int wm_decay
//    double wm_update_efficiency
//    double selection_temperature
//    double switch_rate
//    int monitoring_method
//    double monitoring_efficiency
//    double individual_variability

    rng_set_parameters(&qjep_data[0], 30, 1.00, 0.88, 1.00, 0, 0.55, 0.50);
    rng_set_parameters(&qjep_data[1], 30, 1.00, 0.48, 0.60, 0, 0.00, 0.50);
    rng_set_parameters(&qjep_data[2], 30, 1.00, 0.28, 0.55, 0, 0.00, 0.50);
    rng_set_parameters(&qjep_data[3], 30, 1.00, 0.48, 0.70, 0, 0.35, 0.50);

    qjep_load_human_data();

    page = gtk_vbox_new(FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Effect of secondary task on parameters:");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    gtk_widget_show(tmp);

    tooltips = gtk_tooltips_new();

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST); // Run Block
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(qjep_run_block), globals);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run 36 virtual subjects", NULL);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_FIRST); // Initialise
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(qjep_initialise), globals);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Re-initialise", NULL);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_widget_show(GTK_WIDGET(tool_item));

    // Padding:
    tmp = gtk_label_new("");
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 20);
    gtk_widget_show(tmp);
    //  Spin button to set N:
    tmp = gtk_spin_button_new_with_range(0.0, MAX_SUBJECTS, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), qjep_subjects);
    g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(qjep_spin_subjects), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tmp), FALSE, FALSE, 0);
    gtk_widget_show(tmp);
    //  Label widget:
    tmp = gtk_label_new("N:");
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* Choose to show subject data or simulation data: */
    tmp = gtk_label_new("");
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 20);
    gtk_widget_show(tmp);
    tmp = gtk_radio_button_new_with_label(NULL, "Observation");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (qjep_view == 0));
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(qjep_radio_select_observed), globals);
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);
    tmp = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(tmp)), "Simulation");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (qjep_view == 1));
    g_signal_connect(G_OBJECT(tmp), "toggled", G_CALLBACK(qjep_radio_select_simulated), globals);
    gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* Now set up the the widgets to adjust parameters ... in a series of vboxes */

    hbox = gtk_hbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 5);
    gtk_widget_show(hbox);

    /* Left-most vbox: */

    /* Column 1: */
    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 20);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Condition:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Control:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Digit-Switching:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Two-Back:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Go/No-Go:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    /* Column 2: wm_decay_rate */
    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("WM decay rate:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    for (i = 0; i < 4; i++) {
        tmp = gtk_spin_button_new_with_range(0.0, 500.0, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), qjep_data[i].params.wm_decay_rate);
        g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_wm_decay), &(qjep_data[i].params));
        gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);
    }

    /* Column 3: wm_update_efficiency */

    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("WM update efficiency:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    for (i = 0; i < 4; i++) {
        tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), qjep_data[i].params.wm_update_efficiency);
        g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_update_efficiency), &(qjep_data[i].params));
        gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);
    }

    /* Column 4: double selection_temperature */

    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Temperature:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    for (i = 0; i < 4; i++) {
        tmp = gtk_spin_button_new_with_range(0.0, 10.0, 0.01);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), qjep_data[i].params.selection_temperature);
        g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_selection_temperature), &(qjep_data[i].params));
        gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);
    }

    /* Column 5: switch_rate */

    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Switch rate:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    for (i = 0; i < 4; i++) {
        tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), qjep_data[i].params.switch_rate);
        g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_switch_rate), &(qjep_data[i].params));
        gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);
    }

    /* Column 6: monitoring_efficiency */

    vbox = gtk_vbox_new(TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);
    gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.5, 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);

    tmp = gtk_label_new("Monitoring efficiency:");
    gtk_container_add(GTK_CONTAINER(align), tmp);
    gtk_widget_show(tmp);

    for (i = 0; i < 4; i++) {
        tmp = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(tmp), qjep_data[i].params.monitoring_efficiency);
        g_signal_connect(G_OBJECT(tmp), "value_changed", G_CALLBACK(x_spin_monitoring_efficiency), &(qjep_data[i].params));
        gtk_box_pack_start(GTK_BOX(vbox), tmp, TRUE, TRUE, 0);
        gtk_widget_show(tmp);
    }

    /* A separator for aesthetic reasons: */

    tmp = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(page), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* The output panel: */

    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(analysis_qjep_event_expose), globals);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    gtk_widget_show(tmp);

    qjep_canvas = tmp;

    gtk_widget_show(page);
    return(page);
}

/******************************************************************************/

static void x_redraw_canvases(GtkWidget *caller, XGlobals *globals)
{
    int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(globals->notebook));

// Alternative to the following is to emit an expose event for the relevant
// canvas (and then process all events?)

    if (page == 0) {
        diagram_draw_x(globals);
    }
    else if (page == 1) {
        browser_draw_x(globals);
    }
    else if (page == 2) {
        temperature_graph_canvas_draw();
    }
    else if (page == 3) {
        one_sequence_canvas_draw(globals);
    }
    else if (page == 4) {
        basic_results_canvas_draw(globals);
    }
    else if (page == 5) {
        x_draw_ps1d_canvas(globals);
    }
    else if (page == 6) {
        ps_2d_canvas_draw(&ps_2d);
    }
    else if (page == 7) {
        qjep_draw_canvas(globals);
    }
}

static void x_print_canvas(GtkWidget *caller, XGlobals *globals)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    int page, width, height;
    char *filename = NULL;

    page = gtk_notebook_get_current_page(GTK_NOTEBOOK(globals->notebook));
    width = 1200;
    height = 600;

    if (page == 0) {
        filename = "ScreenDumps/architecture.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        diagram_draw_cairo(cr, globals, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 1) {
        char *filename = "ScreenDumps/browser.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        browser_draw_cairo(cr, globals->gv, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 2) {
        filename = "ScreenDumps/temperature_graph.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        cairo_draw_temperature_graph(cr, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 3) {
        filename = "ScreenDumps/sequence.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        cairo_draw_one_sequence(cr, globals, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 4) {
        filename = "ScreenDumps/results.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        cairo_draw_results_analysis(cr, globals, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 5) {
        width = 1200;
        height = width * sqrt(2) / 5.0;

        filename = "ScreenDumps/parameters_1d.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        cairo_draw_1d_parameter_analysis(cr, globals, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 6) {
        filename = "ScreenDumps/parameters_2d.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        cairo_draw_ps_2d(cr, &ps_2d, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else if (page == 7) {
        filename = "ScreenDumps/qjep_simulation.pdf";
        surface = cairo_pdf_surface_create(filename, width, height);
        cr = cairo_create(surface);
        cairo_draw_qjep_analysis(cr, globals, width, height);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
    else {
        fprintf(stdout, "Sorry - don't know how to print this page\n");
    }
    if (filename != NULL) {
        fprintf(stdout, "Window dumped to %s\n", filename);
    }
}

/******************************************************************************/

Boolean rng_widgets_create(XGlobals *globals, OosVars *gv)
{
    globals->gv = gv;

    if ((globals->window = gtk_window_new(GTK_WINDOW_TOPLEVEL)) == NULL) {
        return(FALSE);
    }
    else {
        GtkWidget *page, *notes, *toolbar;
	GtkToolItem *tool_item;
	GtkTooltips *tooltips;
        int position = 0;

        gtk_window_set_default_size(GTK_WINDOW(globals->window), 800, 600);
        gtk_window_set_position(GTK_WINDOW(globals->window), GTK_WIN_POS_CENTER);
        gtk_window_set_title(GTK_WINDOW(globals->window), "X RNG [Version 6: 20/01/2015]");
        g_signal_connect(G_OBJECT(globals->window), "delete_event", G_CALLBACK(event_delete), globals);
        g_signal_connect(G_OBJECT(globals->window), "destroy", G_CALLBACK(callback_destroy), globals);

        page = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(globals->window), page);
        gtk_widget_show(page);

        /* The toolbar: */

        toolbar = gtk_toolbar_new();
        gtk_box_pack_start(GTK_BOX(page), toolbar, FALSE, FALSE, 0);
        gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
        gtk_widget_show(toolbar);

        tooltips = gtk_tooltips_new();

        tool_item = gtk_separator_tool_item_new();
        gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(tool_item), FALSE);
        gtk_tool_item_set_expand(GTK_TOOL_ITEM(tool_item), TRUE);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, position++);
        gtk_widget_show(GTK_WIDGET(tool_item));

        tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_PRINT); // Save a snapshot of the canvases
        g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_print_canvas), globals);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, position++);
        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Save a snapshot of the current canvas to a PDF file", NULL);
        gtk_widget_show(GTK_WIDGET(tool_item));

        tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH); // Redraw canvases
        g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_redraw_canvases), globals);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, position++);
        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Refresh all canvases", NULL);
        gtk_widget_show(GTK_WIDGET(tool_item));

        tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_CLOSE); // Close/exit
        g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(callback_destroy), globals);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, position++);
        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Close and exit the application", NULL);
        gtk_widget_show(GTK_WIDGET(tool_item));

        notes = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(page), notes, TRUE, TRUE, 0);
        globals->notebook = notes;
        gtk_widget_show(notes);

        gtk_notebook_append_page(GTK_NOTEBOOK(notes), diagram_create_notepage(globals), gtk_label_new("Architecture"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), browser_create_notepage(globals), gtk_label_new("OOS Browser"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), temperature_graph_create_notepage(globals), gtk_label_new("Temperature"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), notepage_one_sequence_create(globals), gtk_label_new("One Sequence"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), notepage_basic_results_create(globals), gtk_label_new("Basic Results"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), notepage_parameter_studies_1d_create(globals), gtk_label_new("1D Parameter Study"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), notepage_parameter_studies_2d_create(globals), gtk_label_new("2D Parameter Study"));
        gtk_notebook_append_page(GTK_NOTEBOOK(notes), notepage_qjep_simulation_create(globals), gtk_label_new("QJEP Simulation"));

        return(TRUE);
    }
}

/******************************************************************************/
