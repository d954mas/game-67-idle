"""Scene, material, primitive and version helpers for Blender build scripts.

Space is Blender's (Z up, metres). glTF is Y up with +Z as the model's front, which lands on
Blender -Y; `gltf_to_blender`/`blender_to_gltf` are the only axis conversions scripts need.
Every Blender-version difference lives here, so scripts never branch on `bpy.app.version`.
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

from ai_studio.assets.tools.blender.lib.color import as_linear

BLENDER_5 = bpy.app.version >= (5, 0, 0)


# --- version compatibility --------------------------------------------------------------------

def eevee_engine():
    """4.2-4.4 call EEVEE 'BLENDER_EEVEE_NEXT'; 5.x renamed it back to 'BLENDER_EEVEE'."""
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    return "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"


def compositor_tree(scene=None):
    """The scene's compositor node tree, created if missing; compositing is switched on.

    5.x keeps it in a node group (`scene.compositing_node_group`, output = Group Output);
    4.x in `scene.node_tree` (output = Composite). `compositor_output` makes the right node.
    """
    scene = scene or bpy.context.scene
    scene.render.use_compositing = True
    if hasattr(scene, "compositing_node_group"):
        tree = scene.compositing_node_group
        if tree is None:
            tree = bpy.data.node_groups.new(f"{scene.name} compositor", "CompositorNodeTree")
            tree.interface.new_socket(name="Image", in_out="OUTPUT", socket_type="NodeSocketColor")
            scene.compositing_node_group = tree
        return tree
    scene.use_nodes = True
    return scene.node_tree


def compositor_output(tree):
    if hasattr(bpy.context.scene, "compositing_node_group"):
        return tree.nodes.new("NodeGroupOutput")
    return tree.nodes.new("CompositorNodeComposite")


def node_tree(owner):
    """Material/world node tree; 5.x always has one and deprecates `use_nodes`."""
    if owner.node_tree is None and not BLENDER_5:
        owner.use_nodes = True
    return owner.node_tree


def find_node(tree, node_type):
    """First node of a type ('BSDF_PRINCIPLED', 'BACKGROUND', ...); names are localised."""
    return next((n for n in tree.nodes if n.type == node_type), None)


# --- scene ------------------------------------------------------------------------------------

def reset_scene(units="METRIC"):
    """Empty factory scene: no default cube, camera or light, so nothing leaks into exports."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = units
    scene.unit_settings.scale_length = 1.0
    return scene


def collection(name, parent=None):
    """Existing collection `name` or a new one linked under `parent` (default: scene root)."""
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
    parent = parent or bpy.context.scene.collection
    if coll.name not in parent.children:
        parent.children.link(coll)
    return coll


def link(obj, coll=None):
    """Move `obj` into exactly one collection."""
    coll = coll or bpy.context.scene.collection
    for other in list(obj.users_collection):
        if other != coll:
            other.objects.unlink(obj)
    if obj.name not in coll.objects:
        coll.objects.link(obj)
    return obj


def look_at(obj, target, up="Y"):
    direction = Vector(target) - obj.matrix_world.translation
    obj.rotation_euler = direction.to_track_quat("-Z", up).to_euler()
    return obj


# --- materials --------------------------------------------------------------------------------

