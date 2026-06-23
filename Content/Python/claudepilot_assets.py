"""
ClaudePilot Assets toolset (Unreal MCP / Toolset Registry).

Create content (not just place existing assets): materials, material instances,
Blueprints - and assign a material onto an actor so you can actually SEE it.
Pure Python on UE's AssetTools + MaterialEditingLibrary.

Paths: pass a content folder like "/Game/ClaudePilot" and a name; the new
asset's full path (e.g. "/Game/ClaudePilot/M_Red.M_Red") is returned and is
what you feed back into spawn / assign_material / create_material_instance.
"""
import toolset_registry
import unreal


def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _actor_subsystem() -> unreal.EditorActorSubsystem:
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _find_actor(label: str):
    for actor in _actor_subsystem().get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


@unreal.uclass()
class ClaudePilotAssetsToolset(unreal.ToolsetDefinition):
    """Create materials / material instances / Blueprints, and assign materials."""

    @toolset_registry.tool_call
    @staticmethod
    def create_color_material(folder: str, name: str, r: float, g: float, b: float) -> str:
        """Create a simple flat-colour Material (Constant3Vector -> Base Color).

        Args:
            folder: Content folder, e.g. "/Game/ClaudePilot".
            name: Asset name, e.g. "M_Red".
            r: Red 0-1.
            g: Green 0-1.
            b: Blue 0-1.

        Returns:
            The new material's full path, or a string starting with "ERROR:".
        """
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(f"{folder}/{name}"):
                return f"{folder}/{name}.{name}"  # reuse - avoids the overwrite modal
            mat = _asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
            if mat is None:
                return f"ERROR: could not create material at {folder}/{name}"
            expr = unreal.MaterialEditingLibrary.create_material_expression(
                mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
            expr.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
            unreal.MaterialEditingLibrary.connect_material_property(
                expr, "", unreal.MaterialProperty.MP_BASE_COLOR)
            unreal.MaterialEditingLibrary.recompile_material(mat)
            return mat.get_path_name()
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def create_material_instance(folder: str, name: str, parent_material_path: str) -> str:
        """Create a Material Instance of an existing parent material.

        Args:
            folder: Content folder, e.g. "/Game/ClaudePilot".
            name: Asset name, e.g. "MI_Red".
            parent_material_path: Full path to the parent material.

        Returns:
            The new instance's full path, or a string starting with "ERROR:".
        """
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(f"{folder}/{name}"):
                return f"{folder}/{name}.{name}"  # reuse - avoids the overwrite modal
            parent = unreal.load_asset(parent_material_path)
            if parent is None:
                return f"ERROR: could not load parent material '{parent_material_path}'"
            factory = unreal.MaterialInstanceConstantFactoryNew()
            factory.set_editor_property("initial_parent", parent)
            mi = _asset_tools().create_asset(name, folder, unreal.MaterialInstanceConstant, factory)
            if mi is None:
                return f"ERROR: could not create instance at {folder}/{name}"
            return mi.get_path_name()
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def create_blueprint(folder: str, name: str, parent_class: str = "Actor") -> str:
        """Create a Blueprint asset with a given parent class.

        Args:
            folder: Content folder, e.g. "/Game/ClaudePilot".
            name: Asset name, e.g. "BP_Door".
            parent_class: Parent class name on the unreal module, e.g. "Actor",
                "Pawn", "Character".

        Returns:
            The new Blueprint's full path, or a string starting with "ERROR:".
        """
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(f"{folder}/{name}"):
                return f"{folder}/{name}.{name}"  # reuse - avoids the overwrite modal
            cls = getattr(unreal, parent_class, None)
            if cls is None:
                return f"ERROR: unknown parent class 'unreal.{parent_class}'"
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", cls)
            bp = _asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
            if bp is None:
                return f"ERROR: could not create Blueprint at {folder}/{name}"
            return bp.get_path_name()
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def assign_material(actor_label: str, material_path: str, slot: int = 0,
                        component_class: str = "StaticMeshComponent") -> str:
        """Assign a material onto an actor's mesh component slot (visible change).

        Args:
            actor_label: The actor's label (from the world toolset's list_actors).
            material_path: Full path to the material or material instance.
            slot: Material slot index (default 0).
            component_class: Mesh component to assign on (default
                "StaticMeshComponent").

        Returns:
            "OK" on success, otherwise a string starting with "ERROR:".
        """
        actor = _find_actor(actor_label)
        if actor is None:
            return f"ERROR: no actor labelled '{actor_label}'"
        try:
            cls = getattr(unreal, component_class, None)
            if cls is None:
                return f"ERROR: unknown component class 'unreal.{component_class}'"
            comps = actor.get_components_by_class(cls)
            if not comps:
                return f"ERROR: actor has no '{component_class}'"
            mat = unreal.load_asset(material_path)
            if mat is None:
                return f"ERROR: could not load material '{material_path}'"
            comps[0].set_material(slot, mat)
            return "OK"
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"


# --- Register with the Toolset Registry (see claudepilot_checklist.py) ---
from toolset_registry.registration import Registration  # noqa: E402

_registration = Registration([ClaudePilotAssetsToolset])
if _registration.register():
    unreal.log("[ClaudePilot] assets toolset REGISTERED with the registry.")
else:
    unreal.log_warning("[ClaudePilot] assets toolset registry not available at import.")
