import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "match_group_labels", Path(__file__).resolve().parents[1] / "experiments" / "match_group_labels.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class GroupLabelMatchingTests(unittest.TestCase):
    def test_render_order_can_differ_from_graph_order(self):
        self.assertEqual(
            module.match_labels([(44, 10, "输出"), (44, 30, "颜色")], [("color", "颜色"), ("out", "输出")]),
            [(44, 10, "out"), (44, 30, "color")],
        )

    def test_duplicate_name_is_rejected(self):
        with self.assertRaises(ValueError):
            module.match_labels([(44, 10, "A"), (44, 30, "A")], [("p1", "A"), ("p2", "A")])

    def test_hidden_or_truncated_labels_are_rejected(self):
        for circles, ports in [([(44, 10, "A")], [("p1", "A"), ("p2", "B")]),
                               ([(44, 10, "A...")], [("p1", "A longer name")])]:
            with self.subTest(circles=circles), self.assertRaises(ValueError):
                module.match_labels(circles, ports)


if __name__ == "__main__":
    unittest.main()
