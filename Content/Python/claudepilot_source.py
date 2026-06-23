"""
ClaudePilot Source toolset (Unreal MCP / Toolset Registry).

The C++ "workflow script" scaffolder: create the right KIND of class
(.h + .cpp) in the project's game module, read/overwrite/list those files.
Claude writes the logic; this tool does the plumbing.

Writes into  <Project>/Source/<ModuleName>/  (the primary game module).

IMPORTANT - the build wall: a NEW UCLASS/UFUNCTION is only usable after you
BUILD the project and RESTART the editor. These tools cannot do that for you;
create_script returns a reminder to do it. (Editing the body of an EXISTING
function is a quick Live Coding recompile, no restart.)
"""
import json
import os

import toolset_registry
import unreal


# class_type -> (prefix, parent class, parent header, template kind)
_TYPES = {
    "Actor":          ("A", "AActor",          "GameFramework/Actor.h",       "actor"),
    "Pawn":           ("A", "APawn",           "GameFramework/Pawn.h",        "actor"),
    "Character":      ("A", "ACharacter",      "GameFramework/Character.h",   "actor"),
    "ActorComponent": ("U", "UActorComponent", "Components/ActorComponent.h", "component"),
    "SceneComponent": ("U", "USceneComponent", "Components/SceneComponent.h", "component"),
    "UObject":        ("U", "UObject",         "UObject/Object.h",            "object"),
}

_HEADERS = {
    "actor": """#pragma once

#include "CoreMinimal.h"
#include "%INCLUDE%"
#include "%NAME%.generated.h"

UCLASS()
class %API% %CLASSNAME% : public %PARENT%
{
\tGENERATED_BODY()

public:
\t%CLASSNAME%();

protected:
\tvirtual void BeginPlay() override;

public:
\tvirtual void Tick(float DeltaTime) override;
};
""",
    "component": """#pragma once

#include "CoreMinimal.h"
#include "%INCLUDE%"
#include "%NAME%.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class %API% %CLASSNAME% : public %PARENT%
{
\tGENERATED_BODY()

public:
\t%CLASSNAME%();

protected:
\tvirtual void BeginPlay() override;

public:
\tvirtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
""",
    "object": """#pragma once

#include "CoreMinimal.h"
#include "%INCLUDE%"
#include "%NAME%.generated.h"

UCLASS(Blueprintable)
class %API% %CLASSNAME% : public %PARENT%
{
\tGENERATED_BODY()

public:
\t%CLASSNAME%();
};
""",
}

_CPPS = {
    "actor": """#include "%NAME%.h"

%CLASSNAME%::%CLASSNAME%()
{
\tPrimaryActorTick.bCanEverTick = true;
}

void %CLASSNAME%::BeginPlay()
{
\tSuper::BeginPlay();
}

void %CLASSNAME%::Tick(float DeltaTime)
{
\tSuper::Tick(DeltaTime);
}
""",
    "component": """#include "%NAME%.h"

%CLASSNAME%::%CLASSNAME%()
{
\tPrimaryComponentTick.bCanEverTick = true;
}

void %CLASSNAME%::BeginPlay()
{
\tSuper::BeginPlay();
}

void %CLASSNAME%::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
\tSuper::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
""",
    "object": """#include "%NAME%.h"

%CLASSNAME%::%CLASSNAME%()
{
}
""",
}


def _module_source():
    """(absolute Source/<Module> dir, ModuleName) for the primary game module."""
    proj_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    module = unreal.Paths.get_base_filename(unreal.Paths.get_project_file_path())
    return os.path.join(proj_dir, "Source", module), module


def _render(template: str, include: str, name: str, classname: str, api: str, parent: str) -> str:
    return (template
            .replace("%INCLUDE%", include)
            .replace("%CLASSNAME%", classname)
            .replace("%NAME%", name)
            .replace("%API%", api)
            .replace("%PARENT%", parent))


def _safe_name(filename: str):
    """Reject path traversal - source tools only touch the module dir."""
    if not filename or "/" in filename or "\\" in filename or filename.startswith("."):
        raise ValueError(f"invalid filename '{filename}' (no paths, just a file name)")
    return filename


