import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from apple2 import Machine
import trace_house as th
import trace_eyes as te
from junction_graph import follow_table, parse_records, NONE, DOWN, UP, RIGHT, LEFT


def rec(x, y, down=NONE, up=NONE, right=NONE, left=NONE):
    return bytes([x, y, down, up, right, left, 0, 0])


def chain():
    # 0 -down-> 1 -down-> 2 (2 is the "door"); 3 is an island that points at nothing valid
    return parse_records(rec(10, 10, down=1) + rec(10, 20, up=0, down=2) + rec(10, 30, up=1) + rec(50, 50),
                         table=0x4000, count=4, origin=0x4000)


class EyesRoute(unittest.TestCase):
    def test_follow_table_counts_steps_and_flags_broken_walks(self):
        g = chain()
        walk = follow_table(g, [DOWN, DOWN, UP, DOWN], doors=(2,))
        self.assertEqual(walk, {0: 2, 1: 1, 2: 0, 3: None})            # junction 3 has no exits: broken

    def test_loops_and_non_exits_are_reported_as_broken(self):
        g = parse_records(rec(10, 10, down=1) + rec(10, 20, up=0), table=0x4000, count=2, origin=0x4000)
        walk = follow_table(g, [DOWN, UP], doors=(5,))                   # 0 -> 1 -> 0 -> ... never reaches a door
        self.assertEqual(walk, {0: None, 1: None})
        self.assertEqual(follow_table(g, [UP, UP], doors=(5,))[0], None) # direction with no exit

    def test_route_extracts_eyes_decisions_of_one_ghost(self):
        events = [{'ghost': 1, 'f79': 1, 'type73': 2, 'dir72': 3}, {'ghost': 1, 'f79': 0, 'type73': 5, 'dir72': 0},
                  {'ghost': 2, 'f79': 1, 'type73': 7, 'dir72': 1}, {'ghost': 1, 'f79': 1, 'type73': 4, 'dir72': 2}]
        table = [0, 1, 2, 3, 0, 1, 2, 3]
        self.assertEqual(te.route(events, 1, table), [(2, 2, 3), (4, 0, 2)])

    def test_make_eyes_sets_the_flags_in_the_right_record(self):
        mc = Machine()
        mc.m[0x98 + 8] = 1
        te.make_eyes(mc, 2)
        self.assertEqual((mc.m[0x98 + 9], mc.m[0x98 + 8], mc.m[0x84 + 9]), (1, 0, 0))


class HouseWatch(unittest.TestCase):
    def test_changes_of_record_fields_are_logged_per_ghost(self):
        mc = Machine()
        # LDA #0 ; STA $AB (ghost 3 record $AC? no: $98+17 = $A9 = ghost 2 in-house flag) ; LDA #5 ; STA $D2 (ghost 4 countdown)
        mc.m[0xA9] = 1
        mc.m[0x300:0x30A] = bytes([0xA9, 0x00, 0x85, 0xA9, 0xA9, 0x05, 0x85, 0xD2, 0xEA, 0xEA])
        mc.cpu.pc = 0x300
        events = th.install(mc)
        mc.run(4)
        got = [(e['ghost'], e['field'], e['old'], e['new']) for e in events]
        self.assertEqual(got, [(2, 'in_house($81)', 1, 0), (4, 'countdown($82)', 0, 5)])


if __name__ == '__main__':
    unittest.main()
