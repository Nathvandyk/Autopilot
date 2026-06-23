"""
ClaudePilot Properties toolset (Unreal MCP / Toolset Registry).

Generic reflection over actors: get / set ANY editor property, and describe an
actor's components so Claude knows what's settable and where. This is the
high-leverage toolset - one set_property covers material params, light
intensity/colour, scale, physics flags, gameplay variables, etc.

Pure Python on UE's reflection (get_editor_property / set_editor_property).

Two things to know:
  * Property names are UE's *snake_case* editor names: "intensity",
    "light_color", "static_mesh", "relative_scale3d", "hidden_in_game", ...
  * The interesting properties usually live on a COMPONENT, not the actor.
    Pass component_class (e.g. "PointLightComponent", "StaticMeshComponent")
    to target it; leave it blank to target the actor itself. Use describe_actor
    to discover an actor's components.

Value coercion: pass the value as JSON (a string is fine too). We read the
property's current value to learn the expected type, then coerce:
  number/bool/string -> as-is; [x,y,z] -> Vector; [pitch,yaw,roll] -> Rotator;
  [r,g,b,a] -> LinearColor; an asset path string -> the loaded object (for
  object-reference properties like a mesh or material).
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


def _resolve_target(actor, component_class: str):
    """Return the actor itself, or its first component of component_class."""
    if not component_class:
        return actor
    cls = getattr(unreal, component_class, None)
    if cls is None:
        raise ValueError(f"unknown component class 'unreal.{component_class}'")
    comps = actor.get_components_by_class(cls)
    if not comps:
        raise ValueError(f"actor has no '{component_class}' component")
    return comps[0]


def _nums(value, keys):
    """Coerce a list/tuple or keyed dict into a list of floats."""
    if isinstance(value, dict):
        return [float(value.get(k, 0.0)) for k in keys]
    if isinstance(value, (list, tuple)):
        return [float(x) for x in value]
    raise ValueError(f"expected a list {list(keys)} or dict, got {value!r}")


def _coerce(value, current):
    """Coerce a JSON value to match the type of the property's current value."""
    # Structs: build empty + set fields (constructor arg order is version-touchy).
    if isinstance(current, unreal.Vector):
        n = _nums(value, ("x", "y", "z"))
        v = unreal.Vector(); v.x, v.y, v.z = n[0], n[1], n[2]; return v
    if isinstance(current, unreal.Rotator):
        n = _nums(value, ("pitch", "yaw", "roll"))
        r = unreal.Rotator(); r.pitch, r.yaw, r.roll = n[0], n[1], n[2]; return r
    if isinstance(current, unreal.LinearColor):
        n = _nums(value, ("r", "g", "b", "a"))
        if len(n) == 3:
            n.append(1.0)
        c = unreal.LinearColor(); c.r, c.g, c.b, c.a = n[0], n[1], n[2], n[3]; return c
    if isinstance(current, unreal.Color):  # FColor (bytes)
        n = _nums(value, ("r", "g", "b", "a"))
        if len(n) == 3:
            n.append(255.0)
        c = unreal.Color()
        c.r, c.g, c.b, c.a = int(n[0]), int(n[1]), int(n[2]), int(n[3])
        return c
    if isinstance(current, unreal.Vector2D):
        n = _nums(value, ("x", "y"))
        v = unreal.Vector2D(); v.x, v.y = n[0], n[1]; return v
    if isinstance(current, unreal.EnumBase):  # enum: accept member name or int
        enum_type = type(current)
        if isinstance(value, str):
            for cand in (value, value.upper()):
                try:
                    return getattr(enum_type, cand)
                except AttributeError:
                    pass
            raise ValueError(f"unknown enum value '{value}' for {enum_type.__name__}")
        return value
    # Primitives (bool before int: bool is a subclass of int).
    if isinstance(current, bool):
        return bool(value)
    if isinstance(current, int):
        return int(value)
    if isinstance(current, float):
        return float(value)
    if isinstance(current, str):
        return str(value)
    if isinstance(current, unreal.Name):
        return str(value)
    # Object reference (or currently-empty reference): value is an asset path.
    if current is None or isinstance(current, unreal.Object):
        if value in (None, ""):
            return None
        if isinstance(value, str):
            obj = unreal.load_asset(value)
            if obj is None:
                raise ValueError(f"could not load asset '{value}'")
            return obj
        raise ValueError("object property needs an asset path string")
    return value  # fallback: let set_editor_property try


