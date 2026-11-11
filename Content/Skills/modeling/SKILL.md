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

For actual logo geometry see [modeled-lettering.md](modeled-lettering.md); for capture and delivery
see [showcase-and-delivery.md](showcase-and-delivery.md). Both are registered as agent sub-docs.
