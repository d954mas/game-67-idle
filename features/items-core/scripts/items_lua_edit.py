#!/usr/bin/env python3
"""Restricted source-preserving edits for existing canonical Items Lua scalars."""

from __future__ import annotations

from dataclasses import dataclass
import re


MAX_EXACT_INTEGER = 9_007_199_254_740_991
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
DECIMAL_INTEGER = re.compile(r"[0-9]+")


class EditFailure(ValueError):
    pass


def _fail(code: str, message: str) -> None:
    raise EditFailure(f"{code}: {message}")


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    start: int
    end: int
    line: int


@dataclass(frozen=True)
class EditResult:
    source: str
    old_value: int
    start: int
    end: int


def _long_bracket_end(source: str, start: int) -> int | None:
    match = re.match(r"\[(=*)\[", source[start:])
    if match is None:
        return None
    closing = "]" + match.group(1) + "]"
    end = source.find(closing, start + match.end())
    if end < 0:
        _fail("edit.tokenize", "unterminated long bracket")
    return end + len(closing)


def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    index = 0
    line = 1
    length = len(source)
    while index < length:
        char = source[index]
        if char.isspace():
            line += char == "\n"
            index += 1
            continue
        if source.startswith("--", index):
            long_end = _long_bracket_end(source, index + 2)
            end = long_end if long_end is not None else source.find("\n", index + 2)
            if end < 0:
                end = length
            line += source.count("\n", index, end)
            index = end
            continue
        if char in {'"', "'"}:
            start, token_line, quote = index, line, char
            index += 1
            escaped = False
            while index < length:
                current = source[index]
                if current == "\n":
                    line += 1
                if escaped:
                    escaped = False
                elif current == "\\":
                    escaped = True
                elif current == quote:
                    index += 1
                    break
                index += 1
            else:
                _fail("edit.tokenize", "unterminated quoted string")
            tokens.append(Token("string", source[start:index], start, index, token_line))
            continue
        long_end = _long_bracket_end(source, index) if char == "[" else None
        if long_end is not None:
            token_line = line
            line += source.count("\n", index, long_end)
            tokens.append(Token("string", source[index:long_end], index, long_end, token_line))
            index = long_end
            continue
        identifier = IDENTIFIER.match(source, index)
        if identifier is not None:
            end = identifier.end()
            tokens.append(Token("identifier", source[index:end], index, end, line))
            index = end
            continue
        if char.isdigit():
            start = index
            while index < length and (source[index].isalnum() or source[index] in "._"):
                index += 1
            tokens.append(Token("number", source[start:index], start, index, line))
            continue
        tokens.append(Token("symbol", char, index, index + 1, line))
        index += 1
    return tokens


def _matching(tokens: list[Token], opening: int, left: str, right: str) -> int:
    if opening >= len(tokens) or tokens[opening].text != left:
        _fail("edit.structure", f"expected {left}")
    depth = 0
    for index in range(opening, len(tokens)):
        if tokens[index].text == left:
            depth += 1
        elif tokens[index].text == right:
            depth -= 1
            if depth == 0:
                return index
    _fail("edit.structure", f"unclosed {left}")


def _definition_table(
    tokens: list[Token], definition_line: int, item_id: str,
) -> tuple[int, int]:
    matches = []
    for index in range(len(tokens) - 4):
        if (
            tokens[index].line == definition_line
            and [token.text for token in tokens[index:index + 5]]
            == ["items", ".", "define", "(", "{"]
        ):
            matches.append(index + 4)
    if len(matches) != 1:
        _fail("edit.definition", "source line must identify exactly one items.define table")
    opening = matches[0]
    closing = _matching(tokens, opening, "{", "}")
    id_equals = _direct_assignment(tokens, opening, closing, "id")
    value = tokens[id_equals + 1] if id_equals + 1 < closing else None
    if value is None or value.kind != "string" or value.text not in {f'"{item_id}"', f"'{item_id}'"}:
        _fail("edit.definition", "definition source does not match requested item id")
    return opening, closing


def _depths(tokens: list[Token], opening: int, closing: int):
    curly = paren = square = 0
    for index in range(opening + 1, closing):
        token = tokens[index].text
        yield index, curly, paren, square
        if token == "{":
            curly += 1
        elif token == "}":
            curly -= 1
        elif token == "(":
            paren += 1
        elif token == ")":
            paren -= 1
        elif token == "[":
            square += 1
        elif token == "]":
            square -= 1


def _direct_assignment(
    tokens: list[Token], opening: int, closing: int, name: str,
) -> int:
    matches = [
        index + 1
        for index, curly, paren, square in _depths(tokens, opening, closing)
        if curly == paren == square == 0
        and tokens[index].kind == "identifier" and tokens[index].text == name
        and index + 1 < closing and tokens[index + 1].text == "="
    ]
    if len(matches) != 1:
        _fail("edit.field_not_found", f"expected one direct assignment for {name}")
    return matches[0]


def _call_table(
    tokens: list[Token], value_index: int, owner: str, function: str,
) -> tuple[int, int]:
    expected = [owner, ".", function, "(", "{"]
    if [token.text for token in tokens[value_index:value_index + 5]] != expected:
        _fail("edit.source_shape", f"expected {owner}.{function}({{...}})")
    opening = value_index + 4
    return opening, _matching(tokens, opening, "{", "}")


def _plain_table(tokens: list[Token], value_index: int) -> tuple[int, int]:
    if value_index >= len(tokens) or tokens[value_index].text != "{":
        _fail("edit.source_shape", "expected an explicit table")
    return value_index, _matching(tokens, value_index, "{", "}")


