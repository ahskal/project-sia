"""Split the five disconnected hand tools out of the imported Fab source mesh.

Run from Unreal Editor's Python console or Python Script menu. The source asset
is read-only. Outputs are created once in purpose-specific tool folders; rerunning
with a complete output set is a no-op. Partial output sets are rejected to avoid
replacing editor-authored assets.
"""

import json
from pathlib import Path
import unreal


SOURCE_PATH = (
    "/Game/Fab/Survival_Game_Tool_Pack/"
    "survival_game_tool_pack_free_agustin_honnun/StaticMeshes/"
    "survival_game_tool_pack_free_agustin_honnun"
)
OUTPUTS = [
    ("Axe", "/Game/Sia/Harvesting/Tools/Axes/SM_Axe"),
    ("Hammer", "/Game/Sia/Harvesting/Tools/Hammers/SM_Hammer"),
    ("Hoe", "/Game/Sia/Harvesting/Tools/Pickaxes/SM_Pickaxe"),
    ("Pickaxe", "/Game/Sia/Harvesting/Tools/Hoes/SM_Hoe"),
    ("Shovel", "/Game/Sia/Harvesting/Tools/Shovels/SM_Shovel"),
]
MANIFEST_PATH = str(
    Path(unreal.Paths.project_dir()) / "Tools" / "HarvestToolExtractionManifest.json"
)
EXPECTED_TOOL_COUNT = 5
X_GAP_THRESHOLD = 2500.0
SOURCE_UNIT_SCALE = 0.01


def mesh_bounds(mesh):
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    minimum = box.get_editor_property("min")
    maximum = box.get_editor_property("max")
    return minimum, maximum


def bounds_center_x(mesh):
    minimum, maximum = mesh_bounds(mesh)
    return (minimum.x + maximum.x) * 0.5


def group_components(components):
    ordered = sorted(components, key=bounds_center_x)
    groups = []
    for component in ordered:
        if not groups:
            groups.append([component])
            continue
        previous_x = bounds_center_x(groups[-1][-1])
        current_x = bounds_center_x(component)
        if current_x - previous_x > X_GAP_THRESHOLD:
            groups.append([component])
        else:
            groups[-1].append(component)
    if len(groups) != EXPECTED_TOOL_COUNT:
        raise RuntimeError(
            "Expected five spatial tool groups, found {}. No assets were created.".format(
                len(groups)
            )
        )
    return groups


def create_tool_asset(group, tool_name, asset_path, source_materials):
    combined = unreal.DynamicMesh()
    for part in group:
        unreal.GeometryScript_MeshEdits.append_mesh(
            combined, part, unreal.Transform()
        )

    minimum, maximum = mesh_bounds(combined)
    center = unreal.Vector(
        (minimum.x + maximum.x) * 0.5,
        (minimum.y + maximum.y) * 0.5,
        (minimum.z + maximum.z) * 0.5,
    )
    unreal.GeometryScript_MeshTransforms.translate_mesh(combined, -center)
    unreal.GeometryScript_MeshTransforms.scale_mesh(
        combined,
        unreal.Vector(SOURCE_UNIT_SCALE, SOURCE_UNIT_SCALE, SOURCE_UNIT_SCALE),
    )

    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_collision", False)
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        combined, asset_path, options
    )
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS or not asset:
        raise RuntimeError("Failed to create {}: {}".format(asset_path, outcome))

    if source_materials:
        asset.set_editor_property("static_materials", source_materials)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

    minimum, maximum = mesh_bounds(combined)
    return {
        "tool": tool_name,
        "asset": asset.get_path_name(),
        "source": SOURCE_PATH,
        "component_count": len(group),
        "vertices": unreal.GeometryScript_MeshQueries.get_vertex_count(combined),
        "bounds_min_cm": [minimum.x, minimum.y, minimum.z],
        "bounds_max_cm": [maximum.x, maximum.y, maximum.z],
        "source_scale": SOURCE_UNIT_SCALE,
        "materials": [
            material.get_editor_property("material_interface").get_path_name()
            if material.get_editor_property("material_interface")
            else None
            for material in source_materials
        ],
    }


