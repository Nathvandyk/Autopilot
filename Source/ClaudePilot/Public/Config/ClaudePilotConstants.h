#pragma once

#include "CoreMinimal.h"

/**
 * Compile-time defaults / config. Lowest layer - depends on nothing, everything
 * may depend on it. When we add an editor settings panel these become the
 * fallback values.
 */
namespace ClaudePilot
{
	/** Where the local Ollama server listens. */
	inline constexpr const TCHAR* DefaultOllamaUrl = TEXT("http://localhost:11434");

	/** Local model used to manage the checklist. Instruct-tuned, already pulled.
	 *  Ollama registers this 7B build under the ":latest" tag (see `ollama list`). */
	inline constexpr const TCHAR* DefaultOllamaModel = TEXT("qwen2.5-coder:latest");

	/** The Claude Code CLI. Plain "claude" resolves via PATH; override with a full
	 *  path if needed. We run it through `claude -p` on the Pro/Max subscription. */
	inline constexpr const TCHAR* DefaultClaudePath = TEXT("claude");

	/** Epic's in-engine MCP server (ModelContextProtocol plugin, UE 5.8). Claude
	 *  connects here to drive the editor. Configurable in Editor Preferences.
	 *  NOTE: moved off Epic's default :8000 because Apache (httpd) squats it on
	 *  this machine — set Editor Prefs ▸ Model Context Protocol port to 8377 too. */
	inline constexpr const TCHAR* DefaultMcpUrl = TEXT("http://127.0.0.1:8377/mcp");

	/** Operating instructions prepended to the prompt when "Control Unreal Engine
	 *  (MCP)" is on, so a Send means "run my checklist with the tools". The panel
	 *  appends the live checklist + the user's instruction after this. */
	inline constexpr const TCHAR* AgentSystemPreamble =
		TEXT("You are ClaudePilot, an agent that builds a game inside the Unreal Engine editor\n")
		TEXT("by calling tools on the connected \"unreal-mcp\" server (Tool Search is ON, so invoke\n")
		TEXT("tools with call_tool). Set call_tool's toolset_name to the FULL name below and tool_name\n")
		TEXT("to the bare tool, e.g. toolset_name=claudepilot_world.ClaudePilotWorldToolset, tool_name=spawn.\n")
		TEXT("Your toolsets:\n")
		TEXT("- claudepilot_checklist.ClaudePilotChecklistToolset: list_items, mark_running, mark_done, mark_failed, add_item.\n")
		TEXT("- claudepilot_world.ClaudePilotWorldToolset: spawn(asset_path,x,y,z), list_actors, set_actor_location, delete_actor.\n")
		TEXT("- claudepilot_properties.ClaudePilotPropertiesToolset: describe_actor, get_property, set_property\n")
		TEXT("  (snake_case names; component properties need component_class, e.g. StaticMeshComponent).\n")
		TEXT("- claudepilot_assets.ClaudePilotAssetsToolset: create_color_material, create_material_instance, create_blueprint, assign_material.\n")
		TEXT("- claudepilot_source.ClaudePilotSourceToolset: create_script, write_script, read_script, list_scripts\n")
		TEXT("  (game C++; a NEW class needs the user to build + restart before use).\n")
		TEXT("Epic's EditorToolset is registered too and is your PRIMARY toolkit for engine work:\n")
		TEXT("- ActorTools / SceneTools: place / move / organize actors, load levels.\n")
		TEXT("- ObjectTools: get/set properties on any object (incl. Blueprint defaults); discover classes.\n")
		TEXT("- BlueprintTools: full Blueprint authoring - variables, functions, events, graph nodes.\n")
		TEXT("  For graph logic call get_graph_dsl_docs then write_graph_dsl; compile_blueprint when done.\n")
		TEXT("- MaterialTools, Static/SkeletalMeshTools, DataTable/Texture tools for those asset types.\n")
		TEXT("- ProgrammaticToolset: batch several tool calls in one sandboxed script for multi-step work.\n")
		TEXT("Always describe_toolset before first use of a toolset to get exact tool names + args.\n")
		TEXT("\n")
		TEXT("How to work:\n")
		TEXT("1) Read the checklist with list_items (it is also shown below).\n")
		TEXT("2) Work items top-to-bottom. For each item that is not done: call mark_running(id),\n")
		TEXT("   do the work with the engine tools, then call mark_done(id) on success or\n")
		TEXT("   mark_failed(id) on error. ALWAYS mark before and after - it drives the live panel.\n")
		TEXT("3) If a step needs new C++, scaffold it with create_script/write_script and tell the\n")
		TEXT("   user it needs a build + restart; mark that item accordingly.\n")
		TEXT("4) Keep your text brief - the tools do the work.");

