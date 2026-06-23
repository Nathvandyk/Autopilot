"""
ClaudePilot World toolset (Unreal MCP / Toolset Registry).

First real engine-action toolset: place / move / delete / query actors in the
current level. Pure Python on UE's own editor API (unreal.EditorActorSubsystem)
- no C++, no compile. Same authoring/registration pattern as the checklist
toolset (see claudepilot_checklist.py and the canonical demo_toolset.py).

Generic property setting (any UPROPERTY, with value coercion) is intentionally
left out of this first pass - it needs type handling. These four use typed
params only, so they're robust to start with.
"""
import json

import toolset_registry
import unreal


def _actor_subsystem() -> unreal.EditorActorSubsystem:
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _find_actor(label: str):
    for actor in _actor_subsystem().get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


@unreal.uclass()
class ClaudePilotWorldToolset(unreal.ToolsetDefinition):
    """Place, move, delete and query actors in the current level."""

    @toolset_registry.tool_call
    @staticmethod
    def spawn(asset_path: str, x: float, y: float, z: float) -> str:
        """Spawn/place an asset or Blueprint into the level at a location.

        Works for anything spawnable: a Static Mesh, a Blueprint actor class,
        etc. (e.g. "/Engine/BasicShapes/Cube.Cube" or "/Game/BP/BP_Door.BP_Door").

        Args:
            asset_path: Full content path to the asset or Blueprint.
            x: World X.
            y: World Y.
            z: World Z.

        Returns:
            The spawned actor's label, or a string starting with "ERROR:".
        """
        asset = unreal.load_asset(asset_path)
        if asset is None:
            return f"ERROR: could not load asset at '{asset_path}'"

        location = unreal.Vector(x, y, z)
        rotation = unreal.Rotator(0.0, 0.0, 0.0)
        sub = _actor_subsystem()

        if isinstance(asset, unreal.Blueprint):
            actor = sub.spawn_actor_from_class(asset.generated_class(), location, rotation)
        else:
            actor = sub.spawn_actor_from_object(asset, location, rotation)

        if actor is None:
            return f"ERROR: spawn failed for '{asset_path}'"
        return actor.get_actor_label()

    @toolset_registry.tool_call
    @staticmethod
    def list_actors() -> str:
        """List the actors in the current level.

        Returns:
            JSON array of objects: {"label": ..., "class": ...}.
        """
        data = [
            {"label": a.get_actor_label(), "class": a.get_class().get_name()}
            for a in _actor_subsystem().get_all_level_actors()
        ]
        return json.dumps(data)

    @toolset_registry.tool_call
    @staticmethod
    def set_actor_location(actor_label: str, x: float, y: float, z: float) -> bool:
        """Move an actor to a world location.

        Args:
            actor_label: The actor's label (from list_actors).
            x: World X.
            y: World Y.
            z: World Z.

        Returns:
            True if the actor was found and moved.
        """
        actor = _find_actor(actor_label)
        if actor is None:
            return False
        actor.set_actor_location(unreal.Vector(x, y, z), False, False)
        return True

    @toolset_registry.tool_call
    @staticmethod
    def delete_actor(actor_label: str) -> bool:
        """Delete an actor from the level.

        Args:
            actor_label: The actor's label (from list_actors).

        Returns:
            True if the actor was found and deleted.
        """
        actor = _find_actor(actor_label)
        if actor is None:
            return False
        _actor_subsystem().destroy_actor(actor)
        return True


# --- Register with the Toolset Registry (see claudepilot_checklist.py) ---
from toolset_registry.registration import Registration  # noqa: E402

_registration = Registration([ClaudePilotWorldToolset])
if _registration.register():
    unreal.log("[ClaudePilot] world toolset REGISTERED with the registry.")
else:
    unreal.log_warning("[ClaudePilot] world toolset registry not available at import.")
