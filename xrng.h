
#ifndef _xrng_h_

#define _xrng_h_

#include "oos.h"
#include "rng.h"
#include <stdlib.h>
#include <time.h>
#include <gtk/gtk.h>

#define MAX_GROUPS 21

typedef enum canvas1_view {
    CANVAS_TABLE, CANVAS_GLOBAL, CANVAS_SECOND_ORDER, CANVAS_STAR
} Canvas1View;

typedef enum canvas2_view {
    CANVAS_VARY_DECAY, CANVAS_VARY_TEMPERATURE, CANVAS_VARY_SWITCH, CANVAS_VARY_MONITORING, CANVAS_VARY_UPDATE
} Canvas2View;

typedef struct x_globals {
    OosVars *gv;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *diagram;
    GtkWidget *browser;
    GtkWidget *browser_label;
    GtkWidget *seq_canvas;
    GtkWidget *output_panel1;
    GtkWidget *output_panel2;
    Boolean dynamic_canvases;
    Boolean colour;
    Boolean running;
    Canvas1View canvas1_selection;
    Canvas2View canvas2_selection;
    RngParameters params;
    int group_index;
    RngScores group_mean[MAX_GROUPS];
    RngScores group_sd[MAX_GROUPS];
} XGlobals;

extern void diagram_draw_x(XGlobals *globals);
extern void diagram_draw_cairo(cairo_t *cr, XGlobals *globals, int width, int height);
extern GtkWidget *diagram_create_notepage(XGlobals *globals);

extern void browser_draw_x(XGlobals *globals);
extern void browser_draw_cairo(cairo_t *cr, OosVars *gv, int width, int height);
extern GtkWidget *browser_create_notepage(XGlobals *globals);
extern Boolean rng_widgets_create(XGlobals *globals, OosVars *gv);

#endif