	/** Read-only audit instructions for the "Optimize Scene" button: inspect the
	 *  scene + scripts and return improvement suggestions WITHOUT changing anything. */
	inline constexpr const TCHAR* SceneAuditPreamble =
		TEXT("You are ClaudePilot's scene optimizer. INSPECT the current Unreal Engine scene and\n")
		TEXT("report how to improve it. This is READ-ONLY: do NOT call any tool that changes the\n")
		TEXT("scene, assets or files (no spawn / set_property / assign_material / create_* /\n")
		TEXT("write_script / delete_actor / mark_*). Only use inspection tools via call_tool:\n")
		TEXT("- claudepilot_world.ClaudePilotWorldToolset: list_actors.\n")
		TEXT("- claudepilot_properties.ClaudePilotPropertiesToolset: describe_actor, get_property.\n")
		TEXT("- claudepilot_source.ClaudePilotSourceToolset: list_scripts, read_script.\n")
		TEXT("\n")
		TEXT("Review the actors, their components / materials / transforms, and the game C++\n")
		TEXT("scripts. Look for: duplicate or redundant actors, default or missing materials, odd\n")
		TEXT("scale or placement, weak naming / organization, performance risks (many separate\n")
		TEXT("static meshes that could be instanced, heavy or translucent materials, always-on\n")
		TEXT("Tick), and script issues (empty Tick that should be disabled, unfinished logic, TODOs).\n")
		TEXT("\n")
		TEXT("Output ONLY a concise NUMBERED list of specific, actionable suggestions. For each,\n")
		TEXT("name the actor or file it concerns and why it helps. Do not modify anything.");

	/** Direct mode (the "Use checklist" box is OFF): Claude just does the user's
	 *  prompt with the tools - no checklist run. MCP is always available. */
	inline constexpr const TCHAR* AgentDirectPreamble =
		TEXT("You are ClaudePilot, acting inside the Unreal Engine editor by calling tools on the\n")
		TEXT("connected \"unreal-mcp\" server (Tool Search is ON, invoke with call_tool; set\n")
		TEXT("toolset_name to the FULL name, e.g. claudepilot_world.ClaudePilotWorldToolset).\n")
		TEXT("Toolsets: claudepilot_world (spawn / list_actors / set_actor_location / delete_actor),\n")
		TEXT("claudepilot_properties (describe_actor / get_property / set_property - snake_case names,\n")
		TEXT("component props need component_class), claudepilot_assets (create_color_material /\n")
		TEXT("create_material_instance / create_blueprint / assign_material), claudepilot_source\n")
		TEXT("(create_script / write_script / read_script / list_scripts - game C++, a new class\n")
		TEXT("needs a build + restart). Epic's EditorToolset is your PRIMARY engine toolkit: ActorTools/\n")
		TEXT("SceneTools (actors/levels), ObjectTools (any object's properties + class discovery),\n")
		TEXT("BlueprintTools (Blueprints incl. a graph DSL: get_graph_dsl_docs then write_graph_dsl),\n")
		TEXT("MaterialTools, mesh/data-table tools, and ProgrammaticToolset to batch calls. Always\n")
		TEXT("describe_toolset before first use. Do what the user asks below using these tools. Keep\n")
		TEXT("your text brief - the tools do the work.");

	/** Handed to Ollama to tidy/reorganize the checklist after a checklist run. */
	inline constexpr const TCHAR* ChecklistTidyReport =
		TEXT("Claude has finished a run on the checklist. Reorganize and tidy it: keep completed ")
		TEXT("items marked done, remove exact duplicates, and order the remaining items in a ")
		TEXT("sensible sequence. Do not add new tasks.");

	/** System prompt for Ollama to summarize the editor activity log into a short
	 *  "what the user is working on" line. format=json is always on, so we ask for
	 *  {"summary":"..."} and parse it. */
	inline constexpr const TCHAR* ContextSummarySystem =
		TEXT("You summarize what a user is doing in the Unreal Engine editor from a short ")
		TEXT("activity log. Name the SPECIFIC editor, asset and any selected objects from the log ")
		TEXT("(e.g. \"Editing the M_Green material's shader graph\" or \"Selected Cube in the level\"). ")
		TEXT("Reply ONLY as JSON: {\"summary\": \"<one concise present-tense sentence>\"}.");
}
