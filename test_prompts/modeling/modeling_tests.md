# Mesh Modeling Test Prompts

Exercise the surviving Vibe3D service alongside native Unreal 5.9 GeometryScript via
`execute_python` after loading the `modeling` skill. Use an exclusively owned session and an
empty task-owned `/Game/Developers/Vibe3DModelingTests` directory. Verify cleanup even on failure;
never delete pre-existing assets or other users' handles. For a no-agent service-surface check,
run `Content/Skills/modeling/scripts/exercise_all.txt`; success prints `RESULT: PASS`.

## 1. Session and native modeling

Create a handle, retrieve its live DynamicMesh, append a 100 cm box with native GeometryScript,
and report triangle count, bounds, closedness and signed volume using native queries. List live
handles, release a second test handle and verify the first still exists. A handle is not an asset.

## 2. Primitives and material IDs

Make a native box and linear staircase with distinct primitive material IDs. Use the retained
`append_torus` and `append_revolve_polygon` service functions with two more IDs. Query the actual
triangle IDs with native GeometryScript; explain why the native 5.9 torus and revolve generators
are unsuitable when the primitive option's ID must be honored.

## 3. Booleans, selection and processing

Subtract a sphere from a closed box using native GeometryScript. Verify closedness and orientation.
Select the top using native GeometryScript, report count and bounds, inset and recess it. Recreate
the selection after topology changes. Repair and unwrap natively, checking closedness again.

## 4. Sweep and orientation

Create a tapered wing using a native polygon sweep and verify caps, closedness, bounds and positive
signed volume separately for the wing before assembly. Demonstrate the local YZ profile/frame scale
mapping on a simple section; do not assume the deleted loft wrapper's outward/capping guarantees.

## 5. Native selection to service rigging

Create a mesh with a separately selectable control surface. Create a native mesh selection for it,
store it with `set_selection`, then create root and control bones with `create_bones`, bind with
`bind_selection_to_bone`, and verify using `inspect_bone_weights`. Use `select_by_bone_weight`,
`scale_bone_weights` with an explicit receiver, and `smooth_selection_bone_weights`; compare weight
reports and verify the deformed pose. Clear the named selections. Save a new skeletal mesh with an
empty skeleton path, confirm the skeleton asset exists, and assign a material with
`set_asset_materials`.

## 6. UVs, texture, details and assets

Compact and unwrap a mesh using native XAtlas. Report `get_uv_stats` and verify no unset triangles
and near-zero overlap. Create a noise texture with `create_noise_texture`, then draw a line with
`draw_on_texture`. On a separate detail copy use `append_mesh_along_polyline` and
`cut_groove_along_polyline`; bake via native GeometryScript if requested. Create a new static mesh
with native asset utilities, replace its LOD0 using native asset utilities, and check both outcomes.
Inspect material slots, collision and LODs instead of assuming a successful save configured them.

## 7. Error handling and cleanup

Try `set_selection` with an invalid handle and confirm it returns -1. Call a service result-returning
operation on an invalid handle and report the failure. Release task-owned handles and delete only
the task-owned assets and directory. Verify no owned handles or assets remain. Use
`release_all_meshes` only if the complete editor session is exclusively yours.
