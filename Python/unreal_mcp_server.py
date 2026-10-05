"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import socket
import sys
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp.log'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("UnrealMCP")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(30)  # 30 second timeout
            
            # Set socket options for better stability
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            
            # Set larger buffer sizes
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(30)  # 30 second timeout
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                # Process the data received so far
                data = b''.join(chunks)
                decoded_data = data.decode('utf-8')
                
                # Try to parse as JSON to check if complete
                try:
                    json.loads(decoded_data)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    # Not complete JSON yet, continue reading
                    logger.debug(f"Received partial response, waiting for more data...")
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                # If we have some data already, try to use it
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except:
                    pass
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        # Always reconnect for each command, since Unreal closes the connection after each command
        # This is different from Unity which keeps connections alive
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        
        try:
            # Match Unity's command format exactly
            command_obj = {
                "type": command,  # Use "type" instead of "command"
                "params": params or {}  # Use Unity's params or {} pattern
            }
            
            # Send without newline, exactly like Unity
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            
            # Read response using improved handler
            response_data = self.receive_full_response(self.socket)
            response = json.loads(response_data.decode('utf-8'))
            
            # Log complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check for both error formats: {"status": "error", ...} and {"success": false, ...}
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                # We want to preserve the original error structure but ensure error is accessible
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                # This format uses {"success": false, "error": "message"} or {"success": false, "message": "message"}
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                # Convert to the standard format expected by higher layers
                response = {
                    "status": "error",
                    "error": error_message
                }
            
            # Always close the connection after command is complete
            # since Unreal will close it on its side anyway
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            # Always reset connection state on any error
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                _unreal_connection = None
        else:
            # Verify connection is still valid with a ping-like test
            try:
                # Simple test by sending an empty buffer to check if socket is still connected
                _unreal_connection.socket.sendall(b'\x00')
                logger.debug("Connection verified with ping test")
            except Exception as e:
                logger.warning(f"Existing connection failed: {e}")
                _unreal_connection.disconnect()
                _unreal_connection = None
                # Try to reconnect
                _unreal_connection = UnrealConnection()
                if not _unreal_connection.connect():
                    logger.warning("Could not reconnect to Unreal Engine")
                    _unreal_connection = None
                else:
                    logger.info("Successfully reconnected to Unreal Engine")
        
        return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("UnrealMCP server starting up")
    try:
        _unreal_connection = get_unreal_connection()
        if _unreal_connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        _unreal_connection = None
    
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP server shut down")

# Initialize server
mcp = FastMCP(
    "UnrealMCP",
    instructions="Unreal Engine integration via Model Context Protocol",
    lifespan=server_lifespan
)

# Import and register tools
from tools.editor_tools import register_editor_tools
from tools.blueprint_tools import register_blueprint_tools
from tools.node_tools import register_blueprint_node_tools
from tools.project_tools import register_project_tools
from tools.landscape_tools import register_landscape_tools
from tools.ai_tools import register_ai_tools
from tools.umg_tools import register_umg_tools
from tools.audio_tools import register_audio_tools
from tools.niagara_tools import register_niagara_tools
from tools.sequencer_tools import register_sequencer_tools
from tools.component_tools import register_component_tools
from tools.material_node_tools import register_material_node_tools
from tools.viewport_tools import register_viewport_tools
from tools.rendering_tools import register_rendering_tools
from tools.foliage_tools import register_foliage_tools
from tools.vfx_tools import register_vfx_tools

# Register tools
register_editor_tools(mcp)
register_blueprint_tools(mcp)
register_blueprint_node_tools(mcp)
register_project_tools(mcp)
register_landscape_tools(mcp)
register_ai_tools(mcp)
register_umg_tools(mcp)  
register_audio_tools(mcp)
register_niagara_tools(mcp)
register_sequencer_tools(mcp)
register_component_tools(mcp)
register_material_node_tools(mcp)
register_viewport_tools(mcp)
register_rendering_tools(mcp)
register_foliage_tools(mcp)
register_vfx_tools(mcp)

# Expone cada accion de cada router como tool individual.
# Dos estilos en el repo:
#   1) router con closure `ACTIONS = {accion: funcion}` -> se registra la propia funcion
#   2) modulo con `_XXX_ACTIONS = {accion: "firma: doc"}` y passthrough a
#      unreal.send_command -> se genera una funcion con la firma parseada del docstring
# Nombre de tool: "<dominio>_<accion>" (ej. actor_spawn_actor, niagara_spawn_niagara_system).
# Los routers originales se mantienen.
import ast as _ast
import re as _re
from typing import List as _List