def palette_material(name, colour, roughness=0.8, metallic=0.0):
    """Principled material with a flat colour ('#hex' sRGB or linear tuple); reused by name."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgba = (tuple(as_linear(colour)) + (1.0,))[:4]
    bsdf = find_node(node_tree(mat), "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = rgba
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    mat.diffuse_color = rgba
    return mat


def vertex_colour_material(name, layer=""):
    """Principled material whose base colour reads a colour attribute ('' = the active one)."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    tree = node_tree(mat)
    bsdf = find_node(tree, "BSDF_PRINCIPLED")
    attr = find_node(tree, "VERTEX_COLOR") or tree.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = layer
    tree.links.new(attr.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    return mat


def emission_material(name, colour, strength=1.0):
    """Unlit flat colour: overrides, masks, silhouettes."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    tree = node_tree(mat)
    tree.nodes.clear()
    emit = tree.nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (tuple(as_linear(colour)) + (1.0,))[:4]
    emit.inputs["Strength"].default_value = strength
    out = tree.nodes.new("ShaderNodeOutputMaterial")
    tree.links.new(emit.outputs[0], out.inputs["Surface"])
    return mat


def studio_world(scene=None, mode="studio_hdr", hdr=None, strength=1.0):
    """World lighting for reviews: the shared studio HDR, or 'flat' uniform white ambient.

    Without the HDR file, falls back to a key and a fill sun so faces still read apart.
    Returns the mode actually used.
    """
    scene = scene or bpy.context.scene
    world = scene.world or bpy.data.worlds.new("World")
    scene.world = world
    tree = node_tree(world)
    bg = find_node(tree, "BACKGROUND")
    for node in [n for n in tree.nodes if n.type == "TEX_ENVIRONMENT"]:
        tree.nodes.remove(node)
    if mode == "studio_hdr":
        if hdr is None:
            from ai_studio.assets.tools.blender.lib.studio_root import find_studio_root
            hdr = find_studio_root() / "ai_studio" / "assets" / "previews" / "studio_env.hdr"
        if hdr and os.path.isfile(str(hdr)):
            env = tree.nodes.new("ShaderNodeTexEnvironment")
            env.image = bpy.data.images.load(str(hdr), check_existing=True)
            tree.links.new(env.outputs["Color"], bg.inputs["Color"])
            bg.inputs["Strength"].default_value = strength
            return "studio_hdr"
        for label, energy, rot in (("key", 2.6, (40, 6, -55)), ("fill", 0.6, (62, 0, 125))):
            sun = bpy.data.objects.new(label, bpy.data.lights.new(label, "SUN"))
            sun.data.energy = energy
            sun.rotation_euler = [math.radians(a) for a in rot]
            scene.collection.objects.link(sun)
        bg.inputs["Color"].default_value = (1, 1, 1, 1)
        bg.inputs["Strength"].default_value = 0.4
        return "suns"
    bg.inputs["Color"].default_value = (1, 1, 1, 1)
    bg.inputs["Strength"].default_value = strength
    return "flat"


# --- primitives -------------------------------------------------------------------------------

def _object_from_bmesh(bm, name, location, rotation, material, coll, smooth, anchor_z):
    if anchor_z:
        bmesh.ops.translate(bm, vec=Vector((0, 0, anchor_z)), verts=bm.verts[:])
    for face in bm.faces:
        face.smooth = smooth
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    obj.rotation_euler = [math.radians(a) for a in rotation]
    if material is not None:
        mesh.materials.append(material)
    link(obj, coll)
    return obj


def _anchor(anchor, height):
    """Mesh z shift so `location` is the centre ('center') or the bottom centre ('bottom')."""
    if anchor not in ("center", "bottom"):
        raise ValueError(f"anchor must be 'center' or 'bottom', not {anchor!r}")
    return height / 2 if anchor == "bottom" else 0.0


def box(name, size=1.0, location=(0, 0, 0), rotation=(0, 0, 0), material=None, coll=None,
        anchor="center"):
    """Box of `size` (scalar or x, y, z) metres; rotation in degrees."""
    sx, sy, sz = (size,) * 3 if np.isscalar(size) else size
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Diagonal((sx, sy, sz, 1.0)))
    return _object_from_bmesh(bm, name, location, rotation, material, coll, False, _anchor(anchor, sz))


def cylinder(name, radius=0.5, depth=1.0, segments=16, location=(0, 0, 0), rotation=(0, 0, 0),
             material=None, coll=None, radius_top=None, anchor="center", smooth=False):
    """Z-aligned cylinder, or a cone/frustum when `radius_top` differs."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments, radius1=radius,
                          radius2=radius if radius_top is None else radius_top, depth=depth)
    return _object_from_bmesh(bm, name, location, rotation, material, coll, smooth, _anchor(anchor, depth))


