---
name: modeling
display_name: Mesh Modeling
description: Model dynamic meshes with native GeometryScript and use ModelingService for session handles, rigging, UV statistics, textures and polyline details.
vibe3d_classes:
  - ModelingService
unreal_classes:
  - DynamicMesh
  - StaticMesh
  - GeometryScript_Primitives
  - GeometryScript_MeshBooleans
  - GeometryScript_UVs
keywords:
  - modeling
  - mesh
  - geometry script
  - primitive
  - boolean
  - extrude
  - remesh
  - uv
  - rigging
  - bake
---
<!-- Copyright Buckley Builds LLC 2026 All Rights Reserved. -->

# Mesh Modeling Skill

Model in editor `execute_python` with native `unreal.GeometryScript_*` libraries. Vibe3D's
`unreal.ModelingService` supplies session handles and the specific gaps listed below; it is no
longer a general wrapper around Modeling Mode. Work on the live `UDynamicMesh` returned by
`get_dynamic_mesh`, then release only your own handles. No viewport or Blender is needed to build,
but inspect the result visually before accepting it.

## Quick start

```python
import unreal
svc = unreal.ModelingService
created = svc.create_mesh()
assert created.success, created.message
h = created.handle
try:
    dm = svc.get_dynamic_mesh(h)
    assert dm is not None
    unreal.GeometryScript_Primitives.append_box(
        dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), 100, 100, 100)
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dm)
    unreal.GeometryScript_MeshRepair.compact_mesh(dm)
    unreal.GeometryScript_UVs.auto_generate_x_atlas_mesh_u_vs(
        dm, 0, unreal.GeometryScriptXAtlasOptions())
    stats = svc.get_uv_stats(h)
    assert stats.success and stats.num_unset_triangles == 0, stats.message
    print(stats.message)
finally:
    svc.release_mesh(h)
```

To create a static asset, call `GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, opts)` and check the returned `outcome == unreal.GeometryScriptOutcomePins.SUCCESS`. For an existing asset, `GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm, sm, unreal.GeometryScriptCopyMeshToAssetOptions(), unreal.GeometryScriptMeshWriteLOD(), True)` returns `(dm, outcome)`; check the same outcome. Establish ownership and replacement policy before writing. A newly created asset is not automatically proof of its appearance, collision or LOD setup.

## Native GeometryScript and Python facts

Use `GeometryScript_Primitives` for box, sphere, cylinder, polygon extrude and sweep;
`GeometryScript_MeshBooleans` for boolean, self-union, plane cut and mirror;
`GeometryScript_MeshSelection` / `MeshSelectionQueries` for selections and bounds;
`GeometryScript_MeshModeling`, `MeshEdits`, `Remeshing`, `MeshRepair`, `MeshSimplification`,
`MeshDeformers` and `MeshVoxelProcessing` for editing and processing. Use `GeometryScript_UVs`,
`Normals`, `PolyGroups`, `Materials`, `VertexColors`, `MeshSpatial`, `MeshQueries`, `Bake`,
`AssetUtils` and `NewAssetUtils` for the corresponding UV, shading, query, bake and asset work.
Use the editor asset/actor APIs for asset management and placement. Discover exact signatures and
options in the UE 5.9 source or reflected API, including `ScriptName` overrides.

Python splits some acronyms unusually: `auto_generate_x_atlas_mesh_u_vs`, `layout_mesh_u_vs`,
`recompute_mesh_u_vs`, `enable_material_i_ds`, `get_all_triangle_material_i_ds`, and
`get_num_triangle_i_ds`. Many calls return `(mesh, value...)`, while `get_max_material_id(m)`
returns `(int, has_material_i_ds)` and selection calls return `(mesh, selection)`; when unsure,
inspect the returned values and pick by type rather than guessing a position. Native
`append_torus` requires a `GeometryScriptRevolveOptions` argument and, like native
`append_revolve_polygon` in 5.9, ignores `PrimitiveOptions.MaterialID`: use the service variants
when those IDs matter. Native box and linear stairs honor that option.