def write_manifest(tools, component_count):
    manifest = {
        "source": SOURCE_PATH,
        "source_component_count": component_count,
        "grouping": "Connected mesh islands grouped by spatial X gaps over {} cm".format(
            X_GAP_THRESHOLD
        ),
        "tools": tools,
    }
    with open(MANIFEST_PATH, "w", encoding="utf-8") as stream:
        json.dump(manifest, stream, ensure_ascii=False, indent=2)


def inspect_tool_asset(tool_name, asset_path):
    asset = unreal.load_asset(asset_path)
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError("Invalid extracted tool asset: {}".format(asset_path))
    dynamic_mesh = unreal.DynamicMesh()
    _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        asset,
        dynamic_mesh,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD(),
    )
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("Could not inspect extracted tool: {}".format(asset_path))
    minimum, maximum = mesh_bounds(dynamic_mesh)
    materials = asset.get_editor_property("static_materials")
    return {
        "tool": tool_name,
        "asset": asset.get_path_name(),
        "source": SOURCE_PATH,
        "component_count": unreal.GeometryScript_MeshQueries.get_num_connected_components(dynamic_mesh),
        "vertices": unreal.GeometryScript_MeshQueries.get_vertex_count(dynamic_mesh),
        "bounds_min_cm": [minimum.x, minimum.y, minimum.z],
        "bounds_max_cm": [maximum.x, maximum.y, maximum.z],
        "source_scale": SOURCE_UNIT_SCALE,
        "materials": [
            slot.get_editor_property("material_interface").get_path_name()
            if slot.get_editor_property("material_interface")
            else None
            for slot in materials
        ],
    }


def main():
    output_paths = [asset_path for _, asset_path in OUTPUTS]
    existing = [unreal.EditorAssetLibrary.does_asset_exist(path) for path in output_paths]
    if all(existing):
        source = unreal.load_asset(SOURCE_PATH)
        source_mesh = unreal.DynamicMesh()
        _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            source,
            source_mesh,
            unreal.GeometryScriptCopyMeshFromAssetOptions(),
            unreal.GeometryScriptMeshReadLOD(),
        )
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError("Could not inspect the source tool pack.")
        component_count = unreal.GeometryScript_MeshQueries.get_num_connected_components(source_mesh)
        tools = [inspect_tool_asset(name, path) for name, path in OUTPUTS]
        write_manifest(tools, component_count)
        unreal.log("Harvest tool assets already exist; skipping without asset edits.")
        return
    if any(existing):
        raise RuntimeError(
            "Only some extraction outputs exist. Inspect them before rerunning; no assets were replaced."
        )

    source = unreal.load_asset(SOURCE_PATH)
    if not isinstance(source, unreal.StaticMesh):
        raise RuntimeError("Tool pack source mesh is missing: {}".format(SOURCE_PATH))

    dynamic_mesh = unreal.DynamicMesh()
    _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        source,
        dynamic_mesh,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD(),
    )
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("Could not read the source mesh: {}".format(outcome))

    _, components = unreal.GeometryScript_MeshDecomposition.split_mesh_by_components(
        dynamic_mesh, None
    )
    if not components:
        raise RuntimeError("The source mesh produced no connected components.")

    materials = source.get_editor_property("static_materials")
    groups = group_components(components)
    for _, asset_path in OUTPUTS:
        unreal.EditorAssetLibrary.make_directory(asset_path.rsplit("/", 1)[0])
    tools = [
        create_tool_asset(group, tool_name, asset_path, materials)
        for group, (tool_name, asset_path) in zip(groups, OUTPUTS)
    ]
    write_manifest(tools, len(components))
    unreal.log(
        "Created {} isolated tool clusters from {} connected components.".format(
            len(groups), len(components)
        )
    )


main()
