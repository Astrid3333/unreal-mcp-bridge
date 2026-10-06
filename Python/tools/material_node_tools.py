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
          ScalarParameter, VectorParameter. Usa list_available_expression_types
          para verificar en vivo la lista completa contra el bridge real.
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

    def _list_available_expression_types(ctx: Context) -> Dict[str, Any]:
        """Lista en vivo los expression_type soportados por add_material_expression.

        Introspeccion directa contra el mapa EXPRESSION_TYPES del bridge (el
        mismo que usa CreateExpressionByType/ResolveExpressionClass) — no es
        una lista hardcodeada del lado Python, asi que sirve para verificar
        el vocabulario documentado en material_node_tools.md contra el
        estado real del plugin compilado. No incluye la tabla de target_pin
        por tipo (esa sigue viviendo solo en el codigo de SetExpressionInputPin
        y en el doc); esto solo confirma que tipos existen.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('list_available_expression_types', {}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error listing available expression types: {e}'}

    def _create_empty_material(ctx: Context, name: str, path: str = '/Game/Materials') -> Dict[str, Any]:
        """Crea un material vacio (sin nodos) y lo guarda a disco.

        Es el material base para alimentar add_material_expression. Si el
        material ya existe lo vacia en sitio (conserva las referencias de
        actores que ya lo tengan asignado). Devuelve material_path, que es
        lo que se pasa al resto de los comandos de este dominio.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('create_empty_material', {'name': name, 'path': path}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error creating empty material: {e}'}

    def _delete_material_expression(ctx: Context, material_path: str, node_id: str) -> Dict[str, Any]:
        """Borra un nodo del grafo, anula todas sus referencias y guarda a disco.

        El node_id queda libre para reusarse en una proxima creacion.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('delete_material_expression',
                                      {'material_path': material_path, 'node_id': node_id}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error deleting material expression: {e}'}

    def _set_material_expression_property(ctx: Context, material_path: str, node_id: str,
                                          property: str, value: Any) -> Dict[str, Any]:
        """Setea cualquier propiedad escalar/reflejada de un nodo y guarda a disco.

        property: nombre real de la UPROPERTY en C++ (ej. R, G, B, ConstA,
          Texture, SamplerType, bClamp). Usa get_material_expression para
          ver que propiedades expone un nodo y sus valores actuales.
        value: numero, string, bool o [r,g,b,a] segun el tipo de propiedad.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'node_id': node_id,
                      'property': property, 'value': value}
            return unreal.send_command('set_material_expression_property', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting material expression property: {e}'}

    def _get_material_expression(ctx: Context, material_path: str, node_id: str) -> Dict[str, Any]:
        """Lee un nodo: clase, expression_type, posicion, pines de entrada
        (con su conexion), outputs disponibles y dump de propiedades escalares.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('get_material_expression',
                                      {'material_path': material_path, 'node_id': node_id}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error getting material expression: {e}'}

    def _disconnect_material_input(ctx: Context, material_path: str, node_id: str,
                                   target_pin: str) -> Dict[str, Any]:
        """Desconecta el pin de entrada target_pin de un nodo (lo deja sin fuente)
        y guarda a disco. Si el pin no existe, el error lista los pines disponibles.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'node_id': node_id, 'target_pin': target_pin}
            return unreal.send_command('disconnect_material_input', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error disconnecting material input: {e}'}

    def _save_material(ctx: Context, material_path: str) -> Dict[str, Any]:
        """Guarda el .uasset del material a disco (solo materiales de /Game/)."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('save_material', {'material_path': material_path}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error saving material: {e}'}

    ACTIONS = {
        'add_material_expression': _add_material_expression,
        'connect_material_expressions': _connect_material_expressions,
        'set_material_output': _set_material_output,
        'list_material_expressions': _list_material_expressions,
        'set_material_expression_constant': _set_material_expression_constant,
        'list_available_expression_types': _list_available_expression_types,
        'create_empty_material': _create_empty_material,
        'delete_material_expression': _delete_material_expression,
        'set_material_expression_property': _set_material_expression_property,
        'get_material_expression': _get_material_expression,
        'disconnect_material_input': _disconnect_material_input,
        'save_material': _save_material,
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
          - list_available_expression_types(): introspeccion en vivo de expression_type soportados
          - create_empty_material(name, path): material vacio base para el grafo
          - delete_material_expression(material_path, node_id)
          - set_material_expression_property(material_path, node_id, property, value)
          - get_material_expression(material_path, node_id): pines + propiedades
          - disconnect_material_input(material_path, node_id, target_pin)
          - save_material(material_path)
        """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Acción desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Parámetros inválidos para acción '{action}': {e}"}

    logger.info('Material node tools registered successfully')
