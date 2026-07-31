<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">unreal-mcp</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/chongdashu/unreal-mcp)

</div>

This project enables AI assistant clients like Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP).

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state. The API, functionality, and implementation details are subject to significant changes. While we encourage testing and feedback, please be aware that:

- Breaking changes may occur without notice
- Features may be incomplete or unstable
- Documentation may be outdated or missing
- Production use is not recommended at this time

## 🌟 Overview

The Unreal MCP integration provides comprehensive tools for controlling Unreal Engine through natural language:

| Category | Capabilities |
|----------|-------------|
| **Actor Management** | • Create, duplicate, and delete actors (cubes, spheres, lights, cameras, etc.)<br>• Set actor transforms (position, rotation, scale)<br>• Query actor properties, bounds, and material assignments<br>• Attach actors to one another (with optional socket)<br>• Find actors by name pattern; list all actors in the current level |
| **Materials** | • Create new Material assets with editable BaseColor/Roughness/Metallic<br>• Assign materials to actor mesh slots; query the currently assigned material<br>• Create dynamic material instances; set scalar and vector (color) parameters at runtime |
| **Foliage** | • Scatter instanced foliage of a static mesh across a region of a landscape, with randomized scale |
| **Landscape** | • Query landscape info<br>• Sculpt landscape regions<br>• Paint landscape layers |
| **Blueprint Development** | • Create new Blueprint classes with custom components<br>• Add and configure components (mesh, camera, light, etc.)<br>• Set component properties and physics settings<br>• Compile Blueprints and spawn Blueprint actors<br>• Create input mappings for player controls |
| **Blueprint Node Graph** | • Add event nodes (BeginPlay, Tick, etc.)<br>• Create function call nodes and connect them<br>• Add variables with custom types and default values<br>• Create component and self references<br>• Find and manage nodes in the graph |
| **UMG / Widgets** | • Create UMG Widget Blueprints<br>• Add text blocks and buttons; bind button events<br>• Bind text block properties; add widgets to the viewport |
| **Audio** | • Create sound cues, sound classes, and sound attenuation assets<br>• Spawn ambient sound actors and configure their properties<br>• Play 2D sounds |
| **Niagara** | • Spawn Niagara systems and activate/deactivate components<br>• Set float, vector, and color user parameters<br>• Add and list user-exposed parameters |
| **Sequencer** | • Create and open Level Sequences<br>• Add actors to a sequence and set the playback range<br>• Add camera cut tracks<br>• Keyframe transforms and arbitrary properties |
| **AI / Navigation** | • Query navmesh info and rebuild navigation<br>• Find paths between points<br>• Create Behavior Trees and Blackboards; add blackboard keys<br>• Run a Behavior Tree on an actor |
| **Editor Control** | • Focus viewport on specific actors or locations, with configurable distance and camera orientation<br>• Capture a screenshot of the active viewport |

All these capabilities are accessible through natural language commands via AI assistants, making it easy to automate and control Unreal Engine workflows.

## 🧩 Components

### Sample Project (MCPGameProject) `MCPGameProject`
- Based off the Blank Project, but with the UnrealMCP plugin added.

### Plugin (UnrealMCP) `MCPGameProject/Plugins/UnrealMCP`
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server `Python/unreal_mcp_server.py`
- Implemented in `unreal_mcp_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## 📂 Directory Structure

- **MCPGameProject/** - Example Unreal project
  - **Plugins/UnrealMCP/** - C++ plugin source
    - **Source/UnrealMCP/** - Plugin source code
    - **UnrealMCP.uplugin** - Plugin definition

- **Python/** - Python server and tools
  - **tools/** - Tool modules for actor, editor, and blueprint operations
  - **scripts/** - Example scripts and demos

- **Docs/** - Comprehensive documentation
  - See [Docs/README.md](Docs/README.md) for documentation index

## 🚀 Quick Start Guide

### Prerequisites
- Unreal Engine 5.5+
- Python 3.12+
- MCP Client (e.g., Claude Desktop, Cursor, Windsurf)

### Sample project

For getting started quickly, feel free to use the starter project in `MCPGameProject`. This is a UE 5.5 Blank Starter Project with the `UnrealMCP.uplugin` already configured. 

1. **Prepare the project**
   - Right-click your .uproject file
   - Generate Visual Studio project files
2. **Build the project (including the plugin)**
   - Open solution (`.sln`)
   - Choose `Development Editor` as your target.
   - Build

### Plugin
Otherwise, if you want to use the plugin in your existing project:

1. **Copy the plugin to your project**
   - Copy `MCPGameProject/Plugins/UnrealMCP` to your project's Plugins folder

2. **Enable the plugin**
   - Edit > Plugins
   - Find "UnrealMCP" in Editor category
   - Enable the plugin
   - Restart editor when prompted

3. **Build the plugin**
   - Right-click your .uproject file
   - Generate Visual Studio project files
   - Open solution (`.sln)
   - Build with your target platform and output settings

### Python Server Setup

See [Python/README.md](Python/README.md) for detailed Python setup instructions, including:
- Setting up your Python environment
- Running the MCP server
- Using direct or server-based connections

### Configuring your MCP Client

Use the following JSON for your mcp configuration based on your MCP client.

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<path/to/the/folder/PYTHON>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

An example is found in `mcp.json`

### MCP Configuration Locations

Depending on which MCP client you're using, the configuration file location will differ:

| MCP Client | Configuration File Location | Notes |
|------------|------------------------------|-------|
| Claude Desktop | `~/.config/Claude/claude_desktop_config.json` (note capital `Claude`) | On Windows: `%APPDATA%\Claude\claude_desktop_config.json` |
| Cursor | `.cursor/mcp.json` | Located in your project root directory |
| Windsurf | `~/.config/windsurf/mcp.json` | On Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

Each client uses the same JSON format as shown in the example above. 
Simply place the configuration in the appropriate location for your MCP client.


## License
MIT

## Questions

For questions, you can reach me on X/Twitter: [@chongdashu](https://www.x.com/chongdashu)