def _indexed_row(
    tokens: list[Token], opening: int, closing: int, level: int,
) -> tuple[int, int]:
    matches: list[tuple[int, int]] = []
    for index, curly, paren, square in _depths(tokens, opening, closing):
        if curly != 0 or paren != 0 or square != 0 or tokens[index].text != "[":
            continue
        if index + 4 >= closing:
            continue
        key, close, equals, row = tokens[index + 1:index + 5]
        if (
            key.kind == "number" and DECIMAL_INTEGER.fullmatch(key.text)
            and int(key.text) == level and close.text == "]" and equals.text == "="
            and row.text == "{"
        ):
            matches.append((index + 4, _matching(tokens, index + 4, "{", "}")))
    if len(matches) != 1:
        _fail("edit.level_not_found", f"expected one explicit row for level {level}")
    return matches[0]


def _integer_literal(tokens: list[Token], index: int) -> tuple[int, int, int]:
    if index >= len(tokens):
        _fail("edit.literal_required", "missing integer literal")
    sign = 1
    start = tokens[index].start
    if tokens[index].text == "-":
        sign = -1
        sign_end = tokens[index].end
        index += 1
        if index >= len(tokens) or tokens[index].start != sign_end:
            _fail("edit.literal_required", "minus sign and decimal digits must be contiguous")
    if index >= len(tokens) or tokens[index].kind != "number" or DECIMAL_INTEGER.fullmatch(tokens[index].text) is None:
        _fail("edit.literal_required", "existing value must be one decimal integer token")
    digits = tokens[index].text
    if (len(digits) > 1 and digits.startswith("0")) or (sign < 0 and digits == "0"):
        _fail("edit.literal_required", "existing integer must use canonical decimal spelling")
    if index + 1 >= len(tokens) or tokens[index + 1].text not in {",", ";", "}"}:
        _fail("edit.literal_required", "existing value must end after one decimal integer token")
    return start, tokens[index].end, sign * int(tokens[index].text)


def _replace(
    source: str, tokens: list[Token], equals: int, value: int,
) -> EditResult:
    if type(value) is not int or value < -MAX_EXACT_INTEGER or value > MAX_EXACT_INTEGER:
        _fail("edit.value", f"value must be an exact integer in +/-{MAX_EXACT_INTEGER}")
    start, end, old_value = _integer_literal(tokens, equals + 1)
    return EditResult(
        source=source[:start] + str(value) + source[end:],
        old_value=old_value,
        start=start,
        end=end,
    )


def level_set(
    source: str, *, definition_line: int, item_id: str,
    level: int, field: str, value: int,
) -> EditResult:
    tokens = tokenize(source)
    definition = _definition_table(tokens, definition_line, item_id)
    levels_equals = _direct_assignment(tokens, *definition, "levels")
    table = _call_table(tokens, levels_equals + 1, "levels", "table")
    row = _indexed_row(tokens, *table, level)
    return _replace(source, tokens, _direct_assignment(tokens, *row, field), value)


def curve_set(
    source: str, *, definition_line: int, item_id: str,
    field: str, parameter: str, value: int,
) -> EditResult:
    if parameter not in {"start", "step"}:
        _fail("edit.parameter", "levels.linear parameter must be start or step")
    tokens = tokenize(source)
    definition = _definition_table(tokens, definition_line, item_id)
    levels_equals = _direct_assignment(tokens, *definition, "levels")
    columns = _call_table(tokens, levels_equals + 1, "levels", "columns")
    field_equals = _direct_assignment(tokens, *columns, field)
    curve = _call_table(tokens, field_equals + 1, "levels", "linear")
    return _replace(source, tokens, _direct_assignment(tokens, *curve, parameter), value)


def override_set(
    source: str, *, definition_line: int, item_id: str,
    level: int, field: str, value: int,
) -> EditResult:
    tokens = tokenize(source)
    definition = _definition_table(tokens, definition_line, item_id)
    levels_equals = _direct_assignment(tokens, *definition, "levels")
    columns = _call_table(tokens, levels_equals + 1, "levels", "columns")
    overrides_equals = _direct_assignment(tokens, *columns, "overrides")
    overrides = _plain_table(tokens, overrides_equals + 1)
    row = _indexed_row(tokens, *overrides, level)
    return _replace(source, tokens, _direct_assignment(tokens, *row, field), value)


def max_level_set(
    source: str, *, definition_line: int, item_id: str,
    value: int, direction: str,
) -> EditResult:
    if direction not in {"append", "truncate"}:
        _fail("edit.direction", "max-level direction must be append or truncate")
    if type(value) is not int or value < 1 or value > MAX_EXACT_INTEGER:
        _fail("edit.value", f"max_level must be between 1 and {MAX_EXACT_INTEGER}")
    tokens = tokenize(source)
    definition = _definition_table(tokens, definition_line, item_id)
    levels_equals = _direct_assignment(tokens, *definition, "levels")
    value_index = levels_equals + 1
    call = [token.text for token in tokens[value_index:value_index + 3]]
    if call == ["levels", ".", "generate"]:
        table = _call_table(tokens, value_index, "levels", "generate")
    elif call == ["levels", ".", "columns"]:
        table = _call_table(tokens, value_index, "levels", "columns")
    else:
        _fail(
            "edit.source_shape",
            "max-level edits require explicit levels.generate or levels.columns",
        )
    equals = _direct_assignment(tokens, *table, "max_level")
    start, end, old_value = _integer_literal(tokens, equals + 1)
    del start, end
    if (direction == "append" and value <= old_value) or (
        direction == "truncate" and value >= old_value
    ):
        _fail("edit.direction", f"{direction} target must move from current max_level {old_value}")
    return _replace(source, tokens, equals, value)