For hard-angle normals set `split_by_opening_angle` and `opening_angle_deg` on
`GeometryScriptSplitNormalsOptions`, then call `GeometryScript_Normals.compute_split_normals`
with that struct and `GeometryScriptCalculateNormalsOptions`.

## ModelingService: kept gaps

| Area | Python functions |
|---|---|
| Session | `create_mesh`, `release_mesh`, `release_all_meshes`, `list_meshes`, `get_dynamic_mesh` (Python only), `set_selection`, `clear_selections` |
| Primitives with material IDs | `append_torus`, `append_revolve_polygon` |
| Rigging | `create_bones`, `bind_selection_to_bone`, `select_by_bone_weight`, `scale_bone_weights`, `smooth_selection_bone_weights`, `inspect_bone_weights` |
| Assets | `save_mesh_to_skeletal_mesh` (empty skeleton path creates `<AssetPath>_Skeleton`), `set_asset_materials` (adds missing slots) |
| UV and texture | `get_uv_stats`, `create_noise_texture`, `draw_on_texture` |
| Detail | `append_mesh_along_polyline`, `cut_groove_along_polyline` |
| Selection edits (by stored name, refresh the selection after topology changes) | `get_mesh_info`, `selection_count`, `selection_bounds`, `select_connected`, `expand_contract_selection`, `invert_selection`, `delete_faces`, `extrude_faces`, `inset_faces`, `outset_faces`, `offset_faces`, `translate_selection`, `smooth`, `noise`, `displace_from_texture`, `set_material_id`, `set_polygroup`, `set_vertex_color`, `project_uv`, `recompute_uvs`, `layout_uv`, `transform_uv` (native GeometryScript cannot read a selection stored by name, e.g. from `select_hidden_by`) |
| Kept where native does less | `append_stairs` (material IDs), `copy_mesh` (keeps skeletal origin and pose records), `load_mesh_from_actor`, `split_by_components` (registered handles, largest first), `repair` (refills holes a repair opened), `pack_uv_per_material`, `set_num_uv_layers`, `list_bones` (per-bone vertex counts) |
| Asset writes that save | `save_mesh_to_static_mesh` (collision-safe path, saves), `generate_collision`, `set_lods`, `bake_textures`, `bake_texture_transfer` |

Check `FModelingResult.success` and `.message`; other methods return counts, booleans, arrays, or
`FModelingUVStats`. `set_selection` returns the count or -1. `release_all_meshes` affects other
users of the session too: prefer individual release unless the whole session is yours.

## Bridge native selections to rigging

Build a `GeometryScriptMeshSelection` with a native selection function, then store it on the same
handle using `set_selection(handle, name, selection)`. `""` means the whole mesh for rigging
methods accepting a selection name. Rebuild selections after topology changes; element IDs can
be invalid or refer to different geometry. Do not assume IDs still denote the intended region.

```python
# dm and h refer to the same session mesh; the mesh already has geometry.
outputs = unreal.GeometryScript_MeshSelection.create_select_all_mesh_selection(dm)
selection = next(x for x in outputs if isinstance(x, unreal.GeometryScriptMeshSelection))
assert svc.set_selection(h, "all", selection) > 0
root = unreal.ModelingBoneDef(name="root")
r = svc.create_bones(h, [root])
assert r.success, r.message
r = svc.bind_selection_to_bone(h, "all", "root", 1.0)
assert r.success, r.message
print(svc.inspect_bone_weights(h, "all").message)
```

For moving parts, build separate connected solids, verify component counts and select the desired
piece natively before storing it. `create_bones` takes root first; parents must precede children,
and bone transforms are in mesh space. Point a hinge bone's X axis along the hinge. For weight
repair `select_by_bone_weight` selects the territory bones actually drag; `scale_bone_weights`
redistributes freed weight to a named receiver or the strongest other influence, skipping vertices
bound only to the targeted bones if no receiver is named. Smooth the seam, inspect weight sums,
and test a deformed pose before saving. `clear_selections` drops named selections.

