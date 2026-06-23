# ClaudePilot

An Unreal Engine 5.8 editor plugin that drives game creation through Claude.

It gives the editor its own panel — a prompt box, a live checklist, an activity
log, and a scene-optimization report — and a set of tools Claude calls to build
inside the engine: spawning and configuring actors, creating materials and
Blueprints, and scaffolding C++. You author a checklist, hit **Send to Claude**,
and it works the list top-to-bottom with the tools while the panel updates live.

## How it works

ClaudePilot is a thin orchestrator over three pieces:

- **The panel** (Slate) — prompt, a single ordered checklist with status badges,
  a Done list, a live activity log, and a read-only scene-optimization section.
- **The Claude bridge** — shells out to the local `claude` CLI on your Pro/Max
  subscription (`claude -p`, streaming `stream-json`), handing it Epic's in-engine
  **Model Context Protocol** (MCP) server so it can act in the editor.
- **Toolsets** — registered with Epic's **Toolset Registry** (Python, in
  `Content/Python/`), these are what Claude actually calls:
  - `world` — spawn / move / delete / query actors
  - `properties` — inspect and get/set any property on an actor or component
  - `assets` — create materials, material instances and Blueprints; assign materials
  - `source` — scaffold, read and write game C++ classes
  - `checklist` — drive the panel's checklist (mark running / done / failed)

The same MCP server is reachable from Claude Desktop too, via an `mcp-remote` shim.

## Requirements

- Unreal Engine 5.8 with the **ModelContextProtocol** and **ToolsetRegistry**
  plugins enabled, and the MCP server running (Editor Preferences ▸ Model Context
  Protocol — the port is configurable).
- The [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) installed
  and logged in (`claude` → `/login`) on a Claude Pro/Max subscription. The bridge
  clears `ANTHROPIC_API_KEY` for the child process so it uses the subscription.

## Install

1. Copy this folder into your project's `Plugins/ClaudePilot/` directory.
2. Regenerate project files and build (it is a C++ editor plugin).
3. Enable **ClaudePilot**, then open **Window ▸ Claude Pilot**.

## Status

Early / experimental — built incrementally as a solo project.
