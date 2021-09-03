
#include "pl.h"
#include "xrng.h"
#include "lib_cairox.h"
#include <ctype.h>
#include <cairo-pdf.h>

#define LINE_SEP 20
#define BAR_SIZE 400

/******************************************************************************/

void browser_draw_cairo(cairo_t *cr, OosVars *gv, int width, int height)
{
    TimestampedClauseList *ct;
    CairoxTextParameters p;
    PangoLayout *layout;
    BoxList *tmpb;
    MessageList *tmpm;
    char buffer[1024], cl_buffer[1024];
    int y = 1;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    layout = pango_cairo_create_layout(cr);
    pangox_layout_set_font_size(layout, 14);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    pangox_layout_set_font_face(layout, PS_FONT_SANS);

    if (gv->messages == NULL) {
        cairox_text_parameters_set(&p, 5, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
        cairox_paint_pango_text(cr, &p, layout, "Recent Messages: <NONE>");
        y++;
    }
    else {
        cairox_text_parameters_set(&p, 5, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
        cairox_paint_pango_text(cr, &p, layout, "Recent Messages:");
        y++;
    }

    for (tmpm = gv->messages; tmpm != NULL; tmpm = tmpm->next) {
        cairox_text_parameters_set(&p, 20, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
        g_snprintf(buffer, 1024, "%s => %s", oos_box_name(gv, tmpm->source), oos_box_name(gv, tmpm->target));
        cairox_paint_pango_text(cr, &p, layout, buffer);

        cairox_text_parameters_set(&p, 320, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
        cairox_paint_pango_text(cr, &p, layout, oos_message_type_name[tmpm->mt]);

        if (tmpm->content != NULL) {
            cairox_text_parameters_set(&p, 420, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
            sprint_clause(cl_buffer, tmpm->content);
            cairox_paint_pango_text(cr, &p, layout, cl_buffer);
	}

        y++;
    }

    cairox_paint_line(cr, 1.0, 10, y*LINE_SEP, width-10, y*LINE_SEP);
    y++;

    for (tmpb = gv->components; tmpb != NULL; tmpb = tmpb->next) {
        if (tmpb->bt == BOX_BUFFER) {
            if (tmpb->content == NULL) {
                cairox_text_parameters_set(&p, 5, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
                g_snprintf(buffer, 1024, "%s: <EMPTY>", tmpb->name);
                cairox_paint_pango_text(cr, &p, layout, buffer);
                y++;
            }
            else {
                cairox_text_parameters_set(&p, 5, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
                g_snprintf(buffer, 1024, "%s:", tmpb->name);
                cairox_paint_pango_text(cr, &p, layout, buffer);
                y++;
                for (ct = tmpb->content; ct != NULL; ct = ct->tail) {
                    cairox_text_parameters_set(&p, 20, y * LINE_SEP, PANGOX_XALIGN_LEFT, PANGOX_YALIGN_CENTER, 0.0);
                    sprint_clause(cl_buffer, ct->head);
                    g_snprintf(buffer, 1024, "%4ld: %s <%5.3f>", ct->timestamp, cl_buffer, ct->activation);
                    cairox_paint_pango_text(cr, &p, layout, buffer);

                    cairo_set_line_width(cr, 1.0);
                    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                    cairo_rectangle(cr, width-20-BAR_SIZE, (y-0.5) * LINE_SEP, ct->activation * BAR_SIZE, LINE_SEP-2);
                    cairo_fill(cr);
                    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
                    cairo_rectangle(cr, width-20-BAR_SIZE, (y-0.5) * LINE_SEP, BAR_SIZE, LINE_SEP-2);
                    cairo_stroke(cr);

                    y++;
                }
            }
        }
    }

    g_object_unref(layout);
}

/******************************************************************************/

void browser_draw_x(XGlobals *globals)
{
    if (globals->browser != NULL) {
        cairo_surface_t *s;
        cairo_t *cr;
        int width, height;

        /* Double buffer: First create and draw to a surface: */
        gtk_widget_realize(globals->browser);
        width = globals->browser->allocation.width;
        height = globals->browser->allocation.height;
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create(s);
        browser_draw_cairo(cr, globals->gv, width, height);
        cairo_destroy(cr);

        /* Now copy the surface to the window: */
        cr = gdk_cairo_create(globals->browser->window);
        cairo_set_source_surface(cr, s, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);

        /* And destroy the surface: */
        cairo_surface_destroy(s);
    }

    if ((globals->browser_label != NULL) && (globals->gv != NULL)) {
        char buffer[64];

        g_snprintf(buffer, 64, "Current cycle: %d", globals->gv->cycle);
        gtk_label_set_text(GTK_LABEL(globals->browser_label), buffer);
    }
}

/******************************************************************************/

static void x_task_initialise(GtkWidget *caller, XGlobals *globals)
{
    rng_create(globals->gv, &(globals->params));
    rng_initialise_subject(globals->gv);
    browser_draw_x(globals);
}

static void x_task_step_cycle(GtkWidget *caller, XGlobals *globals)
{
    RngSubjectData *subject;

    globals->running = TRUE;
    browser_draw_x(globals);
    gtk_main_iteration_do(FALSE);
    if (!oos_step(globals->gv)) {
        subject = &(((RngData *)globals->gv->task_data)->subject[globals->gv->block]);
        rng_analyse_subject_responses(NULL, subject, globals->gv->trials_per_subject);
    }
    globals->running = FALSE;
    browser_draw_x(globals);
}

static void x_task_pause_block(GtkWidget *caller, XGlobals *globals)
{
    globals->running = FALSE;
}

static void x_task_step_block(GtkWidget *caller, XGlobals *globals)
{
    RngSubjectData *subject;

    globals->running = TRUE;
    do {
        browser_draw_x(globals);
        gtk_main_iteration_do(FALSE);
    } while ((globals->running) && (oos_step(globals->gv)));
    subject = &(((RngData *)globals->gv->task_data)->subject[globals->gv->block]);
    rng_analyse_subject_responses(NULL, subject, globals->gv->trials_per_subject);
    globals->running = FALSE;
}

static int browser_canvas_event_expose(GtkWidget *da, GdkEventConfigure *event, XGlobals *globals)
{
    browser_draw_x(globals);
    return(1);
}

/******************************************************************************/

GtkWidget *browser_create_notepage(XGlobals *globals)
{
    GtkWidget *page, *hbox, *tmp;
    GtkToolItem *tool_item;
    GtkTooltips *tooltips;

    page = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(page);

    /* Button here to step through or run to completion */

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    tmp = gtk_label_new("Current cycle: ");
    gtk_box_pack_start(GTK_BOX(hbox), tmp, FALSE, FALSE, 5);
    globals->browser_label = tmp;
    gtk_widget_show(tmp);

    tooltips = gtk_tooltips_new();

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_STOP); // Pause
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_pause_block), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Pause model execution", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST); // Step one block
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_step_block), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run to the end of the block of trials", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD); // Step one cycle
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_step_cycle), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Run one processing cycle", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    tool_item = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_FIRST); // Initialise
    g_signal_connect(G_OBJECT(tool_item), "clicked", G_CALLBACK(x_task_initialise), globals);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(tool_item), FALSE, FALSE, 5);
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(tool_item), "Initialise the model", NULL);
    gtk_widget_show(GTK_WIDGET(tool_item));

    /* A separator for aesthetic reasons: */
    tmp = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(page), tmp, FALSE, FALSE, 0);
    gtk_widget_show(tmp);

    /* The drawing panel: */
    tmp = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(tmp), "expose_event", G_CALLBACK(browser_canvas_event_expose), globals);
    gtk_box_pack_end(GTK_BOX(page), tmp, TRUE, TRUE, 0);
    globals->browser = tmp;
    gtk_widget_show(tmp);

    return(page);
}

/******************************************************************************/