def _split_signature(doc: str):
    """'f(a, b=1): texto' -> ('a, b=1', 'texto'); si no matchea -> (None, doc)."""
    match = _re.match(r"\s*\w+\s*\(", doc or "")
    if not match:
        return None, doc
    start = match.end() - 1
    depth = 0
    end = start
    for end in range(start, len(doc)):
        if doc[end] == "(":
            depth += 1
        elif doc[end] == ")":
            depth -= 1
            if depth == 0:
                break
    else:
        return None, doc
    rest = doc[end + 1:].lstrip()
    if not rest.startswith(":"):
        return None, doc
    return doc[start + 1:end].strip(), rest[1:].strip()

def _literal(node):
    try:
        return _ast.literal_eval(node)
    except Exception:
        return None

def _annotation_for(value, has_default):
    if not has_default:
        return "_Any"
    if value is None:
        return "_Optional[_Any]"
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, int):
        return "int"
    if isinstance(value, float):
        return "float"
    if isinstance(value, str):
        return "str"
    if isinstance(value, (list, tuple)):
        if value and all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in value):
            return "_List[float]"
        return "_List[_Any]"
    if isinstance(value, dict):
        return "_Dict[str, _Any]"
    return "_Any"

def _build_passthrough(action_name: str, args_src: str, doc: str, tool_fn_name: str):
    """Genera una tool con la firma parseada de `args_src` que hace send_command."""
    tree = _ast.parse(f"def _sig({args_src}):\n    pass")
    args = tree.body[0].args
    positional = list(args.posonlyargs) + list(args.args)
    ndflt = len(args.defaults)
    entries = []
    for index, arg in enumerate(positional):
        has_default = index >= len(positional) - ndflt
        node = args.defaults[index - (len(positional) - ndflt)] if has_default else None
        entries.append((arg.arg, has_default, node))
    for arg, node in zip(args.kwonlyargs, args.kw_defaults):
        entries.append((arg.arg, node is not None, node))

    parts = []
    payload = []
    for name, has_default, node in entries:
        payload.append(f"'{name}': {name}")
        if has_default:
            value = _literal(node)
            parts.append(f"{name}: {_annotation_for(value, True)} = {_ast.unparse(node)}")
        else:
            parts.append(f"{name}: _Any")
    signature = ", ".join(["ctx: Context"] + parts)
    summary = (doc or "").replace("\\", "\\\\").replace('"""', "'''").strip()
    source = (
        f'async def {tool_fn_name}({signature}) -> _Dict[str, _Any]:\n'
        f'    """{summary}"""\n'
        f'    from unreal_mcp_server import get_unreal_connection\n'
        f'    _params = {{{", ".join(payload)}}}\n'
        f'    try:\n'
        f'        unreal = get_unreal_connection()\n'
        f'        if not unreal:\n'
        f'            return {{"success": False, "message": "Failed to connect to Unreal Engine"}}\n'
        f'        response = unreal.send_command("{action_name}", _params)\n'
        f'        if not response:\n'
        f'            return {{"success": False, "message": "No response from Unreal Engine"}}\n'
        f'        return response\n'
        f'    except Exception as exc:\n'
        f'        return {{"success": False, "message": f"Error ejecutando {action_name}: {{exc}}"}}\n'
    )
    namespace = {
        "Context": Context,
        "_Any": Any,
        "_Dict": Dict,
        "_List": _List,
        "_Optional": Optional,
    }
    exec(source, namespace)  # noqa: S102 - fuente generada localmente
    return namespace[tool_fn_name]

def _flatten_routers(server: FastMCP) -> int:
    manager = server._tool_manager
    added = 0
    for router_name in [n for n in list(manager._tools) if n.startswith("unreal_")]:
        router_fn = getattr(manager._tools[router_name], "fn", None)
        if router_fn is None or not hasattr(router_fn, "__code__"):
            continue
        domain = router_name[len("unreal_"):]
        freevars = router_fn.__code__.co_freevars
        closure_actions = None
        if "ACTIONS" in freevars:
            closure_actions = router_fn.__closure__[freevars.index("ACTIONS")].cell_contents

        if closure_actions:
            for action_name, action_fn in closure_actions.items():
                tool_name = f"{domain}_{action_name}"
                if tool_name in manager._tools:
                    logger.warning(f"Flatten: nombre duplicado {tool_name}, se omite")
                    continue
                try:
                    manager.add_tool(action_fn, name=tool_name)
                    added += 1
                except Exception as exc:
                    logger.warning(f"Flatten: no se pudo registrar {tool_name}: {exc}")
            continue

        module_actions = None
        for key, value in getattr(router_fn, "__globals__", {}).items():
            if key.endswith("_ACTIONS") and isinstance(value, dict) and value:
                module_actions = value
                break
        if not module_actions:
            logger.warning(f"Flatten: {router_name} no expuso ACTIONS")
            continue

        for action_name, doc in module_actions.items():
            tool_name = f"{domain}_{action_name}"
            if tool_name in manager._tools:
                logger.warning(f"Flatten: nombre duplicado {tool_name}, se omite")
                continue
            args_src, summary = _split_signature(doc if isinstance(doc, str) else "")
            if args_src is None:
                args_src, summary = "", (doc if isinstance(doc, str) else "")
            try:
                fn = _build_passthrough(action_name, args_src, doc, f"_{tool_name}")
                manager.add_tool(fn, name=tool_name, description=doc)
                added += 1
            except Exception as exc:
                logger.warning(f"Flatten: no se pudo generar {tool_name}: {exc}")
    logger.info(f"Flatten: {added} tools individuales registradas")
    return added

