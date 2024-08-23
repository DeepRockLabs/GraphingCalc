#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>

#define MAX_EXPR_LENGTH 256
#define GRAPH_WIDTH 380
#define GRAPH_HEIGHT 200

GtkWidget *display, *graph_area, *coord_label;
char display_text[MAX_EXPR_LENGTH] = "";
char graph_expr[MAX_EXPR_LENGTH] = "";
double x_min = -10, x_max = 10, y_min = -10, y_max = 10;

typedef struct {
    const char *name;
    double (*func)(double);
} Function;

double shannon_entropy(double p) {
    if (p <= 0 || p >= 1) return 0;
    
    double q = 1 - p;
    double entropy = 0;
    
    if (p > DBL_EPSILON) {
        entropy -= p * log2(p);
    }
    if (q > DBL_EPSILON) {
        entropy -= q * log2(q);
    }
    
    return entropy;
}

Function functions[] = {
    {"sin", sin},
    {"cos", cos},
    {"tan", tan},
    {"asin", asin},
    {"acos", acos},
    {"atan", atan},
    {"log", log10},
    {"ln", log},
    {"sqrt", sqrt},
    {"exp", exp},
    {"entropy", shannon_entropy},
    {NULL, NULL}
};

void update_display() {
    gtk_entry_set_text(GTK_ENTRY(display), display_text);
}

double evaluate_expression(const char *expr, double x);

double apply_operator(double left, double right, char op) {
    switch (op) {
        case '+': return left + right;
        case '-': return left - right;
        case '*': return left * right;
        case '/': return right != 0 ? left / right : NAN;
        case '^': return pow(left, right);
        default: return NAN;
    }
}

int is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

int get_operator_precedence(char op) {
    switch (op) {
        case '+':
        case '-': return 1;
        case '*':
        case '/': return 2;
        case '^': return 3;
        default: return 0;
    }
}

