/*
 * Copiright (C) 2019 Santiago León O.
 */

// TODO: Support float and int here.
// TODO: Move this to common.h
enum c_type_t {
    STR,
    DOUBLE
};

char* to_string (mem_pool_t *pool, enum c_type_t type, void *value)
{
    char *res = NULL;

    switch (type)
    {
        case DOUBLE:
            res = pprintf (pool, "%f", *(double*)value);
            break;
        case STR:
            res = (char *)value;
            break;
    }

    return res;
}

typedef enum {
    CSS_FONT_WEIGHT_NONE,
    CSS_FONT_WEIGHT_NORMAL,
    CSS_FONT_WEIGHT_BOLD
} css_font_weight_t;

struct font_style_t {
    css_font_weight_t weight;
    int size;
    const char *family;
};
#define CSS_FONT_STYLE(weight,size,family) ((struct font_style_t){weight,size,family})

void cr_setup_font (cairo_t *cr, struct font_style_t *font_style)
{
    cairo_font_weight_t cairo_font_weight = CAIRO_FONT_WEIGHT_NORMAL;
    switch (font_style->weight) {
        case CSS_FONT_WEIGHT_BOLD:
            cairo_font_weight = CAIRO_FONT_WEIGHT_BOLD;
            break;
        case CSS_FONT_WEIGHT_NONE:
        case CSS_FONT_WEIGHT_NORMAL:
            cairo_font_weight = CAIRO_FONT_WEIGHT_NORMAL;
            break;
        default:
            invalid_code_path;
            break;
    }

    cairo_select_font_face (cr, font_style->family, CAIRO_FONT_SLANT_NORMAL, cairo_font_weight);
    cairo_set_font_size (cr, font_style->size);
}

struct table_column_t {
    char *name;
    double width;

    struct table_column_t *next;
};

struct table_value_t {
    void *value;
    enum c_type_t type;

    struct table_value_t *next;
};

struct table_row_t {
    struct table_value_t *values;
    struct table_value_t *values_end;

    struct table_row_t *next;
};

struct table_t {
    mem_pool_t pool;

    double width;
    double height;
    GtkWidget *widget;
    GtkWidget *window;
    struct font_style_t font_style;

    LINKED_LIST_DECLARE (struct table_column_t, columns)
    LINKED_LIST_DECLARE (struct table_row_t, rows)

    bool are_columns_defined;
    int num_columns;

    struct table_column_t *curr_column;
};

void table_destroy (struct table_t *table)
{
    mem_pool_destroy (&table->pool);
}

gboolean table_draw_callback (GtkWidget *widget, cairo_t *cr, gpointer data)
{
    double margin_h = 6;
    double margin_v = 3;
    dvec4 text_color = RGB_255(66,66,66);

    struct table_t *table = (struct table_t *)data;
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

    if (table->rows == NULL) {
        // TODO: Show a "table empty" message
        return TRUE;
    }

    cr_setup_font (cr, &table->font_style);

    cairo_font_extents_t font_extents;
    cairo_font_extents (cr, &font_extents);
    double row_height = font_extents.ascent + font_extents.descent + 2*margin_v;

    // Compute the effective table height
    double table_height = 0;
    if (table->height == 0) {
        // TODO: We can store the number of rows, then this is just a
        // multiplication.
        struct table_row_t *curr_row = table->rows;
        while (curr_row != NULL) {
            table_height += row_height;

            curr_row = curr_row->next;
        }

        // TODO: Don't allow table to be taller than the screen
        //table_height = MIN (table_height, screen_height);

    } else {
        table_height = table->height;
    }

    // Compute the effective table width
    double table_width = 0;
    if (table->width == 0) {
        struct table_column_t *curr_column = table->columns;
        while (curr_column != NULL) {
            table_width += curr_column->width + 2*margin_h;

            curr_column = curr_column->next;
        }

        // TODO: Don't allow table to be wider than the screen
        //table_width = MIN (table_width, screen_width);

    } else {
        table_width = table->width;
    }

    gtk_widget_set_size_request (widget, table_width, table_height);

    mem_pool_t pool = {0};
    double y = font_extents.ascent + margin_v;

    cairo_set_source_rgb (cr, ARGS_RGB(text_color));

    struct table_row_t *curr_row = table->rows;
    while (curr_row != NULL) {
        double x = margin_h;

        struct table_column_t *curr_column = table->columns;
        struct table_value_t *curr_value = curr_row->values;
        while (curr_column != NULL) {
            // We substract the x_bearing because that's the position where the
            // actual 'ink' of the text starts.
            char *str = to_string(&pool, curr_value->type, curr_value->value);
            cairo_text_extents_t e;
            cairo_text_extents (cr, str, &e);
            cairo_move_to (cr, x - e.x_bearing, y);

            cairo_show_text (cr, str);

            x += curr_column->width + 2*margin_h;

            curr_value = curr_value->next;
            curr_column = curr_column->next;
        }

        y += row_height;

        curr_row = curr_row->next;
    }

    mem_pool_destroy (&pool);

    return TRUE;
}

