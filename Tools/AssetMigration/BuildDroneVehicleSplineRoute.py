"""Create reusable vehicle spline route Blueprint assets without opening or saving a map."""

from __future__ import annotations

import traceback
import unreal


PREFIX = "DRONE_VEHICLE_SPLINE_ROUTE"
SPECS = (
    ("/Game/Drone/Vehicles/Blueprints", "BP_DroneVehicleSplineRoute", "/Script/Drone.DroneVehicleSplineRoute"),
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> None:
    for folder, name, parent_path in SPECS:
        path = f"{folder}/{name}"
        parent = unreal.load_class(None, parent_path)
        require(parent is not None, f"Native class unavailable: {parent_path}")
        unreal.EditorAssetLibrary.make_directory(folder)
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if blueprint is None:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, folder, unreal.Blueprint.static_class(), factory, overwrite_existing=False)
            unreal.log(f"{PREFIX}|CREATED|{path}")
        require(isinstance(blueprint, unreal.Blueprint), f"Not a Blueprint: {path}")
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR,
                f"Compile failed: {path}")
        require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False),
                f"Could not save: {path}")
    unreal.log(f"{PREFIX}|VALIDATION_OK|assets=1|maps_saved=0")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
