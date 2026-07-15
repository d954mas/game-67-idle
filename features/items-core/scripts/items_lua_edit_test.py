#!/usr/bin/env python3
"""Proof tests for source-preserving restricted Items Lua scalar edits."""

from __future__ import annotations

import unittest

import items_lua_edit as EDIT


SOURCE = '''local items = require("studio.items")
local levels = require("studio.levels")

-- lookalike: [2] = { attack = 999 }
items.define({
  id = "game.table_sword",
  kind = "weapon",
  stack = 1,
  levels = levels.table({
    [1] = { attack = 10 },
    [2] = { attack = 15, cost_to_reach = items.free() },
  }),
})

items.define({
  id = "game.curve_sword",
  kind = "weapon",
  stack = 1,
  levels = levels.columns({
    max_level = 3,
    attack = levels.linear({ start = 10, step = 5 }),
    overrides = { [3] = { attack = 21 } },
  }),
})
'''


class ItemsLuaEditTests(unittest.TestCase):
    def test_level_cell_replaces_only_existing_integer_token(self):
        changed = EDIT.level_set(
            SOURCE, definition_line=5, item_id="game.table_sword",
            level=2, field="attack", value=17,
        )
        self.assertEqual(changed.old_value, 15)
        self.assertEqual(changed.source.count("attack = 17"), 1)
        self.assertIn("-- lookalike: [2] = { attack = 999 }", changed.source)
        self.assertEqual(changed.source.replace("attack = 17", "attack = 15", 1), SOURCE)

    def test_curve_parameter_and_existing_override_preserve_surrounding_source(self):
        curve = EDIT.curve_set(
            SOURCE, definition_line=15, item_id="game.curve_sword",
            field="attack", parameter="step", value=7,
        )
        self.assertEqual(curve.old_value, 5)
        self.assertIn("levels.linear({ start = 10, step = 7 })", curve.source)

        override = EDIT.override_set(
            curve.source, definition_line=15, item_id="game.curve_sword",
            level=3, field="attack", value=25,
        )
        self.assertEqual(override.old_value, 21)
        self.assertIn("overrides = { [3] = { attack = 25 } }", override.source)

        negative_source = SOURCE.replace("attack = 15", "attack = -15", 1)
        negative = EDIT.level_set(
            negative_source, definition_line=5, item_id="game.table_sword",
            level=2, field="attack", value=-17,
        )
        self.assertEqual(negative.old_value, -15)
        self.assertIn("attack = -17", negative.source)

    def test_refuses_missing_rows_computed_values_and_invalid_integer_range(self):
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.level_not_found"):
            EDIT.level_set(
                SOURCE, definition_line=5, item_id="game.table_sword",
                level=3, field="attack", value=17,
            )
        computed = SOURCE.replace("attack = 15", "attack = studio.math.add(10, 5)")
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.literal_required"):
            EDIT.level_set(
                computed, definition_line=5, item_id="game.table_sword",
                level=2, field="attack", value=17,
            )
        arithmetic = SOURCE.replace("attack = 15", "attack = 10 + 5")
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.literal_required"):
            EDIT.level_set(
                arithmetic, definition_line=5, item_id="game.table_sword",
                level=2, field="attack", value=17,
            )
        numeric_expression = SOURCE.replace("attack = 15", "attack = 10 and 5")
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.literal_required"):
            EDIT.level_set(
                numeric_expression, definition_line=5, item_id="game.table_sword",
                level=2, field="attack", value=17,
            )
        split_negative = SOURCE.replace("attack = 15", "attack = - 15")
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.literal_required"):
            EDIT.level_set(
                split_negative, definition_line=5, item_id="game.table_sword",
                level=2, field="attack", value=17,
            )
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.value"):
            EDIT.level_set(
                SOURCE, definition_line=5, item_id="game.table_sword",
                level=2, field="attack", value=9_007_199_254_740_992,
            )

    def test_definition_line_must_identify_exact_items_define_call(self):
        with self.assertRaisesRegex(EDIT.EditFailure, "edit.definition"):
            EDIT.level_set(
                SOURCE, definition_line=4, item_id="game.table_sword",
                level=2, field="attack", value=17,
            )


if __name__ == "__main__":
    unittest.main()
