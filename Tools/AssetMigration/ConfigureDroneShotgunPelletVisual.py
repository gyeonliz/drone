"""Create/apply or validate the production Shotgun Pellet visual.

Apply explicitly with DRONE_SHOTGUN_PELLET_VISUAL_APPLY=1.  Validation is
read-only so normal checks never overwrite later Blueprint tuning.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_SHOTGUN_PELLET_VISUAL"
MATERIAL_FOLDER = "/Game/Drone/AI/Materials"
MATERIAL_NAME = "M_ShotgunPelletGlow"
MATERIAL_PATH = f"{MATERIAL_FOLDER}/{MATERIAL_NAME}"
PELLET_BLUEPRINT_PATH = "/Game/Drone/AI/Blueprints/Projectiles/BP_ShotgunPelletProjectile"


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def create_glow_material(editor_assets: unreal.EditorAssetSubsystem) -> unreal.Material:
    unreal.EditorAssetLibrary.make_directory(MATERIAL_FOLDER)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        MATERIAL_FOLDER,
        unreal.Material.static_class(),
        unreal.MaterialFactoryNew(),
        overwrite_existing=False,
    )
    require(isinstance(material, unreal.Material), f"Could not create Material: {MATERIAL_PATH}")
    material.modify()
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -420, -40
    )
    strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -420, 100
    )
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -150, 0
    )
    require(color is not None and strength is not None and multiply is not None, "Could not create Material expressions")
    color.set_editor_property("constant", unreal.LinearColor(1.0, 0.12, 0.015, 1.0))
    strength.set_editor_property("r", 35.0)
    require(
        unreal.MaterialEditingLibrary.connect_material_expressions(color, "", multiply, "A"),
        "Could not connect Pellet color",
    )
    require(
        unreal.MaterialEditingLibrary.connect_material_expressions(strength, "", multiply, "B"),
        "Could not connect Pellet emissive strength",
    )
    require(
        unreal.MaterialEditingLibrary.connect_material_property(
            multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        ),
        "Could not connect Pellet emissive output",
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    require(editor_assets.save_loaded_asset(material, only_if_is_dirty=False), "Could not save Pellet glow Material")
    log(f"CREATED_MATERIAL|{MATERIAL_PATH}|unlit=1|emissive=35")
    return material


def load_or_create_material(
    editor_assets: unreal.EditorAssetSubsystem, apply_changes: bool
) -> unreal.Material:
    material = editor_assets.load_asset(MATERIAL_PATH)
    if material is None and apply_changes:
        material = create_glow_material(editor_assets)
    require(isinstance(material, unreal.Material), f"Missing Pellet glow Material: {MATERIAL_PATH}")
    return material


def configure_or_validate(apply_changes: bool) -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem is unavailable")
    material = load_or_create_material(editor_assets, apply_changes)

    blueprint = editor_assets.load_asset(PELLET_BLUEPRINT_PATH)
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Pellet Blueprint: {PELLET_BLUEPRINT_PATH}")
    pellet_class = blueprint.generated_class()
    require(pellet_class is not None, "Pellet generated Class is unavailable")
    pellet_cdo = unreal.get_default_object(pellet_class)
    require(pellet_cdo is not None, "Pellet CDO is unavailable")
    core = pellet_cdo.get_projectile_visual()
    tracer = pellet_cdo.get_projectile_trail_visual()
    require(core is not None and tracer is not None, "Pellet visual components are unavailable")

    if apply_changes:
        blueprint.modify()
        pellet_cdo.modify()
        core.modify()
        tracer.modify()
        core.set_relative_scale3d(unreal.Vector(0.04, 0.04, 0.04))
        core.set_material(0, material)
        tracer.set_relative_location(unreal.Vector(-10.0, 0.0, 0.0), False, False)
        tracer.set_relative_scale3d(unreal.Vector(0.20, 0.0125, 0.0125))
        tracer.set_material(0, material)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        require(
            blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
            "Pellet Blueprint compile failed",
        )
        require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Pellet Blueprint")
        log("APPLIED|core_scale=0.04|tracer_length=0.20|tracer_thickness=0.0125")

    core_scale = core.get_editor_property("relative_scale3d")
    tracer_scale = tracer.get_editor_property("relative_scale3d")
    require(abs(core_scale.x - 0.04) < 0.0001, "Pellet core scale mismatch")
    require(abs(tracer_scale.x - 0.20) < 0.0001, "Pellet tracer length mismatch")
    require(abs(tracer_scale.y - 0.0125) < 0.0001, "Pellet tracer thickness mismatch")
    require(core.get_material(0) == material, "Pellet core does not use its glow Material")
    require(tracer.get_material(0) == material, "Pellet tracer does not use its glow Material")
    log("VALIDATION_OK|production_blueprint=1|emissive=1|core=1|tracer=1")


if __name__ == "__main__":
    try:
        configure_or_validate(os.environ.get("DRONE_SHOTGUN_PELLET_VISUAL_APPLY") == "1")
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