def sphere(name, radius=0.5, segments=16, rings=8, location=(0, 0, 0), rotation=(0, 0, 0),
           material=None, coll=None, smooth=True):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=radius)
    return _object_from_bmesh(bm, name, location, rotation, material, coll, smooth, 0.0)


def capsule(name, radius=0.25, height=1.0, segments=12, rings=8, location=(0, 0, 0),
            rotation=(0, 0, 0), material=None, coll=None, anchor="center", smooth=True):
    """Z-aligned capsule of total `height` (caps included)."""
    if height < 2 * radius:
        raise ValueError(f"capsule height {height} is under its diameter {2 * radius}")
    bm = bmesh.new()
    # An odd ring count leaves no vertex on the equator, so the band of faces crossing it
    # stretches into the cylindrical side when the two halves move apart.
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings | 1, radius=radius)
    half = height / 2 - radius
    for vert in bm.verts:
        vert.co.z += half if vert.co.z > 0 else -half
    return _object_from_bmesh(bm, name, location, rotation, material, coll, smooth, _anchor(anchor, height))


def prism(name, outline, depth=1.0, location=(0, 0, 0), rotation=(0, 0, 0), material=None,
          coll=None, anchor="center"):
    """Extrude a closed 2D outline [(x, y), ...] (counter-clockwise) along Z by `depth`."""
    bm = bmesh.new()
    low = [bm.verts.new((x, y, -depth / 2)) for x, y in outline]
    high = [bm.verts.new((x, y, depth / 2)) for x, y in outline]
    bm.faces.new(low[::-1])
    bm.faces.new(high)
    for i in range(len(outline)):
        j = (i + 1) % len(outline)
        bm.faces.new((low[i], low[j], high[j], high[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return _object_from_bmesh(bm, name, location, rotation, material, coll, False, _anchor(anchor, depth))


# --- axes, transforms, measurement ------------------------------------------------------------

def gltf_to_blender(v):
    return (v[0], -v[2], v[1])


def blender_to_gltf(v):
    return (v[0], v[2], -v[1])


def apply_transforms(obj, location=True, rotation=True, scale=True):
    """Bake the object's transform into its mesh (children keep their world placement)."""
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj],
                                   selected_editable_objects=[obj]):
        bpy.ops.object.transform_apply(location=location, rotation=rotation, scale=scale)
    return obj


def _meshes(objs):
    if objs is None:
        objs = [o for o in bpy.context.scene.objects if o.visible_get()]
    return [o for o in objs if o.type == "MESH"]


def evaluated_points(objs=None):
    """World-space vertex positions after modifiers, armatures and shape keys (N x 3, Blender axes)."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    chunks = []
    for obj in _meshes(objs):
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        co = np.empty(len(mesh.vertices) * 3)
        mesh.vertices.foreach_get("co", co)
        evaluated.to_mesh_clear()
        if len(co):
            m = np.array(evaluated.matrix_world)
            chunks.append(co.reshape(-1, 3) @ m[:3, :3].T + m[:3, 3])
    return np.vstack(chunks) if chunks else np.zeros((0, 3))


def tri_count(objs=None):
    """Triangles the exporter will write: evaluated meshes, visible ones by default."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    total = 0
    for obj in _meshes(objs):
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        mesh.calc_loop_triangles()
        total += len(mesh.loop_triangles)
        evaluated.to_mesh_clear()
    return total


def world_bounds(objs=None):
    """(min, max) Vectors of the evaluated meshes in Blender world space."""
    points = evaluated_points(objs)
    if not len(points):
        raise ValueError("no mesh vertices to measure")
    return Vector(points.min(axis=0)), Vector(points.max(axis=0))
