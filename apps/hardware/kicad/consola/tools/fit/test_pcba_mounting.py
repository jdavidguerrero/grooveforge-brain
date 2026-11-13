"""Observable mounting regressions; method in PCB-A THEORY-strip.md."""
import unittest

import check_pcba_mounting as mounting


class MountingTests(unittest.TestCase):
    def test_four_board_holes_cut_panel_and_standoffs_clear_k(self):
        result = mounting.audit()
        self.assertTrue(result['passes'], result)
        self.assertEqual([h['panel_mm'] for h in result['holes']],
                         [(72, 181), (118, 192), (144, 192), (248, 192)])
        self.assertEqual(result['standoff_z_mm'], [65, 83])
        self.assertEqual(result['pcb_k_z_mm'], [80, 81.6])

    def test_original_h1_breaks_required_laser_web(self):
        tap = ('rectangle', 58, 174, 15.4, 15.4)
        self.assertAlmostEqual(mounting.cut_gap((62, 183), 2.1, tap), -.8)
        self.assertLess(mounting.cut_gap((62, 183), 2.1, tap), mounting.MIN_WEB_MM)

    def test_standoff_collision_even_when_center_is_outside_k(self):
        k = [mounting.fit.rect(0, 0, 68, 221)]
        self.assertLess(mounting.standoff_gap((70, 183), k), 0)
        self.assertAlmostEqual(mounting.standoff_gap((72, 181), k), 1.2)

    def test_laser_dxf_contains_four_real_mounting_circles(self):
        path = mounting.fit.ROOT / 'apps/design/3d-models/consola/panel-aluminio-v1-laser.dxf'
        lines = path.read_text().splitlines()
        pairs = [(k.strip(), v.strip()) for k, v in zip(lines[::2], lines[1::2])]
        circles = []
        for i, pair in enumerate(pairs):
            if pair != ('0', 'CIRCLE'):
                continue
            j = i + 1
            while j < len(pairs) and pairs[j][0] != '0':
                j += 1
            fields = dict(pairs[i+1:j])
            circles.append(tuple(float(fields[k]) for k in ('10', '20', '30', '40')))
        for x, y in [(72, 181), (118, 192), (144, 192), (248, 192)]:
            self.assertEqual(circles.count((x, y, 84, 2.1)), 1)
