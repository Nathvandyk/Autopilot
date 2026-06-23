"""
ClaudePilot Python entry point.

Unreal auto-runs <plugin>/Content/Python/init_unreal.py at editor startup (it
does NOT auto-import arbitrary .py files). We use it to import each toolset
module so its ToolsetDefinition class is defined and self-registers with the
Toolset Registry. Add new toolset modules to the list below.

NOTE: hot-reload does NOT work for @unreal.uclass() toolset modules - a change
to a toolset (or adding one here) needs a FULL EDITOR RESTART, then
ModelContextProtocol.RefreshTools.
"""
import unreal

_TOOLSET_MODULES = (
    "claudepilot_checklist",
    "claudepilot_world",
    "claudepilot_properties",
    "claudepilot_assets",
    "claudepilot_source",
)

for _mod in _TOOLSET_MODULES:
    try:
        __import__(_mod)
        unreal.log("[ClaudePilot] imported {0} OK.".format(_mod))
    except Exception as exc:  # noqa: BLE001 - surface any toolset load error in the log
        unreal.log_error("[ClaudePilot] failed to import {0}: {1}".format(_mod, exc))
