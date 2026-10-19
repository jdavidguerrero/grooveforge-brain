// Tests de las luces del panel (theory/04 de Studio §8): arco del anillo según NAV y teclas fijas.
#include <unity.h>

#include "ui/panel_leds.h"
#include "ui/studio_ui.h"

using namespace studio;

void setUp() {}
void tearDown() {}

void test_arco_reparte_los_12_leds() {
    RingArc a = ring_arc(0, 3);
    TEST_ASSERT_EQUAL_UINT8(0, a.first);
    TEST_ASSERT_EQUAL_UINT8(4, a.len);
    a = ring_arc(5, 6);
    TEST_ASSERT_EQUAL_UINT8(10, a.first);
    TEST_ASSERT_EQUAL_UINT8(2, a.len);
    a = ring_arc(11, 12);
    TEST_ASSERT_EQUAL_UINT8(11, a.first);
    TEST_ASSERT_EQUAL_UINT8(1, a.len);
    a = ring_arc(20, 4);                     // fuera de rango: queda en la última
    TEST_ASSERT_EQUAL_UINT8(9, a.first);
}

void test_nav_por_pagina_y_fx() {
    GF_UiState u{};
    uint8_t pos, n;
    u.page = AI;
    nav_position(u, 0, 9, &pos, &n);
    TEST_ASSERT_EQUAL_UINT8(1, pos);
    TEST_ASSERT_EQUAL_UINT8(4, n);                    // PERFORM · AI · SPECTRUM · PANEL
    u.page = PANEL;
    nav_position(u, 0, 9, &pos, &n);
    TEST_ASSERT_EQUAL_UINT8(3, pos);
    u.page = LFO;
    u.cursor = 2;
    nav_position(u, 2, 9, &pos, &n);          // Prophet: LFO · POLY-MOD · WHEEL-MOD
    TEST_ASSERT_EQUAL_UINT8(2, pos);
    TEST_ASSERT_EQUAL_UINT8(3, n);
    u.mode = 1;
    u.fx_slot = 7;
    nav_position(u, 0, 9, &pos, &n);
    TEST_ASSERT_EQUAL_UINT8(7, pos);
    TEST_ASSERT_EQUAL_UINT8(12, n);
}

void test_teclas_fijas_segun_pagina_y_shift() {
    GF_UiState u{};
    uint8_t k[KEY_LEDS];
    u.page = ENV;
    u.shift = 1;
    key_levels(u, k);
    TEST_ASSERT_TRUE(k[1] > k[0]);
    TEST_ASSERT_TRUE(k[5] > k[0]);
    u.page = PERFORM;
    u.shift = 0;
    key_levels(u, k);
    TEST_ASSERT_EQUAL_UINT8(k[0], k[3]);
    TEST_ASSERT_EQUAL_UINT8(k[0], k[5]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_arco_reparte_los_12_leds);
    RUN_TEST(test_nav_por_pagina_y_fx);
    RUN_TEST(test_teclas_fijas_segun_pagina_y_shift);
    return UNITY_END();
}
