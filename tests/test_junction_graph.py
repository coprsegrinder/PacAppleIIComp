import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from junction_graph import parse_records, exits, check_graph, routine_exit_check, NONE, DOWN, UP, RIGHT, LEFT


def rec(x, y, down=NONE, up=NONE, right=NONE, left=NONE):
    return bytes([x, y, down, up, right, left, 0, 0])


def two_by_two():
    # 0 (10,10)  1 (20,10)
    # 2 (10,20)  3 (20,20)
    return parse_records(rec(10, 10, down=2, right=1) + rec(20, 10, down=3, left=0) + rec(10, 20, up=0, right=3) + rec(20, 20, up=1, left=2),
                         table=0x4000, count=4, origin=0x4000)


class Graph(unittest.TestCase):
    def test_parse_and_exits(self):
        g = two_by_two()
        self.assertEqual((g[0]['x'], g[0]['y'], g[0]['next']), (10, 10, [2, NONE, 1, NONE]))
        self.assertEqual(exits(g[0]), {DOWN, RIGHT})
        self.assertEqual(exits(g[3]), {UP, LEFT})

    def test_consistent_graph_has_no_problems(self):
        p = check_graph(two_by_two())
        self.assertEqual((p['missing_target'], p['not_symmetric'], p['geometry']), ([], [], []))

    def test_detects_one_way_missing_and_wrong_geometry(self):
        g = two_by_two()
        g[1]['next'][LEFT] = NONE                      # 0 -> 1 right, but 1 has no way back
        self.assertEqual(check_graph(g)['not_symmetric'], [(0, RIGHT, 1)])
        g = two_by_two()
        g[0]['next'][DOWN] = 9                         # junction 9 does not exist
        self.assertEqual(check_graph(g)['missing_target'], [(0, DOWN, 9)])
        g = two_by_two()
        g[2]['y'] = 5                                  # "down" neighbour now above
        self.assertTrue(check_graph(g)['geometry'])

    def test_tunnel_wrap_shows_as_a_geometry_exception_only(self):
        g = parse_records(rec(10, 10, left=1) + rec(90, 10, right=0), table=0x4000, count=2, origin=0x4000)
        p = check_graph(g)
        self.assertEqual(len(p['geometry']), 2)
        self.assertEqual(p['not_symmetric'], [])

    def test_routine_answers_must_be_real_exits(self):
        g = two_by_two()
        index = [0, 0, 0, 0] * 4                      # every junction/ghost uses routine 0
        ok = routine_exit_check(g, index, {0: {DOWN}})
        self.assertEqual(ok[0], 16)
        # DOWN is an exit of junctions 0 and 1 only; junctions 2 and 3 (4 ghosts each) violate it
        self.assertEqual(sorted({v[0] for v in ok[1]}), [2, 3])
        self.assertEqual(len(ok[1]), 8)
        # {DOWN, UP}: UP is an exit of junctions 2 and 3 only, so with DOWN also required 0,1 violate UP and 2,3 violate DOWN
        bad = routine_exit_check(g, index, {0: {DOWN, UP}})
        self.assertEqual(sorted({v[0] for v in bad[1]}), [0, 1, 2, 3])


if __name__ == '__main__':
    unittest.main()
