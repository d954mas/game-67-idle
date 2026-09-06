"""Bake a sourced model's material colour into COLOR_0 and normalise it to the flat-colour contract.

Free CC0 models usually carry their colour in one shared palette atlas. A game that draws
COLOR_0 with no textures at all imports such a model grey, so the colour has to move into the
vertex stream before the model is usable.

Three entry points share one pipeline, so the panel and the command produce the same file:

  add-on   Install the file in Blender, then View3D sidebar > AI Studio > Vertex Colour Bake.
  bake     blender --background --factory-startup --python vertex_color_bake.py -- --input a.glb ...
  audit    python vertex_color_bake.py --audit shipped.glb

The audit path is deliberately free of Blender so a shipped file can be checked anywhere.
"""

import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

bl_info = {
    "name": "AI Studio Vertex Colour Bake",
    "author": "AI Studio",
    "version": (1, 0, 0),
    "blender": (4, 2, 0),
    "location": "View3D > Sidebar > AI Studio",
    "description": "Bake material and palette-atlas colour into COLOR_0 and export a contract-clean GLB.",
    "category": "Import-Export",
}

try:
    import bpy
    import mathutils
except ImportError:  # plain python: only the audit half of the file is usable
    bpy = None
    mathutils = None

COLOUR_LAYER = "Color"
MATERIAL_NAME = "Vertex colour"
SAMPLE_MODES = ("face", "corner")
FILTERS = ("nearest", "bilinear")
# Fractions of the axis-aligned bounds, in Blender's Z-up axes, that each preset lands on the origin.
PIVOTS = {"floor": (.5, .5, 0), "center": (.5, .5, .5), "top": (.5, .5, 1), "keep": None}
MODEL_IMPORTERS = {".glb": "gltf", ".gltf": "gltf", ".fbx": "fbx", ".obj": "obj"}

MAGIC = 0x46546C67
JSON_CHUNK = 0x4E4F534A
BINARY_CHUNK = 0x004E4942
FLOAT = 5126
COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
FORMAT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", FLOAT: "f"}
# A normal an engine can light without renormalising.
NORMAL_TOLERANCE = .01


# --- colour ---------------------------------------------------------------------------------

def srgb_to_linear(value):
    """glTF COLOR_0 is linear while a source atlas is sRGB; skipping this washes the model out."""
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def linear_to_srgb(value):
    return value * 12.92 if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055


def srgb_hex(colour):
    return "#" + "".join(f"{max(0, min(255, round(linear_to_srgb(c) * 255))):02x}" for c in colour[:3])


def texel(pixels, width, column, row, decode_srgb):
    offset = (row * width + column) * 4
    values = pixels[offset:offset + 4]
    if decode_srgb:
        return tuple(srgb_to_linear(v) for v in values[:3]) + tuple(values[3:4])
    return tuple(values)


def sample_texture(pixels, width, height, u, v, texture_filter="nearest", decode_srgb=False):
    """Sample a Blender image buffer (bottom-up RGBA floats) with repeat wrapping.

    Nearest is the default because a palette atlas must return the exact patch colour; bilinear
    across a patch boundary would invent a fill the source never had. Any weighting happens after
    the sRGB decode, since averaging encoded values darkens the result.
    """
    if texture_filter == "bilinear":
        x, y = u * width - .5, v * height - .5
        x0, y0 = math.floor(x), math.floor(y)
        fx, fy = x - x0, y - y0
        result = [0.0, 0.0, 0.0, 0.0]
        for dy, wy in ((0, 1 - fy), (1, fy)):
            for dx, wx in ((0, 1 - fx), (1, fx)):
                values = texel(pixels, width, (x0 + dx) % width, (y0 + dy) % height, decode_srgb)
                for channel in range(4):
                    result[channel] += values[channel] * wx * wy
        return tuple(result)
    return texel(pixels, width, int(math.floor(u * width)) % width,
                 int(math.floor(v * height)) % height, decode_srgb)


# --- geometry contract ----------------------------------------------------------------------

def game_to_blender(vector):
    """The exporter writes Y-up glTF from Z-up Blender, so an offset given in game axes rotates."""
    return (vector[0], -vector[2], vector[1])


def pivot_point(low, high, preset, offset=(0, 0, 0)):
    """Return the Blender-space point that lands on the origin, given a game-axis offset."""
    fractions = PIVOTS[preset]
    if fractions is None:
        base = (0.0, 0.0, 0.0)
    else:
        base = tuple(low[i] + (high[i] - low[i]) * fractions[i] for i in range(3))
    shift = game_to_blender(offset)
    return tuple(base[i] + shift[i] for i in range(3))


def scale_for_height(low, high, height):
    """Uniform scale only: a non-uniform one would leave the exported normals off unit length."""
    if not height:
        return 1.0
    extent = high[2] - low[2]
    if extent <= 0:
        raise ValueError("the model has no height to scale")
    return height / extent