## Rules that save a rebuild

- Units are centimeters, +Z up. Primitive origin options matter: confirm bounds and floor position.
- Python `unreal.Rotator` positional order is roll, pitch, yaw, unlike C++; use keyword arguments.
  For +Z primitives, pitch -90 points along +X and roll -90 along +Y. Verify bounds and a capture.
- Low-step cylinder/torus flats lie inside the nominal radius: account for this when placing rails,
  posts and other attachments.
- Booleans and voxel operations need closed solids. Check closedness and signed volume on **each**
  component before assembly: inward components may be discarded as negative space by self-union.
  Reverse triangle orientation, not just shading normals, if necessary. Nearly coincident surfaces
  produce slivers; use clearance or retain nested closed components instead of forcing a union.
- Hand-built triangles: Unreal treats clockwise-from-above (in XY) as front-facing, so a triangle
  with a positive mathematical XY cross product faces DOWN; its collision only hits from below
  (verified 5.9). After saving, trace top-down on the placed component and require a hit with
  positive normal Z; if only underside traces hit, reverse just the triangles you appended.
- Collision-flag edits on a static mesh's body setup made from Python may not reach existing
  components until the asset and map are saved and reopened (seen in 5.9). Trust traces after a
  reopen, not the flag readback.
- For loft-like native sweeps, the profile lies in each frame's local YZ plane: frame scale Y scales
  profile X and scale Z scales profile Y. Test a simple section and verify bounds. A polyline sweep's
  `bLoop` closes the path, not the profile. Native sweeps do not inherit the old wrapper's automatic
  cap/outward guarantees; explicitly check caps, closedness and orientation.
- Booleans and hole filling can leave ID gaps: compact before XAtlas. Repair can open a mesh; check
  closedness again, especially before boolean/voxel work. Recreate selections after topology edits.
- Check unwraps with `get_uv_stats`: zero unset triangles and near-zero overlap unless deliberate.
  Conformal unwraps can fold on cylindrical islands; XAtlas or suitable projection can be safer.
  Vertex-color masks smear across large triangles; use per-pixel logic or a densely tessellated copy.
- Build detail on a copy before baking. Big bakes can outlast a tool timeout: log and inspect actual
  results before retrying, rather than accidentally launching a duplicate bake.
- Verify material IDs before asset creation, then assign real slots; `set_asset_materials` can add
  missing slots. Verify the asset, collision, LODs and references after saving. Changing geometry
  may require rebuilding collision and LODs. Obtain permission before overwriting existing assets.
- A successful call is not visual proof. Check bounds, counts, topology and a viewport capture;
  a new level may need lighting. Keep the original asset and a recovery point for destructive edits.


## ModelingService: skeletal LODs, garment fit, accessories, generated props

These tools have no native equivalent; native loads do not record the origin, pose and vertex order they need.

