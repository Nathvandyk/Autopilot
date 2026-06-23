"""
ClaudePilot checklist toolset (Unreal MCP / Toolset Registry).

Tools Claude calls to drive the in-editor checklist as it works: read the list,
flip an item to running / done / failed, and add a new item. Each tool forwards
to UClaudePilotChecklistLibrary (C++), which talks to the live
FClaudePilotController, so the panel's badges update live.

Authored to match Epic's canonical example:
  Engine/Plugins/Experimental/ToolsetRegistry/Content/Python/toolset_registry/
  tests/demo_toolset.py
  - class inherits unreal.ToolsetDefinition, decorated @unreal.uclass()
  - each tool is a @staticmethod marked with @toolset_registry.tool_call
    (toolsets are Blueprint Function Libraries, so tools must be static)
  - all params + return types annotated; Google-style docstrings -> schema
Discovered at editor startup (via init_unreal.py) and on
ModelContextProtocol.RefreshTools.
"""
import toolset_registry
import unreal


@unreal.uclass()
class ClaudePilotChecklistToolset(unreal.ToolsetDefinition):
    """Drive the ClaudePilot checklist as you work through it."""

    @toolset_registry.tool_call
    @staticmethod
    def list_items() -> str:
        """Return the current checklist as a JSON array.

        Returns:
            JSON string of objects with fields: id, title, description, status
            (status is one of: pending, running, done, failed).
        """
        return unreal.ClaudePilotChecklistLibrary.get_checklist_json()

    @toolset_registry.tool_call
    @staticmethod
    def set_status(item_id: int, status: str) -> bool:
        """Set a checklist item's status.

        Args:
            item_id: The id of the item (from list_items).
            status: One of "pending", "running", "done", "failed".

        Returns:
            True if the item existed and was updated.
        """
        return unreal.ClaudePilotChecklistLibrary.set_item_status(item_id, status)

    @toolset_registry.tool_call
    @staticmethod
    def mark_running(item_id: int) -> bool:
        """Mark a checklist item as the one being worked on right now.

        Args:
            item_id: The id of the item.

        Returns:
            True if the item existed and was updated.
        """
        return unreal.ClaudePilotChecklistLibrary.set_item_status(item_id, 'running')

    @toolset_registry.tool_call
    @staticmethod
    def mark_done(item_id: int) -> bool:
        """Mark a checklist item complete.

        Args:
            item_id: The id of the item.

        Returns:
            True if the item existed and was updated.
        """
        return unreal.ClaudePilotChecklistLibrary.set_item_status(item_id, 'done')

    @toolset_registry.tool_call
    @staticmethod
    def mark_failed(item_id: int) -> bool:
        """Mark a checklist item as failed (you attempted it and it errored).

        Args:
            item_id: The id of the item.

        Returns:
            True if the item existed and was updated.
        """
        return unreal.ClaudePilotChecklistLibrary.set_item_status(item_id, 'failed')

    @toolset_registry.tool_call
    @staticmethod
    def add_item(title: str, description: str) -> int:
        """Add a new checklist item (e.g. a sub-step you discovered).

        Args:
            title: Short name of the step.
            description: What to do for this step.

        Returns:
            The new item's id, or 0 if it could not be added.
        """
        return unreal.ClaudePilotChecklistLibrary.add_item(title, description)


# --- Register with the Toolset Registry ---
# Defining the class is NOT enough: Python toolsets must be explicitly
# registered. (C++ toolsets auto-register, which is why AgentSkillToolset "just
# appears.") Doing this at import time means it also survives a
# `toolset_registry.reload_module(claudepilot_checklist)` hot-reload.
from toolset_registry.registration import Registration  # noqa: E402

_registration = Registration([ClaudePilotChecklistToolset])
if _registration.register():
    unreal.log("[ClaudePilot] checklist toolset REGISTERED with the registry.")
else:
    unreal.log_warning(
        "[ClaudePilot] toolset registry not available at import time - "
        "NOT registered yet (re-run after the editor finishes loading).")