# --- shipped-file contract ------------------------------------------------------------------

def read_glb(path):
    raw = Path(path).read_bytes()
    magic, version, _ = struct.unpack_from("<III", raw, 0)
    if magic != MAGIC or version != 2:
        raise ValueError(f"{path} is not binary glTF 2.0")
    json_length, json_type = struct.unpack_from("<II", raw, 12)
    if json_type != JSON_CHUNK:
        raise ValueError(f"{path} has no JSON chunk")
    document = json.loads(raw[20:20 + json_length])
    offset = 20 + json_length
    binary = b""
    if offset < len(raw):
        binary_length, binary_type = struct.unpack_from("<II", raw, offset)
        if binary_type == BINARY_CHUNK:
            binary = raw[offset + 8:offset + 8 + binary_length]
    return document, binary


def accessor_rows(document, binary, index):
    item = document["accessors"][index]
    view = document["bufferViews"][item["bufferView"]]
    fmt = "<" + FORMAT[item["componentType"]] * COMPONENTS[item["type"]]
    size = struct.calcsize(fmt)
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", size)
    return [struct.unpack_from(fmt, binary, start + i * stride) for i in range(item["count"])], item


def bounds_of(points):
    return {"min": [min(p[i] for p in points) for i in range(3)],
            "max": [max(p[i] for p in points) for i in range(3)]}


def force_rgba_float(path):
    """Repack COLOR_0 as tight float RGBA, since the exporter may emit VEC3 or normalised integers."""
    path = Path(path)
    document, binary = read_glb(path)
    streams = [primitive["attributes"]["COLOR_0"]
               for mesh in document.get("meshes", []) for primitive in mesh["primitives"]
               if "COLOR_0" in primitive["attributes"]]
    if all(document["accessors"][index]["componentType"] == FLOAT
           and document["accessors"][index]["type"] == "VEC4"
           and not document["accessors"][index].get("normalized") for index in streams):
        return {"rewritten": False, "streams": len(streams)}
    payload = bytearray(binary)
    for index in streams:
        item = document["accessors"][index]
        values, _ = accessor_rows(document, binary, index)
        divisor = {5121: 255, 5123: 65535}.get(item["componentType"], 1) if item.get("normalized") else 1
        packed = bytearray()
        for row in values:
            channels = [v / divisor for v in row] + [1.0] * (4 - len(row))
            packed.extend(struct.pack("<4f", *channels[:4]))
        while len(payload) % 4:
            payload.append(0)
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(payload),
                                        "byteLength": len(packed), "target": 34962})
        payload.extend(packed)
        item.update(bufferView=len(document["bufferViews"]) - 1, byteOffset=0,
                    componentType=FLOAT, type="VEC4")
        item.pop("normalized", None)
    document["buffers"][0]["byteLength"] = len(payload)
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * ((-len(encoded)) % 4)
    payload += b"\0" * ((-len(payload)) % 4)
    output = struct.pack("<III", MAGIC, 2, 28 + len(encoded) + len(payload))
    output += struct.pack("<II", len(encoded), JSON_CHUNK) + encoded
    output += struct.pack("<II", len(payload), BINARY_CHUNK) + payload
    pending = path.with_suffix(".rgba.pending")
    pending.write_bytes(output)
    pending.replace(path)
    return {"rewritten": True, "streams": len(streams)}


