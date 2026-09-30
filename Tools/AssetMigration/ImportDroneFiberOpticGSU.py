"""Import and apply the user-supplied GSU Fiber Optic Drone visual.

The supplied GSU file is a vertical fiber canister, not a complete airframe.
It is mounted in the dedicated FiberSpoolMeshComponent while the project-owned
DroneSpy body and four animated rotor parts provide the airframe. Gameplay, collision,
detonation, jamming immunity, and the trailing fiber spline remain owned by
ADronePrototypePawn.
"""

from __future__ import annotations

import os
import traceback

import unreal


PREFIX = "DRONE_FIBER_GSU"
DEFAULT_SOURCE_ROOT = r"C:\Users\Metacon_41\Downloads\새 폴더\광섬유"
SOURCE_ROOT = os.environ.get("DRONE_FIBER_GSU_SOURCE", DEFAULT_SOURCE_ROOT)

TARGET_ROOT = "/Game/Drone/ThirdParty/FiberOpticGSU"
MESH_NAME = "SM_FiberOpticGSU"
MESH_PATH = f"{TARGET_ROOT}/{MESH_NAME}"
MATERIAL_NAME = "M_FiberOpticGSU"
MATERIAL_PATH = f"{TARGET_ROOT}/{MATERIAL_NAME}"
FIBER_BLUEPRINT_PATH = "/Game/Drone/Integrations/RoleDrones/BP_DroneFiberOpticIntegration"
FIBER_DEFINITION_PATH = "/Game/Drone/Data/Drones/DA_Drone_FiberOptic_Greybox"
FIBER_PREVIEW_MESH_PATH = "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01Body"

FIBER_SPY_PARTS = {
    "VisualMeshComponent": (
        "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01Body",
        (0.0, 0.0, 0.0), (0.0, -90.0, 0.0), (6.0, 6.0, 6.0), False,
    ),
    "FPVRotorA": (
        "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r1",
        (67.44, 6.12, 0.24), (0.0, 0.0, 0.0), (6.0, 6.0, 6.0), True,
    ),
    "FPVRotorB": (
        "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r2",
        (-6.072, -67.56, 0.24), (0.0, 0.0, 0.0), (6.0, 6.0, 6.0), True,
    ),
    "FPVRotorC": (
        "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r3",
        (6.12, 67.32, 0.24), (0.0, 0.0, 0.0), (6.0, 6.0, 6.0), True,
    ),
    "FPVRotorD": (
        "/Game/Drone/ThirdParty/DronePack/D_Mesh/DroneSpy/SM_Drone01_r4",
        (-67.392, -6.24, 0.24), (0.0, 0.0, 0.0), (6.0, 6.0, 6.0), True,
    ),
}

TEXTURE_IMPORTS = {
    "BaseColor": ("GSU_Material_BaseColor.png", "T_FiberOpticGSU_BaseColor"),
    "Emissive": ("GSU_Material_Emissive.png", "T_FiberOpticGSU_Emissive"),
    "Normal": ("GSU_Material_Normal.png", "T_FiberOpticGSU_Normal"),
    "ORM": (
        "GSU_Material_OcclusionRoughnessMetallic.png",
        "T_FiberOpticGSU_ORM",
    ),
}


def log(message: str) -> None:
    unreal.log(f"{PREFIX}|{message}")


def require(condition, message: str):
    if not condition:
        raise RuntimeError(message)
    return condition


def source_file(*parts: str) -> str:
    path = os.path.join(SOURCE_ROOT, *parts)
    require(os.path.isfile(path), f"Missing source file: {path}")
    return path


