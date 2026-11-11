# Vibe3D

Vibe3D complements Unreal Engine 5.9's native GeometryScript with a focused AI Editor Toolset
and a `modeling` agent skill. Model meshes directly with `unreal.GeometryScript_*` from editor
`execute_python`; `unreal.ModelingService` provides session handles and the remaining gaps in
rigging, asset creation, UV statistics, texture painting and polyline detailing.

## Install

```
cd <YourProject>/Plugins
git clone https://github.com/kevinpbuckley/Vibe3D.git
```

Build the project, enable **Vibe3D** under Edit > Plugins, and restart. Requires UE 5.9 and
its GeometryScripting, ToolsetRegistry, PythonScriptPlugin and EditorScriptingUtilities plugins.
Vibe3D works alongside [VibeUE](https://github.com/kevinpbuckley/VibeUE) but does not depend on it.

## What you get

- **`Vibe3D.ModelingService`** on the ToolsetRegistry/MCP endpoint: session handles, named
  selection bridging, ID-preserving torus and revolve primitives, skeletal save and material slots,
  rigging and regional weight repair, UV statistics, texture painting and polyline detailing.
  `get_dynamic_mesh` is Python-only and hands the live mesh to native GeometryScript.
- **`modeling` skill**: native modeling, Python naming, selection bridging, frame semantics,
  texturing, rigging and gotchas. Registered at editor start; refresh with `Vibe3D.ReloadSkills`.
- **Agent guide**: `Vibe3D.GenerateAgentConfig [ClaudeCode|Gemini|Codex|Hermes|Cursor|Copilot|All] [import]`
  installs the generated block from `Content/samples/AGENTS.md.sample`; re-running refreshes that
  block without replacing the rest of the target file. `import` uses an `@path` for clients that
  support imports.

Quick taste (editor Python):

```python
import unreal
svc = unreal.ModelingService
r = svc.create_mesh()
assert r.success, r.message
h = r.handle
try:
    dm = svc.get_dynamic_mesh(h)
    unreal.GeometryScript_Primitives.append_box(dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), 100, 60, 40)
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dm)
    unreal.GeometryScript_MeshRepair.compact_mesh(dm)
    unreal.GeometryScript_UVs.auto_generate_x_atlas_mesh_u_vs(dm, 0, unreal.GeometryScriptXAtlasOptions())
    stats = svc.get_uv_stats(h)
    assert stats.success and stats.num_unset_triangles == 0, stats.message
    print(stats.message)
finally:
    svc.release_mesh(h)
```

Use native GeometryScript's asset utilities to save a static mesh, and check the outcome before
reporting success. Use the service to save a skeletal mesh, creating a skeleton when needed.

## Testing

- Automation: `Automation RunTests Vibe3D.Modeling` (editor foreground may be required for the
  automation framework's frame-rate gate).
- Run `Content/Skills/modeling/scripts/exercise_all.txt` as editor Python in an exclusively owned
  session. It exercises the surviving service surface and native GeometryScript, removes its
  temporary assets and handles, and prints `RESULT: PASS` on success. It refuses to overwrite an
  existing `/Game/Developers/Vibe3DModelingExercise` directory.
- Agent prompts: `test_prompts/modeling/modeling_tests.md`.

## Layout

```
Source/Vibe3D/            module and ModelingService
Content/Skills/modeling/  SKILL.md, sub-docs and scripts/exercise_all.txt
Content/Python/           native AgentSkill registration
Content/samples/          generated agent guide template
test_prompts/modeling/    agent validation prompts
```

## License

MIT — see [LICENSE](LICENSE).