def audit_glb(path, node_name=None):
    """One identity node, float RGBA colour, unit normals, no texture payload."""
    path = Path(path)
    failures = []

    def require(condition, check, **detail):
        if not condition:
            failures.append({"check": check, **detail})
        return condition

    document, binary = read_glb(path)
    external = [item["uri"] for group in ("buffers", "images")
                for item in document.get(group, []) if "uri" in item]
    require(not external, "no external URIs", uris=external)
    require(not document.get("images"), "no images", images=len(document.get("images", [])))
    require(not document.get("textures"), "no textures", textures=len(document.get("textures", [])))
    nodes, meshes = document.get("nodes", []), document.get("meshes", [])
    require(len(nodes) == 1, "one node", nodes=len(nodes))
    require(len(meshes) == 1, "one mesh", meshes=len(meshes))
    report = {"asset": path.as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
              "bytes": path.stat().st_size, "axes": "Y-up"}
    if len(nodes) == 1 and len(meshes) == 1:
        node, mesh = nodes[0], meshes[0]
        report["node"] = node.get("name")
        require(set(node).isdisjoint({"translation", "rotation", "scale", "matrix"}),
                "node transform is identity")
        if node_name is not None:
            require(node.get("name") == node_name, "node name",
                    value=node.get("name"), expected=node_name)
        require(len(mesh["primitives"]) == 1, "one primitive", primitives=len(mesh["primitives"]))
        primitive = mesh["primitives"][0]
        attributes = primitive["attributes"]
        if require({"POSITION", "NORMAL", "COLOR_0"}.issubset(attributes),
                   "POSITION NORMAL COLOR_0", attributes=sorted(attributes)):
            points, position = accessor_rows(document, binary, attributes["POSITION"])
            normals, _ = accessor_rows(document, binary, attributes["NORMAL"])
            colours, colour = accessor_rows(document, binary, attributes["COLOR_0"])
            indices, _ = accessor_rows(document, binary, primitive["indices"])
            require(position["componentType"] == FLOAT and position["type"] == "VEC3",
                    "POSITION is float VEC3")
            require(colour["componentType"] == FLOAT and colour["type"] == "VEC4",
                    "COLOR_0 is float VEC4", componentType=colour["componentType"], type=colour["type"])
            require(len(indices) % 3 == 0, "triangle list", indices=len(indices))
            require(all(math.isfinite(v) for row in points + normals + colours for v in row),
                    "values are finite")
            require(all(0 <= v <= 1 for row in colours for v in row), "colours in 0..1")
            lengths = [math.sqrt(sum(v * v for v in row)) for row in normals]
            require(lengths and abs(min(lengths) - 1) < NORMAL_TOLERANCE
                    and abs(max(lengths) - 1) < NORMAL_TOLERANCE, "unit normals",
                    min=min(lengths, default=0), max=max(lengths, default=0))
            report.update(vertices=len(points), triangles=len(indices) // 3, bounds=bounds_of(points),
                          distinct_colours=len({tuple(round(c, 6) for c in row) for row in colours}))
    report.update(status="fail" if failures else "pass", failures=failures)
    return report


# --- settings -------------------------------------------------------------------------------

def absolute(value):
    """Blender's file fields hand back blend-relative `//` paths that no other caller understands."""
    if not value:
        return ""
    text = str(value)
    if text.startswith("//") and bpy is not None:
        text = bpy.path.abspath(text)
    return str(Path(text).resolve())


class Settings:
    """One parameter set shared by the panel and the command line."""

    FIELDS = ("source", "output", "name", "height", "pivot", "pivot_offset", "yaw",
              "sample", "texture_filter", "report", "proof", "proof_size")

    def __init__(self, source="", output="", name="", height=0.0, pivot="floor",
                 pivot_offset=(0.0, 0.0, 0.0), yaw=0.0, sample="face", texture_filter="nearest",
                 report="", proof="", proof_size=512):
        if pivot not in PIVOTS:
            raise ValueError(f"unknown pivot {pivot}")
        if sample not in SAMPLE_MODES:
            raise ValueError(f"unknown sample mode {sample}")
        if texture_filter not in FILTERS:
            raise ValueError(f"unknown filter {texture_filter}")
        # Blender resolves a relative render path against the blend file, not the shell, so the
        # panel and the command line only agree once every path is absolute.
        self.source = absolute(source)
        self.output = absolute(output)
        self.name = str(name) or Path(self.output).stem
        self.height = float(height)
        self.pivot = pivot
        self.pivot_offset = tuple(float(v) for v in pivot_offset)
        self.yaw = float(yaw)
        self.sample = sample
        self.texture_filter = texture_filter
        self.report = absolute(report)
        self.proof = absolute(proof)
        self.proof_size = int(proof_size)

    def to_dict(self):
        return {field: getattr(self, field) for field in self.FIELDS}


# --- Blender pipeline -----------------------------------------------------------------------

def _require_blender():
    if bpy is None:
        raise RuntimeError("this entry point has to run inside Blender")


def import_model(path):
    """Import one source file and return the objects it created."""
    _require_blender()
    path = Path(path)
    kind = MODEL_IMPORTERS.get(path.suffix.lower())
    if kind is None:
        raise ValueError(f"unsupported source format {path.suffix}; expected one of "
                         + ", ".join(sorted(MODEL_IMPORTERS)))
    before = set(bpy.data.objects)
    if kind == "gltf":
        bpy.ops.import_scene.gltf(filepath=str(path))
    elif kind == "fbx":
        bpy.ops.import_scene.fbx(filepath=str(path))
    else:
        bpy.ops.wm.obj_import(filepath=str(path))
    return [obj for obj in bpy.data.objects if obj not in before]


def image_buffer(image, cache):
    """Read an image once; the decode flag travels with it because sampling is per face."""
    key = image.name_full
    if key in cache:
        return cache[key]
    width, height = image.size
    pixels = [0.0] * (width * height * 4)
    image.pixels.foreach_get(pixels)
    # Byte buffers hand back their stored sRGB values; float buffers are already scene linear.
    entry = {"pixels": pixels, "width": width, "height": height, "name": image.name,
             "colorspace": image.colorspace_settings.name,
             "decoded_srgb": image.colorspace_settings.name == "sRGB" and not image.is_float}
    cache[key] = entry
    return entry