def import_task(filename: str, destination_name: str, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", TARGET_ROOT)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    return task


def import_source_assets(editor_assets: unreal.EditorAssetSubsystem):
    unreal.EditorAssetLibrary.make_directory(TARGET_ROOT)
    texture_tasks = []
    for source_name, asset_name in TEXTURE_IMPORTS.values():
        texture_tasks.append(import_task(source_file("Tex", source_name), asset_name))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(texture_tasks)

    fbx_options = unreal.FbxImportUI()
    fbx_options.set_editor_property("import_mesh", True)
    fbx_options.set_editor_property("import_as_skeletal", False)
    fbx_options.set_editor_property("import_materials", False)
    fbx_options.set_editor_property("import_textures", False)
    fbx_options.set_editor_property("automated_import_should_detect_type", False)
    fbx_options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    static_options = fbx_options.get_editor_property("static_mesh_import_data")
    static_options.set_editor_property("combine_meshes", True)
    static_options.set_editor_property("generate_lightmap_u_vs", True)
    static_options.set_editor_property("auto_generate_collision", False)

    mesh_task = import_task(source_file("GSU.fbx"), MESH_NAME, fbx_options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([mesh_task])
    mesh = editor_assets.load_asset(MESH_PATH)
    require(isinstance(mesh, unreal.StaticMesh), f"Static Mesh import failed: {MESH_PATH}")

    textures = {}
    for key, (_, asset_name) in TEXTURE_IMPORTS.items():
        path = f"{TARGET_ROOT}/{asset_name}"
        texture = editor_assets.load_asset(path)
        require(isinstance(texture, unreal.Texture2D), f"Texture import failed: {path}")
        textures[key] = texture

    textures["Normal"].set_editor_property("srgb", False)
    textures["Normal"].set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
    )
    textures["ORM"].set_editor_property("srgb", False)
    textures["ORM"].set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_MASKS
    )
    for texture in textures.values():
        require(editor_assets.save_loaded_asset(texture, only_if_is_dirty=False), f"Could not save {texture.get_path_name()}")
    return mesh, textures


