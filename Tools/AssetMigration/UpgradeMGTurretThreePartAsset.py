"""Reparent only BP_SO_MGTurret to the dedicated three-part native turret class.

Set DRONE_MG_VALIDATE_ONLY=1 for a read-only validation pass. No other Smart Object
Blueprint or map is modified by this migration.
"""

import os
import unreal


BLUEPRINT_PATH = "/Game/Drone/AI/SmartObjects/Blueprints/BP_SO_MGTurret"
EXPECTED_PARENT_PATH = "/Script/Drone.DroneMGTurretStation"
EXPECTED_DEFINITION_PATH = "/Game/Drone/AI/SmartObjects/Definitions/SO_Def_MGTurret"
TEMP_CYLINDER_PATH = "/Engine/BasicShapes/Cylinder"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def validate(blueprint, expected_parent, expected_definition, cylinder):
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == expected_parent,
        "BP_SO_MGTurret does not use DroneMGTurretStation as its direct parent",
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(
        blueprint.get_editor_property("status") == unreal.BlueprintStatus.BS_UP_TO_DATE,
        "BP_SO_MGTurret did not compile cleanly",
    )

    cdo = unreal.get_default_object(blueprint.generated_class())
    require(cdo is not None, "BP_SO_MGTurret CDO is unavailable")
    require(
        cdo.get_editor_property("activity") == unreal.DroneSmartObjectActivity.MG_TURRET,
        "BP_SO_MGTurret Activity is not MGTurret",
    )
    require(
        cdo.get_smart_object_definition() == expected_definition,
        "BP_SO_MGTurret lost its Smart Object Definition",
    )

    base_mount = cdo.get_mg_turret_base_mount()
    yaw_pivot = cdo.get_mg_turret_yaw_pivot()
    pitch_pivot = cdo.get_mg_turret_aim_pivot()
    muzzle = cdo.get_mg_turret_muzzle()
    base_mesh = cdo.get_mg_turret_base_mesh()
    body_mesh = cdo.get_mg_turret_body_mesh()
    barrel_mesh = cdo.get_mg_turret_barrel_mesh()
    operator_anchor = cdo.get_mg_turret_operator_anchor()
    require(all((base_mount, yaw_pivot, pitch_pivot, muzzle)), "MG Turret rotation hierarchy is incomplete")
    require(all((base_mesh, body_mesh, barrel_mesh)), "MG Turret must have exactly the three authored mesh parts")
    require(operator_anchor is not None, "MG Turret must expose a Blueprint-adjustable operator anchor")
    require(yaw_pivot.get_attach_parent() == base_mount, "Yaw pivot must follow the fixed BaseMount")
    require(pitch_pivot.get_attach_parent() == yaw_pivot, "Pitch pivot must follow the Yaw pivot")
    require(base_mesh.get_attach_parent() == base_mount, "Base mesh must remain fixed under BaseMount")
    require(body_mesh.get_attach_parent() == yaw_pivot, "Body mesh must follow only the Yaw pivot")
    require(barrel_mesh.get_attach_parent() == pitch_pivot, "Barrel mesh must follow the Pitch pivot")
    require(muzzle.get_attach_parent() == pitch_pivot, "Muzzle must follow the Pitch pivot")
    require(operator_anchor.get_attach_parent() == yaw_pivot, "Operator anchor must follow the MG Yaw body")
    require(
        abs(cdo.get_mg_turret_operator_distance() - 120.0) <= 0.01,
        "MG Turret default operator distance must be 120 cm",
    )
    require(
        all(
            part.get_editor_property("static_mesh") == cylinder
            for part in (base_mesh, body_mesh, barrel_mesh)
        ),
        "All three temporary turret parts must use the Engine Cylinder mesh",
    )


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    expected_parent = unreal.load_class(None, EXPECTED_PARENT_PATH)
    expected_definition = unreal.EditorAssetLibrary.load_asset(EXPECTED_DEFINITION_PATH)
    cylinder = unreal.EditorAssetLibrary.load_asset(TEMP_CYLINDER_PATH)
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Blueprint: {BLUEPRINT_PATH}")
    require(expected_parent is not None, f"Missing native Class: {EXPECTED_PARENT_PATH}")
    require(isinstance(expected_definition, unreal.SmartObjectDefinition), "Missing MG Smart Object Definition")
    require(isinstance(cylinder, unreal.StaticMesh), "Missing Engine Cylinder mesh")

    validate_only = os.environ.get("DRONE_MG_VALIDATE_ONLY") == "1"
    current_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    if not validate_only and current_parent != expected_parent:
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, expected_parent)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        require(
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
            "Could not save the migrated BP_SO_MGTurret",
        )
        unreal.log("DRONE_MG_THREE_PART|REPARENTED_AND_SAVED")

    validate(blueprint, expected_parent, expected_definition, cylinder)
    unreal.log(
        "DRONE_MG_THREE_PART|VALIDATION_OK|base=fixed cylinder|body=yaw cylinder|barrel=pitch cylinder|operator=rear adjustable anchor"
    )


main()
