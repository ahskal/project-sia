"""Extract the separate stones from STF's SM_RockGroup01 for harvesting."""

import json
from pathlib import Path
import unreal


SOURCE_PATH = "/Game/STF/Pack03-LandscapePro/Environment/Foliage/RocksSmall/SM_RockGroup01"
OUTPUT_DIR = "/Game/Sia/Harvesting/Rocks/Fragments/SM_RockGroup01"
OUTPUT_COUNT = 4
MANIFEST_PATH = Path(unreal.Paths.project_dir()) / "Tools" / "HarvestRockGroupManifest.json"


def bounds(mesh):
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    return box.get_editor_property("min"), box.get_editor_property("max")


def main():
    output_paths = ["{}/SM_RockGroup01_Piece{:02d}".format(OUTPUT_DIR, i + 1)
                    for i in range(OUTPUT_COUNT)]
    existing = [unreal.EditorAssetLibrary.does_asset_exist(path) for path in output_paths]
    if all(existing):
        unreal.log("SM_RockGroup01 fragments already exist; no asset edits made.")
        return
    if any(existing):
        raise RuntimeError("Partial rock extraction exists. Inspect before rerunning.")

    source = unreal.load_asset(SOURCE_PATH)
    if not isinstance(source, unreal.StaticMesh):
        raise RuntimeError("Missing source rock group: " + SOURCE_PATH)
    whole = unreal.DynamicMesh()
    _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        source, whole, unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD())
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("Could not read source rock group.")
    _, components = unreal.GeometryScript_MeshDecomposition.split_mesh_by_components(whole, None)
    if len(components) != OUTPUT_COUNT:
        raise RuntimeError("Expected four disconnected rocks, found {}.".format(len(components)))

    group_min, group_max = bounds(whole)
    group_center = unreal.Vector((group_min.x + group_max.x) * .5,
                                 (group_min.y + group_max.y) * .5,
                                 (group_min.z + group_max.z) * .5)
    pieces = []
    for component in components:
        piece_min, piece_max = bounds(component)
        center = unreal.Vector((piece_min.x + piece_max.x) * .5,
                               (piece_min.y + piece_max.y) * .5,
                               (piece_min.z + piece_max.z) * .5)
        extents = unreal.Vector((piece_max.x - piece_min.x) * .5,
                                (piece_max.y - piece_min.y) * .5,
                                (piece_max.z - piece_min.z) * .5)
        pieces.append({
            "mesh": component,
            "center": center,
            "extents": extents,
            "radius_sq": ((center - group_center).x ** 2
                          + (center - group_center).y ** 2
                          + (center - group_center).z ** 2),
            "volume": 8.0 * extents.x * extents.y * extents.z,
            "vertices": unreal.GeometryScript_MeshQueries.get_vertex_count(component),
        })
    pieces.sort(key=lambda piece: piece["radius_sq"], reverse=True)

    unreal.EditorAssetLibrary.make_directory(OUTPUT_DIR)
    source_materials = source.get_editor_property("static_materials")
    records = []
    for index, piece in enumerate(pieces):
        mesh = piece["mesh"]
        unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, -piece["center"])
        asset_path = output_paths[index]
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.set_editor_property("enable_collision", False)
        asset, result = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            mesh, asset_path, options)
        if result != unreal.GeometryScriptOutcomePins.SUCCESS or not asset:
            raise RuntimeError("Could not create rock fragment " + asset_path)
        asset.set_editor_property("static_materials", source_materials)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        records.append({
            "asset": asset.get_path_name(),
            "source": SOURCE_PATH,
            "relative_location_cm": [piece["center"].x, piece["center"].y, piece["center"].z],
            "bounds_extent_cm": [piece["extents"].x, piece["extents"].y, piece["extents"].z],
            "vertices": piece["vertices"],
            "edge_order": index + 1,
            "distance_from_group_center_cm": piece["radius_sq"] ** .5,
            "bounding_box_volume_cm3": piece["volume"],
        })
    largest = max(records, key=lambda record: record["bounding_box_volume_cm3"])
    manifest = {
        "source": SOURCE_PATH,
        "source_component_count": len(components),
        "source_materials": [
            slot.get_editor_property("material_interface").get_path_name()
            if slot.get_editor_property("material_interface") else None
            for slot in source_materials
        ],
        "edge_order": "descending distance from the group bounds center; independent of hit point",
        "stone_pickup_asset": largest["asset"],
        "pieces": records,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("Extracted four SM_RockGroup01 pieces in a fixed outside-in order.")


main()