double evaluate_expression(const char *expr, double x) {
    char temp_expr[MAX_EXPR_LENGTH * 2];
    strcpy(temp_expr, expr);

    // Count open parentheses and add closing ones if needed
    int open_parens = 0;
    int len = strlen(temp_expr);
    for (int i = 0; i < len; i++) {
        if (temp_expr[i] == '(') {
            open_parens++;
        } else if (temp_expr[i] == ')') {
            if (open_parens > 0) {
                open_parens--;
            } else {
                // More closing than opening parentheses, invalid expression
                return NAN;
            }
        }
    }
    // Add closing parentheses if needed
    while (open_parens > 0) {
        strcat(temp_expr, ")");
        open_parens--;
    }
    
    // Replace 'x' with its value
    char x_str[32];
    snprintf(x_str, sizeof(x_str), "%f", x);
    char *pos = strstr(temp_expr, "x");
    while (pos != NULL) {
        memmove(pos + strlen(x_str), pos + 1, strlen(pos));
        memcpy(pos, x_str, strlen(x_str));
        pos = strstr(pos + strlen(x_str), "x");
    }
    
    // Replace constants
    pos = strstr(temp_expr, "pi");
    while (pos != NULL) {
        memmove(pos + strlen("3.141592653589793"), pos + 2, strlen(pos) - 1);
        memcpy(pos, "3.141592653589793", strlen("3.141592653589793"));
        pos = strstr(pos + strlen("3.141592653589793"), "pi");
    }
    
    pos = strstr(temp_expr, "tau");
    while (pos != NULL) {
        memmove(pos + strlen("6.283185307179586"), pos + 3, strlen(pos) - 2);
        memcpy(pos, "6.283185307179586", strlen("6.283185307179586"));
        pos = strstr(pos + strlen("6.283185307179586"), "tau");
    }
    
    pos = strstr(temp_expr, "e");
    while (pos != NULL && (pos == temp_expr || !isalnum(*(pos-1)))) {
        memmove(pos + strlen("2.718281828459045"), pos + 1, strlen(pos));
        memcpy(pos, "2.718281828459045", strlen("2.718281828459045"));
        pos = strstr(pos + strlen("2.718281828459045"), "e");
    }
    
    // Evaluate functions
    for (int i = 0; functions[i].name != NULL; i++) {
        char *func_pos = strstr(temp_expr, functions[i].name);
        while (func_pos != NULL) {
            char *open_paren = strchr(func_pos, '(');
            char *close_paren = strchr(open_paren, ')');
            if (open_paren && close_paren) {
                char arg_str[MAX_EXPR_LENGTH];
                strncpy(arg_str, open_paren + 1, close_paren - open_paren - 1);
                arg_str[close_paren - open_paren - 1] = '\0';
                double arg = evaluate_expression(arg_str, x);
                double result = functions[i].func(arg);
                char result_str[32];
                snprintf(result_str, sizeof(result_str), "%f", result);
                memmove(func_pos + strlen(result_str), close_paren + 1, strlen(close_paren));
                memcpy(func_pos, result_str, strlen(result_str));
            }
            func_pos = strstr(func_pos + 1, functions[i].name);
        }
    }
    
    // Tokenize and evaluate arithmetic operations
    double values[MAX_EXPR_LENGTH];
    char operators[MAX_EXPR_LENGTH];
    int value_count = 0, operator_count = 0;
    
    char *token = temp_expr;
    char *end;
    int expecting_operator = 0;
    
    while (*token) {
        if (isspace(*token)) {
            token++;
            continue;
        }
        
        if (expecting_operator) {
            if (is_operator(*token)) {
                operators[operator_count++] = *token;
                expecting_operator = 0;
            } else {
                // Implicit multiplication
                operators[operator_count++] = '*';
                expecting_operator = 0;
                continue;
            }
        } else {
            if (*token == '-' && (token == temp_expr || is_operator(*(token-1)))) {
                // Negative number
                values[value_count++] = -strtod(token+1, &end);
            } else {
                values[value_count++] = strtod(token, &end);
            }
            if (end == token) {
                // Invalid token
                return NAN;
            }
            token = end - 1;
            expecting_operator = 1;
        }
        token++;
    }
    
    // Apply operators in order of precedence
    for (int precedence = 3; precedence > 0; precedence--) {
        for (int i = 0; i < operator_count; i++) {
            if (get_operator_precedence(operators[i]) == precedence) {
                values[i] = apply_operator(values[i], values[i+1], operators[i]);
                for (int j = i + 1; j < value_count - 1; j++) {
                    values[j] = values[j+1];
                }
                for (int j = i; j < operator_count - 1; j++) {
                    operators[j] = operators[j+1];
                }
                value_count--;
                operator_count--;
                i--;
            }
        }
    }
    
    return values[0];
}

static void apply_css(GtkWidget *widget, const gchar *class_name) {
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_class(context, class_name);
}

void update_graph() {
    gtk_widget_queue_draw(graph_area);
}

void zoom(double factor) {
    double center_x = (x_max + x_min) / 2;
    double center_y = (y_max + y_min) / 2;
    double new_width = (x_max - x_min) * factor;
    double new_height = (y_max - y_min) * factor;
    
    x_min = center_x - new_width / 2;
    x_max = center_x + new_width / 2;
    y_min = center_y - new_height / 2;
    y_max = center_y + new_height / 2;
    
    update_graph();
}

void zoom_in() {
    zoom(0.8);  // Zoom in by 20%
}

void zoom_out() {
    zoom(1.25);  // Zoom out by 25%
}

void reset_zoom() {
    x_min = -10; x_max = 10; y_min = -10; y_max = 10;
    update_graph();
}

gboolean on_graph_click(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->button == 1) {  // Left mouse button
        int width = gtk_widget_get_allocated_width(widget);
        int height = gtk_widget_get_allocated_height(widget);
        
        double graph_x = x_min + (event->x / width) * (x_max - x_min);
        double graph_y = y_max - (event->y / height) * (y_max - y_min);
        
        char coord_text[100];
        snprintf(coord_text, sizeof(coord_text), "Clicked: (%.2f, %.2f)", graph_x, graph_y);
        gtk_label_set_text(GTK_LABEL(coord_label), coord_text);
        
        return TRUE;
    }
    return FALSE;
}

