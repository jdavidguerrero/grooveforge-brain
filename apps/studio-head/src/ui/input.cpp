#include "ui/input.h"

#if defined(STUDIO_DISPLAY_SDL)
#include "sdl/sdl.h"
#endif

namespace {

constexpr int QUEUE = 16;
Ctl queue[QUEUE];
int head = 0, tail = 0;

void on_tap(lv_event_t* e) {
    input_post(static_cast<Ctl>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
}

#if defined(STUDIO_DISPLAY_SDL)
lv_indev_drv_t kb_drv;

void on_key(lv_event_t* e) {
    switch (lv_event_get_key(e)) {
        case LV_KEY_RIGHT: input_post(Ctl::NavNext); break;
        case LV_KEY_LEFT:  input_post(Ctl::NavPrev); break;
        case LV_KEY_ENTER: input_post(Ctl::NavPush); break;
        case '1': input_post(Ctl::B1); break;
        case '2': input_post(Ctl::B2); break;
        case '3': input_post(Ctl::B3); break;
        case '4': input_post(Ctl::B4); break;
        case 'm': case 'M': input_post(Ctl::Mode); break;
        case 's': case 'S': input_post(Ctl::Shift); break;
        default: break;
    }
}
#endif

}  // namespace

void input_post(Ctl c) {
    const int next = (head + 1) % QUEUE;
    if (next == tail) return;  // cola llena: se descarta (regla 4 del theory doc §2.3)
    queue[head] = c;
    head = next;
}

bool input_poll(Ctl& out) {
    if (tail == head) return false;
    out = queue[tail];
    tail = (tail + 1) % QUEUE;
    return true;
}

void input_attach_tap(lv_obj_t* obj, Ctl c) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, on_tap, LV_EVENT_CLICKED, reinterpret_cast<void*>(uintptr_t(c)));
}

void input_keyboard_init() {
#if defined(STUDIO_DISPLAY_SDL)
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_t* kb = lv_indev_drv_register(&kb_drv);

    // Un objeto invisible en la capa superior recibe las teclas en cualquier pantalla.
    lv_obj_t* sink = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(sink);
    lv_obj_set_size(sink, 1, 1);
    lv_obj_add_event_cb(sink, on_key, LV_EVENT_KEY, nullptr);
    lv_group_t* g = lv_group_create();
    lv_group_add_obj(g, sink);
    lv_indev_set_group(kb, g);
#endif
}
