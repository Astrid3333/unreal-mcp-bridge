"""
Dominio `game` para unreal-mcp: control de juego (ciclo 10).

Cuatro bloques:

  * Consola:   `game_execute_console_command(command="stat fps")` ejecuta un
               comando de consola del motor y devuelve la salida capturada.
  * Sesion:    `game_play_start(mode=...)` -> `game_play_status()` ->
               `game_play_stop()`. Arranca/detiene PIE (play) o SIE
               (simulate) en el editor.
  * Input:     `game_simulate_input(key=..., action=...)` envia teclas/raton
               al PlayerController de la sesion activa (solo durante PIE).
  * Funciones: `game_list_functions(target=...)` lista las funciones
               llamables de una clase/Blueprint/actor; 
               `game_call_actor_function(actor=..., function=..., params=...)`
               las invoca por reflection y devuelve out-params + return.

Notas de uso:
  * El input simulado necesita una sesion en curso (`mode='play'`); en
    `mode='simulate'` no hay PlayerController.
  * Si hay Blueprints sin compilar, Unreal puede mostrar un dialogo al
    arrancar la sesion: el editor queda esperando respuesta hasta que
    alguien lo cierre.
  * Las funciones `BlueprintImplementableEvent` sin implementacion no hacen
    nada al llamarlas (lo indica el campo `note`).
"""
import logging
import time
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


def _send(ctx: Context, command: str, params: Dict[str, Any]) -> Any:
    from unreal_mcp_server import get_unreal_connection
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    response = unreal.send_command(command, params)
    if not response:
        return {"success": False, "message": "No response from Unreal Engine"}
    if response.get("status") == "error":
        return {"success": False,
                "message": response.get("message") or response.get("error", "unknown error")}
    return response.get("result", response)