def colour_source(material, cache):
    """Describe where a material's flat colour comes from, following the usual importer graphs."""
    if material is None:
        return {"kind": "fallback", "colour": (.8, .8, .8), "reason": "no material"}
    if not material.node_tree:
        colour = tuple(material.diffuse_color)[:3]
        return {"kind": "constant", "colour": colour}
    output = next((node for node in material.node_tree.nodes
                   if node.bl_idname == "ShaderNodeOutputMaterial" and node.inputs["Surface"].is_linked),
                  None)
    if output is None:
        return {"kind": "fallback", "colour": tuple(material.diffuse_color)[:3], "reason": "no surface"}
    shader = output.inputs["Surface"].links[0].from_node
    socket = None
    for candidate in ("Base Color", "Color"):
        if candidate in shader.inputs:
            socket = shader.inputs[candidate]
            break
    if socket is None:
        return {"kind": "fallback", "colour": tuple(material.diffuse_color)[:3],
                "reason": f"{shader.bl_idname} has no base colour"}
    return resolve_colour_socket(socket, material, cache)


def resolve_colour_socket(socket, material, cache, depth=0):
    if not socket.is_linked:
        return {"kind": "constant", "colour": tuple(socket.default_value)[:3]}
    node = socket.links[0].from_node
    if depth > 4:
        return {"kind": "fallback", "colour": tuple(material.diffuse_color)[:3], "reason": "graph too deep"}
    if node.bl_idname == "ShaderNodeTexImage":
        if node.image is None:
            return {"kind": "fallback", "colour": tuple(material.diffuse_color)[:3], "reason": "empty texture"}
        return {"kind": "texture", "image": image_buffer(node.image, cache),
                "extension": node.extension}
    if node.bl_idname == "ShaderNodeRGB":
        return {"kind": "constant", "colour": tuple(node.outputs[0].default_value)[:3]}
    if node.bl_idname in ("ShaderNodeVertexColor", "ShaderNodeColorAttribute"):
        return {"kind": "attribute", "name": node.layer_name}
    if node.bl_idname == "ShaderNodeAttribute":
        return {"kind": "attribute", "name": node.attribute_name}
    if node.bl_idname in ("ShaderNodeMixRGB", "ShaderNodeMix"):
        inputs = [socket for socket in node.inputs if socket.type == "RGBA"]
        blend = getattr(node, "blend_type", "MIX")
        if blend == "MULTIPLY" and len(inputs) >= 2:
            return {"kind": "product",
                    "terms": [resolve_colour_socket(item, material, cache, depth + 1) for item in inputs[:2]]}
        if inputs:
            return resolve_colour_socket(inputs[-1], material, cache, depth + 1)
    return {"kind": "fallback", "colour": tuple(material.diffuse_color)[:3],
            "reason": f"unsupported node {node.bl_idname}"}


def evaluate_source(source, uv, existing, loop, vertex, texture_filter):
    kind = source["kind"]
    if kind == "texture":
        buffer = source["image"]
        return sample_texture(buffer["pixels"], buffer["width"], buffer["height"], uv[0], uv[1],
                              texture_filter, buffer["decoded_srgb"])[:3]
    if kind == "product":
        terms = [evaluate_source(term, uv, existing, loop, vertex, texture_filter)
                 for term in source["terms"]]
        return tuple(math.prod(term[channel] for term in terms) for channel in range(3))
    if kind == "attribute":
        values = existing.get(source["name"])
        if values is None:
            return (1.0, 1.0, 1.0)
        return values[loop if values.per_loop else vertex]
    return tuple(source["colour"])[:3]


class _AttributeReader:
    """Read an existing colour attribute per loop, whichever domain it was authored on."""

    def __init__(self, attribute, mesh):
        self.per_loop = attribute.domain == "CORNER"
        count = len(mesh.loops) if self.per_loop else len(mesh.vertices)
        flat = [0.0] * (count * 4)
        attribute.data.foreach_get("color", flat)
        self.values = [tuple(flat[i * 4:i * 4 + 3]) for i in range(count)]

    def __getitem__(self, index):
        return self.values[index]


