import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
import ghost_routines as gr
import trace_ghosts as tg


class RunRoutine(unittest.TestCase):
    def machine_with_corner_routine(self):
        mc = Machine()
        # CMP #$01 ; BEQ +3 ; LDA #$00 ; RTS ; LDA #$02 ; RTS   (same shape as the game's routine 0)
        mc.m[0x400:0x400 + 10] = bytes([0xC9, 0x01, 0xF0, 0x03, 0xA9, 0x00, 0x60, 0xA9, 0x02, 0x60])
        return mc

    def test_returns_answer_in_a(self):
        mc = self.machine_with_corner_routine()
        self.assertEqual(gr.run_routine(mc, 0x400, 1, 0, 0)[0], 2)
        self.assertEqual(gr.run_routine(mc, 0x400, 0, 0, 0)[0], 0)

    def test_ghost_position_is_set_for_the_routine(self):
        mc = Machine()
        mc.m[0x400:0x405] = bytes([0xE4, 0x70, 0xB0, 0x00, 0x60])       # CPX $70 ; BCS +0 ; RTS
        gr.run_routine(mc, 0x400, 0, 0, 0)
        self.assertEqual((mc.m[0x70], mc.m[0x71]), gr.GHOST)

    def test_dependency_and_outputs(self):
        mc = self.machine_with_corner_routine()
        t = gr.table(mc, 0x400)
        self.assertEqual(gr.dependency(t), 'dir')                       # answer depends on the direction only
        self.assertEqual(gr.outputs_by_dir(t), {0: [0], 1: [2], 2: [0], 3: [0]})

    def test_dependency_on_x_and_y(self):
        # LDA #0 ; CPX $70 ; BCC +1 ; ... : answer differs with X relative to $70
        mc = Machine()
        mc.m[0x400:0x40B] = bytes([0xE4, 0x70, 0x90, 0x03, 0xA9, 0x01, 0x60, 0xA9, 0x00, 0x60, 0x00])
        t = gr.table(mc, 0x400)
        self.assertEqual(gr.dependency(t), 'x')


class Dispatch(unittest.TestCase):
    def test_check_dispatch_counts_matches_and_mismatches(self):
        index = [0, 1] * 128                      # type*4 + ghost - 1 -> routine number
        words = [0x8E1E, 0x8E28]
        events = [
            {'decided': True, 'routine': 0x8E1E, 'type73': 0, 'ghost': 1},   # index 0 -> routine 0 -> ok
            {'decided': True, 'routine': 0x8E1E, 'type73': 0, 'ghost': 2},   # index 1 -> routine 1 -> mismatch
            {'decided': False, 'type73': 0, 'ghost': 1},                      # not decided: ignored
            {'decided': True, 'type73': 0, 'ghost': 1},                       # no routine recorded: ignored
        ]
        self.assertEqual(tg.check_dispatch(events, index, words), (2, 1))

    def test_routine_number(self):
        self.assertEqual(tg.routine_number(0x8E28, [0x8E1E, 0x8E28]), 1)
        self.assertIsNone(tg.routine_number(0x1234, [0x8E1E]))


class Hook(unittest.TestCase):
    def test_events_record_entry_decision_and_result(self):
        mc = Machine()
        for a in (tg.ENTRY, tg.DECIDE, tg.AFTER_CALL):
            mc.m[a] = 0xEA                          # NOPs at the three hook addresses
        mc.m[0x73], mc.m[0xE0], mc.m[0x72], mc.m[0x78] = 12, 3, 2, 1
        mc.m[0x8CA7], mc.m[0x8CA8] = 0x1E, 0x8E
        events = tg.install(mc)
        mc.cpu.pc, mc.cpu.a = tg.ENTRY, 0
        mc.cpu.step()
        mc.cpu.pc, mc.cpu.a = tg.DECIDE, 0
        mc.cpu.step()
        mc.cpu.pc, mc.cpu.a = tg.AFTER_CALL, 3
        mc.cpu.step()
        self.assertEqual(len(events), 1)
        e = events[0]
        self.assertEqual((e['type73'], e['ghost'], e['dir72'], e['f78'], e['decided'], e['routine'], e['result']),
                         (12, 3, 2, 1, True, 0x8E1E, 3))


if __name__ == '__main__':
    unittest.main()
