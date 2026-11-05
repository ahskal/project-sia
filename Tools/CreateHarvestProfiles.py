"""Create the configurable tree/rock profiles and placeable resource blueprints."""

import json
from pathlib import Path
import unreal


ROOT = Path(unreal.Paths.project_dir())
CONTENT = ROOT / "Content"
ROCK_MANIFEST = ROOT / "Tools" / "HarvestRockGroupManifest.json"

TREE_FOLDER = "/Game/Sia/Harvesting/Trees"
ROCK_FOLDER = "/Game/Sia/Harvesting/Rocks"
TREE_MESH = "/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree01/SM_green-tree01"
LOG_MESH = "/Game/Mega_Survival_Tools/Models/SM_Log"


def enum_value(enum_type, name):
    for candidate in (name.upper(), name):
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    raise RuntimeError("Missing enum value {}.{}".format(enum_type, name))


def load_mesh(path):
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Required static mesh is missing: " + path)
    return mesh


def create_or_load_data_asset(name, folder, asset_class):
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
        if not isinstance(asset, asset_class):
            raise RuntimeError("Existing asset has the wrong class: " + path)
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, asset_class, factory
    )
    if not asset:
        raise RuntimeError("Could not create data asset: " + path)
    return asset


def create_or_load_blueprint(name, folder, parent_class):
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = unreal.load_asset(path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise RuntimeError("Existing resource asset is not a Blueprint: " + path)
        return blueprint
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Blueprint, factory
    )
    if not blueprint:
        raise RuntimeError("Could not create resource Blueprint: " + path)
    return blueprint


def drop(item_id, probability, minimum, maximum):
    rule = unreal.HarvestDropRule()
    rule.set_editor_property("item_id", item_id)
    rule.set_editor_property("probability", probability)
    rule.set_editor_property("min_amount", minimum)
    rule.set_editor_property("max_amount", maximum)
    return rule


def set_blueprint_profile(blueprint, profile):
    generated_class = blueprint.generated_class()
    if not generated_class:
        raise RuntimeError("Resource Blueprint has no generated class: " + blueprint.get_name())
    class_default_object = unreal.get_default_object(generated_class)
    class_default_object.set_editor_property("profile", profile)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)


def main():
    unreal.EditorAssetLibrary.make_directory(TREE_FOLDER + "/Profiles")
    unreal.EditorAssetLibrary.make_directory(ROCK_FOLDER + "/Profiles")

    tree_profile = create_or_load_data_asset(
        "DA_TreeHarvestProfile", TREE_FOLDER + "/Profiles", unreal.TreeHarvestProfile
    )
    tree_profile.set_editor_property("max_health", 100.0)
    tree_profile.set_editor_property("preferred_tool", enum_value(unreal.HarvestToolType, "Axe"))
    tree_profile.set_editor_property("preferred_tool_damage_multiplier", 1.5)
    tree_profile.set_editor_property("size_yield_exponent", 1.0)
    tree_profile.set_editor_property("drops", [
        drop("Log", 1.0, 2, 4),
        drop("Apple", 0.18, 1, 2),
    ])
    tree_profile.set_editor_property("standing_mesh", load_mesh(TREE_MESH))
    tree_profile.set_editor_property("hit_chip_mesh", load_mesh(LOG_MESH))
    tree_profile.set_editor_property("chips_per_hit", 2)
    tree_profile.set_editor_property("chip_scale", 0.12)
    tree_profile.set_editor_property("chip_radius", 8.0)
    tree_profile.set_editor_property("chip_impulse", 450.0)
    tree_profile.set_editor_property("fall_duration", 1.25)
    tree_profile.set_editor_property("fall_angle_degrees", 88.0)
    tree_profile.set_editor_property("cleanup_delay_seconds", 4.0)
    unreal.EditorAssetLibrary.save_loaded_asset(tree_profile, only_if_is_dirty=False)

    with ROCK_MANIFEST.open("r", encoding="utf-8") as stream:
        rock_manifest = json.load(stream)
    rock_profile = create_or_load_data_asset(
        "DA_RockHarvestProfile", ROCK_FOLDER + "/Profiles", unreal.RockHarvestProfile
    )
    rock_profile.set_editor_property("max_health", 80.0)
    rock_profile.set_editor_property("preferred_tool", enum_value(unreal.HarvestToolType, "Pickaxe"))
    rock_profile.set_editor_property("preferred_tool_damage_multiplier", 1.5)
    rock_profile.set_editor_property("size_yield_exponent", 1.0)
    rock_profile.set_editor_property("drops", [drop("Stone", 1.0, 2, 4)])
    pieces = []
    for record in rock_manifest["pieces"]:
        piece = unreal.HarvestRockPiece()
        piece.set_editor_property("mesh", load_mesh(record["asset"]))
        x, y, z = record["relative_location_cm"]
        z += record["bounds_extent_cm"][2]
        transform = unreal.Transform(
            unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, 0.0),
            unreal.Vector(1.0, 1.0, 1.0)
        )
        piece.set_editor_property("relative_transform", transform)
        pieces.append(piece)
    rock_profile.set_editor_property("pieces", pieces)
    rock_profile.set_editor_property("pieces_released_per_hit", 1)
    rock_profile.set_editor_property("piece_impulse", 550.0)
    rock_profile.set_editor_property("cleanup_delay_seconds", 3.0)
    unreal.EditorAssetLibrary.save_loaded_asset(rock_profile, only_if_is_dirty=False)

    tree_blueprint = create_or_load_blueprint(
        "BP_TreeResource", TREE_FOLDER, unreal.TreeResource
    )
    rock_blueprint = create_or_load_blueprint(
        "BP_RockResource", ROCK_FOLDER, unreal.RockResource
    )
    set_blueprint_profile(tree_blueprint, tree_profile)
    set_blueprint_profile(rock_blueprint, rock_profile)

    asset_manifest = {
        "feature": "Sia resource harvesting",
        "placeable_blueprints": [
            tree_blueprint.get_path_name(), rock_blueprint.get_path_name()
        ],
        "profiles": [tree_profile.get_path_name(), rock_profile.get_path_name()],
        "asset_groups": {
            "tools": "/Game/Sia/Harvesting/Tools/{Axes,Hammers,Pickaxes,Hoes,Shovels}",
            "trees": "/Game/Sia/Harvesting/Trees",
            "rocks": "/Game/Sia/Harvesting/Rocks/Fragments/SM_RockGroup01",
            "tree_source": TREE_MESH,
            "rock_source": rock_manifest["source"],
            "tool_source": "/Game/Fab/Survival_Game_Tool_Pack/survival_game_tool_pack_free_agustin_honnun/StaticMeshes/survival_game_tool_pack_free_agustin_honnun",
            "log_and_chip_source": LOG_MESH,
            "stone_pickup_piece": rock_manifest["stone_pickup_asset"],
        },
        "defaults": {
            "tree": {"health": 100, "axe_multiplier": 1.5, "apple_probability": 0.18},
            "rock": {"health": 80, "pickaxe_multiplier": 1.5, "edge_piece_order": "outside-in"},
            "rock_piece_ground_offset": "each piece half-height",
        },
    }
    manifest_path = ROOT / "Tools" / "HarvestAssetManifest.json"
    manifest_path.write_text(json.dumps(asset_manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("Created and saved harvesting profiles, placeable blueprints, and asset manifest.")


main()