def bake_object_colour(mesh, materials, settings, cache):
    """Write COLOR_0 on the corner domain so a shared vertex keeps a hard edge between two fills."""
    uv_layer = mesh.uv_layers.active
    existing = {attribute.name: _AttributeReader(attribute, mesh)
                for attribute in mesh.color_attributes}
    sources = [colour_source(material, cache) for material in materials] or [colour_source(None, cache)]
    for source in sources:
        if source["kind"] == "texture" and uv_layer is None:
            source.update(kind="fallback", colour=(.8, .8, .8), reason="textured material without UVs")
    uvs = uv_layer.data if uv_layer else None
    colours = [1.0] * (len(mesh.loops) * 4)
    tally = [{} for _ in sources]
    for polygon in mesh.polygons:
        index = min(polygon.material_index, len(sources) - 1)
        source = sources[index]
        loops = list(polygon.loop_indices)
        if settings.sample == "face":
            if uvs is not None:
                centre = (sum(uvs[loop].uv[0] for loop in loops) / len(loops),
                          sum(uvs[loop].uv[1] for loop in loops) / len(loops))
            else:
                centre = (0.0, 0.0)
            rgb = evaluate_source(source, centre, existing, loops[0],
                                  mesh.loops[loops[0]].vertex_index, settings.texture_filter)
            for loop in loops:
                colours[loop * 4:loop * 4 + 3] = rgb
            key = tuple(round(c, 5) for c in rgb)
            tally[index][key] = tally[index].get(key, 0) + 1
        else:
            for loop in loops:
                uv = uvs[loop].uv if uvs is not None else (0.0, 0.0)
                rgb = evaluate_source(source, uv, existing, loop,
                                      mesh.loops[loop].vertex_index, settings.texture_filter)
                colours[loop * 4:loop * 4 + 3] = rgb
                key = tuple(round(c, 5) for c in rgb)
                tally[index][key] = tally[index].get(key, 0) + 1
    for attribute in list(mesh.color_attributes):
        if attribute.name == COLOUR_LAYER:
            mesh.color_attributes.remove(attribute)
    attribute = mesh.color_attributes.new(name=COLOUR_LAYER, type="FLOAT_COLOR", domain="CORNER")
    attribute.data.foreach_set("color", colours)
    return [describe_source(materials[i] if i < len(materials) else None, sources[i], tally[i])
            for i in range(len(sources))]


def describe_source(material, source, tally):
    fills = sorted(tally.items(), key=lambda item: (-item[1], item[0]))
    row = {"material": material.name if material else None, "source": source["kind"],
           "distinct_fills": len(fills),
           "fills": [{"linear": list(colour), "srgb": srgb_hex(colour), "faces": count}
                     for colour, count in fills[:16]]}
    if source["kind"] == "texture":
        row.update(image=source["image"]["name"], image_size=[source["image"]["width"],
                                                              source["image"]["height"]],
                   colorspace=source["image"]["colorspace"],
                   decoded_srgb=source["image"]["decoded_srgb"])
    if source["kind"] == "fallback":
        row.update(reason=source["reason"], colour=list(source["colour"]))
    if source["kind"] == "attribute":
        row.update(attribute=source["name"])
    return row


def mesh_bounds(mesh):
    coordinates = [0.0] * (len(mesh.vertices) * 3)
    mesh.vertices.foreach_get("co", coordinates)
    points = [coordinates[i:i + 3] for i in range(0, len(coordinates), 3)]
    low = [min(p[i] for p in points) for i in range(3)]
    high = [max(p[i] for p in points) for i in range(3)]
    return low, high


def vertex_colour_material():
    material = bpy.data.materials.new(MATERIAL_NAME)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = .75
    if "Metallic" in shader.inputs:
        shader.inputs["Metallic"].default_value = 0
    attribute = material.node_tree.nodes.new("ShaderNodeVertexColor")
    attribute.layer_name = COLOUR_LAYER
    material.node_tree.links.new(attribute.outputs["Color"], shader.inputs["Base Color"])
    return material


def export_glb(obj, destination):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    destination.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(destination), export_format="GLB", use_selection=True,
                              export_apply=True, export_animations=False, export_skins=False,
                              export_yup=True, export_extras=False, export_materials="EXPORT",
                              export_vertex_color="ACTIVE", export_normals=True, export_tangents=False,
                              export_attributes=False)


