"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""
import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
logger = logging.getLogger('UnrealMCP')

def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""

    def _get_actors_in_level(ctx: Context) -> List[Dict[str, Any]]:
        """Get a list of all actors in the current level."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning('Failed to connect to Unreal Engine')
                return []
            response = unreal.send_command('get_actors_in_level', {})
            if not response:
                logger.warning('No response from Unreal Engine')
                return []
            logger.info(f'Complete response from Unreal: {response}')
            if 'result' in response and 'actors' in response['result']:
                actors = response['result']['actors']
                logger.info(f'Found {len(actors)} actors in level')
                return actors
            elif 'actors' in response:
                actors = response['actors']
                logger.info(f'Found {len(actors)} actors in level')
                return actors
            logger.warning(f'Unexpected response format: {response}')
            return []
        except Exception as e:
            logger.error(f'Error getting actors: {e}')
            return []

    def _find_actors_by_name(ctx: Context, pattern: str) -> List[str]:
        """Find actors by name pattern."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning('Failed to connect to Unreal Engine')
                return []
            response = unreal.send_command('find_actors_by_name', {'pattern': pattern})
            if not response:
                return []
            return response.get('actors', [])
        except Exception as e:
            logger.error(f'Error finding actors: {e}')
            return []

    def _spawn_actor(ctx: Context, name: str, type: str, location: List[float]=[0.0, 0.0, 0.0], rotation: List[float]=[0.0, 0.0, 0.0]) -> Dict[str, Any]:
        """Create a new actor in the current level.
        
        Args:
            ctx: The MCP context
            name: The name to give the new actor (must be unique)
            type: The type of actor to create (e.g. StaticMeshActor, PointLight)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the created actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'name': name, 'type': type.upper(), 'location': location, 'rotation': rotation}
            for param_name in ['location', 'rotation']:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f'Invalid {param_name} format: {param_value}. Must be a list of 3 float values.')
                    return {'success': False, 'message': f'Invalid {param_name} format. Must be a list of 3 float values.'}
                params[param_name] = [float(val) for val in param_value]
            logger.info(f"Creating actor '{name}' of type '{type}' with params: {params}")
            response = unreal.send_command('spawn_actor', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Actor creation response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error creating actor: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error creating actor: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            response = unreal.send_command('delete_actor', {'name': name})
            return response or {}
        except Exception as e:
            logger.error(f'Error deleting actor: {e}')
            return {}

    def _set_actor_transform(ctx: Context, name: str, location: List[float]=None, rotation: List[float]=None, scale: List[float]=None) -> Dict[str, Any]:
        """Set the transform of an actor."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'name': name}
            if location is not None:
                params['location'] = location
            if rotation is not None:
                params['rotation'] = rotation
            if scale is not None:
                params['scale'] = scale
            response = unreal.send_command('set_actor_transform', params)
            return response or {}
        except Exception as e:
            logger.error(f'Error setting transform: {e}')
            return {}

    def _get_actor_properties(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all properties of an actor."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            response = unreal.send_command('get_actor_properties', {'name': name})
            return response or {}
        except Exception as e:
            logger.error(f'Error getting properties: {e}')
            return {}

    def _set_actor_property(ctx: Context, name: str, property_name: str, property_value) -> Dict[str, Any]:
        """
        Set a property on an actor.
        
        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Dict containing response from Unreal with operation status
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            response = unreal.send_command('set_actor_property', {'name': name, 'property_name': property_name, 'property_value': property_value})
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Set actor property response: {response}')
            return response
        except Exception as e:
            error_msg = f'Error setting actor property: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def focus_viewport(ctx: Context, target: str=None, location: List[float]=None, distance: float=1000.0, orientation: List[float]=None) -> Dict[str, Any]:
        """
        Focus the viewport on a specific actor or location.
        
        Args:
            target: Name of the actor to focus on (if provided, location is ignored)
            location: [X, Y, Z] coordinates to focus on (used if target is None)
            distance: Distance from the target/location
            orientation: Optional [Pitch, Yaw, Roll] for the viewport camera
            
        Returns:
            Response from Unreal Engine
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {}
            if target:
                params['target'] = target
            elif location:
                params['location'] = location
            if distance:
                params['distance'] = distance
            if orientation:
                params['orientation'] = orientation
            response = unreal.send_command('focus_viewport', params)
            return response or {}
        except Exception as e:
            logger.error(f'Error focusing viewport: {e}')
            return {'status': 'error', 'message': str(e)}

    def _spawn_blueprint_actor(ctx: Context, blueprint_name: str, actor_name: str, location: List[float]=[0.0, 0.0, 0.0], rotation: List[float]=[0.0, 0.0, 0.0]) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.
        
        Args:
            ctx: The MCP context
            blueprint_name: Name of the Blueprint to spawn from
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the spawned actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'blueprint_name': blueprint_name, 'actor_name': actor_name, 'location': location or [0.0, 0.0, 0.0], 'rotation': rotation or [0.0, 0.0, 0.0]}
            for param_name in ['location', 'rotation']:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f'Invalid {param_name} format: {param_value}. Must be a list of 3 float values.')
                    return {'success': False, 'message': f'Invalid {param_name} format. Must be a list of 3 float values.'}
                params[param_name] = [float(val) for val in param_value]
            logger.info(f'Spawning blueprint actor with params: {params}')
            response = unreal.send_command('spawn_blueprint_actor', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Spawn blueprint actor response: {response}')
            return response
        except Exception as e:
            error_msg = f'Error spawning blueprint actor: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _spawn_foliage_instances(ctx: Context, mesh_path: str, landscape_name: str, count: int=100, region_center: List[float]=[0.0, 0.0], region_radius: float=5000.0, min_scale: float=0.8, max_scale: float=1.5) -> Dict[str, Any]:
        """Scatter instanced foliage of a static mesh across a region of a landscape.
        
        Args:
            ctx: The MCP context
            mesh_path: Path to the static mesh asset (e.g. "/Engine/BasicShapes/Cone.Cone")
            landscape_name: Name of the target landscape actor
            count: Number of instances to attempt to spawn
            region_center: The [x, y] world center of the scatter region
            region_radius: Radius in world units of the scatter region
            min_scale: Minimum random uniform scale per instance
            max_scale: Maximum random uniform scale per instance
            
        Returns:
            Dict containing spawn results (requested_count, spawned_count, success)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            if not isinstance(region_center, list) or len(region_center) != 2:
                logger.error(f'Invalid region_center format: {region_center}. Must be a list of 2 float values.')
                return {'success': False, 'message': 'Invalid region_center format. Must be a list of 2 float values.'}
            params = {'mesh_path': mesh_path, 'landscape_name': landscape_name, 'count': int(count), 'region_center': [float(v) for v in region_center], 'region_radius': float(region_radius), 'min_scale': float(min_scale), 'max_scale': float(max_scale)}
            logger.info(f"Spawning foliage '{mesh_path}' on landscape '{landscape_name}' with params: {params}")
            response = unreal.send_command('spawn_foliage_instances', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Foliage spawn response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error spawning foliage: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error spawning foliage: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _set_actor_material(ctx: Context, actor_name: str, material_path: str, slot_index: int=0) -> Dict[str, Any]:
        """Assign a material to a StaticMeshActor.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the target actor
            material_path: Path to the material asset (e.g. "/Game/Materials/M_Bark")
            slot_index: Material slot index to override (default 0)
            
        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'material_path': material_path, 'slot_index': int(slot_index)}
            logger.info(f"Setting material on '{actor_name}': {params}")
            response = unreal.send_command('set_actor_material', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Set material response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error setting material: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error setting material: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _get_actor_material(ctx: Context, actor_name: str, slot_index: int=0) -> Dict[str, Any]:
        """Get the material currently assigned to a StaticMeshActor's slot.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the target actor
            slot_index: Material slot index to inspect (default 0)
            
        Returns:
            Dict containing material_path, is_dynamic_instance, success
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'slot_index': int(slot_index)}
            logger.info(f"Getting material on '{actor_name}': {params}")
            response = unreal.send_command('get_actor_material', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Get material response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error getting material: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error getting material: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _create_dynamic_material_instance(ctx: Context, actor_name: str, slot_index: int=0) -> Dict[str, Any]:
        """Create a UMaterialInstanceDynamic on a StaticMeshActor's slot so its parameters can be changed at runtime.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the target actor
            slot_index: Material slot index to convert (default 0)
            
        Returns:
            Dict containing dynamic_instance_name, success
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'slot_index': int(slot_index)}
            logger.info(f"Creating dynamic material instance on '{actor_name}': {params}")
            response = unreal.send_command('create_dynamic_material_instance', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Create dynamic material instance response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error creating dynamic material instance: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error creating dynamic material instance: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _set_material_scalar_parameter(ctx: Context, actor_name: str, parameter_name: str, value: float, slot_index: int=0) -> Dict[str, Any]:
        """Set a scalar (float) parameter on a Material Instance, e.g. roughness, metallic, emissive strength.
        
        Automatically promotes the slot to a dynamic material instance if it isn't one already.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the target actor
            parameter_name: Name of the scalar parameter as defined in the material
            value: The float value to set
            slot_index: Material slot index to target (default 0)
            
        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'parameter_name': parameter_name, 'value': float(value), 'slot_index': int(slot_index)}
            logger.info(f"Setting scalar parameter on '{actor_name}': {params}")
            response = unreal.send_command('set_material_scalar_parameter', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Set scalar parameter response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error setting scalar parameter: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error setting scalar parameter: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _set_material_vector_parameter(ctx: Context, actor_name: str, parameter_name: str, r: float, g: float, b: float, a: float=1.0, slot_index: int=0) -> Dict[str, Any]:
        """Set a vector/color parameter on a Material Instance, e.g. base color tint, emissive color.
        
        Automatically promotes the slot to a dynamic material instance if it isn't one already.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the target actor
            parameter_name: Name of the vector parameter as defined in the material
            r: Red channel (0.0-1.0, values above 1.0 allowed for HDR/emissive)
            g: Green channel
            b: Blue channel
            a: Alpha channel (default 1.0)
            slot_index: Material slot index to target (default 0)
            
        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'parameter_name': parameter_name, 'value': {'r': float(r), 'g': float(g), 'b': float(b), 'a': float(a)}, 'slot_index': int(slot_index)}
            logger.info(f"Setting vector parameter on '{actor_name}': {params}")
            response = unreal.send_command('set_material_vector_parameter', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Set vector parameter response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error setting vector parameter: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error setting vector parameter: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _duplicate_actor(ctx: Context, actor_name: str, new_name: str='', location_offset: List[float]=[0.0, 0.0, 0.0]) -> Dict[str, Any]:
        """Clone an existing actor, copying its mesh, materials and other properties.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the source actor to duplicate
            new_name: Label for the new actor (defaults to '<actor_name>_Copy' if empty)
            location_offset: [x, y, z] offset applied to the new actor relative to the source
            
        Returns:
            Dict containing source_actor, new_actor, success
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            if not isinstance(location_offset, list) or len(location_offset) != 3:
                logger.error(f'Invalid location_offset format: {location_offset}. Must be a list of 3 float values.')
                return {'success': False, 'message': 'Invalid location_offset format. Must be a list of 3 float values.'}
            params = {'actor_name': actor_name, 'new_name': new_name, 'location_offset': [float(v) for v in location_offset]}
            logger.info(f"Duplicating actor '{actor_name}': {params}")
            response = unreal.send_command('duplicate_actor', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Duplicate actor response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error duplicating actor: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error duplicating actor: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _get_actor_bounds(ctx: Context, actor_name: str) -> Dict[str, Any]:
        """Get the world-space bounding box of an actor (origin and extent).
        
        Args:
            ctx: The MCP context
            actor_name: Name of the target actor
            
        Returns:
            Dict containing origin {x,y,z}, box_extent {x,y,z}, min_z, max_z, success
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name}
            logger.info(f"Getting bounds for '{actor_name}'")
            response = unreal.send_command('get_actor_bounds', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Get actor bounds response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error getting actor bounds: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error getting actor bounds: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _attach_actor_to_actor(ctx: Context, actor_name: str, parent_actor_name: str, socket_name: str='', attachment_rule: str='KeepRelative') -> Dict[str, Any]:
        """Attach (parent) one actor to another, optionally at a specific socket.
        
        Args:
            ctx: The MCP context
            actor_name: Name of the actor to attach (the child)
            parent_actor_name: Name of the actor to attach to (the parent)
            socket_name: Optional socket name on the parent's mesh to attach to
            attachment_rule: One of 'KeepRelative', 'KeepWorld', 'SnapToTarget' (default 'KeepRelative')
            
        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            valid_rules = ['KeepRelative', 'KeepWorld', 'SnapToTarget']
            if attachment_rule not in valid_rules:
                logger.error(f'Invalid attachment_rule: {attachment_rule}. Must be one of {valid_rules}.')
                return {'success': False, 'message': f'Invalid attachment_rule. Must be one of {valid_rules}.'}
            params = {'actor_name': actor_name, 'parent_actor_name': parent_actor_name, 'socket_name': socket_name, 'attachment_rule': attachment_rule}
            logger.info(f"Attaching '{actor_name}' to '{parent_actor_name}': {params}")
            response = unreal.send_command('attach_actor_to_actor', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Attach actor response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error attaching actor: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error attaching actor: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _take_screenshot(ctx: Context, filepath: str) -> Dict[str, Any]:
        """Capture a screenshot of the active editor viewport and save it to disk.

        Note: the C++ handler (HandleTakeScreenshot) only accepts 'filepath' — it
        appends '.png' automatically if missing. There is no show_ui or resolution
        parameter on the engine side despite what older docs may say.

        Args:
            ctx: The MCP context
            filepath: Path (absolute or engine-relative) to save the .png to

        Returns:
            Dict containing filepath, success
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'filepath': filepath}
            logger.info(f'Taking screenshot: {params}')
            response = unreal.send_command('take_screenshot', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Take screenshot response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error taking screenshot: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error taking screenshot: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}

    def _create_material(ctx: Context, name: str, path: str='/Game/Materials', base_color: List[float]=None, roughness: float=0.5, metallic: float=0.0, assign_to_actor: str='', slot_index: int=0) -> Dict[str, Any]:
        """Create a new Material asset with editable BaseColor/Roughness/Metallic scalar+vector
        parameters, save it to disk, and optionally assign it to an actor's mesh slot.

        Args:
            ctx: The MCP context
            name: Name for the new material asset
            path: Content-browser folder to create it in (default '/Game/Materials')
            base_color: Optional [r, g, b] default base color (0.0-1.0 each, default mid-grey)
            roughness: Default roughness value (default 0.5)
            metallic: Default metallic value (default 0.0)
            assign_to_actor: Optional actor name to assign the new material to immediately
            slot_index: Material slot index to use if assign_to_actor is given (default 0)

        Returns:
            Dict containing material_path, saved_to_disk, assigned_to_actor, success
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error('Failed to connect to Unreal Engine')
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'name': name, 'path': path, 'roughness': float(roughness), 'metallic': float(metallic)}
            if base_color is not None:
                if not isinstance(base_color, list) or len(base_color) < 3:
                    logger.error(f'Invalid base_color format: {base_color}. Must be a list of at least 3 float values.')
                    return {'success': False, 'message': 'Invalid base_color format. Must be a list of at least 3 float values.'}
                params['base_color'] = [float(v) for v in base_color]
            if assign_to_actor:
                params['assign_to_actor'] = assign_to_actor
                params['slot_index'] = int(slot_index)
            logger.info(f"Creating material '{name}': {params}")
            response = unreal.send_command('create_material', params)
            if not response:
                logger.error('No response from Unreal Engine')
                return {'success': False, 'message': 'No response from Unreal Engine'}
            logger.info(f'Create material response: {response}')
            if response.get('status') == 'error':
                error_message = response.get('error', 'Unknown error')
                logger.error(f'Error creating material: {error_message}')
                return {'success': False, 'message': error_message}
            return response
        except Exception as e:
            error_msg = f'Error creating material: {e}'
            logger.error(error_msg)
            return {'success': False, 'message': error_msg}
    logger.info('Editor tools registered successfully')
    def _get_material_properties(ctx: Context, material_path: str) -> Dict[str, Any]:
        """Read blend mode, shading model, and whether a material is a Material Instance."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path}
            response = unreal.send_command('get_material_properties', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error getting material properties: {e}'}

    def _set_material_blend_mode(ctx: Context, material_path: str, blend_mode: str) -> Dict[str, Any]:
        """Set blend mode on a base UMaterial and save it to disk."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'material_path': material_path, 'blend_mode': blend_mode}
            response = unreal.send_command('set_material_blend_mode', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting material blend mode: {e}'}

    def _create_moss_stone_material(ctx: Context, name: str, path: str='/Game/Materials', stone_color: List[float]=None, moss_color: List[float]=None, roughness: float=0.8, moss_amount: float=0.4, noise_scale: float=20.0, assign_to_actor: str='', slot_index: int=0) -> Dict[str, Any]:
        """Create a procedural stone+moss Material (noise-driven, no external textures) and save it to disk."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'name': name, 'path': path, 'roughness': roughness, 'moss_amount': moss_amount, 'noise_scale': noise_scale}
            if stone_color is not None:
                params['stone_color'] = stone_color
            if moss_color is not None:
                params['moss_color'] = moss_color
            if assign_to_actor:
                params['assign_to_actor'] = assign_to_actor
                params['slot_index'] = slot_index
            response = unreal.send_command('create_moss_stone_material', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error creating moss stone material: {e}'}

    def _import_texture(ctx: Context, source_path: str, name: str = '', path: str = '/Game/Textures', srgb: bool = True) -> Dict[str, Any]:
        """Import a local image file (png/jpg/tga/exr) as a UTexture2D."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'source_path': source_path, 'path': path, 'srgb': srgb}
            if name:
                params['name'] = name
            response = unreal.send_command('import_texture', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error importing texture: {e}'}

    def _create_pbr_material(ctx: Context, name: str, path: str = '/Game/Materials',
                              base_color_texture: str = '', normal_texture: str = '',
                              roughness_texture: str = '', metallic_texture: str = '',
                              ao_texture: str = '', assign_to_actor: str = '', slot_index: int = 0) -> Dict[str, Any]:
        """Build a PBR material wiring up to 5 already-imported textures to their correct pins."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {
                'name': name, 'path': path,
                'base_color_texture': base_color_texture, 'normal_texture': normal_texture,
                'roughness_texture': roughness_texture, 'metallic_texture': metallic_texture,
                'ao_texture': ao_texture, 'assign_to_actor': assign_to_actor, 'slot_index': slot_index,
            }
            response = unreal.send_command('create_pbr_material', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error creating PBR material: {e}'}

    ACTIONS = {'get_actors_in_level': _get_actors_in_level, 'find_actors_by_name': _find_actors_by_name, 'spawn_actor': _spawn_actor, 'delete_actor': _delete_actor, 'set_actor_transform': _set_actor_transform, 'get_actor_properties': _get_actor_properties, 'set_actor_property': _set_actor_property, 'focus_viewport': focus_viewport, 'take_screenshot': _take_screenshot, 'spawn_blueprint_actor': _spawn_blueprint_actor, 'spawn_foliage_instances': _spawn_foliage_instances, 'set_actor_material': _set_actor_material, 'get_actor_material': _get_actor_material, 'create_dynamic_material_instance': _create_dynamic_material_instance, 'set_material_scalar_parameter': _set_material_scalar_parameter, 'set_material_vector_parameter': _set_material_vector_parameter, 'create_material': _create_material, 'duplicate_actor': _duplicate_actor, 'get_actor_bounds': _get_actor_bounds, 'attach_actor_to_actor': _attach_actor_to_actor, 'get_material_properties': _get_material_properties, 'set_material_blend_mode': _set_material_blend_mode, 'create_moss_stone_material': _create_moss_stone_material, 'import_texture': _import_texture, 'create_pbr_material': _create_pbr_material}

    @mcp.tool()
    def unreal_actor(ctx: Context, action: str, params: Dict[str, Any]={}) -> Any:
        """Router para operaciones de dominio 'unreal_actor' en Unreal Engine.
    
    Parametros:
      action: nombre de la operacion (ver lista abajo)
      params: dict con los argumentos de esa operacion
    
    Operaciones disponibles:
      - get_actors_in_level((sin params)): Get a list of all actors in the current level.
      - find_actors_by_name(pattern): Find actors by name pattern.
      - spawn_actor(name, type, location, rotation): Create a new actor in the current level.
      - delete_actor(name): Delete an actor by name.
      - set_actor_transform(name, location, rotation, scale): Set the transform of an actor.
      - get_actor_properties(name): Get all properties of an actor.
      - set_actor_property(name, property_name, property_value): Set a property on an actor.
      - focus_viewport(target, location, distance, orientation): Focus the editor viewport on an actor or location.
      - take_screenshot(filepath): Capture a screenshot of the active viewport and save it as .png.
      - spawn_blueprint_actor(blueprint_name, actor_name, location, rotation): Spawn an actor from a Blueprint.
      - spawn_foliage_instances(mesh_path, landscape_name, count, region_center, region_radius, min_scale, max_scale): Scatter instanced foliage of a static mesh across a region of a landscape.
      - set_actor_material(actor_name, material_path, slot_index): Assign a material to a StaticMeshActor.
      - get_actor_material(actor_name, slot_index): Get the material currently assigned to a StaticMeshActor's slot.
      - create_dynamic_material_instance(actor_name, slot_index): Create a UMaterialInstanceDynamic on a StaticMeshActor's slot so its parameters can be changed at runtime.
      - set_material_scalar_parameter(actor_name, parameter_name, value, slot_index): Set a scalar (float) parameter on a Material Instance, e.g. roughness, metallic, emissive strength.
      - set_material_vector_parameter(actor_name, parameter_name, r, g, b, a, slot_index): Set a vector/color parameter on a Material Instance, e.g. base color tint, emissive color.
      - create_material(name, path, base_color, roughness, metallic, assign_to_actor, slot_index): Create a new Material asset with editable parameters, save it to disk, and optionally assign it to an actor.
      - get_material_properties(material_path): Read blend mode, shading model, and whether it's a Material Instance.
      - set_material_blend_mode(material_path, blend_mode): Set blend mode ('opaque'|'masked'|'translucent'|'additive'|'modulate'|'alphacomposite') on a base UMaterial and save to disk.
      - create_moss_stone_material(name, path, stone_color, moss_color, roughness, moss_amount, noise_scale, assign_to_actor, slot_index): Create a procedural stone+moss Material (noise-driven, no textures) and save it to disk.
      - duplicate_actor(actor_name, new_name, location_offset): Clone an existing actor, copying its mesh, materials and other properties.
      - get_actor_bounds(actor_name): Get the world-space bounding box of an actor (origin and extent).
      - attach_actor_to_actor(actor_name, parent_actor_name, socket_name, attachment_rule): Attach (parent) one actor to another, optionally at a specific socket.
    """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Accion desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Parametros invalidos para accion '{action}': {e}"}