GtkWidget* table_init (struct table_t *table, GtkWidget *window, double width, double height)
{
    table->window = window;
    table->width = width;
    table->height = height;
    table->font_style = CSS_FONT_STYLE(CSS_FONT_WEIGHT_NORMAL, 12, "Open Sans");

    table->widget = gtk_drawing_area_new ();

    // We make the size request for the witget from table_draw_callback(), for
    // some reason it doesn't work unless we also make this request for 0x0
    // size. If we don't do this, when placing the GtkDrawingArea widget into a
    // GtkPaned, the pane is collapsed and the divider needs to be dragged. It's
    // weird because when the drag starts the divider instantly snaps to the
    // width set in the draw callback. This feels like a GTK bug...
    gtk_widget_set_size_request (table->widget, 0, 0);

    g_signal_connect (G_OBJECT (table->widget),
                      "draw",
                      G_CALLBACK (table_draw_callback),
                      table);

    gtk_widget_set_vexpand (table->widget, TRUE);
    gtk_widget_set_hexpand (table->widget, TRUE);

    return table->widget;
}

void table_start_row (struct table_t *table)
{
    LINKED_LIST_APPEND_NEW (struct table_row_t, table->rows, &table->pool, new_row);
    table->curr_column = table->columns;
}

void table_end_row (struct table_t *table)
{
    if (!table->are_columns_defined) {
        table->are_columns_defined = true;
    }
}

#define table_row_set_value(table,column_name,value) table_row_set_value_type(table,column_name,STR,value)
void table_row_set_value_type (struct table_t *table, char *column_name, enum c_type_t type, void *value)
{
    assert (table->rows != NULL && "Call table_start_row() before adding values");

    if (!table->are_columns_defined) {
        LINKED_LIST_APPEND_NEW (struct table_column_t, table->columns, &table->pool, new_column);

        // For the first row curr_columns is set here, and not by iterating the
        // column list, because next pointer will always be NULL.
        table->curr_column = new_column;

        table->num_columns++;
        new_column->name = column_name;
    }

    LINKED_LIST_APPEND_NEW (struct table_value_t, table->rows_end->values, &table->pool, new_value);

    // Update column width
    {
        mem_pool_t pool = {0};
        cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 500, 300);
        cairo_t *cr = cairo_create (surface);
        char *value_str = to_string (&pool, type, value);

        cr_setup_font (cr, &table->font_style);
        cairo_text_extents_t extents;
        cairo_text_extents (cr, value_str, &extents);

        table->curr_column->width = MAX(table->curr_column->width, extents.width);

        cairo_surface_destroy(surface);
        mem_pool_destroy (&pool);

        table->curr_column = table->curr_column->next;
    }

    new_value->value = value;
    new_value->type = type;
}
