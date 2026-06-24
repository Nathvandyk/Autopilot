# ClaudePilot

An Unreal Engine 5.8 editor plugin that drives game creation through Claude.

It gives the editor its own panel — a prompt box, a live checklist, an activity
log, a read-only scene-optimization report, and an editor-context monitor — plus
a set of tools Claude calls to build inside the engine: spawning and configuring
actors, creating materials and Blueprints, and scaffolding C++. You author a
checklist, hit **Send to Claude**, and it works the list top-to-bottom with the
tools while the panel updates live.

## How it works

ClaudePilot is a thin orchestrator over a few pieces:

- **The panel** (Slate) — prompt + a "Use checklist" toggle, a "Working in: …"
  context line, an ordered checklist with status badges, a Done list, a live
  activity log, and a scene-optimization section.
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
- **Ollama** — a local model that tidies the checklist and summarizes what you're
  working on from the editor-context log.

The same MCP server is reachable from Claude Desktop too, via an `mcp-remote` shim.

## Requirements

| Need | Notes |
| --- | --- |
| **Unreal Engine 5.8** | A C++ project (the plugin builds a C++ editor module). |
| **Epic's `ModelContextProtocol` plugin** | Ships with UE 5.8 — enable it in the project. This is the in-engine MCP server Claude connects to. |
| **Epic's `ToolsetRegistry` plugin** | Ships with UE 5.8 — enable it. ClaudePilot registers its Python tools here. |
| **Claude Code CLI** | Installed and signed in on a **Pro/Max subscription**. The bridge clears `ANTHROPIC_API_KEY` so the child process uses the subscription, not the paid API. |
| **Ollama** | Running locally, with a small instruct model pulled (default `qwen2.5-coder:latest`). Used for the checklist tidy pass + the context summaries. |

## Setup

### 1. Install the plugin
1. Copy this folder into your project's `Plugins/ClaudePilot/`.
2. Right-click the `.uproject` → **Generate Visual Studio project files**.
3. Build **Development Editor / Win64**.
4. Launch the editor, make sure **ClaudePilot** is enabled, and open **Window ▸ Claude Pilot**.

### 2. Enable + start Epic's MCP
1. **Edit ▸ Plugins** → enable **Model Context Protocol** and **Toolset Registry**, then restart the editor.
2. **Editor Preferences ▸ Model Context Protocol**:
   - **Server Port Number → `8377`** (ClaudePilot defaults to this; `8000` is avoided because Apache/XAMPP-style stacks often squat it).
   - Enable **Auto Start Server** (or run `ModelContextProtocol.StartServer` in the console).
3. After the editor loads, run `ModelContextProtocol.RefreshTools` in the console once — the log should show ClaudePilot's toolsets registering.

### 3. Install the Claude CLI
```bash
npm install -g @anthropic-ai/claude-code
claude            # then run /login and sign in to your Pro/Max account
```
The bridge launches `claude -p` with `ANTHROPIC_API_KEY` cleared, so it uses your
subscription. If `claude` isn't on `PATH`, set the full path in
`ClaudePilotConstants.h` (`DefaultClaudePath`).

### 4. Install Ollama + the model
Install Ollama from <https://ollama.com>, then pull the model:
```bash
ollama pull qwen2.5-coder:latest
```
Ollama serves at `http://localhost:11434` by default — what ClaudePilot expects
(`DefaultOllamaUrl` / `DefaultOllamaModel` in `ClaudePilotConstants.h`).

## MCP configuration

**Plugin (automatic).** When you click *Send to Claude*, the plugin writes this to
`<Project>/Saved/ClaudePilot_mcp.json` and passes it to `claude -p` — you don't
need to create it by hand:
```json
{
  "mcpServers": {
    "unreal-mcp": { "type": "http", "url": "http://127.0.0.1:8377/mcp" }
  }
}
```

**Claude Desktop (optional second front door).** To drive the same engine from
Claude Desktop, add this to `claude_desktop_config.json`
(Windows: `%APPDATA%\Claude\claude_desktop_config.json`), then restart Desktop
while the editor and its MCP server are running:
```json
{
  "mcpServers": {
    "unreal-mcp": {
      "command": "npx",
      "args": ["mcp-remote", "http://127.0.0.1:8377/mcp", "--transport", "http-only"]
    }
  }
}
```

> **Keep the port in sync.** `8377` must match in all three places: the editor's
> MCP **Server Port Number**, the plugin's `DefaultMcpUrl` (in
> `ClaudePilotConstants.h`), and any Desktop config. Change one, change them all.

## Status

Early / experimental — built incrementally as a solo project.
