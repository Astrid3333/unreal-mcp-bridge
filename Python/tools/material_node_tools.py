"""Material Node Tools for Unreal MCP - grafo de material generico."""
import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


def register_material_node_tools(mcp: FastMCP):

    def _add_material_expression(ctx: Context, material_path: str, expression_type: str, node_id: str,
                                  position_x: float = 0.0, position_y: float = 0.0) -> Dict[str, Any]:
        """Agrega un nodo (UMaterialExpression) a un material existente.

        expression_type: Add, Subtract, Multiply, Divide, LinearInterpolate, Clamp,
          Power, OneMinus, Desaturation, DotProduct, Normalize, ComponentMask,
          AppendVector, Distance, Fresnel, Panner, Time, TextureCoordinate,
          TextureSample, TransformPosition, WorldPosition, VertexNormalWS, Noise,
          Constant, Constant2Vector, Constant3Vector, Constant4Vector,
          ScalarParameter, VectorParameter.
        node_id: nombre unico que vos elegis para referenciar este nodo despues
          en connect_material_expressions / set_material_output.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'expression_type': expression_type, 'node_id': node_id,
                      'position_x': position_x, 'position_y': position_y}
            return unreal.send_command('add_material_expression', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error adding material expression: {e}'}

    def _connect_material_expressions(ctx: Context, material_path: str, source_node_id: str, target_node_id: str,
                                       target_pin: str, source_output_index: int = 0) -> Dict[str, Any]:
        """Conecta el output de source_node_id al pin target_pin de target_node_id.

        target_pin depende de la clase del nodo destino (ej: 'A'/'B' para Multiply,
        'A'/'B'/'Alpha' para LinearInterpolate, 'Input' para Clamp/OneMinus,
        'Position' para Noise, 'Coordinates' para TextureSample).
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'source_node_id': source_node_id,
                      'target_node_id': target_node_id, 'target_pin': target_pin,
                      'source_output_index': source_output_index}
            return unreal.send_command('connect_material_expressions', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error connecting material expressions: {e}'}

    def _set_material_output(ctx: Context, material_path: str, output_pin: str, node_id: str,
                              output_index: int = 0) -> Dict[str, Any]:
        """Conecta un nodo al output final del material y guarda a disco.

        output_pin: BaseColor, Metallic, Specular, Roughness, EmissiveColor,
          Opacity, OpacityMask, Normal, WorldPositionOffset, AmbientOcclusion.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'output_pin': output_pin, 'node_id': node_id,
                      'output_index': output_index}
            return unreal.send_command('set_material_output', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting material output: {e}'}

    def _list_material_expressions(ctx: Context, material_path: str) -> Dict[str, Any]:
        """Lista los nodos (node_id + clase) que ya existen en un material."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('list_material_expressions', {'material_path': material_path}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error listing material expressions: {e}'}

    def _set_material_expression_constant(ctx: Context, material_path: str, node_id: str,
                                           value: List[float]) -> Dict[str, Any]:
        """Setea el valor default de un nodo Constant/Constant2-4Vector/ScalarParameter/VectorParameter.

        value: [x] para escalar, [r,g,b] o [r,g,b,a] para vector.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'node_id': node_id, 'value': value}
            return unreal.send_command('set_material_expression_constant', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting material expression constant: {e}'}

    ACTIONS = {
        'add_material_expression': _add_material_expression,
        'connect_material_expressions': _connect_material_expressions,
        'set_material_output': _set_material_output,
        'list_material_expressions': _list_material_expressions,
        'set_material_expression_constant': _set_material_expression_constant,
    }

    @mcp.tool()
    def unreal_material_node(ctx: Context, action: str, params: Dict[str, Any] = {}) -> Any:
        """Router para operaciones de dominio 'unreal_material_node' en Unreal Engine
        (grafo de material generico: agregar nodos, conectarlos, setear outputs).

        Operaciones disponibles:
          - add_material_expression(material_path, expression_type, node_id, position_x, position_y)
          - connect_material_expressions(material_path, source_node_id, target_node_id, target_pin, source_output_index)
          - set_material_output(material_path, output_pin, node_id, output_index)
          - list_material_expressions(material_path)
          - set_material_expression_constant(material_path, node_id, value)
        """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Acción desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Parámetros inválidos para acción '{action}': {e}"}

    logger.info('Material node tools registered successfully')
