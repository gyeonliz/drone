"""Create automatic turrets plus a four-point ground-conforming vehicle test in the NPC/SO map.

The script is intentionally idempotent. It owns only its Blueprint assets, exact turret/
vehicle labels, and the rough-road labels below; all existing NPC, Smart Object, lighting,
and navigation content is left untouched. Set DRONE_AUTO_TURRET_VALIDATE_ONLY=1 for a
read-only pass.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_AUTO_TURRET"
BLUEPRINT_FOLDER = "/Game/Drone/AI/AutomaticTurrets/Blueprints"
VEHICLE_BLUEPRINT_FOLDER = "/Game/Drone/Vehicles/Blueprints"
MAP_PATH = "/Game/Drone/Maps/Lvl_NPCSmartObjectGreybox"
CUBE_PATH = "/Engine/BasicShapes/Cube"

SPECS = (
    {
        "name": "BP_AutoTurret_Emplaced",
        "parent": "/Script/Drone.DroneEmplacedAutomaticTurret",
        "label": "AutoTurret_Emplaced_A",
        "location": (2600.0, 1600.0, 0.0),
        "yaw": 180.0,
    },
    {
        "name": "BP_AutoTurret_Vehicle",
        "parent": "/Script/Drone.DroneVehicleAutomaticTurret",
        "label": "AutoTurret_Vehicle_A",
        "location": (2700.0, -2200.0, 120.0),
        "yaw": 180.0,
    },
)

VEHICLE_SPEC = {
    "name": "BP_GroundConformingVehicle_Greybox",
    "parent": "/Script/Drone.DroneGroundConformingVehicle",
    "folder": VEHICLE_BLUEPRINT_FOLDER,
    "wheel_spin_direction": 1.0,
}
VEHICLE_CARRIER_LABEL = "AutoTurret_VehicleCarrier_Greybox"
VEHICLE_CARRIER_LOCATION = (2700.0, -2200.0, 120.0)
VEHICLE_CARRIER_YAW = 180.0
ROUGH_ROAD_TAG = unreal.Name("DroneVehicleRoughRoad")
ROUGH_ROAD_SPECS = (
    ("VehicleRoughRoad_01", (2700.0, -2200.0, 8.0), 0.0),
    ("VehicleRoughRoad_02", (2400.0, -2200.0, 22.0), 6.0),
    ("VehicleRoughRoad_03", (2100.0, -2200.0, 30.0), -5.0),
    ("VehicleRoughRoad_04", (1800.0, -2200.0, 18.0), 5.0),
    ("VehicleRoughRoad_05", (1500.0, -2200.0, 8.0), 0.0),
)
ROUGH_ROAD_SCALE = (3.0, 4.0, 0.25)


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def asset_path(spec: dict[str, object]) -> str:
    return f"{spec.get('folder', BLUEPRINT_FOLDER)}/{spec['name']}"


def compile_blueprint(blueprint: unreal.Blueprint) -> None:
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(
        blueprint.get_editor_property("status") == unreal.BlueprintStatus.BS_UP_TO_DATE,
        f"Blueprint did not compile cleanly: {blueprint.get_path_name()}",
    )


def create_or_load_blueprint(
    editor_assets: unreal.EditorAssetSubsystem,
    spec: dict[str, object],
) -> unreal.Blueprint:
    path = asset_path(spec)
    parent_class = unreal.load_class(None, str(spec["parent"]))
    require(parent_class is not None, f"Native Blueprint parent Class is unavailable: {spec['parent']}")

    if editor_assets.does_asset_exist(path):
        blueprint = editor_assets.load_asset(path)
        require(isinstance(blueprint, unreal.Blueprint), f"Existing asset is not a Blueprint: {path}")
        require(
            unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == parent_class,
            f"Existing Blueprint has the wrong direct parent: {path}",
        )
        log(f"REUSED_BLUEPRINT|{path}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            str(spec["name"]),
            str(spec.get("folder", BLUEPRINT_FOLDER)),
            unreal.Blueprint.static_class(),
            factory,
            overwrite_existing=False,
        )
        require(isinstance(blueprint, unreal.Blueprint), f"Could not create Blueprint: {path}")
        log(f"CREATED_BLUEPRINT|{path}")

    compile_blueprint(blueprint)
    require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), f"Could not save: {path}")
    return blueprint


def validate_blueprint(blueprint: unreal.Blueprint, spec: dict[str, object]) -> None:
    expected_parent = unreal.load_class(None, str(spec["parent"]))
    compile_blueprint(blueprint)
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == expected_parent,
        f"Blueprint direct parent mismatch: {blueprint.get_path_name()}",
    )
    cdo = unreal.get_default_object(blueprint.generated_class())
    require(cdo is not None, f"Blueprint CDO unavailable: {blueprint.get_path_name()}")
    require(cdo.get_smart_object_definition() is None, "Automatic turret must not be claimable as a Smart Object")
    require(cdo.is_automatic_turret_enabled(), "Automatic turret must start enabled")
    require(cdo.get_lose_target_range() >= cdo.get_detection_range(), "Target hysteresis range is invalid")
    require(cdo.requires_target_line_of_sight(), "Automatic turret must require line of sight by default")
    require(cdo.get_mg_turret_base_mesh() is not None, "Automatic turret base mesh is missing")
    require(cdo.get_mg_turret_body_mesh() is not None, "Automatic turret Yaw body mesh is missing")
    require(cdo.get_mg_turret_barrel_mesh() is not None, "Automatic turret Pitch barrel mesh is missing")


def validate_vehicle_blueprint(blueprint: unreal.Blueprint) -> None:
    expected_parent = unreal.load_class(None, str(VEHICLE_SPEC["parent"]))
    compile_blueprint(blueprint)
    require(
        unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint) == expected_parent,
        f"Vehicle Blueprint direct parent mismatch: {blueprint.get_path_name()}",
    )
    cdo = unreal.get_default_object(blueprint.generated_class())
    require(cdo is not None, f"Vehicle Blueprint CDO unavailable: {blueprint.get_path_name()}")
    require(cdo.get_vehicle_collision() is not None, "Vehicle collision component is missing")
    require(cdo.get_vehicle_body_mesh() is not None, "Vehicle body mesh is missing")
    require(cdo.get_turret_mount() is not None, "Vehicle turret mount is missing")
    require(len(cdo.get_wheel_meshes()) == 4, "Vehicle must expose four wheel visuals")
    require(
        abs(cdo.get_wheel_visual_spin_direction_multiplier() - float(VEHICLE_SPEC["wheel_spin_direction"])) < 0.001,
        "Vehicle Blueprint wheel spin direction does not match the manually verified Greybox direction",
    )


def configure_vehicle_blueprint(
    editor_assets: unreal.EditorAssetSubsystem,
    blueprint: unreal.Blueprint,
) -> None:
    cdo = unreal.get_default_object(blueprint.generated_class())
    require(cdo is not None, f"Vehicle Blueprint CDO unavailable: {blueprint.get_path_name()}")
    cdo.set_editor_property(
        "wheel_visual_spin_direction_multiplier",
        float(VEHICLE_SPEC["wheel_spin_direction"]),
    )
    require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), f"Could not save: {blueprint.get_path_name()}")
    log(f"CONFIGURED_VEHICLE_WHEEL_SPIN|direction={VEHICLE_SPEC['wheel_spin_direction']}")


def actor_by_label(actors: unreal.EditorActorSubsystem, label: str) -> unreal.Actor | None:
    matches = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == label]
    require(len(matches) <= 1, f"Duplicate owned actor label: {label}")
    return matches[0] if matches else None


def spawn_actor(
    actors: unreal.EditorActorSubsystem,
    actor_class: unreal.Class,
    label: str,
    location: tuple[float, float, float],
    yaw: float = 0.0,
) -> unreal.Actor:
    actor = actors.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0),
    )
    require(actor is not None, f"Could not spawn actor: {label}")
    actor.set_actor_label(label)
    return actor


def create_or_load_rough_road(actors: unreal.EditorActorSubsystem) -> None:
    cube = unreal.load_asset(CUBE_PATH)
    require(isinstance(cube, unreal.StaticMesh), "Rough-road Cube is unavailable")
    for label, location, pitch in ROUGH_ROAD_SPECS:
        road = actor_by_label(actors, label)
        if road is not None and not isinstance(road, unreal.StaticMeshActor):
            require(actors.destroy_actor(road), f"Could not replace wrong rough-road Actor: {label}")
            road = None
        if road is None:
            road = spawn_actor(actors, unreal.StaticMeshActor, label, location)
            log(f"CREATED_ROUGH_ROAD|{label}")
        road.set_actor_location(unreal.Vector(*location), False, False)
        road.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=0.0, roll=0.0), False)
        road.set_actor_scale3d(unreal.Vector(*ROUGH_ROAD_SCALE))
        road.set_editor_property("tags", [ROUGH_ROAD_TAG])
        component = road.get_component_by_class(unreal.StaticMeshComponent)
        require(component is not None, f"Rough-road StaticMeshComponent is missing: {label}")
        if component.get_editor_property("static_mesh") != cube:
            require(component.set_static_mesh(cube), f"Could not assign road Cube: {label}")
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)


def create_or_load_vehicle_carrier(
    actors: unreal.EditorActorSubsystem,
    vehicle_blueprint: unreal.Blueprint,
) -> unreal.Actor:
    carrier = actor_by_label(actors, VEHICLE_CARRIER_LABEL)
    expected_class = vehicle_blueprint.generated_class()
    if carrier is not None and carrier.get_class() != expected_class:
        vehicle_turret = actor_by_label(actors, "AutoTurret_Vehicle_A")
        if vehicle_turret is not None and vehicle_turret.get_attach_parent_actor() == carrier:
            vehicle_turret.detach_from_actor(
                unreal.DetachmentRule.KEEP_WORLD,
                unreal.DetachmentRule.KEEP_WORLD,
                unreal.DetachmentRule.KEEP_WORLD,
            )
        require(actors.destroy_actor(carrier), "Could not replace the old static vehicle carrier")
        log(f"REPLACED_STATIC_CARRIER|{VEHICLE_CARRIER_LABEL}")
        carrier = None
    if carrier is None:
        carrier = spawn_actor(
            actors,
            expected_class,
            VEHICLE_CARRIER_LABEL,
            VEHICLE_CARRIER_LOCATION,
            VEHICLE_CARRIER_YAW,
        )
        log(f"CREATED_ACTOR|{VEHICLE_CARRIER_LABEL}")
    require(carrier.get_class() == expected_class, "Vehicle carrier label belongs to the wrong Actor Class")
    carrier.set_actor_location(unreal.Vector(*VEHICLE_CARRIER_LOCATION), False, False)
    carrier.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=VEHICLE_CARRIER_YAW, roll=0.0), False)
    carrier.set_editor_property(
        "wheel_visual_spin_direction_multiplier",
        float(VEHICLE_SPEC["wheel_spin_direction"]),
    )
    carrier.set_greybox_auto_drive_enabled(True)
    require(carrier.refresh_ground_conform_now(True), "Vehicle carrier could not find its four-point road surface")
    return carrier


def create_or_load_turret_actor(
    actors: unreal.EditorActorSubsystem,
    blueprint: unreal.Blueprint,
    spec: dict[str, object],
) -> unreal.Actor:
    actor = actor_by_label(actors, str(spec["label"]))
    if actor is None:
        actor = spawn_actor(
            actors,
            blueprint.generated_class(),
            str(spec["label"]),
            spec["location"],
            float(spec["yaw"]),
        )
        log(f"CREATED_ACTOR|{spec['label']}")
    require(actor.get_class() == blueprint.generated_class(), f"Placed Actor has wrong Class: {spec['label']}")
    return actor


def load_level_subsystems() -> tuple[unreal.LevelEditorSubsystem, unreal.EditorActorSubsystem]:
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    require(level_editor is not None and actors is not None, "Editor level subsystems are unavailable")
    require(level_editor.load_level(MAP_PATH), f"Could not load map: {MAP_PATH}")
    return level_editor, actors


def validate_map(
    actors: unreal.EditorActorSubsystem,
    blueprints: dict[str, unreal.Blueprint],
    vehicle_blueprint: unreal.Blueprint,
) -> None:
    carrier = actor_by_label(actors, VEHICLE_CARRIER_LABEL)
    require(carrier is not None, "Ground-conforming vehicle carrier is missing")
    require(carrier.get_class() == vehicle_blueprint.generated_class(), "Vehicle carrier Class mismatch")
    require(carrier.is_greybox_auto_drive_enabled(), "Vehicle carrier Greybox auto drive is disabled")
    require(len(carrier.get_wheel_meshes()) == 4, "Placed vehicle does not have four wheel visuals")
    require(
        abs(carrier.get_wheel_visual_spin_direction_multiplier() - float(VEHICLE_SPEC["wheel_spin_direction"])) < 0.001,
        "Placed vehicle wheel spin direction does not match the manually verified Greybox direction",
    )
    # LastGroundContactCount is transient and is intentionally zero immediately after a
    # validation-only map load. Runtime contact behavior is covered by the focused C++
    # suspension test; this asset pass validates only the saved map contract.
    rough_roads = [
        actor for actor in actors.get_all_level_actors()
        if ROUGH_ROAD_TAG in actor.get_editor_property("tags")
    ]
    require(len(rough_roads) == len(ROUGH_ROAD_SPECS), "Rough-road segment count mismatch")
    for spec in SPECS:
        actor = actor_by_label(actors, str(spec["label"]))
        blueprint = blueprints[str(spec["name"])]
        require(actor is not None, f"Placed automatic turret is missing: {spec['label']}")
        require(actor.get_class() == blueprint.generated_class(), f"Placed automatic turret Class mismatch: {spec['label']}")
        require(actor.get_smart_object_definition() is None, f"Placed automatic turret became claimable: {spec['label']}")

    vehicle = actor_by_label(actors, "AutoTurret_Vehicle_A")
    require(vehicle.get_attach_parent_actor() == carrier, "Vehicle turret must be attached to its carrier preview")
    log("MAP_VALIDATION_OK|emplaced=1|vehicle=1|vehicle_attached=true|suspension=4|road=5")


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem is unavailable")
    validate_only = os.environ.get("DRONE_AUTO_TURRET_VALIDATE_ONLY") == "1"

    blueprints: dict[str, unreal.Blueprint] = {}
    for spec in SPECS:
        path = asset_path(spec)
        if validate_only:
            blueprint = editor_assets.load_asset(path)
            require(isinstance(blueprint, unreal.Blueprint), f"Missing Blueprint: {path}")
        else:
            blueprint = create_or_load_blueprint(editor_assets, spec)
        validate_blueprint(blueprint, spec)
        blueprints[str(spec["name"])] = blueprint
        log(f"VALIDATED_BLUEPRINT|{path}")

    vehicle_path = asset_path(VEHICLE_SPEC)
    if validate_only:
        vehicle_blueprint = editor_assets.load_asset(vehicle_path)
        require(isinstance(vehicle_blueprint, unreal.Blueprint), f"Missing Blueprint: {vehicle_path}")
    else:
        vehicle_blueprint = create_or_load_blueprint(editor_assets, VEHICLE_SPEC)
        configure_vehicle_blueprint(editor_assets, vehicle_blueprint)
    validate_vehicle_blueprint(vehicle_blueprint)
    log(f"VALIDATED_BLUEPRINT|{vehicle_path}")

    level_editor, actors = load_level_subsystems()
    if not validate_only:
        create_or_load_rough_road(actors)
        carrier = create_or_load_vehicle_carrier(actors, vehicle_blueprint)
        for spec in SPECS:
            turret = create_or_load_turret_actor(actors, blueprints[str(spec["name"])], spec)
            if spec["name"] == "BP_AutoTurret_Vehicle":
                mount = carrier.get_turret_mount()
                require(mount is not None, "Vehicle turret mount component is unavailable")
                turret.attach_to_component(
                    mount,
                    unreal.Name(),
                    unreal.AttachmentRule.SNAP_TO_TARGET,
                    unreal.AttachmentRule.SNAP_TO_TARGET,
                    unreal.AttachmentRule.KEEP_WORLD,
                )
                log("ATTACHED_VEHICLE_TURRET|AutoTurret_Vehicle_A|mount=TurretMount")
        require(level_editor.save_current_level(), f"Could not save map: {MAP_PATH}")
        log(f"SAVED_MAP|{MAP_PATH}")

    validate_map(actors, blueprints, vehicle_blueprint)
    log("VALIDATION_OK")
    if not validate_only:
        log("CREATED_OK")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