@unreal.uclass()
class ClaudePilotSourceToolset(unreal.ToolsetDefinition):
    """Scaffold and edit C++ source in the project's game module."""

    @toolset_registry.tool_call
    @staticmethod
    def create_script(class_type: str, name: str, overwrite: bool = False) -> str:
        """Scaffold a new C++ class (.h + .cpp) in the game module.

        Args:
            class_type: One of Actor, Pawn, Character, ActorComponent,
                SceneComponent, UObject.
            name: Bare class name WITHOUT the prefix, e.g. "Door"
                (becomes ADoor in Door.h/Door.cpp).
            overwrite: Replace existing files if they exist.

        Returns:
            A message with the file paths + the build/restart reminder, or a
            string starting with "ERROR:".
        """
        cfg = _TYPES.get(class_type)
        if cfg is None:
            return f"ERROR: unknown class_type '{class_type}'. Options: {', '.join(_TYPES)}"
        prefix, parent, include, kind = cfg
        try:
            src_dir, module = _module_source()
            if not os.path.isdir(src_dir):
                return f"ERROR: module source dir not found: {src_dir}"
            classname = prefix + name
            api = module.upper() + "_API"
            h_path = os.path.join(src_dir, name + ".h")
            cpp_path = os.path.join(src_dir, name + ".cpp")
            if not overwrite and (os.path.exists(h_path) or os.path.exists(cpp_path)):
                return f"ERROR: {name}.h/.cpp already exists (pass overwrite=true to replace)"
            with open(h_path, "w", encoding="utf-8") as f:
                f.write(_render(_HEADERS[kind], include, name, classname, api, parent))
            with open(cpp_path, "w", encoding="utf-8") as f:
                f.write(_render(_CPPS[kind], include, name, classname, api, parent))
            return (f"Created {classname} -> {h_path} and {cpp_path}. "
                    f"NOW: build the project (VS, or the editor's Compile) and RESTART "
                    f"the editor to load the new class before spawning/using it.")
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def write_script(filename: str, content: str) -> str:
        """Overwrite a source file's full contents (e.g. to fill in logic).

        Args:
            filename: A file name in the module, e.g. "Door.cpp" or "Door.h".
            content: The complete new file contents.

        Returns:
            "OK <path>" on success, otherwise a string starting with "ERROR:".
        """
        try:
            filename = _safe_name(filename)
            src_dir, _ = _module_source()
            path = os.path.join(src_dir, filename)
            with open(path, "w", encoding="utf-8") as f:
                f.write(content)
            return f"OK {path} (build + restart if you changed declarations / added UFUNCTIONs)"
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def read_script(filename: str) -> str:
        """Read a source file's contents.

        Args:
            filename: A file name in the module, e.g. "Door.h".

        Returns:
            The file text, or a string starting with "ERROR:".
        """
        try:
            filename = _safe_name(filename)
            src_dir, _ = _module_source()
            path = os.path.join(src_dir, filename)
            if not os.path.exists(path):
                return f"ERROR: no such file '{filename}' in module source"
            with open(path, "r", encoding="utf-8") as f:
                return f.read()
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"

    @toolset_registry.tool_call
    @staticmethod
    def list_scripts() -> str:
        """List the .h/.cpp files in the project's game module.

        Returns:
            JSON array of file names, or a string starting with "ERROR:".
        """
        try:
            src_dir, _ = _module_source()
            if not os.path.isdir(src_dir):
                return f"ERROR: module source dir not found: {src_dir}"
            files = sorted(
                f for f in os.listdir(src_dir)
                if f.endswith(".h") or f.endswith(".cpp")
            )
            return json.dumps(files)
        except Exception as exc:  # noqa: BLE001
            return f"ERROR: {exc}"


# --- Register with the Toolset Registry (see claudepilot_checklist.py) ---
from toolset_registry.registration import Registration  # noqa: E402

_registration = Registration([ClaudePilotSourceToolset])
if _registration.register():
    unreal.log("[ClaudePilot] source toolset REGISTERED with the registry.")
else:
    unreal.log_warning("[ClaudePilot] source toolset registry not available at import.")
