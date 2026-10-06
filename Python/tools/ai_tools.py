"""
MCP tools for NavMesh + Behavior Tree commands (unreal_ai router).
Same structure as landscape_tools.py: each @mcp.tool() forwards to the C++
FUnrealMCPAICommands handler via the existing Unreal connection.
"""

from typing import List, Optional
from mcp.server.fastmcp import FastMCP




def _send(command: str, params: dict) -> dict:
    """Envia un comando C++ con guard de conexion (editor caido -> error legible)."""
    from unreal_mcp_server import get_unreal_connection
    conn = get_unreal_connection()
    if not conn:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    return conn.send_command(command, params) or {
        "success": False, "message": "No response from Unreal Engine"}


def register_ai_tools(mcp: FastMCP):

    @mcp.tool()
    def get_navmesh_info() -> dict:
        """Get info about the current RecastNavMesh: cell size, agent params, bounds."""
        return _send("get_navmesh_info", {})

    @mcp.tool()
    def build_navigation() -> dict:
        """Rebuild navigation data for the current editor world (equivalent to the 'Build Paths' button)."""
        return _send("build_navigation", {})

    @mcp.tool()
    def find_path(start: List[float], end: List[float]) -> dict:
        """
        Find a path between two world-space points using the nav mesh.
        start / end: [x, y, z]
        Returns path_points (list of [x, y, z]) and is_partial.
        """
        return _send("find_path", {"start": start, "end": end})

    @mcp.tool()
    def create_behavior_tree(name: str, path: str = "/Game/AI", blackboard_path: Optional[str] = None) -> dict:
        """Create a new BehaviorTree asset. Optionally link an existing Blackboard by its asset path."""
        params = {"name": name, "path": path}
        if blackboard_path:
            params["blackboard_path"] = blackboard_path
        return _send("create_behavior_tree", params)

    @mcp.tool()
    def create_blackboard(name: str, path: str = "/Game/AI") -> dict:
        """Create a new BlackboardData asset (empty, no keys yet)."""
        return _send("create_blackboard", {"name": name, "path": path})

    @mcp.tool()
    def add_blackboard_key(blackboard_path: str, key_name: str, key_type: str) -> dict:
        """
        Add a key to an existing Blackboard asset.
        key_type: one of Bool, Int, Float, Vector, Object, String
        """
        return _send("add_blackboard_key", {
            "blackboard_path": blackboard_path,
            "key_name": key_name,
            "key_type": key_type,
        })

    @mcp.tool()
    def run_behavior_tree_on_actor(actor_name: str, behavior_tree_path: str) -> dict:
        """
        Start running a Behavior Tree on a Pawn's existing AIController.
        The pawn must already be possessed by an AIController (AIControllerClass set,
        AutoPossessAI configured, etc.) - this tool won't spawn a controller for you.
        """
        return _send("run_behavior_tree_on_actor", {
            "actor_name": actor_name,
            "behavior_tree_path": behavior_tree_path,
        })