void draw_graph(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    // Clear background
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    cairo_paint(cr);
    
    // Draw grid
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.5);
    cairo_set_line_width(cr, 0.5);
    for (int i = 0; i <= 20; i++) {
        double x = i * (width / 20.0);
        double y = i * (height / 20.0);
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    cairo_stroke(cr);
    
    // Draw axes
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, height / 2);
    cairo_line_to(cr, width, height / 2);
    cairo_move_to(cr, width / 2, 0);
    cairo_line_to(cr, width / 2, height);
    cairo_stroke(cr);
    
    // Draw axis labels
    cairo_set_font_size(cr, 10);
    char label[20];
    for (int i = (int)x_min; i <= (int)x_max; i++) {
        if (i == 0) continue;
        double x = (i - x_min) / (x_max - x_min) * width;
        snprintf(label, sizeof(label), "%d", i);
        cairo_move_to(cr, x - 10, height / 2 + 15);
        cairo_show_text(cr, label);
    }
    for (int i = (int)y_min; i <= (int)y_max; i++) {
        if (i == 0) continue;
        double y = height - (i - y_min) / (y_max - y_min) * height;
        snprintf(label, sizeof(label), "%d", i);
        cairo_move_to(cr, width / 2 + 5, y + 5);
        cairo_show_text(cr, label);
    }
    
    // Draw graph
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.9);
    cairo_set_line_width(cr, 2.0);
    int first_point = 1;
    for (int i = 0; i < width; i++) {
        double x = x_min + (x_max - x_min) * i / width;
        double y = evaluate_expression(graph_expr, x);
        if (isnan(y) || isinf(y)) continue;
        int graph_y = height - (int)((y - y_min) / (y_max - y_min) * height);
        
        if (graph_y < 0) graph_y = 0;
        if (graph_y > height) graph_y = height;
        
        if (first_point) {
            cairo_move_to(cr, i, graph_y);
            first_point = 0;
        } else {
            cairo_line_to(cr, i, graph_y);
        }
    }
    cairo_stroke(cr);
}

const char *functions_with_parens[] = {
    "sin", "cos", "tan", "asin", "acos", "atan",
    "log", "ln", "sqrt", "exp", "entropy", NULL
};