def run(settings):
    """Import, bake, normalise, export and audit. The panel and the CLI both land here."""
    _require_blender()
    if not settings.output:
        raise ValueError("an output path is required")
    output = Path(settings.output)
    temporaries = []
    report = {"schema_version": 1, "tool": "vertex_color_bake",
              "blender": bpy.app.version_string, "settings": settings.to_dict()}
    if settings.source:
        source_path = Path(settings.source)
        temporaries = import_model(source_path)
        sources = [obj for obj in temporaries if obj.type == "MESH"]
        report["input"] = {"path": source_path.as_posix(), "bytes": source_path.stat().st_size,
                           "sha256": hashlib.sha256(source_path.read_bytes()).hexdigest()}
    else:
        selected = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
        sources = selected or [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
        report["input"] = {"path": "<scene>", "objects": [obj.name for obj in sources]}
    if not sources:
        raise ValueError("no mesh to bake")

    cache = {}
    depsgraph = bpy.context.evaluated_depsgraph_get()
    parts, materials_report = [], []
    for obj in sources:
        mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph))
        mesh.transform(obj.matrix_world)
        if obj.matrix_world.determinant() < 0:
            mesh.flip_normals()
        slots = [slot.material for slot in obj.material_slots]
        materials_report += bake_object_colour(mesh, slots, settings, cache)
        part = bpy.data.objects.new(f"{obj.name}__bake", mesh)
        bpy.context.scene.collection.objects.link(part)
        parts.append(part)
    report["materials"] = materials_report
    report["input"]["source_meshes"] = len(parts)

    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    merged = bpy.context.view_layer.objects.active
    for obj in temporaries:
        bpy.data.objects.remove(obj, do_unlink=True)

    mesh = merged.data
    if settings.yaw:
        mesh.transform(mathutils.Matrix.Rotation(math.radians(settings.yaw), 4, "Z"))
    low, high = mesh_bounds(mesh)
    scale = scale_for_height(low, high, settings.height)
    if scale != 1.0:
        mesh.transform(mathutils.Matrix.Scale(scale, 4))
    low, high = mesh_bounds(mesh)
    mesh.transform(mathutils.Matrix.Translation(
        -mathutils.Vector(pivot_point(low, high, settings.pivot, settings.pivot_offset))))
    report["normalisation"] = {"scale": scale, "yaw_degrees": settings.yaw, "pivot": settings.pivot,
                               "pivot_offset_game_axes": list(settings.pivot_offset)}

    if settings.proof:
        report["proof"] = render_proof(merged, Path(settings.proof), settings.proof_size)
    else:
        strip_source_material(mesh)

    merged.name = settings.name
    if merged.name != settings.name:
        raise ValueError(f"the scene already holds an object named {settings.name}; rename it first")
    merged.location = (0, 0, 0)
    merged.rotation_euler = (0, 0, 0)
    merged.scale = (1, 1, 1)
    export_glb(merged, output)
    bpy.data.objects.remove(merged, do_unlink=True)

    report["rgba"] = force_rgba_float(output)
    report["contract"] = audit_glb(output, node_name=settings.name)
    report["output"] = {"path": output.as_posix(), "sha256": report["contract"]["sha256"],
                        "bytes": report["contract"]["bytes"],
                        "vertices": report["contract"].get("vertices"),
                        "triangles": report["contract"].get("triangles"),
                        "bounds_game_axes": report["contract"].get("bounds"),
                        "distinct_colours": report["contract"].get("distinct_colours")}
    report["status"] = report["contract"]["status"]
    if settings.report:
        path = Path(settings.report)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


def strip_source_material(mesh):
    """Drop everything the flat-colour contract forbids: textures, UVs and spare colour layers."""
    while mesh.uv_layers:
        mesh.uv_layers.remove(mesh.uv_layers[0])
    for attribute in list(mesh.color_attributes):
        if attribute.name != COLOUR_LAYER:
            mesh.color_attributes.remove(attribute)
    mesh.materials.clear()
    mesh.materials.append(vertex_colour_material())
    for polygon in mesh.polygons:
        polygon.material_index = 0
    mesh.color_attributes.active_color_index = 0
    mesh.color_attributes.render_color_index = 0


# --- proof render ---------------------------------------------------------------------------

def render_proof(obj, destination, size):
    """Render the same geometry twice, textured then vertex-coloured, and measure the difference.

    Both passes use flat Workbench shading and the Standard view transform, so a correct sRGB
    decode makes the two halves identical; a wrong one shows up as a visible colour shift.
    """
    scene = bpy.context.scene
    # Only the baked model may reach the frame, or an unrelated scene object shifts the comparison.
    hidden = [other for other in scene.objects if other is not obj and not other.hide_render]
    for other in hidden:
        other.hide_render = True
    previous = {"engine": scene.render.engine, "resolution": (scene.render.resolution_x,
                                                              scene.render.resolution_y),
                "percentage": scene.render.resolution_percentage,
                "filepath": scene.render.filepath, "camera": scene.camera,
                "view_transform": scene.view_settings.view_transform}
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "Standard"
    shading = scene.display.shading
    shading.light = "FLAT"
    shading.background_type = "VIEWPORT"
    shading.background_color = (.18, .18, .18)
    shading.show_object_outline = False
    shading.show_specular_highlight = False

    low, high = mesh_bounds(obj.data)
    centre = mathutils.Vector([(low[i] + high[i]) / 2 for i in range(3)])
    # The three-quarter view projects the bounding diagonal, not one axis, so frame on that.
    diagonal = max(math.sqrt(sum((high[i] - low[i]) ** 2 for i in range(3))), 1e-4)
    camera_data = bpy.data.cameras.new("PROOF_CAM")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = diagonal * 1.1
    camera_data.clip_start = diagonal * .01
    camera_data.clip_end = diagonal * 10
    camera = bpy.data.objects.new("PROOF_CAM", camera_data)
    scene.collection.objects.link(camera)
    direction = mathutils.Vector((.85, -1, .55)).normalized()
    camera.location = centre + direction * diagonal * 3
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera

    destination.parent.mkdir(parents=True, exist_ok=True)
    before = shoot(scene, shading, "TEXTURE", destination.with_name(destination.stem + "_before.png"))
    strip_source_material(obj.data)
    after = shoot(scene, shading, "VERTEX", destination.with_name(destination.stem + "_after.png"))
    difference = compose(before, after, destination)

    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(camera_data)
    for other in hidden:
        other.hide_render = False
    scene.render.engine = previous["engine"]
    scene.render.resolution_x, scene.render.resolution_y = previous["resolution"]
    scene.render.resolution_percentage = previous["percentage"]
    scene.render.filepath = previous["filepath"]
    scene.camera = previous["camera"]
    scene.view_settings.view_transform = previous["view_transform"]
    difference.update(before=before.as_posix(), after=after.as_posix(),
                      side_by_side=destination.as_posix())
    return difference


