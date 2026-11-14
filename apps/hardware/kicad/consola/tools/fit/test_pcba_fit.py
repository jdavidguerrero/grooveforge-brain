"""Observable geometry checks and the actual Console fit demo. No board is saved."""
import math
import unittest
import xml.etree.ElementTree as ET

import check_pcba_fit as fit


class GeometryTests(unittest.TestCase):
    def test_known_intersection_area(self):
        a = fit.rect(0, 0, 10, 10)
        b = fit.rect(8, 3, 15, 7)
        self.assertEqual(fit.collision([a], [b]), dict(overlap_mm2=8., clearance_mm=0., status='SOLAPE'))

    def test_edge_contact_is_not_clearance(self):
        r = fit.collision([fit.rect(0,0,10,10)], [fit.rect(10,2,15,8)])
        self.assertEqual(r['status'], 'CONTACTO')
        self.assertEqual(r['overlap_mm2'], 0.)

    def test_euclidean_corner_distance(self):
        r = fit.collision([fit.rect(0,0,2,2)], [fit.rect(5,6,7,8)])
        self.assertEqual(r['clearance_mm'], 5.)

    def test_concave_outline_does_not_use_bounding_box(self):
        polygon = [(0,0),(10,0),(10,10),(8,10),(8,2),(0,2)]
        cs = fit.cells(polygon)
        self.assertAlmostEqual(sum(abs(fit.signed_area(c)) for c in cs), 36)
        r = fit.collision(cs, [fit.rect(2,4,5,8)])
        self.assertEqual(r['status'], 'LIBRE')
        self.assertEqual(r['clearance_mm'], 2.)

    def test_window_subtraction(self):
        cs = fit.cells(fit.rect(0,0,10,10), [fit.rect(2,2,8,8)])
        self.assertEqual(sum(abs(fit.signed_area(c)) for c in cs), 64)
        self.assertEqual(fit.collision(cs,[fit.rect(4,4,6,6)])['status'], 'LIBRE')

    def test_rejects_unimplemented_geometry(self):
        with self.assertRaisesRegex(ValueError,'Non-orthogonal'):
            fit.cells([(0,0),(10,0),(4,5)])

    def test_rotated_polygon_clipping(self):
        diamond = [(0,1),(1,0),(2,1),(1,2)]
        self.assertAlmostEqual(abs(fit.signed_area(fit.clip(diamond,fit.rect(0,0,1,2)))),1)

    def test_kicad_rotation_and_offset_axis(self):
        fp = dict(origin=(10,20),angle_deg=90)
        self.assertEqual(fit.fp_point(fp,(7.5,2.5)),(12.5,12.5))

    def test_rigid_fit_handles_rotation_and_translation(self):
        source = [(0,0),(26,0),(52,0)]
        target = [(8,9),(8,35),(8,61)]
        fitting = fit.fit(source,target)
        self.assertAlmostEqual(fitting['angle_deg'],90)
        for a,b in zip(source,target):
            self.assertLess(math.dist(fit.transform(a,fitting),b),1e-10)

    def test_pitch_error_cannot_be_hidden_by_scaling(self):
        source = [(25.8*i,179) for i in range(8)]
        target = [(79+26*i,192) for i in range(8)]
        fitting = fit.fit(source,target)
        residuals = [math.dist(fit.transform(a,fitting),b) for a,b in zip(source,target)]
        self.assertAlmostEqual(max(residuals),.7)
        self.assertGreater(max(residuals),.1)

    def test_segment_window_and_height(self):
        interval = fit.segment_box_interval((5,0,100),(5,20,0),(0,8,10,12))
        self.assertEqual(interval,(.4,.6))
        self.assertEqual([100*(1-t) for t in interval],[60,40])
        self.assertIsNone(fit.segment_box_interval((20,0),(20,20),(0,8,10,12)))


class ActualConsoleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = fit.audit()

    def test_actual_encoder_centers_and_footprint_datum(self):
        r = self.result
        self.assertTrue(r['encoder_pass'])
        self.assertEqual(r['pitches_mm'],[26]*7)
        self.assertEqual(r['encoders'][0]['axis_kicad'],(35,179))
        self.assertEqual(r['transformation']['translation_mm'],(44,371))
        self.assertEqual(r['encoders'][-1]['axis_panel'],(261,192))

    def test_strip_clears_approved_panel_reservations(self):
        checks = self.result['plan_checks']
        self.assertEqual(checks['PCB-F']['overlap_mm2'],0)
        self.assertEqual(checks['PCB-F']['clearance_mm'],4)
        self.assertEqual(checks['M4 reserva doc28 (ventana +5 mm)']['overlap_mm2'],0)
        self.assertEqual(checks['M4 reserva doc28 (ventana +5 mm)']['clearance_mm'],1)
        self.assertEqual(checks['Ventana M4']['clearance_mm'],6)
        self.assertEqual(checks['M4 nominal CSV']['clearance_mm'],7)
        self.assertAlmostEqual(checks['Pantalla módulo']['clearance_mm'],207.88-204.6)
        self.assertTrue(self.result['plan_pass'])

    def test_stacked_height_and_cable_crossings(self):
        r = self.result
        self.assertEqual(r['heights']['pcba_base_relative_mm'],-20)
        self.assertAlmostEqual(r['heights']['pcba_to_pcbf_substrate_gap_mm'],8.9)
        self.assertEqual(r['through_hole_pins_under_f'],[])
        self.assertTrue(r['cable_pass'])
        for route in r['routes']:
            self.assertFalse(route['crossings']['Ventana M4']['crosses_xy'])
            self.assertEqual(route['straight_hits_pcba_substrate'], route['source']=='J3')
            self.assertEqual(route['crossings']['Pantalla módulo']['crosses_xy'],route['source']!='J3')
            if route['source']!='J3':
                self.assertAlmostEqual(route['crossings']['Pantalla módulo']['below_model_mm'],
                                       {'J1': 19.7216833667, 'J2': 10.6980740741}[route['source']])

    def test_generated_svg_is_complete_xml(self):
        root = ET.parse(fit.HERE/'pcba-fit.svg').getroot()
        texts=' '.join(n.text or '' for n in root.iter())
        for name in ('PCB-F','PCB-P','PCB-M','E1','E8','J4 DNP','Torres Ø10'):
            self.assertIn(name,texts)

    def test_extraction_matches_pcbnew_when_available(self):
        try:
            import pcbnew
        except ImportError:
            self.skipTest('Run with the supplied KiCad Python for an independent extraction check')
        native = pcbnew.LoadBoard(str(fit.CONSOLE/'boards/pcb-a/pcb-a.kicad_pcb'))
        parsed,outline,thickness = fit.board(fit.CONSOLE/'boards/pcb-a/pcb-a.kicad_pcb')
        self.assertEqual(len(parsed),len(native.GetFootprints()))
        self.assertAlmostEqual(thickness,pcbnew.ToMM(native.GetDesignSettings().GetBoardThickness()))
        actual_edges = set()
        for d in native.GetDrawings():
            if d.GetLayer()==pcbnew.Edge_Cuts:
                actual_edges.add(tuple(sorted((tuple(pcbnew.ToMM(d.GetStart())),tuple(pcbnew.ToMM(d.GetEnd()))))))
        self.assertEqual(actual_edges,{tuple(sorted(e)) for e in fit.edges(outline)})
        for fp in native.GetFootprints():
            ref = fp.GetReference()
            p = parsed[ref]
            self.assertLess(math.dist(p['origin'],tuple(pcbnew.ToMM(fp.GetPosition()))),1e-6)
            self.assertAlmostEqual(p['angle_deg'],fp.GetOrientationDegrees())
            self.assertEqual(p['layer']=='B.Cu',fp.IsFlipped())
            native_pads = {str(pad.GetNumber()):tuple(pcbnew.ToMM(pad.GetPosition())) for pad in fp.Pads()}
            for pad in fit.children(p['node'],'pad'):
                if pad[1].isdigit():
                    self.assertLess(math.dist(fit.fp_point(p,fit.xy(pad,'at')),native_pads[pad[1]]),1e-6)
            if ref.startswith('RE'):
                native_centers = [tuple(pcbnew.ToMM(g.GetCenter())) for g in fp.GraphicalItems()
                                  if hasattr(g,'GetShape') and g.GetShape()==pcbnew.SHAPE_T_CIRCLE and abs(pcbnew.ToMM(g.GetRadius())-3)<1e-6]
                self.assertTrue(native_centers)
                for center in native_centers:
                    self.assertLess(math.dist(fit.encoder_axis(p)[1],center),1e-6)


if __name__=='__main__':
    unittest.main()