int needs_parenthesis(const char *func) {
    for (int i = 0; functions_with_parens[i] != NULL; i++) {
        if (strcmp(func, functions_with_parens[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void button_clicked(GtkWidget *widget, gpointer data) {
    const char *button_text = gtk_button_get_label(GTK_BUTTON(widget));
    
    if (strcmp(button_text, "=") == 0) {
        double result = evaluate_expression(display_text, 0);
        if (isnan(result)) {
            strcpy(display_text, "Error");
        } else {
            snprintf(display_text, sizeof(display_text), "%.6g", result);
        }
    } else if (strcmp(button_text, "C") == 0) {
        display_text[0] = '\0';
    } else if (strcmp(button_text, "Graph") == 0) {
        strcpy(graph_expr, display_text);
        gtk_widget_queue_draw(graph_area);
    } else if (strcmp(button_text, "entropy") == 0) {
        double p = evaluate_expression(display_text, 0);
        if (p > 0 && p < 1) {
            double entropy = shannon_entropy(p);
            snprintf(display_text, sizeof(display_text), "%.6g", entropy);
        } else {
            strcpy(display_text, "Error: 0 < p < 1");
        }
    } else {
        if (needs_parenthesis(button_text)) {
            strcat(display_text, button_text);
            strcat(display_text, "(");
        } else {
            strcat(display_text, button_text);
        }
    }
    
    update_display();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Modern TI-84 Style Calculator");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 700);  // Increased height to accommodate new buttons
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);


    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background-color: #f0f0f0; }"
        ".display { font-size: 24px; background-color: #ffffff; color: #333333; border: 1px solid #cccccc; border-radius: 5px; }"
        ".button { font-size: 18px; min-height: 50px; min-width: 50px; background-image: none; background-color: #ffffff; color: #333333; border: 1px solid #cccccc; border-radius: 5px; transition: all 0.2s ease; }"
        ".button:hover { background-color: #e0e0e0; }"
        ".operator { background-color: #f0f0f0; }"
        ".function { background-color: #e8e8e8; }"
        ".graph-button { background-color: #4CAF50; color: white; }"
        ".graph-button:hover { background-color: #45a049; }"
        ".clear { background-color: #f44336; color: white; }"
        ".clear:hover { background-color: #da190b; }",
        -1, NULL);
    
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    display = gtk_entry_new();
    gtk_entry_set_alignment(GTK_ENTRY(display), 1);
    gtk_box_pack_start(GTK_BOX(vbox), display, FALSE, FALSE, 0);
    apply_css(display, "display");

    graph_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(graph_area, GRAPH_WIDTH, GRAPH_HEIGHT);
    g_signal_connect(G_OBJECT(graph_area), "draw", G_CALLBACK(draw_graph), NULL);
    gtk_widget_add_events(graph_area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(graph_area), "button-press-event", G_CALLBACK(on_graph_click), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), graph_area, TRUE, TRUE, 0);

    coord_label = gtk_label_new("Click on graph to show coordinates");
    gtk_box_pack_start(GTK_BOX(vbox), coord_label, FALSE, FALSE, 0);

    GtkWidget *zoom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), zoom_box, FALSE, FALSE, 0);

    GtkWidget *zoom_in_button = gtk_button_new_with_label("Zoom In");
    g_signal_connect(zoom_in_button, "clicked", G_CALLBACK(zoom_in), NULL);
    gtk_box_pack_start(GTK_BOX(zoom_box), zoom_in_button, TRUE, TRUE, 0);
    apply_css(zoom_in_button, "button");

    GtkWidget *zoom_out_button = gtk_button_new_with_label("Zoom Out");
    g_signal_connect(zoom_out_button, "clicked", G_CALLBACK(zoom_out), NULL);
    gtk_box_pack_start(GTK_BOX(zoom_box), zoom_out_button, TRUE, TRUE, 0);
    apply_css(zoom_out_button, "button");

    GtkWidget *reset_zoom_button = gtk_button_new_with_label("Reset Zoom");
    g_signal_connect(reset_zoom_button, "clicked", G_CALLBACK(reset_zoom), NULL);
    gtk_box_pack_start(GTK_BOX(zoom_box), reset_zoom_button, TRUE, TRUE, 0);
    apply_css(reset_zoom_button, "button");

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    const char *buttons[] = {
        "sin", "cos", "tan", "C", "(", ")",
        "7", "8", "9", "/", "asin", "pi",
        "4", "5", "6", "*", "acos", "tau",
        "1", "2", "3", "-", "atan", "^",
        "0", ".", "=", "+", "sqrt", "log",
        "x", "Graph", "ln", "exp", "entropy"
    };

    for (int i = 0; i < 35; i++) {
        GtkWidget *button = gtk_button_new_with_label(buttons[i]);
        g_signal_connect(button, "clicked", G_CALLBACK(button_clicked), NULL);
        gtk_widget_set_hexpand(button, TRUE);
        gtk_widget_set_vexpand(button, TRUE);
        gtk_grid_attach(GTK_GRID(grid), button, i % 6, i / 6, 1, 1);
        apply_css(button, "button");
        
        if (strchr("+-*/^", buttons[i][0])) {
            apply_css(button, "operator");
        } else if (strcmp(buttons[i], "Graph") == 0) {
            apply_css(button, "graph-button");
        } else if (strcmp(buttons[i], "C") == 0) {
            apply_css(button, "clear");
        } else if (strlen(buttons[i]) > 1) {
            apply_css(button, "function");
        }
    }

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}