def shoot(scene, shading, colour_type, destination):
    shading.color_type = colour_type
    scene.render.filepath = str(destination.with_suffix(""))
    bpy.ops.render.render(write_still=True)
    rendered = Path(scene.render.filepath + ".png")
    if rendered != destination and rendered.exists():
        rendered.replace(destination)
    if not destination.exists():
        raise RuntimeError(f"the {colour_type} proof pass wrote no image")
    return destination


def compose(before, after, destination):
    """Lay the two passes side by side and report how far the vertex colour drifted."""
    left = bpy.data.images.load(str(before))
    right = bpy.data.images.load(str(after))
    width, height = left.size
    a = [0.0] * (width * height * 4)
    b = [0.0] * (width * height * 4)
    left.pixels.foreach_get(a)
    right.pixels.foreach_get(b)
    sheet = bpy.data.images.new("proof", width * 2, height, alpha=False)
    combined = [0.0] * (width * 2 * height * 4)
    total = worst = 0.0
    counted = drifted = 0
    for row in range(height):
        source = row * width * 4
        target = row * width * 2 * 4
        combined[target:target + width * 4] = a[source:source + width * 4]
        combined[target + width * 4:target + width * 8] = b[source:source + width * 4]
        for column in range(width):
            offset = source + column * 4
            delta = max(abs(a[offset + channel] - b[offset + channel]) for channel in range(3))
            total += delta
            counted += 1
            worst = max(worst, delta)
            if delta > 1 / 255:
                drifted += 1
    sheet.pixels.foreach_set(combined)
    sheet.filepath_raw = str(destination)
    sheet.file_format = "PNG"
    sheet.save()
    for image in (left, right, sheet):
        bpy.data.images.remove(image)
    return {"mean_abs_difference": total / max(counted, 1), "max_difference": worst,
            "pixels_over_one_step": drifted, "pixels": counted}


# --- add-on ---------------------------------------------------------------------------------

if bpy is not None:
    class VertexColourBakeSettings(bpy.types.PropertyGroup):
        source: bpy.props.StringProperty(name="Source", subtype="FILE_PATH",
                                         description="Model to import; empty bakes the selection")
        output: bpy.props.StringProperty(name="Output GLB", subtype="FILE_PATH")
        name: bpy.props.StringProperty(name="Node name",
                                       description="Exported node name; empty uses the output stem")
        height: bpy.props.FloatProperty(name="Height (m)", default=0, min=0,
                                        description="Uniform scale target; 0 keeps the source size")
        pivot: bpy.props.EnumProperty(name="Pivot", default="floor",
                                      items=[(key, key.capitalize(), "") for key in PIVOTS])
        pivot_offset: bpy.props.FloatVectorProperty(name="Pivot offset", size=3, subtype="XYZ",
                                                    description="Extra offset in game Y-up metres")
        yaw: bpy.props.FloatProperty(name="Yaw (deg)", default=0)
        sample: bpy.props.EnumProperty(name="Sample", default="face",
                                       items=[(key, key.capitalize(), "") for key in SAMPLE_MODES])
        texture_filter: bpy.props.EnumProperty(name="Filter", default="nearest",
                                               items=[(key, key.capitalize(), "") for key in FILTERS])
        report: bpy.props.StringProperty(name="Report JSON", subtype="FILE_PATH")
        proof: bpy.props.StringProperty(name="Proof PNG", subtype="FILE_PATH")
        proof_size: bpy.props.IntProperty(name="Proof size", default=512, min=64, max=4096)

        def to_settings(self):
            return Settings(source=self.source, output=self.output, name=self.name,
                            height=self.height, pivot=self.pivot,
                            pivot_offset=tuple(self.pivot_offset), yaw=self.yaw, sample=self.sample,
                            texture_filter=self.texture_filter, report=self.report,
                            proof=self.proof, proof_size=self.proof_size)

    class VERTEXCOLOURBAKE_OT_bake(bpy.types.Operator):
        bl_idname = "assetpipe.vertex_colour_bake"
        bl_label = "Bake vertex colour"
        bl_description = "Bake material colour into COLOR_0 and export a contract-clean GLB"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            try:
                report = run(context.scene.vertex_colour_bake.to_settings())
            except Exception as error:  # a modelling mistake must reach the panel, not the console
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            level = "INFO" if report["status"] == "pass" else "ERROR"
            self.report({level}, f"{report['status']}: {report['output']['path']}")
            return {"FINISHED"}

    class VERTEXCOLOURBAKE_PT_panel(bpy.types.Panel):
        bl_idname = "VERTEXCOLOURBAKE_PT_panel"
        bl_label = "Vertex Colour Bake"
        bl_space_type = "VIEW_3D"
        bl_region_type = "UI"
        bl_category = "AI Studio"

        def draw(self, context):
            settings = context.scene.vertex_colour_bake
            column = self.layout.column(align=True)
            for field in ("source", "output", "name", "height", "pivot", "pivot_offset", "yaw",
                          "sample", "texture_filter", "report", "proof", "proof_size"):
                column.prop(settings, field)
            self.layout.operator(VERTEXCOLOURBAKE_OT_bake.bl_idname, icon="COLOR")

    CLASSES = (VertexColourBakeSettings, VERTEXCOLOURBAKE_OT_bake, VERTEXCOLOURBAKE_PT_panel)

    def register():
        for item in CLASSES:
            bpy.utils.register_class(item)
        bpy.types.Scene.vertex_colour_bake = bpy.props.PointerProperty(type=VertexColourBakeSettings)

    def unregister():
        del bpy.types.Scene.vertex_colour_bake
        for item in reversed(CLASSES):
            bpy.utils.unregister_class(item)


