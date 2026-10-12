#include "ui/painter.h"

#include <cmath>
#include <cstring>

Painter Painter::from(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    return Painter{lv_event_get_draw_ctx(e), a.x1, a.y1};
}

void Painter::fill(float x, float y, float w, float h, uint32_t color) const {
    const int ix = int(std::lround(x)), iy = int(std::lround(y));
    const int iw = int(std::lround(w)), ih = int(std::lround(h));
    if (iw <= 0 || ih <= 0) return;
    lv_area_t a{lv_coord_t(ox + ix), lv_coord_t(oy + iy), lv_coord_t(ox + ix + iw - 1), lv_coord_t(oy + iy + ih - 1)};
    lv_area_t clip;
    if (!_lv_area_intersect(&clip, &a, ctx->clip_area)) return;  // fuera de la franja que se renderiza
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color);
    lv_draw_rect(ctx, &d, &a);
}

void Painter::frame(int x, int y, int w, int h, int t, uint32_t color) const {
    fill(x, y, w, t, color);
    fill(x, y + h - t, w, t, color);
    fill(x, y + t, t, h - 2 * t, color);
    fill(x + w - t, y + t, t, h - 2 * t, color);
}

void Painter::text(float x, float baseline, const char* s, uint32_t color, const lv_font_t* font,
                   lv_text_align_t align) const {
    const int w = text_width(s, font);
    int x1 = int(std::lround(x));
    if (align == LV_TEXT_ALIGN_RIGHT) x1 -= w;
    else if (align == LV_TEXT_ALIGN_CENTER) x1 -= w / 2;
    const int y1 = int(std::lround(baseline)) - (font->line_height - font->base_line);
    lv_area_t a{lv_coord_t(ox + x1), lv_coord_t(oy + y1), lv_coord_t(ox + x1 + w), lv_coord_t(oy + y1 + font->line_height - 1)};
    lv_area_t clip;
    if (!_lv_area_intersect(&clip, &a, ctx->clip_area)) return;
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = font;
    d.color = lv_color_hex(color);
    lv_draw_label(ctx, &d, &a, s, nullptr);
}

void Painter::circle(float cx, float cy, float r, uint32_t fill_c, uint32_t border, int border_w) const {
    lv_area_t a{lv_coord_t(ox + std::lround(cx - r)), lv_coord_t(oy + std::lround(cy - r)),
                lv_coord_t(ox + std::lround(cx + r)), lv_coord_t(oy + std::lround(cy + r))};
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(fill_c);
    d.border_color = lv_color_hex(border);
    d.border_width = lv_coord_t(border_w);
    lv_draw_rect(ctx, &d, &a);
}

void Painter::line(float x1, float y1, float x2, float y2, int width, uint32_t color) const {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.width = lv_coord_t(width);
    d.color = lv_color_hex(color);
    const lv_point_t p1{lv_coord_t(ox + std::lround(x1)), lv_coord_t(oy + std::lround(y1))};
    const lv_point_t p2{lv_coord_t(ox + std::lround(x2)), lv_coord_t(oy + std::lround(y2))};
    lv_draw_line(ctx, &d, &p1, &p2);
}

int text_width(const char* s, const lv_font_t* font) {
    return lv_txt_get_width(s, uint32_t(std::strlen(s)), font, 0, LV_TEXT_FLAG_NONE);
}

lv_obj_t* draw_area(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t cb, void* user) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_add_event_cb(o, cb, LV_EVENT_DRAW_MAIN, user);
    return o;
}