| Area | Service functions |
|---|---|
| Generated-prop intake | `intake_generated_mesh(file, asset_path, target_size_cm, fit_axis="Height", yaw_deg=0, pivot="BottomCenter", max_triangles=20000, max_texture_size=1024, collision="ConvexHulls", max_convex_hulls=4, lod_percents="", target_box_cm=Vector(), enable_nanite=False, force=False)` → `ModelingIntakeReport` (import GLB/FBX/OBJ, merge, scale, pivot, decimate, cap textures, LODs, collision; see **Generated props**) |
| Posed reads and garment fit | `load_mesh_from_skinned_component(actor_label, component_name="", world_space=False, lod_index=0, prefer_play_world=True)` (the component's CURRENT pose; PIE first), `unpose_mesh(handle)`, `measure_penetration(handle, other, "Inside|Outside", margin, max_distance=5, sel="", out_selection="")` → `ModelingPenetrationReport`, `push_inside(handle, other, margin, sel, max_distance, smooth_iterations)`, `push_outside(...)`, `select_hidden_by(handle, name, other, margin, max_distance, all_vertices=True)` — see **Fitting garments** |
| Accessory re-skin | `skin_to_surface(handle, source_handle, mode="Surface|Inpaint|Rigid", rigid_bone_name="", bones_from_skeletal_mesh="", smooth_iterations=0, smooth_strength=0)` (weights from the skinned surface underneath), `remap_lod_bones(asset_path, reference_skeletal_mesh_path)` (move weight off bones the leader drops at lower LODs) — see **Accessories that follow a surface** |
| Skeletal LODs in place | `load_mesh_from_skeletal_mesh(path, lod)` (source model; the handle remembers where it came from), `save_mesh_to_skeletal_mesh_lod(handle, asset_path="", lod_index=-1)` (write ONE LOD back, validated; keeps skeleton, slots and the other LODs; lod_index == LOD count appends), `inspect_skeletal_mesh(path)` (slots, per-LOD source / verts / tris / bones used) — see **Editing a skeletal mesh LOD in place** |

Report types:
- `FModelingPenetrationReport` (`measure_penetration`): `success`, `message`, `vertices_checked`, `violations`, `max_depth`, `average_depth`, `worst_position`, `worst_vertex`.
- `FModelingIntakeReport` (`intake_generated_mesh`): `success`, `message`, `asset_path`, `up_to_date`, `source_mesh_count`, `source_triangles`, `triangle_count`, `vertex_count`, `source_size`, `scale_applied`, `bounds_min`, `bounds_max`, `lod_count`, `collision_shapes`, `material_paths`, `textures`.

## Generated props: image-to-3D model → game asset in one call

Models from image-to-3D services (Hyper3D Rodin, Tripo, Tonetta Forge, ...) arrive at an arbitrary scale with
the pivot in the middle, far too many triangles and 2-4K textures. `intake_generated_mesh` does the whole clean-up
inside the editor — no Blender pass:

```python
r = svc.intake_generated_mesh("C:/gen/knife_tray/knife_tray.glb", "/Game/Props/KnifeTray/SM_KnifeTray",
        target_size_cm=48.0, fit_axis="Width",        # the size you KNOW: a door's height, a tray's width
        yaw_deg=0.0, pivot="BottomCenter",            # "TopCenter" for hanging lamps
        max_triangles=12000, max_texture_size=1024,   # 1024 for web/mobile, 2048 for a hero prop
        collision="ConvexHulls", max_convex_hulls=4,  # "Box" for simple shapes, "None" for decor nobody touches
        lod_percents="1,0.5,0.25")                    # "" = one LOD (small or thin props)
print(r.message)            # tris before/after, final size, scale, LODs, collision shapes, slots, textures
print(r.bounds_min, r.bounds_max, r.textures)
```

What it does, in order: imports the file into a staging folder (GLB/glTF; FBX and OBJ work too), merges every
mesh into ONE StaticMesh at the asset path, turns it `yaw_deg` about Z, scales it (`fit_axis`: `Height` = Z,
`Width` = X, `Depth` = Y, `Longest`, `Box` = stretch to `target_box_cm` per axis, `None`), moves the pivot,
decimates to `max_triangles`, caps every texture's Maximum Texture Size (the built, cooked and loaded size; the
source pixels stay full-size in the .uasset, so raising the cap later needs no re-import), builds LODs and simple collision, keeps the
materials and textures next to the mesh as `MI_<Name>_<i>` (`M_` for a plain material) / `T_<Name>_<Role>`
(BaseColor, Normal, ...; `<Name>` is the asset name without `SM_`), and
deletes everything else the import made.

- **Idempotent.** The asset is tagged with the file's hash and the settings; the same call again returns
  `up_to_date=True` without touching anything. A new file or a changed setting replaces the mesh **in place**, so
  actors already placed keep working (`force=True` redoes it regardless).
- **Only intake's own assets are replaced.** A StaticMesh at the asset path that intake did not make is refused unless
  `force=True`. On a re-run, the previous run's materials and textures are deleted once nothing references them;
  other assets with the same names are never deleted (the new ones get `_2`, `_3`).
