# Project Caesar - Asistente IA en simulacion de coche (Unreal + MCP)

Referencia sobre Project Caesar (repo: ataberkuygar/Project-Caesar), un proyecto de simulacion en Unreal Engine que demuestra un agente de IA (LLM) controlando un entorno 3D y sistemas externos via Model Context Protocol (MCP).

Nota: "Project Caesar" tambien es el nombre en clave de un proximo juego de Paradox Interactive (posible Europa Universalis V) - sin relacion con este proyecto.

## Objetivo del proyecto

Demostrar como un agente de IA puede controlar un entorno 3D (un coche conduciendo por un paisaje simulado) y varios sistemas externos (clima, musica, GPS) a traves de comandos en lenguaje natural, usando MCP como puente de comunicacion.

## Componentes clave (arquitectura del agente)

- **agent_core**: bucle principal del agente de IA; gestiona interaccion con el LLM, memoria y flujo de conversacion.
- **intent_router**: clasifica la entrada del usuario para determinar que herramienta/accion ejecutar.
- **tool_dispatcher**: traduce la intencion del usuario a una llamada concreta a un servidor MCP.
- **dialogue_handler**: gestiona la conversacion y la sintesis de voz (TTS).
- **state_tracker**: mantiene el estado actual del entorno de simulacion (ej. "la musica esta sonando", "la temperatura es de 23 grados").

## Diseno de servidores MCP (multiples servidores especializados)

| Servidor MCP | Funcion | Ejemplo de herramientas |
|---|---|---|
| mcp-sim-actions | Controla componentes del coche simulado | set_temperature, set_music, open_window, adjust_seat |
| mcp-sim-session | Gestiona el ciclo de vida de la simulacion | start_simulation, pause_simulation, reset_simulation, log_event |
| mcp-conversation | Maneja interaccion y dialogo con el usuario | talk, remember, ask_confirm, summarize |
| mcp-external | Interfaz con APIs y datos externos | get_weather, get_location, play_spotify, reroute |

Cada servidor expone su conjunto de herramientas via un esquema REST.

## Entorno de simulacion

Ambientado en Yalova rural (Turquia): colinas verdes, caminos de tierra, casas de pueblo. Un coche conduce por el terreno; el agente de IA activa eventos en respuesta a indicaciones del usuario (ej. "pon musica cuando lleguemos al bosque").

## Relacion con unreal-mcp-bridge

Comparten la misma filosofia (MCP para que una IA controle Unreal Engine), pero con enfoques distintos:

- **unreal-mcp-bridge**: herramienta de proposito general - amplio abanico de comandos para construir y editar escenas en Unreal desde cero.
- **Project Caesar**: aplicacion de simulacion especifica - no construye el mundo, sino que controla una simulacion predefinida (coche + paisaje) y orquesta multiples sistemas (clima, musica, GPS) via agente de IA.

Project Caesar funciona como caso de uso de referencia para un puente como unreal-mcp-bridge: multiples servidores MCP especializados por dominio, en vez de un unico servidor monolitico, y separacion clara entre control de simulacion (sim-actions/sim-session) y orquestacion conversacional (conversation/external).