# --- command line ---------------------------------------------------------------------------

def parse(argv):
    parser = argparse.ArgumentParser(prog="vertex_color_bake", description=__doc__.splitlines()[0])
    parser.add_argument("--audit", type=Path, nargs="+", help="check shipped GLBs without Blender")
    parser.add_argument("--node-name", help="node name the audit requires; defaults to the file stem")
    parser.add_argument("--input", type=Path, help="source model; omit to bake the open scene")
    parser.add_argument("--output", type=Path, help="destination GLB")
    parser.add_argument("--name", default="", help="exported node name; defaults to the output stem")
    parser.add_argument("--height", type=float, default=0, metavar="METRES",
                        help="uniform scale target; 0 keeps the source size")
    parser.add_argument("--pivot", default="floor", choices=sorted(PIVOTS))
    parser.add_argument("--pivot-offset", default="0,0,0", metavar="X,Y,Z",
                        help="extra pivot offset in game Y-up metres")
    parser.add_argument("--yaw", type=float, default=0, metavar="DEGREES")
    parser.add_argument("--sample", default="face", choices=SAMPLE_MODES)
    parser.add_argument("--filter", dest="texture_filter", default="nearest", choices=FILTERS)
    parser.add_argument("--report", type=Path, help="write the JSON report here")
    parser.add_argument("--proof", type=Path, help="render a before/after sheet to this PNG")
    parser.add_argument("--proof-size", type=int, default=512)
    return parser.parse_args(argv)


def settings_from_args(args):
    offset = tuple(float(value) for value in str(args.pivot_offset).split(","))
    if len(offset) != 3:
        raise ValueError("--pivot-offset takes three comma-separated metres")
    return Settings(source=args.input or "", output=args.output or "", name=args.name,
                    height=args.height, pivot=args.pivot, pivot_offset=offset, yaw=args.yaw,
                    sample=args.sample, texture_filter=args.texture_filter,
                    report=args.report or "", proof=args.proof or "",
                    proof_size=args.proof_size)


def main(argv=None):
    if argv is None:
        argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    args = parse(argv)
    if args.audit:
        reports = [audit_glb(path, args.node_name or path.stem) for path in args.audit]
        status = "fail" if any(item["status"] == "fail" for item in reports) else "pass"
        print("VERTEX_COLOR_BAKE_AUDIT", status,
              json.dumps([{item["asset"]: item["failures"]} for item in reports]))
        return 1 if status == "fail" else 0
    if not args.output:
        raise SystemExit("--output is required")
    _require_blender()
    if args.input and not bpy.data.filepath:
        # Blender's startup scene ships a cube and a light; neither belongs in a sourced bake.
        bpy.ops.wm.read_factory_settings(use_empty=True)
    report = run(settings_from_args(args))
    print("VERTEX_COLOR_BAKE", json.dumps({"status": report["status"],
                                           "output": report["output"]["path"],
                                           "sha256": report["output"]["sha256"],
                                           "vertices": report["output"]["vertices"],
                                           "triangles": report["output"]["triangles"],
                                           "bounds": report["output"]["bounds_game_axes"],
                                           "failures": report["contract"]["failures"]}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