- **Read the report before placing.** `message` warns `stretched N%` when a `Box` fit distorted the proportions —
  prefer a uniform fit on the one dimension you know. A source of 0 triangles, a skinned file (imports as a
  SkeletalMesh) or an unknown `fit_axis` / `pivot` / `collision` fails with the reason.
- **Thin parts and decimation.** Lattices, trusses, chain links and wire break apart when decimated: give them a
  higher `max_triangles` and `lod_percents=""` rather than accepting holes.
- **Where the front faces is the model's business.** Generators pick their own forward axis. Render the result
  from several sides (or `CaptureEditorViewport` after placing it) and fix a wrong facing with `yaw_deg` in 90°
  steps; then re-run the same call (it replaces in place).

The full loop for a generated prop:

1. **Reference sheet first.** Build an all-angle identity sheet of the prop (if a `reference-images` skill is
   installed, use it) and look at it; re-roll the whole sheet when views disagree.
2. **Generate from the sheet's views with multi-view input**, never from one image: a single three-quarter view
   makes the generator invent every hidden side (a crystal shard came back as a cup).
3. **`intake_generated_mesh`** with the real-world size.
4. **Look at it**: a turnaround render (the reference-images skill's turnaround tooling, or captures from 4-8 sides)
   next to the sheet. Fix facing or size by re-running intake with new settings.
5. **Place it** with `EditorToolset.LevelToolset.AddToSceneFromAsset`; with the `BottomCenter` pivot the actor's
   location is the point on the floor.

## Editing a skeletal mesh LOD in place (garments, accessories, bodies)

Characters are skeletal: shirts, dresses, accessories and bodies all carry a skeleton, material slots, skin weights
and several LODs. Do not rebuild the asset to change its geometry; round-trip the LOD you are fixing:

```python
print(svc.inspect_skeletal_mesh("/Game/Chars/SKM_Shirt").message)   # slots, LODs (source model or generated), verts, bones used
for lod in range(3):
    h = svc.load_mesh_from_skeletal_mesh("/Game/Chars/SKM_Shirt", lod).handle
    # ... any edit: move vertices, smooth, repair weights, push out of the body ...
    r = svc.save_mesh_to_skeletal_mesh_lod(h)      # back to the asset and LOD it came from
    print(r.success, r.message)
    svc.release_mesh(h)
svc.save_mesh_to_skeletal_mesh_lod(low_handle, "/Game/Chars/SKM_Shirt", 3)   # lod_index == LOD count appends a LOD
```

- **Load reads the source model** (the editable vertex layout). Never mix in positions read through
  `GeometryScriptMeshReadLOD` with the default `MAX_AVAILABLE`/render data: render data splits vertices along seams, so
  its vertex count differs and a writer fails (`dm verts 10364 != weight verts 10350`). A LOD that was *generated by
  reduction* has no source model; the load message says so, and writing it back turns it into a custom LOD.
- **Save validates before it writes**, and writes nothing on failure: every vertex must carry skin weight (an
  unweighted vertex collapses to the root at runtime), every bone on the mesh must exist in the asset's skeleton
  (matched by name, so the order does not matter), bone and material indices must be in range, and a posed handle
  must be `unpose_mesh`-ed first. Afterwards it reads the LOD back and compares the vertex count.
- **What is kept:** skeleton and reference skeleton, every material slot and the LOD's material map, the other LODs,
  the LOD's recompute-normals/tangents settings, which section uses which slot, and — when the topology is unchanged
  since the load — the original vertex order (morph targets and anything else indexed by vertex). The LOD stops
  being auto-generated from LOD0 (it is a custom LOD now).
- A topology change (remesh, delete faces) is allowed; give new triangles a material ID the asset has a slot for and
  re-weight new vertices (native `GeometryScript_BoneWeights.transfer_bone_weights_from_mesh`, or `skin_to_surface`).
- **Normals.** `save_mesh_to_skeletal_mesh_lod` keeps the LOD's build settings: when they recompute normals, the
  engine rebuilds them from the new geometry and the handle's own normals are not used (the message says so).
  `save_mesh_to_skeletal_mesh` on an existing asset stores the handle's normals as they are, as it always has.
- **Lower LODs generated from the one you wrote** (no source model of their own) are regenerated by the engine from
  the new geometry; custom LODs are not touched.

**Changed behaviour of the two older calls** (scripts written before these sections existed):

- `load_mesh_from_skeletal_mesh(path, lod)` now **fails** for a LOD the asset does not have. It used to succeed: the
  engine silently read the nearest existing LOD while the result said the one asked for. Read the LOD count from
  `inspect_skeletal_mesh` instead of probing.
- `save_mesh_to_skeletal_mesh` on an **existing** asset (`replace_existing` defaults to True) replaces LOD0 through
  the validated writer. Kept that used to be lost: which section uses which material slot. Matched by name instead of
  by index: bones. Refused with nothing written, where a broken asset used to be saved: vertices without skin weight,
  bones the asset's skeleton lacks, a material ID with no slot, a posed handle. Creating a new asset is unchanged.

## Fitting garments: skin under cloth, cloth off skin, in the pose the game uses

Poke-through depends on the pose: a fit that is clean in the reference pose can be 10+ cm wrong seated, and
runtime post-process / corrective bones only exist on the running component. So read the meshes **as posed** —
from PIE when it is running — fit them against each other, then take the result back to the reference pose and
write it into the asset LOD:

```python
# the character is posed in PIE (or in the editor world); both handles in the same (component) space
body  = svc.load_mesh_from_skinned_component("BP_Ava", "Body",  False).handle
dress = svc.load_mesh_from_skinned_component("BP_Ava", "Dress", False).handle   # a leader-pose follower reads the leader's pose
r = svc.measure_penetration(body, dress, "Inside", 0.3, 5.0, "", "poking")      # skin poking through the dress
print(r.violations, r.max_depth, r.worst_position)
svc.push_inside(body, dress, 0.3, "", 5.0, 2)          # skin moved under the dress, feathered over 2 rings
svc.unpose_mesh(body)                                  # back to the reference pose (edits carried through the inverse skinning)
svc.save_mesh_to_skeletal_mesh_lod(body)               # into the body asset LOD it came from
```

| Need | Call |
|---|---|
| How bad is it (per garment, per pose) | `measure_penetration(a, b, "Inside", margin)`: `a` belongs under `b` (skin under cloth, an inner layer under an outer one); `"Outside"`: `a` belongs over `b` (cloth over skin, a collar off the face). `violations`, `max_depth`, `average_depth`, `worst_position`; `out_selection` stores the offenders |
| Push skin in under the cloth | `push_inside(body, garment, margin)` |
| Push cloth out of the body or face | `push_outside(garment, body, margin)` |
| Drop skin (or an inner garment) fully hidden under cloth | `select_hidden_by(body, "hidden", garment, margin)`, then delete those triangles natively (`GeometryScript_MeshEdits.delete_selected_triangles_from_mesh`) |

- **Only covered vertices are judged.** A vertex counts when it is within `max_distance` of the other surface AND
  projects onto it (the offset runs along that face's normal). A head above a collar or hands past a sleeve are
  never dragged into the cloth; raise `max_distance` only for loose garments. The other mesh's face normals define
  "outside", so it needs consistent outward normals (any imported garment has them).
- **Push rather than delete for dresses and open garments.** Deleting skin under a dress leaves holes where the
  dress opens or moves (a see-through crotch, hollow legs). Push the skin in with a margin, and keep deletion for
  regions that are covered in every pose.
- **Fit in every pose the game uses.** One pose's fix can break another. Pose the character (PIE, each clip or
  seated/standing state), read, measure, push, unpose, save — once per pose, worst first — and re-measure the
  earlier poses at the end. (If VibeMetaHumans is installed, its wardrobe tools wrap this for MetaHuman bodies and
  their post-process bones; this service is the generic mesh-vs-mesh layer underneath.)
- **A posed handle cannot be saved as a reference pose** (`save_mesh_to_skeletal_mesh_lod` refuses it, and a
  native copy into another handle drops the pose record, so never copy a posed handle). `unpose_mesh` first. Normals are not re-skinned in the pose; only positions are.
- The handle's vertex IDs are the asset LOD's source-model IDs, so selections, weights and the write-back all line
  up; morph targets and cloth simulation are not applied to the posed read.

## Accessories that follow a surface (ties, dog tags, scarves, holsters)

A static accessory attached to one bone rides rigidly while the cloth under it bends: a tie stays put while the
neck turns, dog tags float off a slumped chest. Skin it to the surface it lies on instead — no Blender, no hand-
written nearest-point loop:

```python
body = "/Game/Chars/Ava/SKM_Ava_Body"
# 1. the accessory, placed where it should sit, in the body's component space (here: from its static mesh + the fitted transform)
acc = svc.create_mesh().handle
unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(unreal.load_asset("/Game/Props/SM_Tie"), svc.get_dynamic_mesh(acc),
    unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
unreal.GeometryScript_MeshTransforms.transform_mesh(svc.get_dynamic_mesh(acc), fitted_transform_in_body_space)
# 2. the surface it lies on, in the SAME space: the garment asset (its own reference pose; fine when it matches the body's) ...
shirt = svc.load_mesh_from_skeletal_mesh("/Game/Chars/Ava/SKM_Ava_Shirt").handle
#    ... or the garment exactly as displayed on a placed character (follower of the body):
# shirt = svc.load_mesh_from_skinned_component("BP_Ava", "Shirt", False).handle
# 3. weights from the closest surface point; the result carries the BODY's skeleton so it can follow the body
print(svc.skin_to_surface(acc, shirt, "Surface", "", body).message)     # top bones with their share
svc.save_mesh_to_skeletal_mesh(acc, "/Game/Props/SKM_Tie", "<the body's Skeleton asset path>")
# 4. lower LODs: move weight off bones the body drops at LOD1+ (they stop updating and the accessory streaks)
print(svc.remap_lod_bones("/Game/Props/SKM_Tie", body).message)
```

- **Modes.** `Surface` copies the weights of the closest surface point (thin pieces lying on the cloth). `Inpaint`
  keeps the clear matches and smoothly fills the rest (pieces that stand off the surface: a buckle, a holster).
  `Rigid` puts every vertex 100% on one bone — `rigid_bone_name`, or the bone the surface under it is most weighted
  to — for hard items that must not deform (a badge, a watch face). `smooth_iterations` / `smooth_strength` blur
  the transferred weights.
- **Both meshes must be in the same space.** The weights come from proximity: an accessory in world space against
  a garment in component space finds nothing sensible. Load the surface at the reference pose (asset) or as
  displayed (`load_mesh_from_skinned_component` with `world_space=False`) and put the accessory in that space.
- **`bones_from_skeletal_mesh`** gives the result another mesh's skeleton (the body it will follow); weights are
  matched by bone name. Without it the result carries the source's bones.
- **Attach** the saved mesh as a leader-pose follower of the body (a SkeletalMeshComponent with `set_leader_pose_component(body)`;
  if VibeMetaHumans is installed, its wardrobe service does the component swap for MetaHumans).
- **`remap_lod_bones(asset, reference)`** reads which bones the reference (the leader) keeps at each LOD, moves every
  influence on a dropped bone to its nearest kept ancestor and writes each LOD back in place. The follower's last LOD
  also covers the leader's lower LODs. Bones the leader does not have at all (the follower's own) are left alone.
  Only LODs with something to move are written; a written LOD that was generated by reduction becomes a custom LOD.
  Run it after any re-skin of a follower, and whenever a garment or accessory "streaks" or "explodes" only at distance.

Before raw GeometryScript, read [geometryscript-cheatsheet.md](geometryscript-cheatsheet.md): Python class names cannot be guessed.

For actual logo geometry see [modeled-lettering.md](modeled-lettering.md); for capture and delivery
see [showcase-and-delivery.md](showcase-and-delivery.md). Both are registered as agent sub-docs.