def register_game_tools(mcp: FastMCP):
    """Registra el router `unreal_game` (tools planas `game_*`)."""

    def execute_console_command(ctx: Context, command: str) -> Dict[str, Any]:
        """Ejecuta un comando de consola de Unreal y devuelve su salida.

        Args:
            command: comando completo, ej. "stat fps", "pausable",
                "ke * FrustumTest", "ShowDebug CAMERA".
        """
        if not str(command).strip():
            return {"success": False, "message": "'command' vacio"}
        return _send(ctx, "execute_console_command", {"command": str(command)})

    def play_start(ctx: Context, mode: str = "play",
                   wait_seconds: float = 0.0) -> Dict[str, Any]:
        """Arranca una sesion en el editor: PIE (play) o simulacion (simulate).

        La sesion se encola y arranca en el siguiente tick del editor;
        consulta `game_play_status` para confirmar. Puede abrir un dialogo
        si hay Blueprints sin compilar.

        Args:
            mode: "play" (PIE, con PlayerController) o "simulate" (SIE, sin
                PlayerController).
            wait_seconds: si > 0, espera ese tiempo tras encolar y devuelve
                el estado (`playing`) al terminar.
        """
        if mode not in ("play", "simulate"):
            return {"success": False, "message": "mode debe ser 'play' o 'simulate'"}
        result = _send(ctx, "play_start", {"mode": mode})
        if isinstance(result, dict) and result.get("success") and wait_seconds:
            deadline = time.time() + float(wait_seconds)
            while time.time() < deadline:
                time.sleep(0.5)
                status = _send(ctx, "play_status", {})
                if isinstance(status, dict) and status.get("playing"):
                    result = dict(result)
                    result.update(status)
                    result.setdefault("waited", True)
                    return result
            status = _send(ctx, "play_status", {})
            if isinstance(status, dict):
                result = dict(result)
                result.update(status)
                result["waited"] = True
                result["note"] = (result.get("note") or
                                  "La sesion no indico 'playing' dentro del timeout: "
                                  "¿dialogo de Blueprints sin compilar?")
        return result

    def play_stop(ctx: Context, force: bool = False,
                  settle_timeout: float = 5.0) -> Dict[str, Any]:
        """Detiene la sesion PIE/SIE en curso (o cancela una encolada).

        Proteccion contra crash #8 del motor: si hay una peticion de juego
        encolada pero la sesion no llega a 'playing' (p. ej. un dialogo de
        errores de compilacion de Blueprint la bloquea), cancelar esa
        peticion dejo el motor en estado inconsistente y lo tumbo al
        cerrarse el dialogo (assert IsSet en StartQueuedPlaySessionRequestImpl).
        En ese caso esta herramienta espera `settle_timeout` segundos a que
        la sesion arranque o la peticion se resuelva; si sigue encolada
        devuelve error y NO cancela.

        Args:
            force: cancelar la peticion encolada aunque no haya arrancado
                (peligroso si el bloqueo es un dialogo modal).
            settle_timeout: segundos maximos a esperar el arranque antes
                de negar la cancelacion.
        """
        status = _send(ctx, "play_status", {})
        if (not force and isinstance(status, dict)
                and status.get("request_queued") and not status.get("playing")):
            deadline = time.time() + max(0.5, float(settle_timeout))
            blocked = True
            while time.time() < deadline:
                time.sleep(0.5)
                status = _send(ctx, "play_status", {})
                if not isinstance(status, dict):
                    blocked = False
                    break
                if status.get("playing"):
                    blocked = False
                    break
                if not status.get("request_queued"):
                    return {"success": True, "already_stopped": True,
                            "status": status}
            if (blocked and isinstance(status, dict)
                    and status.get("request_queued")
                    and not status.get("playing")):
                return {
                    "success": False,
                    "message": (
                        f"Peticion de juego encolada sin arrancar tras {settle_timeout}s "
                        "(dialogo de Blueprint sin compilar probable). No se cancelo "
                        "para evitar el crash del motor. Cierre el dialogo y reintente, "
                        "o use force=True para cancelar a ciegas."),
                    "status": status,
                }
        return _send(ctx, "play_stop", {})

    def play_status(ctx: Context) -> Dict[str, Any]:
        """Estado de la sesion: playing, request_queued, mundo y tipo de mundo."""
        return _send(ctx, "play_status", {})

    def simulate_input(ctx: Context, key: str, action: str = "press",
                       amount: float = 1.0, gamepad: bool = False) -> Dict[str, Any]:
        """Envia una pulsacion al PlayerController de la sesion activa (PIE).

        Args:
            key: nombre de la tecla/boton: W, SpaceBar, Escape,
                LeftMouseButton, MouseX, Gamepad_FaceButton_Bottom, ...
            action: "press", "release", "tap" (press+release),
                "repeat" o "axis" (ejes analogicos, amount en [-1,1]).
            amount: valor del eje/estado (1.0 por defecto).
            gamepad: tratar la tecla como de gamepad.
        """
        if action not in ("press", "release", "tap", "repeat", "axis"):
            return {"success": False,
                    "message": "action debe ser press|release|tap|repeat|axis"}
        return _send(ctx, "simulate_input",
                     {"key": key, "action": action, "amount": float(amount),
                      "gamepad": bool(gamepad)})

    def list_functions(ctx: Context, target: str, include_all: bool = False,
                       limit: int = 200) -> Dict[str, Any]:
        """Lista las funciones llamables de una clase, Blueprint o actor.

        Args:
            target: ruta de asset (/Game/Path/BP_Name), nombre de Blueprint,
                nombre de clase nativa o nombre de actor en escena.
            include_all: incluir tambien funciones que no son visibles en
                Blueprints (heredadas de C++).
            limit: maximo de funciones a devolver.
        """
        if not str(target).strip():
            return {"success": False, "message": "'target' vacio"}
        return _send(ctx, "list_functions",
                     {"target": target, "include_all": bool(include_all),
                      "limit": int(limit)})

    def call_actor_function(ctx: Context, actor: str, function: str,
                            params: Optional[Dict[str, Any]] = None,
                            retry: int = 1) -> Dict[str, Any]:
        """Llama una funcion de un actor por reflection y devuelve sus salidas.

        Args:
            actor: nombre del actor en escena (usa find_actors_by_name si no
                lo tienes).
            function: nombre de la funcion UFUNCTION (mira
                game_list_functions para los nombres).
            params: dict nombre -> valor de los parametros de entrada. Los
                parametros omitidos (si no son CPF_RequiredParm) toman su
                valor por defecto como en Blueprints (0/false/vacio) y se
                listan en "defaults_applied" de la respuesta.
            retry: reintentos ante errores transitorios de conexion (0-3).
        """
        if not str(actor).strip() or not str(function).strip():
            return {"success": False, "message": "'actor' y 'function' son obligatorios"}
        payload = {"actor": actor, "function": function,
                   "params": params or {}}
        result = None
        for attempt in range(max(0, int(retry)) + 1):
            result = _send(ctx, "call_actor_function", payload)
            if isinstance(result, dict) and result.get("success"):
                return result
            if isinstance(result, dict) and result.get("message"):
                msg = str(result["message"])
                if "connect" not in msg.lower() and "no response" not in msg.lower():
                    return result
            time.sleep(0.3)
        return result if isinstance(result, dict) else {"success": False,
                                                        "message": "sin respuesta"}

    ACTIONS = {
        'execute_console_command': execute_console_command,
        'play_start': play_start,
        'play_stop': play_stop,
        'play_status': play_status,
        'simulate_input': simulate_input,
        'list_functions': list_functions,
        'call_actor_function': call_actor_function,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_game",
        description=(
            "Control de juego en el editor (PIE/SIE, input simulado, consola\n"
            "    y llamadas a funciones Blueprint/C++ por reflection).\n"
            "    \n"
            "    Flujo tipico: game_list_functions(target='BP_X') ->\n"
            "    game_call_actor_function(actor=..., function=..., params=...) ->\n"
            "    game_play_start(mode='play') -> game_simulate_input(key='W') ->\n"
            "    game_play_stop(). game_execute_console_command corre sin sesion.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_game(ctx: Context, action: str,
                    params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_game action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_game '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Game tools (router) registered: {len(ACTIONS)} acciones")
