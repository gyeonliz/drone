"""Read actual Blueprint overrides, not just DA reference values. Saves no assets."""
import unreal
asset = unreal.load_asset("/Game/Drone/Data/Drones/DA_Drone_FPVStrike_Greybox")
if asset is None:
    assets = unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game/Drone/Data/Drones", True)
    unreal.log("DRONE_FLIGHT_OVERRIDE|Available=" + str([str(a.asset_name) for a in assets]))
else:
    profile = asset.get_editor_property("flight_profile")
    pawn_class = unreal.load_class(None, "/Game/Drone/Integrations/DronePackFPV/BP_DroneFPVIntegration.BP_DroneFPVIntegration_C")
    defaults = unreal.get_default_object(pawn_class)
    override = defaults.get_editor_property("blueprint_flight_profile_override")
    for name, data in (("Definition", profile), ("Blueprint", override)):
        physical = data.get_editor_property("physical_flight_settings")
        unreal.log(f"DRONE_FLIGHT_OVERRIDE|{name}|Max={data.get_editor_property('max_speed_centimeters_per_second')}|Multiplier={physical.get_editor_property('unloaded_maximum_speed_multiplier')}|Control={data.get_editor_property('default_control_mode')}")
    unreal.log(f"DRONE_FLIGHT_OVERRIDE|Enabled={defaults.get_editor_property('override_definition_flight_profile_in_blueprint')}")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level("/Game/Drone/Maps/TestMap/Tutorial/Lvl_Tutorial_Hover_Test")
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if "DefaultEntry" in actor.get_actor_label():
        unreal.log(f"DRONE_TEST_ENTRY_AUDIT|{actor.get_path_name()}|Class={actor.get_class().get_name()}|Tags={actor.get_editor_property('tags')}|Mission={actor.get_editor_property('default_test_mission')}|EditorOnly={actor.get_editor_property('is_editor_only_actor')}")
