#include "libmod_outrun.h"
#include "libmod_outrun_exports.h"
#include <g_video.h>
#include <libbgdi.h>
#include <math.h>

int modoutrun_draw_road(float time) {
    int y;
    float perspective, scale, offset;
    int horizon = 80;
    int screen_height = GLODWORD(libg_graph_screen_y);
    int screen_width = GLODWORD(libg_graph_screen_x);
    int road_height = screen_height - horizon;

    for (y = 0; y < road_height; y++) {
        perspective = (float)y / road_height;
        scale = 1.0f / (perspective + 0.01f);
        offset = sinf((time / 100.0f) + (y / 20.0f)) * 40.0f;

        int x1 = screen_width / 2 - (int)(scale * 50) + (int)offset;
        int x2 = screen_width / 2 + (int)(scale * 50) + (int)offset;
        int draw_y = horizon + y;

        int color;
        if ((y % 20) < 10)
            color = color_get(50, 50, 50);
        else
            color = color_get(80, 80, 80);

        draw_line(x1, draw_y, x2, draw_y, color);
    }

    return 0;
}