def create_material(editor_assets: unreal.EditorAssetSubsystem, textures) -> unreal.Material:
    existing = editor_assets.load_asset(MATERIAL_PATH)
    if existing is not None:
        require(isinstance(existing, unreal.Material), f"Wrong asset type: {MATERIAL_PATH}")
        return existing

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        TARGET_ROOT,
        unreal.Material.static_class(),
        unreal.MaterialFactoryNew(),
        overwrite_existing=False,
    )
    require(isinstance(material, unreal.Material), f"Could not create Material: {MATERIAL_PATH}")
    material.modify()

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -700, -260
    )
    normal = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -700, 80
    )
    orm = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -700, 300
    )
    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -700, -80
    )
    emissive_strength = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -440, -10
    )
    emissive_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -220, -80
    )
    require(
        all(node is not None for node in (base_color, normal, orm, emissive, emissive_strength, emissive_multiply)),
        "Could not create Fiber material expressions",
    )
    base_color.set_editor_property("texture", textures["BaseColor"])
    base_color.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal.set_editor_property("texture", textures["Normal"])
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm.set_editor_property("texture", textures["ORM"])
    orm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    emissive.set_editor_property("texture", textures["Emissive"])
    emissive.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    emissive_strength.set_editor_property("r", 1.5)

    require(unreal.MaterialEditingLibrary.connect_material_property(base_color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR), "BaseColor link failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL), "Normal link failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), "AO link failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS), "Roughness link failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC), "Metallic link failed")
    require(unreal.MaterialEditingLibrary.connect_material_expressions(emissive, "RGB", emissive_multiply, "A"), "Emissive color link failed")
    require(unreal.MaterialEditingLibrary.connect_material_expressions(emissive_strength, "", emissive_multiply, "B"), "Emissive strength link failed")
    require(unreal.MaterialEditingLibrary.connect_material_property(emissive_multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "Emissive output link failed")
    unreal.MaterialEditingLibrary.recompile_material(material)
    require(editor_assets.save_loaded_asset(material, only_if_is_dirty=False), "Could not save Fiber Material")
    return material


def gather_subobjects(blueprint: unreal.Blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    result = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        name = str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)
        if name:
            result[name] = obj
    return result


def calculate_canister_transform(mesh: unreal.StaticMesh):
    bounds = mesh.get_bounding_box()
    bounds_min = bounds.min
    bounds_max = bounds.max
    center = (bounds_min + bounds_max) * 0.5
    size = bounds_max - bounds_min
    largest_dimension = max(abs(size.x), abs(size.y), abs(size.z), 0.001)
    uniform_scale = 28.0 / largest_dimension
    desired_center = unreal.Vector(-24.0, 0.0, -16.0)
    location = unreal.Vector(
        desired_center.x - center.x * uniform_scale,
        desired_center.y - center.y * uniform_scale,
        desired_center.z - center.z * uniform_scale,
    )
    return location, uniform_scale, size, bounds_max


def apply_visual(editor_assets: unreal.EditorAssetSubsystem, mesh: unreal.StaticMesh, material: unreal.Material):
    mesh.modify()
    mesh.set_material(0, material)
    require(editor_assets.save_loaded_asset(mesh, only_if_is_dirty=False), "Could not save Fiber Static Mesh")

    blueprint = editor_assets.load_asset(FIBER_BLUEPRINT_PATH)
    require(isinstance(blueprint, unreal.Blueprint), f"Missing Fiber Blueprint: {FIBER_BLUEPRINT_PATH}")
    blueprint.modify()
    components = gather_subobjects(blueprint)
    for name, (part_path, part_location, part_rotation, part_scale, is_rotor) in FIBER_SPY_PARTS.items():
        component = components.get(name)
        require(isinstance(component, unreal.StaticMeshComponent), f"Fiber {name} is missing")
        part_mesh = editor_assets.load_asset(part_path)
        require(isinstance(part_mesh, unreal.StaticMesh), f"Missing FPV part: {part_path}")
        component.set_editor_property("static_mesh", part_mesh)
        component.set_editor_property("relative_location", unreal.Vector(*part_location))
        component.set_editor_property(
            "relative_rotation",
            unreal.Rotator(pitch=part_rotation[0], yaw=part_rotation[1], roll=part_rotation[2]),
        )
        component.set_editor_property("relative_scale3d", unreal.Vector(*part_scale))
        tags = [unreal.Name("DroneRoleVisual")]
        if is_rotor:
            tags.append(unreal.Name("DroneRotor"))
        component.set_editor_property("component_tags", tags)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("generate_overlap_events", False)
        component.set_editor_property("can_ever_affect_navigation", False)
        component.set_visibility(True, True)
        component.set_hidden_in_game(False, True)

    spool = components.get("FiberSpoolMeshComponent")
    require(isinstance(spool, unreal.StaticMeshComponent), "Fiber spool component is missing")
    location, scale, source_size, bounds_max = calculate_canister_transform(mesh)
    spool.set_editor_property("static_mesh", mesh)
    spool.set_editor_property("relative_location", location)
    spool.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    spool.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
    spool.set_editor_property("component_tags", [unreal.Name("FiberSpoolVisual"), unreal.Name("FiberGSUVisual")])
    spool.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    spool.set_editor_property("generate_overlap_events", False)
    spool.set_editor_property("can_ever_affect_navigation", False)
    spool.set_visibility(True, True)
    spool.set_hidden_in_game(False, True)
    spool.set_material(0, material)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_ERROR, "Fiber Blueprint compile failed")
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.modify()
    cdo.set_editor_property("rotor_visual_spin_enabled", True)
    cdo.set_editor_property("first_person_camera_boom_offset", unreal.Vector(58.0, 0.0, 10.0))
    cdo.set_editor_property("fiber_spool_exit_offset", unreal.Vector(0.0, 0.0, bounds_max.z))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    require(editor_assets.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save Fiber Blueprint")

    definition = editor_assets.load_asset(FIBER_DEFINITION_PATH)
    require(definition is not None, f"Missing Fiber Definition: {FIBER_DEFINITION_PATH}")
    definition.modify()
    preview_mesh = editor_assets.load_asset(FIBER_PREVIEW_MESH_PATH)
    require(isinstance(preview_mesh, unreal.StaticMesh), f"Missing Fiber preview mesh: {FIBER_PREVIEW_MESH_PATH}")
    definition.set_editor_property("preview_mesh", preview_mesh)
    definition.set_editor_property("display_name", "광섬유 자폭 드론")
    definition.set_editor_property(
        "description",
        "유선 광섬유 신호로 재밍을 회피하고 충돌 자폭을 수행하는 FPV 기체입니다.",
    )
    require(editor_assets.save_loaded_asset(definition, only_if_is_dirty=False), "Could not save Fiber Definition")

    log(
        "APPLIED|"
        f"mesh={MESH_PATH}|material={MATERIAL_PATH}|"
        f"source_size=({size_text(source_size)})|canister_height=28.0|scale={scale:.6f}|"
        f"location=({location.x:.3f},{location.y:.3f},{location.z:.3f})|"
        "airframe=DroneSpy|rotors=4|spool_slot=GSU|gameplay_preserved=1"
    )


def size_text(size) -> str:
    return f"{size.x:.3f},{size.y:.3f},{size.z:.3f}"


def main() -> None:
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    require(editor_assets is not None, "EditorAssetSubsystem is unavailable")
    mesh, textures = import_source_assets(editor_assets)
    material = create_material(editor_assets, textures)
    apply_visual(editor_assets, mesh, material)
    require(unreal.EditorAssetLibrary.save_directory(TARGET_ROOT, only_if_is_dirty=False, recursive=True), "Could not save Fiber asset directory")
    log("COMPLETE|assets=6|blueprint=1|definition=1")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"{PREFIX}|FAILED|{exc}")
        unreal.log_error(traceback.format_exc())
        raise