def _value_to_json(val) -> str:
    if isinstance(val, unreal.Vector):
        return json.dumps({"x": val.x, "y": val.y, "z": val.z})
    if isinstance(val, unreal.Rotator):
        return json.dumps({"pitch": val.pitch, "yaw": val.yaw, "roll": val.roll})
    if isinstance(val, unreal.LinearColor):
        return json.dumps({"r": val.r, "g": val.g, "b": val.b, "a": val.a})
    if isinstance(val, unreal.Vector2D):
        return json.dumps({"x": val.x, "y": val.y})
    if val is None:
        return "null"
    if isinstance(val, unreal.Object):
        return val.get_path_name()
    if isinstance(val, (bool, int, float, str)):
        return json.dumps(val)
    return str(val)


@unreal.uclass()
class ClaudePilotPropertiesToolset(unreal.ToolsetDefinition):
    """Get/set any property on an actor or its components, and inspect actors."""

    @toolset_registry.tool_call
    @staticmethod
    def describe_actor(actor_label: str) -> str:
        """Inspect an actor: its class and its components (name + class).

        Use this to find which component to target with set_property/get_property
        (e.g. a light's settings live on its "PointLightComponent").

        Args:
            actor_label: The actor's label (from the world toolset's list_actors).

        Returns:
            JSON: {"label","class","components":[{"name","class"}...]}, or
            a string starting with "ERROR:".
        """
        actor = _find_actor(actor_label)
        if actor is None:
            return f"ERROR: no actor labelled '{actor_label}'"
        comps = actor.get_components_by_class(unreal.ActorComponent)
        return json.dumps({
            "label": actor.get_actor_label(),
            "class": actor.get_class().get_name(),
            "components": [
                {"name": c.get_name(), "class": c.get_class().get_name()}
                for c in comps
            ],
        })

    @toolset_registry.tool_call
    @staticmethod
    def get_property(actor_label: str, property_name: str, component_class: str = "") -> str:
        """Read a property's current value (also tells you its type/shape).

        Args:
            actor_label: The actor's label.
            property_name: UE snake_case editor name, e.g. "intensity",
                "relative_scale3d", "static_mesh".
            component_class: Optional component to read from, e.g.
                "StaticMeshComponent". Blank = the actor itself.

        Returns:
            The value as JSON (Vector/Color/etc. as objects, an object property
            as its asset path), or a string starting with "ERROR:".
        """
        actor = _find_actor(actor_label)
        if actor is None:
            return f"ERROR: no actor labelled '{actor_label}'"
        try:
            target = _resolve_target(actor, component_class)
            return _value_to_json(target.get_editor_property(property_name))
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def set_property(actor_label: str, property_name: str, value: str, component_class: str = "") -> str:
        """Set a property on an actor or one of its components.

        Args:
            actor_label: The actor's label.
            property_name: UE snake_case editor name, e.g. "intensity",
                "light_color", "relative_scale3d", "static_mesh".
            value: The value as JSON. Examples: "5000" (number), "true" (bool),
                "[2,2,2]" (Vector/scale), "[1,0,0,1]" (LinearColor),
                "\"/Game/M/MyMat.MyMat\"" or just /Game/... (object reference).
            component_class: Optional component to target, e.g.
                "PointLightComponent". Blank = the actor itself.

        Returns:
            "OK" on success, otherwise a string starting with "ERROR:".
        """
        actor = _find_actor(actor_label)
        if actor is None:
            return f"ERROR: no actor labelled '{actor_label}'"
        try:
            target = _resolve_target(actor, component_class)
            try:
                parsed = json.loads(value)
            except (ValueError, TypeError):
                parsed = value  # not JSON -> treat as a raw string (e.g. an asset path)
            current = target.get_editor_property(property_name)
            target.set_editor_property(property_name, _coerce(parsed, current))
            return "OK"
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"


# --- Register with the Toolset Registry (see claudepilot_checklist.py) ---
from toolset_registry.registration import Registration  # noqa: E402

_registration = Registration([ClaudePilotPropertiesToolset])
if _registration.register():
    unreal.log("[ClaudePilot] properties toolset REGISTERED with the registry.")
else:
    unreal.log_warning("[ClaudePilot] properties toolset registry not available at import.")