FLATTENED_TOOLS = _flatten_routers(mcp)

@mcp.prompt()
def info():
    """Information about available Unreal MCP tools and best practices."""
    return """
    # Unreal MCP Server Tools and Best Practices

    Este servidor expone cada operacion como tool individual
    (`<dominio>_<accion>`, ej. `actor_spawn_actor`,
    `landscape_sculpt_landscape_region`, `niagara_spawn_niagara_system`),
    ademas de los routers originales `unreal_<dominio>` que siguen funcionando
    con `action` + `params`.

    Ejemplo: `actor_spawn_actor(name=..., type=...)` o, equivalente,
    `unreal_actor(action="spawn_actor", params={"name": ..., "type": ...})`.

    Para ver los parametros de cada tool, leer su schema/descripcion; el detalle
    de cada operacion tambien esta en los modulos `Python/tools/*.py`.

    ## Routers disponibles

    - `unreal_actor` (editor_tools.py) — actores, viewport, materiales:
      get_actors_in_level, find_actors_by_name, spawn_actor, delete_actor,
      set_actor_transform, get_actor_properties, set_actor_property,
      spawn_blueprint_actor, spawn_foliage_instances, set_actor_material,
      get_actor_material, create_dynamic_material_instance,
      set_material_scalar_parameter, set_material_vector_parameter,
      duplicate_actor, get_actor_bounds, attach_actor_to_actor

    - `unreal_blueprint` (blueprint_tools.py) — clases Blueprint:
      create_blueprint, add_component_to_blueprint, set_static_mesh_properties,
      set_component_property, set_physics_properties, compile_blueprint,
      set_blueprint_property

    - `unreal_blueprint_node` (node_tools.py) — grafo de nodos de Blueprint:
      add_blueprint_event_node, add_blueprint_input_action_node,
      add_blueprint_function_node, connect_blueprint_nodes,
      add_blueprint_variable, add_blueprint_get_self_component_reference,
      add_blueprint_self_reference, find_blueprint_nodes

    - `unreal_widget` (umg_tools.py) — UMG / HUD:
      create_umg_widget_blueprint, add_text_block_to_widget,
      add_button_to_widget, bind_widget_event, add_widget_to_viewport,
      set_text_block_binding

    - `unreal_project` (project_tools.py) — configuracion de proyecto:
      create_input_mapping

    Al agregar un dominio nuevo (landscape, sequencer, niagara, audio, ai,
    data, build), seguir el mismo patron: un modulo `tools/xxx_tools.py`
    con `register_xxx_tools(mcp)`, funciones internas sin `@mcp.tool()`,
    un dict ACTIONS (closure) o `_XXX_ACTIONS` a nivel de modulo, y una unica
    funcion router decorada con `@mcp.tool()` al final. El flatten de
    `_flatten_routers()` publica cada accion como tool individual, asi que no
    hace falta registrar una tool por operacion a mano.

    ## Best Practices
    
    ### UMG Widget Development
    - Create widgets with descriptive names that reflect their purpose
    - Use consistent naming conventions for widget components
    - Organize widget hierarchy logically
    - Set appropriate anchors and alignment for responsive layouts
    - Use property bindings for dynamic updates instead of direct setting
    - Handle widget events appropriately with meaningful function names
    - Clean up widgets when no longer needed
    - Test widget layouts at different resolutions
    
    ### Editor and Actor Management
    - Use unique names for actors to avoid conflicts
    - Clean up temporary actors
    - Validate transforms before applying
    - Check actor existence before modifications
    - Take regular viewport screenshots during development
    - Keep the viewport focused on relevant actors during operations
    
    ### Blueprint Development
    - Compile Blueprints after changes
    - Use meaningful names for variables and functions
    - Organize nodes logically
    - Test functionality in isolation
    - Consider performance implications
    - Document complex setups
    
    ### Error Handling
    - Check command responses for success
    - Handle errors gracefully
    - Log important operations
    - Validate parameters
    - Clean up resources on errors
    """

# Run the server
if __name__ == "__main__":
    logger.info("Starting MCP server with stdio transport")
    mcp.run(transport='stdio') 