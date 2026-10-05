"""
Simulaciones cientificas (dominio `sim`) para unreal-mcp.

Dividido en tres partes:

1. Nucleo numerico puro (sin numpy): integrador RK4 y registro de modelos.
   Corre en el proceso del servidor MCP, no necesita Unreal abierto.
2. Gestion de simulaciones con estado: create / step / run / get_state /
   set_params / reset / delete / list. Cada simulacion vive hasta que se
   borra o se reinicia el servidor.
3. Visualizacion en Unreal: `to_unreal` spawnea actores (luces) a lo largo de
   la trayectoria o como puntos vivos del estado actual, `update_unreal` los
   mueve y `clear_unreal` los borra.

Ademas `calc` resuelve formulas cerradas (escape velocity, Arrhenius,
Michaelis-Menten, Hohmann, Tsiolkovsky...) sin crear simulacion.

Todas las magnitudes en SI (m, s, kg, mol, K) salvo que el modelo diga otra
cosa; la visualizacion aplica `scale` (por defecto 1.0 => la unidad sim se
trata como cm de Unreal) y `origen`.
"""
import logging
import math
import time
import uuid
from typing import Any, Callable, Dict, List, Optional, Sequence

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')

R_GAS = 8.314462618
G_NEWTON = 6.67430e-11
M_SUN = 1.98892e30
M_EARTH = 5.9722e24
R_EARTH = 6.371e6

MAX_SIMS = 32
MAX_HISTORY = 2000
MAX_STEPS_PER_RUN = 200000

# ---------------------------------------------------------------------------
# Nucleo numerico
# ---------------------------------------------------------------------------


def _rk4(f: Callable[[float, List[float]], List[float]], t: float,
         y: List[float], dt: float) -> List[float]:
    n = len(y)
    k1 = f(t, y)
    k2 = f(t + dt / 2.0, [y[i] + dt / 2.0 * k1[i] for i in range(n)])
    k3 = f(t + dt / 2.0, [y[i] + dt / 2.0 * k2[i] for i in range(n)])
    k4 = f(t + dt, [y[i] + dt * k3[i] for i in range(n)])
    return [y[i] + dt / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]) for i in range(n)]


def _finite(values: Sequence[float]) -> bool:
    return all(math.isfinite(v) for v in values)


MODELS: Dict[str, Dict[str, Any]] = {}


def _register(name: str, domain: str, description: str, labels: List[str],
              params: Dict[str, Any],
              y0: Callable[[Dict[str, Any]], List[float]],
              derivs: Callable[[float, List[float], Dict[str, Any]], List[float]],
              dt_default: float = 0.01,
              analytic: Optional[Callable[[float, List[float], Dict[str, Any]], List[float]]] = None,
              spatial: Optional[Callable[[List[float], Dict[str, Any], float], List[List[float]]]] = None,
              layout: Optional[Callable[[List[float], Dict[str, Any]], Dict[str, Any]]] = None,
              state_hint: str = "") -> None:
    MODELS[name] = {
        "name": name, "domain": domain, "description": description,
        "labels": labels, "params": params, "y0": y0, "derivs": derivs,
        "dt_default": dt_default, "analytic": analytic, "spatial": spatial,
        "layout": layout, "state_hint": state_hint,
    }


# ------------------------------------------------------------------ fisica
def _p_projectile(p: Dict[str, Any]) -> List[float]:
    angle = math.radians(float(p.get("angle_deg", 45.0)))
    v0 = float(p.get("v0", 20.0))
    return [float(p.get("x0", 0.0)), float(p.get("z0", 0.0)),
            v0 * math.cos(angle), v0 * math.sin(angle)]


def _d_projectile(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    x, z, vx, vz = y
    drag = float(p.get("drag", 0.0))
    g = float(p.get("g", 9.81))
    speed = math.hypot(vx, vz)
    return [vx, vz, -drag * speed * vx, -g - drag * speed * vz]


def _a_projectile(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    x0, z0, vx0, vz0 = _p_projectile(p)
    g = float(p.get("g", 9.81))
    return [x0 + vx0 * t, z0 + vz0 * t - 0.5 * g * t * t, vx0, vz0 - g * t]


def _sp_projectile(y: List[float], p: Dict[str, Any], t: float) -> List[List[float]]:
    return [[y[0], 0.0, y[1]]]


_register(
    "projectile_drag", "physics",
    "Proyectil 2D con gravedad y rozamiento cuadratico opcional (x, z, vx, vz).",
    ["x", "z", "vx", "vz"],
    {"v0": 20.0, "angle_deg": 45.0, "g": 9.81, "drag": 0.0, "x0": 0.0, "z0": 0.0},
    _p_projectile, _d_projectile, dt_default=0.005,
    analytic=_a_projectile, spatial=_sp_projectile,
    state_hint="x/z en m, velocidades en m/s; z<0 = por debajo del suelo.")

_register(
    "harmonic_oscillator", "physics",
    "Oscilador armonico forzado y amortiguado: m x'' + b x' + k x = F0 cos(w t).",
    ["x", "v"],
    {"m": 1.0, "k": 16.0, "b": 0.0, "F0": 0.0, "omega_drive": 0.0, "x0": 1.0, "v0": 0.0},
    lambda p: [float(p.get("x0", 1.0)), float(p.get("v0", 0.0))],
    lambda t, y, p: [y[1],
                     (float(p.get("F0", 0.0)) * math.cos(float(p.get("omega_drive", 0.0)) * t)
                      - float(p.get("b", 0.0)) * y[1] - float(p.get("k", 16.0)) * y[0])
                     / float(p.get("m", 1.0))],
    dt_default=0.005,
    analytic=lambda t, y, p: (
        None if (float(p.get("b", 0.0)) or float(p.get("F0", 0.0)))
        else [y[0] * math.cos(math.sqrt(float(p.get("k", 16.0)) / float(p.get("m", 1.0))) * t)
              + y[1] / math.sqrt(float(p.get("k", 16.0)) / float(p.get("m", 1.0)))
              * math.sin(math.sqrt(float(p.get("k", 16.0)) / float(p.get("m", 1.0))) * t),
              -y[0] * math.sqrt(float(p.get("k", 16.0)) / float(p.get("m", 1.0)))
              * math.sin(math.sqrt(float(p.get("k", 16.0)) / float(p.get("m", 1.0))) * t)
              + y[1] * math.cos(math.sqrt(float(p.get("k", 16.0)) / float(p.get("m", 1.0))) * t)]),
    state_hint="x en m, v en m/s.")


def _d_pendulum(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    th, w = y
    L = float(p.get("L", 1.0))
    g = float(p.get("g", 9.81))
    b = float(p.get("b", 0.0))
    drive = float(p.get("drive", 0.0))
    wd = float(p.get("omega_drive", 0.0))
    return [w, -(g / L) * math.sin(th) - b * w + drive * math.sin(wd * t)]


_register(
    "pendulum", "physics",
    "Pendulo simple (amortiguado y/o forzado): theta'' = -(g/L) sin(theta) - b theta'.",
    ["theta_rad", "omega_rad_s"],
    {"L": 1.0, "g": 9.81, "b": 0.0, "drive": 0.0, "omega_drive": 0.0,
     "theta0": 0.5, "omega0": 0.0},
    lambda p: [float(p.get("theta0", 0.5)), float(p.get("omega0", 0.0))],
    _d_pendulum, dt_default=0.005,
    spatial=lambda y, p, t: [[float(p.get("L", 1.0)) * math.sin(y[0]), 0.0,
                              -float(p.get("L", 1.0)) * math.cos(y[0])]],
    state_hint="theta en rad, omega en rad/s.")


def _d_double_pendulum(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    th1, w1, th2, w2 = y
    m1 = float(p.get("m1", 1.0))
    m2 = float(p.get("m2", 1.0))
    L1 = float(p.get("L1", 1.0))
    L2 = float(p.get("L2", 1.0))
    g = float(p.get("g", 9.81))
    b = float(p.get("b", 0.0))
    d = th1 - th2
    denom = 2.0 * m1 + m2 - m2 * math.cos(2.0 * d)
    a1 = (-g * (2.0 * m1 + m2) * math.sin(th1)
          - m2 * g * math.sin(th1 - 2.0 * th2)
          - 2.0 * math.sin(d) * m2 * (w2 * w2 * L2 + w1 * w1 * L1 * math.cos(d))) / (L1 * denom)
    a2 = (2.0 * math.sin(d) * (w1 * w1 * L1 * (m1 + m2)
                               + g * (m1 + m2) * math.cos(th1)
                               + w2 * w2 * L2 * m2 * math.cos(d))) / (L2 * denom)
    a1 -= b * w1
    a2 -= b * w2
    return [w1, a1, w2, a2]


def _sp_double_pendulum(y: List[float], p: Dict[str, Any], t: float) -> List[List[float]]:
    L1 = float(p.get("L1", 1.0))
    L2 = float(p.get("L2", 1.0))
    x1 = L1 * math.sin(y[0])
    z1 = -L1 * math.cos(y[0])
    x2 = x1 + L2 * math.sin(y[2])
    z2 = z1 - L2 * math.cos(y[2])
    return [[0.0, 0.0, 0.0], [x1, 0.0, z1], [x2, 0.0, z2]]


_register(
    "double_pendulum", "physics",
    "Pendulo doble (caotico): 4 EDOs acopladas, buena para mostrar sensibilidad a condiciones iniciales.",
    ["theta1", "omega1", "theta2", "omega2"],
    {"m1": 1.0, "m2": 1.0, "L1": 1.0, "L2": 1.0, "g": 9.81, "b": 0.0,
     "theta1_0": 2.0, "omega1_0": 0.0, "theta2_0": 2.0, "omega2_0": 0.0},
    lambda p: [float(p.get("theta1_0", 2.0)), float(p.get("omega1_0", 0.0)),
               float(p.get("theta2_0", 2.0)), float(p.get("omega2_0", 0.0))],
    _d_double_pendulum, dt_default=0.002,
    spatial=_sp_double_pendulum,
    state_hint="angulos en rad; la visualizacion muestra soporte + 2 masas.")


def _d_heat_1d(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    alpha = float(p.get("alpha", 1.0e-4))
    dx = float(p.get("dx", 0.01))
    n = len(y)
    out = [0.0] * n
    coeff = alpha / (dx * dx)
    out[0] = 0.0
    out[n - 1] = 0.0
    for i in range(1, n - 1):
        out[i] = coeff * (y[i + 1] - 2.0 * y[i] + y[i - 1])
    return out


def _y0_heat_1d(p: Dict[str, Any]) -> List[float]:
    n = int(p.get("n", 50))
    t_left = float(p.get("T_left", 100.0))
    t_right = float(p.get("T_right", 0.0))
    t_init = p.get("T_init")
    if t_init is not None:
        return [float(t_init)] * n
    return [t_left + (t_right - t_left) * i / (n - 1) for i in range(n)]


_register(
    "heat_1d", "physics",
    "Difusion de calor 1D (FTCS explicito): dT/dt = alpha d2T/d2x con bordes fijos.",
    ["T0", "T1", "..."],
    {"alpha": 1.0e-4, "dx": 0.01, "n": 50, "T_left": 100.0, "T_right": 0.0},
    _y0_heat_1d, _d_heat_1d, dt_default=0.2,
    state_hint="Estados = temperatura de cada celda; inestable si dt > dx^2/(2 alpha).")


def _d_wave_1d(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    n = int(p.get("n", 60))
    c = float(p.get("c", 1.0))
    dx = float(p.get("dx", 1.0))
    u = y[:n]
    v = y[n:]
    du = list(v)
    dv = [0.0] * n
    coeff = (c * c) / (dx * dx)
    for i in range(1, n - 1):
        dv[i] = coeff * (u[i + 1] - 2.0 * u[i] + u[i - 1])
    dv[0] = 0.0
    dv[n - 1] = 0.0
    return du + dv


def _y0_wave_1d(p: Dict[str, Any]) -> List[float]:
    n = int(p.get("n", 60))
    amp = float(p.get("amplitude", 1.0))
    center = float(p.get("center", 0.3))
    width = float(p.get("width", 0.1))
    u = []
    for i in range(n):
        x = i / (n - 1)
        u.append(amp * math.exp(-((x - center) ** 2) / (2.0 * width * width)) if x not in (0.0, 1.0) else 0.0)
    u[0] = 0.0
    u[n - 1] = 0.0
    return u + [0.0] * n


_register(
    "wave_1d", "physics",
    "Ecuacion de onda 1D: d2u/dt2 = c2 d2u/dx2, bordes fijos en 0.",
    ["u0..un-1", "v0..vn-1"],
    {"c": 1.0, "dx": 1.0, "n": 60, "amplitude": 1.0, "center": 0.3, "width": 0.1},
    _y0_wave_1d, _d_wave_1d, dt_default=0.005,
    state_hint="Primera mitad = desplazamiento u, segunda mitad = velocidad.")


# ------------------------------------------------------------- espacial
def _y0_orbit(p: Dict[str, Any]) -> List[float]:
    r0 = float(p.get("r0", 1.0))
    v0 = float(p.get("v0", 1.0))
    return [r0, 0.0, 0.0, 0.0, v0, 0.0]


def _d_orbit(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    mu = float(p.get("mu", 3.986004418e14))
    x, yy, z, vx, vy, vz = y
    r = math.sqrt(x * x + yy * yy + z * z) + 1.0e-12
    f = -mu / (r * r * r)
    return [vx, vy, vz, f * x, f * yy, f * z]


def _sp_orbit(y: List[float], p: Dict[str, Any], t: float) -> List[List[float]]:
    return [[y[0], y[1], y[2]]]


_register(
    "orbital_2body", "space",
    "Órbita de dos cuerpos (Kepler): punto material en el potencial -mu/r, conserve energia y momento angular.",
    ["x", "y", "z", "vx", "vy", "vz"],
    {"mu": 3.986004418e14, "r0": 7.0e6, "v0": 7500.0},
    _y0_orbit, _d_orbit, dt_default=1.0, spatial=_sp_orbit,
    state_hint="SI puro (m, m/s); mu = GM. Para la Tierra mu=3.986e14.")


def _y0_nbody(p: Dict[str, Any]) -> List[float]:
    pos = p.get("positions")
    vel = p.get("velocities")
    if not pos:
        pos = [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0]]
    if not vel:
        vel = [[0.0, 0.0, 0.0], [0.0, 1.0, 0.0]] if len(pos) == 2 else [[0.0, 0.0, 0.0]] * len(pos)
    y: List[float] = []
    for pt in pos:
        y.extend([float(pt[0]), float(pt[1]), float(pt[2])])
    for vv in vel:
        y.extend([float(vv[0]), float(vv[1]), float(vv[2])])
    return y


def _d_nbody(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    masses = p.get("masses") or [1.0] * (len(y) // 6)
    n = len(masses)
    g = float(p.get("G", 1.0))
    eps = float(p.get("softening", 1.0e-6))
    pos = y[:3 * n]
    vel = y[3 * n:]
    acc = [0.0] * (3 * n)
    for i in range(n):
        for j in range(i + 1, n):
            dx = pos[3 * j] - pos[3 * i]
            dy = pos[3 * j + 1] - pos[3 * i + 1]
            dz = pos[3 * j + 2] - pos[3 * i + 2]
            r2 = dx * dx + dy * dy + dz * dz + eps
            inv = 1.0 / (r2 * math.sqrt(r2))
            f = g * inv
            acc[3 * i] += f * masses[j] * dx
            acc[3 * i + 1] += f * masses[j] * dy
            acc[3 * i + 2] += f * masses[j] * dz
            acc[3 * j] -= f * masses[i] * dx
            acc[3 * j + 1] -= f * masses[i] * dy
            acc[3 * j + 2] -= f * masses[i] * dz
    return list(vel) + acc


def _layout_nbody(y: List[float], p: Dict[str, Any]) -> Dict[str, Any]:
    masses = p.get("masses") or [1.0] * (len(y) // 6)
    n = len(masses)
    pos = y[:3 * n]
    vel = y[3 * n:]
    return {
        "bodies": [
            {"mass": masses[i],
             "position": [pos[3 * i], pos[3 * i + 1], pos[3 * i + 2]],
             "velocity": [vel[3 * i], vel[3 * i + 1], vel[3 * i + 2]]}
            for i in range(n)
        ]
    }


def _sp_nbody(y: List[float], p: Dict[str, Any], t: float) -> List[List[float]]:
    return _layout_nbody(y, p)["bodies"] and [b["position"] for b in _layout_nbody(y, p)["bodies"]]


_register(
    "n_body", "space",
    "N cuerpos bajo gravitacion newtoniana (G, masses, posiciones, velocidades). Incluye suavizado (softening).",
    ["x0,y0,z0,...", "vx0,vy0,vz0,..."],
    {"G": 1.0, "masses": [1.0, 1.0], "positions": [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0]],
     "velocities": [[0.0, 0.0, 0.0], [0.0, 1.0, 0.0]], "softening": 1.0e-6},
    _y0_nbody, _d_nbody, dt_default=0.005, layout=_layout_nbody, spatial=_sp_nbody,
    state_hint="Usa G=6.674e-11 con SI, o G=1 con unidades arbitrarias (ej. sistema de dos cuerpos).")


def _d_cr3bp(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    mu = float(p.get("mu", 0.0121505856))
    x, yy, vx, vy = y
    r1 = math.sqrt((x + mu) ** 2 + yy * yy)
    r2 = math.sqrt((x - (1.0 - mu)) ** 2 + yy * yy)
    r1_3 = r1 ** 3 + 1.0e-12
    r2_3 = r2 ** 3 + 1.0e-12
    ax = (2.0 * vy + x
          - (1.0 - mu) * (x + mu) / r1_3
          - mu * (x - (1.0 - mu)) / r2_3)
    ay = (-2.0 * vx + yy
          - (1.0 - mu) * yy / r1_3
          - mu * yy / r2_3)
    return [vx, vy, ax, ay]


_register(
    "cr3bp", "space",
    "Punto deRestricted Circular Restricted 3-Body Problem en coordenadas rotantes (Tierra-Luna u otros).",
    ["x", "y", "vx", "vy"],
    {"mu": 0.0121505856, "x0": 0.8369, "y0": 0.0, "vx0": 0.0, "vy0": -0.2008},
    lambda p: [float(p.get("x0", 0.8369)), float(p.get("y0", 0.0)),
               float(p.get("vx0", 0.0)), float(p.get("vy0", -0.2008))],
    _d_cr3bp, dt_default=0.01,
    state_hint="mu = m2/(m1+m2) (Tierra-Luna: 0.01215); unidades normalizadas.")


# ---------------------------------------------------------------- quimica
_register(
    "first_order_decay", "chemistry",
    "Desintegracion/reaccion de primer orden A -> productos: dA/dt = -k A.",
    ["A"],
    {"k": 0.1, "A0": 1.0},
    lambda p: [float(p.get("A0", 1.0))],
    lambda t, y, p: [-float(p.get("k", 0.1)) * y[0]],
    dt_default=0.05,
    analytic=lambda t, y, p: [y[0] * math.exp(-float(p.get("k", 0.1)) * t)],
    state_hint="A en mol/L, k en 1/s.")

_register(
    "second_order", "chemistry",
    "Segundo orden A + A -> P (o A+B con k efectivo): dA/dt = -k A^2.",
    ["A"],
    {"k": 0.5, "A0": 1.0},
    lambda p: [float(p.get("A0", 1.0))],
    lambda t, y, p: [-float(p.get("k", 0.5)) * y[0] * y[0]],
    dt_default=0.05,
    analytic=lambda t, y, p: [y[0] / (1.0 + float(p.get("k", 0.5)) * y[0] * t)],
    state_hint="A en mol/L, k en L/(mol s).")

_register(
    "reversible", "chemistry",
    "Reaccion reversible A <-> B con constantes k1 (frente) y k2 (inversa).",
    ["A", "B"],
    {"k1": 0.3, "k2": 0.1, "A0": 1.0, "B0": 0.0},
    lambda p: [float(p.get("A0", 1.0)), float(p.get("B0", 0.0))],
    lambda t, y, p: [-float(p.get("k1", 0.3)) * y[0] + float(p.get("k2", 0.1)) * y[1],
                     float(p.get("k1", 0.3)) * y[0] - float(p.get("k2", 0.1)) * y[1]],
    dt_default=0.05,
    state_hint="Estado estacionario: A/B = k2/k1 = Keq.")

_register(
    "autocatalysis", "chemistry",
    "Autocatalitica A + B -> 2B con B0 semilla: dA/dt = -k A B, dB/dt = k A B (curva en S).",
    ["A", "B"],
    {"k": 1.0, "A0": 1.0, "B0": 0.01},
    lambda p: [float(p.get("A0", 1.0)), float(p.get("B0", 0.01))],
    lambda t, y, p: [-float(p.get("k", 1.0)) * y[0] * y[1],
                     float(p.get("k", 1.0)) * y[0] * y[1]],
    dt_default=0.05,
    state_hint="A+B constante; B crece con cinetica autocatalitica.")


# --------------------------------------------------------------- biologia
_register(
    "exponential_growth", "biology",
    "Crecimiento exponencial dN/dt = r N (poblacion sin limites).",
    ["N"],
    {"r": 0.5, "N0": 10.0},
    lambda p: [float(p.get("N0", 10.0))],
    lambda t, y, p: [float(p.get("r", 0.5)) * y[0]],
    dt_default=0.05,
    analytic=lambda t, y, p: [y[0] * math.exp(float(p.get("r", 0.5)) * t)],
    state_hint="N en individuos, r en 1/s (o 1/h si usas horas).")

_register(
    "logistic_growth", "biology",
    "Logistica dN/dt = r N (1 - N/K): crecimiento con capacidad de carga K.",
    ["N"],
    {"r": 0.8, "K": 1000.0, "N0": 10.0},
    lambda p: [float(p.get("N0", 10.0))],
    lambda t, y, p: [float(p.get("r", 0.8)) * y[0] * (1.0 - y[0] / float(p.get("K", 1000.0)))],
    dt_default=0.05,
    analytic=lambda t, y, p: [
        float(p.get("K", 1000.0)) / (1.0 + (float(p.get("K", 1000.0)) / y[0] - 1.0)
                                     * math.exp(-float(p.get("r", 0.8)) * t))],
    state_hint="N0 < K; la solucion analitica existe y se puede comparar con la numerica.")

_register(
    "lotka_volterra", "biology",
    "Presas-depredadores de Lotka-Volterra: dP/dt = a P - b P Z ; dZ/dt = e b P Z - c Z.",
    ["prey", "predator"],
    {"a": 1.1, "b": 0.4, "e": 0.4, "c": 0.4, "prey0": 10.0, "predator0": 5.0},
    lambda p: [float(p.get("prey0", 10.0)), float(p.get("predator0", 5.0))],
    lambda t, y, p: [
        float(p.get("a", 1.1)) * y[0] - float(p.get("b", 0.4)) * y[0] * y[1],
        float(p.get("e", 0.4)) * float(p.get("b", 0.4)) * y[0] * y[1]
        - float(p.get("c", 0.4)) * y[1]],
    dt_default=0.01,
    state_hint="Ciclos conservados; el periodo crece con la amplitud.")

_register(
    "sir", "biology",
    "Epidemia SIR: dS=-b S I/N, dI=b S I/N - g I, dR=g I. R0 = b/g.",
    ["S", "I", "R"],
    {"beta": 0.4, "gamma": 0.1, "N": 1000.0, "S0": 999.0, "I0": 1.0, "R0": 0.0},
    lambda p: [float(p.get("S0", 999.0)), float(p.get("I0", 1.0)), float(p.get("R0", 0.0))],
    lambda t, y, p: [
        -float(p.get("beta", 0.4)) * y[0] * y[1] / float(p.get("N", 1000.0)),
        float(p.get("beta", 0.4)) * y[0] * y[1] / float(p.get("N", 1000.0))
        - float(p.get("gamma", 0.1)) * y[1],
        float(p.get("gamma", 0.1)) * y[1]],
    dt_default=0.05,
    state_hint="S+I+R constante (=N); R0 = beta/gamma.")

_register(
    "pharmacokinetics", "biology",
    "Farmacocinetica 1-compartment con absorcion: A_gut -ka-> A_central -ke-> eliminacion.",
    ["A_gut", "A_central", "concentration"],
    {"ka": 1.0, "ke": 0.2, "V": 50.0, "dose": 100.0},
    lambda p: [float(p.get("dose", 100.0)), 0.0],
    lambda t, y, p: [-float(p.get("ka", 1.0)) * y[0],
                     float(p.get("ka", 1.0)) * y[0] - float(p.get("ke", 0.2)) * y[1]],
    dt_default=0.05,
    layout=lambda y, p: {"dose_units": y[0] + y[1],
                         "concentration": y[1] / float(p.get("V", 50.0))},
    state_hint="C = A_central / V; V en L, dosis en mg.")

def _d_hh(t: float, y: List[float], p: Dict[str, Any]) -> List[float]:
    V, m, h, n = y
    gNa = float(p.get("gNa", 120.0))
    gK = float(p.get("gK", 36.0))
    gL = float(p.get("gL", 0.3))
    ENa = float(p.get("ENa", 50.0))
    EK = float(p.get("EK", -77.0))
    EL = float(p.get("EL", -54.4))
    Cm = float(p.get("Cm", 1.0))
    I = float(p.get("I", 10.0))

    def sig(v: float, x: float) -> float:
        if abs(x) >= 40.0:
            return 0.0 if x > 0 else 1.0
        return (0.1 * (v + 40.0)) / (1.0 - math.exp(-(v + 40.0) / 10.0))

    def tau(v: float, x: float) -> float:
        if x == 1:
            return 0.1 - sig(v, x) / 10.0 if abs(v + 40.0) < 40.0 else 0.05 + 0.14 * math.exp(-(v + 40.0) / 10.0)
        if abs(v + 55.0) >= 40.0:
            return 0.0
        return 0.125 + 0.0 * v if False else (0.125 + 0.0 * v) if False else (
            0.125 + 0.0)

    alpha_m = 0.1 * (V + 40.0) / (1.0 - math.exp(-(V + 40.0) / 10.0)) if abs(V + 40.0) < 1e-6 else (
        0.0 if V + 40.0 > 40 else 0.1 * (V + 40.0) / (1.0 - math.exp(-(V + 40.0) / 10.0)))
    if abs(V + 40.0) > 40.0:
        alpha_m = 0.0 if V > -40.0 else 4.0 * math.exp(-(V + 60.0) / 18.0)
    beta_m = 4.0 * math.exp(-(V + 65.0) / 18.0) if abs(V + 65.0) > 1e-9 else 0.0
    if abs(V + 65.0) > 40.0:
        beta_m = 0.0 if V < -65.0 else 0.1 * (V + 10.0) / (1.0 - math.exp(-(V + 10.0) / 10.0))

    alpha_h = 0.07 * math.exp(-(V + 65.0) / 20.0)
    beta_h = 1.0 / (1.0 + math.exp(-(V + 35.0) / 10.0))
    alpha_n = 0.01 * (V + 55.0) / (1.0 - math.exp(-(V + 55.0) / 10.0)) if abs(V + 55.0) > 1e-6 else 0.1
    if abs(V + 55.0) > 40.0:
        alpha_n = 0.0 if V > -55.0 else 0.125 * math.exp(-(V + 65.0) / 80.0)
    beta_n = 0.125 * math.exp(-(V + 65.0) / 80.0) if abs(V + 65.0) > 1e-9 else 0.0
    if abs(V + 65.0) > 40.0:
        beta_n = 0.0 if V < -65.0 else 0.01 * (V + 15.0) / (1.0 - math.exp(-(V + 15.0) / 10.0))

    INa = gNa * (m ** 3) * h * (V - ENa)
    IK = gK * (n ** 4) * (V - EK)
    IL = gL * (V - EL)
    dV = (I - INa - IK - IL) / Cm
    dm = alpha_m * (1.0 - m) - beta_m * m
    dh = alpha_h * (1.0 - h) - beta_h * h
    dn = alpha_n * (1.0 - n) - beta_n * n
    return [dV, dm, dh, dn]


_register(
    "hh_neuron", "biology",
    "Neurona de Hodgkin-Huxley (1952): 4 EDOs V,m,h,n con corriente inyectada I; genera potenciales de accion.",
    ["V_mV", "m", "h", "n"],
    {"gNa": 120.0, "gK": 36.0, "gL": 0.3, "ENa": 50.0, "EK": -77.0, "EL": -54.4,
     "Cm": 1.0, "I": 10.0, "V0": -65.0, "m0": 0.05, "h0": 0.6, "n0": 0.32},
    lambda p: [float(p.get("V0", -65.0)), float(p.get("m0", 0.05)),
               float(p.get("h0", 0.6)), float(p.get("n0", 0.32))],
    _d_hh, dt_default=0.01,
    state_hint="V en mV; con I=10 hay tren de picos, con I=0 la celula reposa.")


# ---------------------------------------------------------------------------
# Catalogo de calculos cerrados (sim_calc)
# ---------------------------------------------------------------------------
CALC: Dict[str, Dict[str, Any]] = {}


def _calc(name: str, domain: str, description: str,
          fn: Callable[[Dict[str, Any]], Dict[str, Any]],
          params: Dict[str, Any]) -> None:
    CALC[name] = {"name": name, "domain": domain, "description": description,
                  "fn": fn, "params": params}


def _c_projectile(p: Dict[str, Any]) -> Dict[str, Any]:
    v0 = float(p.get("v0", 20.0))
    g = float(p.get("g", 9.81))
    angle = math.radians(float(p.get("angle_deg", 45.0)))
    drag = float(p.get("drag", 0.0))
    if drag:
        dt = 0.001
        x, z, vx, vz = 0.0, 0.0, v0 * math.cos(angle), v0 * math.sin(angle)
        t = 0.0
        zmax = 0.0
        while t < 600.0 and (z >= 0.0 or t < dt):
            speed = math.hypot(vx, vz)
            vx += dt * (-drag * speed * vx)
            vz += dt * (-g - drag * speed * vz)
            x += dt * vx
            z += dt * vz
            t += dt
            zmax = max(zmax, z)
        return {"drag": drag, "range": x, "max_height": zmax, "flight_time": t,
                "impact_speed": math.hypot(vx, vz)}
    t_flight = 2.0 * v0 * math.sin(angle) / g
    range_m = v0 * v0 * math.sin(2.0 * angle) / g
    hmax = (v0 * math.sin(angle)) ** 2 / (2.0 * g)
    return {"drag": 0.0, "range": range_m, "max_height": hmax,
            "flight_time": t_flight, "impact_speed": v0}


def _c_pendulum(p: Dict[str, Any]) -> Dict[str, Any]:
    L = float(p.get("L", 1.0))
    g = float(p.get("g", 9.81))
    T0 = 2.0 * math.pi * math.sqrt(L / g)
    theta0 = p.get("theta0_deg")
    corr = 1.0
    if theta0 is not None:
        th = math.radians(float(theta0))
        corr = 1.0 + (th * th) / 16.0 + (11.0 * th ** 4) / 3072.0
    return {"T_small_angle": T0, "T_with_amplitude": T0 * corr,
            "f_small_angle": 1.0 / T0}


def _c_spring(p: Dict[str, Any]) -> Dict[str, Any]:
    m = float(p.get("m", 1.0))
    k = float(p.get("k", 16.0))
    omega = math.sqrt(k / m)
    return {"omega_rad_s": omega, "f_hz": omega / (2.0 * math.pi),
            "T_s": 2.0 * math.pi / omega, "energy_per_unit_x2": 0.5 * k}


def _c_ke(p: Dict[str, Any]) -> Dict[str, Any]:
    return {"kinetic_energy_J": 0.5 * float(p.get("m", 1.0)) * float(p.get("v", 1.0)) ** 2}


def _c_gpe(p: Dict[str, Any]) -> Dict[str, Any]:
    return {"potential_energy_J": float(p.get("m", 1.0)) * float(p.get("g", 9.81))
            * float(p.get("h", 1.0))}


def _c_momentum(p: Dict[str, Any]) -> Dict[str, Any]:
    m = float(p.get("m", 1.0))
    v = float(p.get("v", 1.0))
    return {"momentum_kg_m_s": m * v, "mass_for_1ms": m * v / 1000.0}


def _c_work(p: Dict[str, Any]) -> Dict[str, Any]:
    F = float(p.get("F", 10.0))
    d = float(p.get("d", 1.0))
    ang = math.radians(float(p.get("angle_deg", 0.0)))
    return {"work_J": F * d * math.cos(ang)}


def _c_arrhenius(p: Dict[str, Any]) -> Dict[str, Any]:
    A = float(p.get("A", 1.0e10))
    Ea = float(p.get("Ea", 50000.0))
    T = float(p.get("T", 298.15))
    k = A * math.exp(-Ea / (R_GAS * T))
    return {"k_per_s": k, "T_K": T, "Ea_kJ_mol": Ea / 1000.0,
            "t_half_first_order_s": math.log(2.0) / k if k > 0 else None}


def _c_ideal_gas(p: Dict[str, Any]) -> Dict[str, Any]:
    n = float(p.get("n", 1.0))
    T = float(p.get("T", 298.15))
    V = float(p.get("V", 0.0245))
    P = n * R_GAS * T / V
    return {"P_Pa": P, "P_bar": P / 1.0e5, "P_atm": P / 101325.0,
            "molar_volume_L_mol": V / n}


def _c_ph(p: Dict[str, Any]) -> Dict[str, Any]:
    c = float(p.get("c", 1.0e-3))
    ph = -math.log10(c)
    return {"pH": ph, "pOH": 14.0 - ph, "concentration_M": c}


def _c_half_life(p: Dict[str, Any]) -> Dict[str, Any]:
    k = float(p.get("k", 0.1))
    order = int(p.get("order", 1))
    C0 = float(p.get("C0", 1.0))
    if order == 1:
        t12 = math.log(2.0) / k
        kind = "first"
    elif order == 2:
        t12 = 1.0 / (k * C0)
        kind = "second"
    elif order == 0:
        t12 = C0 / k
        kind = "zero"
    else:
        return {"error": "order debe ser 0, 1 o 2"}
    return {"t_half": t12, "order": kind, "t_90pct_decayed": t12 * (math.log(10.0) / math.log(2.0))
            if order == 1 else None}


def _c_ice(p: Dict[str, Any]) -> Dict[str, Any]:
    K = float(p.get("Kc", 1.0))
    a0 = float(p.get("A0", 1.0))
    b0 = float(p.get("B0", 1.0))
    c0 = float(p.get("C0", 0.0))
    # A + B <-> C
    # K = (c0 + x) / ((a0 - x)(b0 - x))
    A = 1.0 - K
    B = -K * (a0 + b0) - 1.0
    C = K * a0 * b0 - c0
    if abs(A) < 1e-12:
        x = -C / B if B else 0.0
    else:
        disc = B * B - 4.0 * A * C
        if disc < 0:
            return {"error": "sin solucion real: revisa Kc y concentraciones iniciales"}
        r1 = (-B + math.sqrt(disc)) / (2.0 * A)
        r2 = (-B - math.sqrt(disc)) / (2.0 * A)
        cand = [v for v in (r1, r2) if -1e-9 <= v <= min(a0, b0) + 1e-9]
        if not cand:
            return {"error": "la solucion fisica queda fuera del rango [0, min(A0,B0)]"}
        x = max(cand)
    return {"x": x, "A_eq": a0 - x, "B_eq": b0 - x, "C_eq": c0 + x, "Kc": K}


def _c_nernst(p: Dict[str, Any]) -> Dict[str, Any]:
    T = float(p.get("T", 298.15))
    n = float(p.get("n", 1.0))
    Q = float(p.get("Q", 1.0))
    E = (R_GAS * T) / (n * 96485.33212) * math.log(Q)
    return {"E_V": E, "E_mV": E * 1000.0, "T_K": T}


def _c_michaelis(p: Dict[str, Any]) -> Dict[str, Any]:
    S = float(p.get("S", 10.0))
    Vmax = float(p.get("Vmax", 100.0))
    Km = float(p.get("Km", 5.0))
    v = Vmax * S / (Km + S)
    return {"v": v, "v_over_Vmax": v / Vmax, "S_over_Km": S / Km,
            "lineweaver_burk": {"1_v": (Km / Vmax) * (1.0 / S) + 1.0 / Vmax} if S else None}


def _c_logistic_at(p: Dict[str, Any]) -> Dict[str, Any]:
    N0 = float(p.get("N0", 10.0))
    r = float(p.get("r", 0.8))
    K = float(p.get("K", 1000.0))
    t = float(p.get("t", 5.0))
    Nt = K / (1.0 + (K / N0 - 1.0) * math.exp(-r * t))
    return {"N_t": Nt, "K": K, "doubling_time": math.log(2.0) / r if r else None,
            "fraction_of_K": Nt / K}


def _c_hardy(p: Dict[str, Any]) -> Dict[str, Any]:
    p_al = float(p.get("p", 0.5))
    q = float(p.get("q", 1.0 - p_al))
    if abs(p_al + q - 1.0) > 1e-6:
        return {"error": f"p + q = {p_al + q} debe ser 1"}
    return {"p": p_al, "q": q, "p2": p_al ** 2, "2pq": 2.0 * p_al * q, "q2": q ** 2}


def _c_sir_r0(p: Dict[str, Any]) -> Dict[str, Any]:
    beta = float(p.get("beta", 0.4))
    gamma = float(p.get("gamma", 0.1))
    R0 = beta / gamma
    attack = 0.0
    for _ in range(200):
        attack = 1.0 - math.exp(-R0 * attack) if R0 > 0 else 0.0
    return {"R0": R0, "final_attack_rate": attack, "herd_immunity_threshold": 1.0 - 1.0 / R0
            if R0 > 1 else None, "duration_susceptible_ratio": 1.0 / gamma}


def _c_escape(p: Dict[str, Any]) -> Dict[str, Any]:
    G = float(p.get("G", G_NEWTON))
    M = float(p.get("M", M_EARTH))
    R = float(p.get("R", R_EARTH))
    v = math.sqrt(2.0 * G * M / R)
    return {"escape_velocity_m_s": v, "escape_velocity_km_s": v / 1000.0}


def _c_orbital_vel(p: Dict[str, Any]) -> Dict[str, Any]:
    G = float(p.get("G", G_NEWTON))
    M = float(p.get("M", M_EARTH))
    R = float(p.get("R", R_EARTH))
    v = math.sqrt(G * M / R)
    return {"orbital_velocity_m_s": v, "orbital_velocity_km_s": v / 1000.0,
            "period_s": 2.0 * math.pi * math.sqrt(R ** 3 / (G * M))}


def _c_kepler_period(p: Dict[str, Any]) -> Dict[str, Any]:
    G = float(p.get("G", G_NEWTON))
    M = float(p.get("M", M_SUN))
    a = float(p.get("a", 1.496e11))
    T = 2.0 * math.pi * math.sqrt(a ** 3 / (G * M))
    return {"period_s": T, "period_days": T / 86400.0, "period_years": T / (365.25 * 86400.0)}


def _c_hohmann(p: Dict[str, Any]) -> Dict[str, Any]:
    mu = float(p.get("mu", 3.986004418e14))
    r1 = float(p.get("r1", 7.0e6))
    r2 = float(p.get("r2", 4.2164e7))
    v1 = math.sqrt(mu / r1)
    v2 = math.sqrt(mu / r2)
    a_t = (r1 + r2) / 2.0
    va = math.sqrt(mu * (2.0 / r1 - 1.0 / a_t))
    vb = math.sqrt(mu * (2.0 / r2 - 1.0 / a_t))
    dv1 = va - v1
    dv2 = v2 - vb
    tof = math.pi * math.sqrt(a_t ** 3 / mu)
    return {"dv1_m_s": dv1, "dv2_m_s": dv2, "delta_v_total_m_s": dv1 + dv2,
            "transfer_time_s": tof, "transfer_time_h": tof / 3600.0}


def _c_tsiolkovsky(p: Dict[str, Any]) -> Dict[str, Any]:
    isp = float(p.get("Isp", 300.0))
    m0 = float(p.get("m0", 1000.0))
    mf = float(p.get("mf", 500.0))
    dv = isp * 9.80665 * math.log(m0 / mf)
    return {"delta_v_m_s": dv, "mass_ratio": m0 / mf,
            "propellant_kg": m0 - mf, "exhaust_m_s": isp * 9.80665}


def _c_hill(p: Dict[str, Any]) -> Dict[str, Any]:
    G = float(p.get("G", G_NEWTON))
    a = float(p.get("a", 1.496e11))
    m_planet = float(p.get("m_planet", M_EARTH))
    M_star = float(p.get("M_star", M_SUN))
    r_h = a * (m_planet / (3.0 * M_star)) ** (1.0 / 3.0)
    return {"hill_sphere_m": r_h, "hill_sphere_km": r_h / 1000.0}


def _c_roche(p: Dict[str, Any]) -> Dict[str, Any]:
    R = float(p.get("R_primary", 6.96e8))
    rho_p = float(p.get("rho_primary", 1408.0))
    rho_s = float(p.get("rho_secondary", 3344.0))
    rigid = bool(p.get("rigid", True))
    factor = 2.46 if rigid else 2.44
    d = R * (rho_p / rho_s) ** (1.0 / 3.0) * factor
    return {"roche_limit_m": d, "roche_limit_km": d / 1000.0, "rigid": rigid}


def _c_vis_viva(p: Dict[str, Any]) -> Dict[str, Any]:
    mu = float(p.get("mu", 3.986004418e14))
    a = float(p.get("a", 4.2164e7))
    r = float(p.get("r", 4.2164e7))
    v = math.sqrt(mu * (2.0 / r - 1.0 / a))
    return {"speed_m_s": v, "specific_energy_J_kg": -mu / (2.0 * a)}


def _c_circular_period(p: Dict[str, Any]) -> Dict[str, Any]:
    mu = float(p.get("mu", 3.986004418e14))
    r = float(p.get("r", 4.2164e7))
    T = 2.0 * math.pi * math.sqrt(r ** 3 / mu)
    v = math.sqrt(mu / r)
    return {"period_s": T, "period_h": T / 3600.0, "velocity_m_s": v,
            "altitude_over_earth_km": (r - R_EARTH) / 1000.0 if mu == 3.986004418e14 else None}


def _c_free_fall(p: Dict[str, Any]) -> Dict[str, Any]:
    g = float(p.get("g", 9.81))
    h = float(p.get("h", 20.0))
    t = math.sqrt(2.0 * h / g)
    return {"time_s": t, "impact_speed_m_s": g * t, "impact_speed_km_h": g * t * 3.6}


def _c_wavelength(p: Dict[str, Any]) -> Dict[str, Any]:
    f = float(p.get("f", 5.0e14))
    c = float(p.get("c", 2.99792458e8))
    lam = c / f
    return {"wavelength_m": lam, "wavelength_nm": lam * 1e9,
            "wavenumber_cm_inv": 1.0 / (lam * 100.0)}


_calc("projectile", "physics",
      "Tiro parabolico: alcance, altura maxima y tiempo de vuelo (con drag opcional, integrado).",
      _c_projectile, {"v0": 20.0, "angle_deg": 45.0, "g": 9.81, "drag": 0.0})
_calc("pendulum_period", "physics",
      "Periodo del pendulo simple: T = 2 pi sqrt(L/g) mas correccion por amplitud finita.",
      _c_pendulum, {"L": 1.0, "g": 9.81, "theta0_deg": None})
_calc("spring", "physics", "Oscilador armonico: omega, f y T a partir de m y k.",
      _c_spring, {"m": 1.0, "k": 16.0})
_calc("kinetic_energy", "physics", "Energia cinetica 1/2 m v^2.", _c_ke, {"m": 1.0, "v": 1.0})
_calc("gravitational_energy", "physics", "Energia potencial gravitatoria m g h (campo uniforme).",
      _c_gpe, {"m": 1.0, "g": 9.81, "h": 1.0})
_calc("momentum", "physics", "Momento lineal p = m v.", _c_momentum, {"m": 1.0, "v": 1.0})
_calc("work", "physics", "Trabajo W = F d cos(theta).", _c_work, {"F": 10.0, "d": 1.0, "angle_deg": 0.0})
_calc("free_fall", "physics", "Caida libre desde h: tiempo e impacto.", _c_free_fall, {"h": 20.0, "g": 9.81})
_calc("wavelength", "physics", "Longitud de onda lambda = c/f (luz o sonido).",
      _c_wavelength, {"f": 5.0e14, "c": 2.99792458e8})
_calc("arrhenius", "chemistry", "Constante de velocidad k = A exp(-Ea/(R T)). Ea en J/mol.",
      _c_arrhenius, {"A": 1.0e10, "Ea": 50000.0, "T": 298.15})
_calc("ideal_gas", "chemistry", "Gas ideal P V = n R T (R = 8.314).",
      _c_ideal_gas, {"n": 1.0, "T": 298.15, "V": 0.0245})
_calc("ph", "chemistry", "pH de un acido fuerte monoprotico a partir de la molaridad.",
      _c_ph, {"c": 1.0e-3})
_calc("half_life", "chemistry", "Vida media para orden 0, 1 o 2.",
      _c_half_life, {"k": 0.1, "order": 1, "C0": 1.0})
_calc("equilibrium_ice", "chemistry",
      "Mesa ICE para A + B <-> C: resuelve la cuadratica y devuelve el equilibrio.",
      _c_ice, {"Kc": 1.0, "A0": 1.0, "B0": 1.0, "C0": 0.0})
_calc("nernst", "chemistry", "Potencial de Nernst E = (RT/nF) ln Q.", _c_nernst, {"T": 298.15, "n": 1.0, "Q": 1.0})
_calc("michaelis_menten", "biology", "Velocidad enzimatica v = Vmax [S]/(Km + [S]).",
      _c_michaelis, {"S": 10.0, "Vmax": 100.0, "Km": 5.0})
_calc("logistic_at", "biology", "Poblacion logistica N(t) cerrada.",
      _c_logistic_at, {"N0": 10.0, "r": 0.8, "K": 1000.0, "t": 5.0})
_calc("hardy_weinberg", "biology", "Equilibrio de Hardy-Weinberg a partir de p.",
      _c_hardy, {"p": 0.5, "q": 0.5})
_calc("sir_r0", "biology", "R0, ataque final y umbral de inmunidad de rebaño de un SIR.",
      _c_sir_r0, {"beta": 0.4, "gamma": 0.1})
_calc("escape_velocity", "space", "Velocidad de escape v = sqrt(2 G M / R).",
      _c_escape, {"G": G_NEWTON, "M": M_EARTH, "R": R_EARTH})
_calc("orbital_velocity", "space", "Velocidad orbital circular v = sqrt(G M / R) + periodo.",
      _c_orbital_vel, {"G": G_NEWTON, "M": M_EARTH, "R": R_EARTH})
_calc("kepler_period", "space", "Periodo orbital de Kepler T = 2 pi sqrt(a^3/(G M)).",
      _c_kepler_period, {"G": G_NEWTON, "M": M_SUN, "a": 1.496e11})
_calc("circular_period", "space", "Periodo y velocidad de una orbita circular de radio r.",
      _c_circular_period, {"mu": 3.986004418e14, "r": 4.2164e7})
_calc("hohmann", "space", "Transferencia de Hohmann: deltas-v y tiempo de transferencia.",
      _c_hohmann, {"mu": 3.986004418e14, "r1": 7.0e6, "r2": 4.2164e7})
_calc("tsiolkovsky", "space", "Ecuacion del cohete Tsiolkovsky: delta_v = Isp g0 ln(m0/mf).",
      _c_tsiolkovsky, {"Isp": 300.0, "m0": 1000.0, "mf": 500.0})
_calc("hill_sphere", "space", "Esfera de Hill de un planeta: a (m/(3M))^(1/3).",
      _c_hill, {"a": 1.496e11, "m_planet": M_EARTH, "M_star": M_SUN})
_calc("roche_limit", "space", "Limite de Roche (rigido o fluido).",
      _c_roche, {"R_primary": 6.96e8, "rho_primary": 1408.0, "rho_secondary": 3344.0, "rigid": True})
_calc("vis_viva", "space", "Energia vis-viva: v = sqrt(mu (2/r - 1/a)).",
      _c_vis_viva, {"mu": 3.986004418e14, "a": 4.2164e7, "r": 4.2164e7})

# ---------------------------------------------------------------------------
# Registro de simulaciones vivas
# ---------------------------------------------------------------------------
_SIMS: Dict[str, Dict[str, Any]] = {}


def _record(sim: Dict[str, Any]) -> None:
    hist = sim["history"]
    if len(hist) >= MAX_HISTORY:
        drop = max(1, len(hist) // 4)
        del hist[:drop]


def _state_view(sim: Dict[str, Any], include_history: bool, last: int) -> Dict[str, Any]:
    model = MODELS[sim["model"]]
    y = sim["y"]
    view: Dict[str, Any] = {
        "id": sim["id"],
        "domain": model["domain"],
        "model": sim["model"],
        "t": sim["t"],
        "steps": sim["steps"],
        "dt": sim["dt"],
        "y": [float(v) for v in y],
        "labels": model["labels"],
        "params": sim["params"],
        "created_at": sim["created_at"],
    }
    if model.get("layout"):
        try:
            view["derived"] = model["layout"](y, sim["params"])
        except Exception as exc:
            view["derived_error"] = str(exc)
    if model.get("state_hint"):
        view["hint"] = model["state_hint"]
    if include_history:
        hist = sim["history"]
        view["history_len"] = len(hist)
        view["history"] = [{"t": h[0], "y": [float(v) for v in h[1]]} for h in hist[-max(1, int(last)):]]
    return view


def register_sim_tools(mcp: FastMCP):
    """Registra el router `unreal_sim` (cada accion se publica como `sim_<accion>`)."""

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

    def _merge(model_name: str, params: Optional[Dict[str, Any]]) -> Dict[str, Any]:
        model = MODELS[model_name]
        merged = dict(model["params"])
        unknown: List[str] = []
        for key, value in (params or {}).items():
            if key in model["params"]:
                merged[key] = value
            else:
                unknown.append(key)
        if unknown:
            merged["_unknown_params"] = unknown
        return merged

    def list_models(ctx: Context, domain: Optional[str] = None) -> Dict[str, Any]:
        """Lista los modelos dinamicos y los calculos cerrados disponibles.

        Args:
            domain: filtra por "physics", "chemistry", "biology" o "space" (opcional).
        """
        models = [
            {"name": m["name"], "domain": m["domain"], "description": m["description"],
             "labels": m["labels"], "default_params": m["params"], "default_dt": m["dt_default"],
             "has_analytic": bool(m.get("analytic")), "state_hint": m["state_hint"]}
            for m in MODELS.values()
            if domain is None or m["domain"] == domain
        ]
        calcs = [
            {"name": c["name"], "domain": c["domain"], "description": c["description"],
             "default_params": c["params"]}
            for c in CALC.values()
            if domain is None or c["domain"] == domain
        ]
        return {"success": True, "domain": domain, "models": models, "calculators": calcs,
                "model_count": len(models), "calculator_count": len(calcs)}

    def create(ctx: Context, model: str, params: Optional[Dict[str, Any]] = None,
               dt: Optional[float] = None) -> Dict[str, Any]:
        """Crea una simulacion nueva y devuelve su id.

        Args:
            model: nombre de un modelo de list_models (ej. "pendulum", "sir", "n_body").
            params: sobreescribe los parametros por defecto (ver default_params).
            dt: paso de integracion (por defecto el del modelo).
        """
        if model not in MODELS:
            return {"success": False,
                    "message": f"Modelo desconocido: '{model}'. Usa list_models para ver {sorted(MODELS)}"}
        if len(_SIMS) >= MAX_SIMS:
            return {"success": False,
                    "message": f"Limite de {MAX_SIMS} simulaciones simultaneas; borra alguna con delete."}
        merged = _merge(model, params)
        unknown = merged.pop("_unknown_params", [])
        m = MODELS[model]
        try:
            y0 = [float(v) for v in m["y0"](merged)]
        except Exception as exc:
            return {"success": False, "message": f"Parametros invalidos: {exc}"}
        if not _finite(y0):
            return {"success": False, "message": "Condiciones iniciales no finitas"}
        sim_id = f"{model}_{int(time.time())}_{uuid.uuid4().hex[:4]}"
        sim = {
            "id": sim_id, "model": model, "params": merged, "y0": y0,
            "y": list(y0), "t": 0.0, "dt": float(dt or m["dt_default"]),
            "steps": 0, "history": [(0.0, list(y0))],
            "created_at": time.strftime("%Y-%m-%dT%H:%M:%S"), "viz": None,
        }
        _SIMS[sim_id] = sim
        out = _state_view(sim, include_history=False, last=0)
        out["success"] = True
        if unknown:
            out["warnings"] = f"Parametros ignorados (no existen en {model}): {unknown}"
        return out

    def _get(sim_id: str) -> Dict[str, Any]:
        sim = _SIMS.get(sim_id)
        if sim is None:
            raise KeyError(f"Simulacion '{sim_id}' no existe (activas: {sorted(_SIMS)})")
        return sim

    def _advance(sim: Dict[str, Any], steps: int) -> None:
        m = MODELS[sim["model"]]
        f = lambda t, y: m["derivs"](t, y, sim["params"])
        dt = sim["dt"]
        for _ in range(int(steps)):
            y_new = _rk4(f, sim["t"], sim["y"], dt)
            if not _finite(y_new):
                raise OverflowError(
                    f"Estado no finito en t={sim['t']:.4g}: el paso es demasiado grande "
                    f"o el modelo diverge. Baja dt (actual {dt}) o revisa los parametros.")
            sim["t"] += dt
            sim["y"] = y_new
            sim["steps"] += 1
            _record(sim)
            sim["history"].append((sim["t"], list(y_new)))

    def step(ctx: Context, sim_id: str, steps: int = 1,
             dt: Optional[float] = None) -> Dict[str, Any]:
        """Avanza la simulacion N pasos de integracion.

        Args:
            sim_id: id devuelto por create.
            steps: numero de pasos (default 1).
            dt: sobreescribe el paso por esta llamada (opcional).
        """
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        steps = max(1, min(int(steps), 100000))
        if dt is not None:
            sim["dt"] = float(dt)
        try:
            _advance(sim, steps)
        except (OverflowError, ValueError) as exc:
            return {"success": False, "message": str(exc), "id": sim_id, "t": sim["t"]}
        out = _state_view(sim, False, 0)
        out["success"] = True
        return out

    def run(ctx: Context, sim_id: str, t_end: Optional[float] = None,
            steps: Optional[int] = None, max_steps: int = MAX_STEPS_PER_RUN) -> Dict[str, Any]:
        """Ejecuta la simulacion hasta un tiempo t o un numero de pasos.

        Args:
            sim_id: id devuelto por create.
            t_end: tiempo final (se integra hasta alcanzarlo).
            steps: numero de pasos si no das t_end.
            max_steps: tope de seguridad (default 200000).
        """
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        limit = max(1, min(int(max_steps), MAX_STEPS_PER_RUN))
        done = 0
        try:
            if t_end is not None:
                target = float(t_end)
                if target < sim["t"]:
                    return {"success": False,
                            "message": f"t_end={target} < t actual ({sim['t']}); usa reset para volver atras."}
                while sim["t"] + sim["dt"] <= target + 1e-12 and done < limit:
                    _advance(sim, 1)
                    done += 1
                if done >= limit and sim["t"] + sim["dt"] < target:
                    return {"success": False,
                            "message": f"Alcanzo el tope de {limit} pasos en t={sim['t']:.6g} "
                                       f"(dt={sim['dt']}); sube max_steps o agranda dt.",
                            **_state_view(sim, False, 0)}
            elif steps is not None:
                _advance(sim, max(1, min(int(steps), limit)))
                done = int(steps)
            else:
                return {"success": False, "message": "Pasa t_end o steps"}
        except (OverflowError, ValueError) as exc:
            return {"success": False, "message": str(exc), "id": sim_id, "t": sim["t"]}
        out = _state_view(sim, False, 0)
        out.update({"success": True, "steps_done": done})
        return out

    def get_state(ctx: Context, sim_id: str, history: bool = False,
                  last: int = 20, analytic: bool = False) -> Dict[str, Any]:
        """Devuelve el estado actual (y opcionalmente el historial o la solucion analitica).

        Args:
            sim_id: id de la simulacion.
            history: incluir los ultimos `last` puntos grabados.
            last: cuantos puntos del historial devolver.
            analytic: si el modelo tiene solucion cerrada, devuelve su valor en el t actual
                      (para comparar con la numerica).
        """
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        out = _state_view(sim, bool(history), int(last))
        out["success"] = True
        if analytic:
            m = MODELS[sim["model"]]
            if not m.get("analytic"):
                out["analytic"] = None
                out["analytic_note"] = f"'{sim['model']}' no tiene solucion cerrada"
            else:
                try:
                    val = m["analytic"](sim["t"], sim["y0"], sim["params"])
                    if val is None:
                        out["analytic"] = None
                        out["analytic_note"] = "solucion analitica no aplica con estos parametros"
                    else:
                        out["analytic"] = [float(v) for v in val]
                        out["y_numeric"] = [float(v) for v in sim["y"]]
                        out["abs_error"] = [abs(float(val[i]) - float(sim["y"][i]))
                                            for i in range(min(len(val), len(sim["y"])))]
                except Exception as exc:
                    out["analytic_error"] = str(exc)
        return out

    def set_params(ctx: Context, sim_id: str,
                   params: Dict[str, Any]) -> Dict[str, Any]:
        """Cambia parametros de una simulacion viva (k, masa, beta, mu...).

        Args:
            sim_id: id de la simulacion.
            params: pares clave=valor a sobreescribir; se mantienen los demas.
        """
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        model_params = MODELS[sim["model"]]["params"]
        applied, unknown = {}, []
        for key, value in (params or {}).items():
            if key in model_params:
                sim["params"][key] = value
                applied[key] = value
            else:
                unknown.append(key)
        out = {"success": not unknown, "id": sim_id, "applied": applied}
        if unknown:
            out["message"] = f"Parametros desconocidos para {sim['model']}: {unknown}"
            out["known"] = sorted(model_params)
        return out

    def reset(ctx: Context, sim_id: str) -> Dict[str, Any]:
        """Vuelve la simulacion a t=0 con sus condiciones iniciales y limpia el historial."""
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        sim["y"] = list(sim["y0"])
        sim["t"] = 0.0
        sim["steps"] = 0
        sim["history"] = [(0.0, list(sim["y0"]))]
        out = _state_view(sim, False, 0)
        out["success"] = True
        return out

    def delete(ctx: Context, sim_id: str) -> Dict[str, Any]:
        """Elimina una simulacion (y sus actores de visualizacion si los tenia)."""
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        viz = sim.get("viz")
        cleaned = None
        if viz and viz.get("actors"):
            cleaned = clear_unreal(ctx, sim_id=sim_id)
        _SIMS.pop(sim_id, None)
        return {"success": True, "deleted": sim_id, "viz_cleaned": cleaned}

    def list_sims(ctx: Context) -> Dict[str, Any]:
        """Lista las simulaciones activas con su tiempo y modelo."""
        return {"success": True, "count": len(_SIMS),
                "limit": MAX_SIMS,
                "simulations": [
                    {"id": s["id"], "model": s["model"], "domain": MODELS[s["model"]]["domain"],
                     "t": s["t"], "steps": s["steps"], "dt": s["dt"],
                     "has_viz": bool(s.get("viz")), "created_at": s["created_at"]}
                    for s in _SIMS.values()]}

    def calc(ctx: Context, kind: str,
             params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Resuelve una formula cerrada (sin crear simulacion).

        Args:
            kind: nombre del calculo, ver list_models (ej. "escape_velocity", "arrhenius",
                  "hohmann", "michaelis_menten", "equilibrium_ice").
            params: parametros del calculo (default_params en list_models).
        """
        if kind not in CALC:
            return {"success": False,
                    "message": f"Calculo desconocido: '{kind}'. Usa list_models para ver {sorted(CALC)}"}
        entry = CALC[kind]
        merged = dict(entry["params"])
        for key, value in (params or {}).items():
            merged[key] = value
        try:
            result = entry["fn"](merged)
        except Exception as exc:
            return {"success": False, "message": f"Error en '{kind}': {exc}"}
        if isinstance(result, dict) and result.get("error"):
            return {"success": False, "message": result["error"], "kind": kind}
        return {"success": True, "kind": kind, "domain": entry["domain"],
                "params": merged, "result": result}

    # ------------------------------------------------------------ visualizacion
    def _spatial_points(sim: Dict[str, Any]) -> List[List[float]]:
        m = MODELS[sim["model"]]
        if m.get("spatial"):
            return m["spatial"](sim["y"], sim["params"], sim["t"])
        return [[sim["t"], float(sim["y"][0]), 0.0]]

    def _to_ue(point: List[float], origin: List[float], scale: float) -> List[float]:
        return [origin[0] + point[0] * scale,
                origin[1] + (point[1] if len(point) > 1 else 0.0) * scale,
                origin[2] + (point[2] if len(point) > 2 else 0.0) * scale]

    def to_unreal(ctx: Context, sim_id: str, prefix: Optional[str] = None,
                  mode: str = "path", max_points: int = 200, scale: float = 1.0,
                  origin: Optional[List[float]] = None, color: Optional[List[int]] = None,
                  intensity: float = 5000.0) -> Dict[str, Any]:
        """Dibuja la simulacion en Unreal con actores (PointLight).

        Args:
            sim_id: simulacion a visualizar.
            prefix: prefijo de nombres; si se omite se usa "SIM_<id>". Reutiliza el mismo
                    prefijo con update_unreal y clear_unreal.
            mode: "path" = estela con los puntos del historial; "state" = puntos vivos de
                    la posicion actual (para n_body, orbitas, pendulo).
            max_points: maximo de actores a crear en modo path.
            scale: multiplicador sim -> cm de Unreal (SI en metros usa 100).
            origin: traslacion [x, y, z] en cm.
            color: [r, g, b] 0-255 (default blanco).
            intensity: intensidad de cada luz.
        """
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        if mode not in ("path", "state"):
            return {"success": False, "message": "mode debe ser 'path' o 'state'"}
        origin = list(origin or [0.0, 0.0, 0.0])
        prefix = prefix or f"SIM_{sim_id}"
        color = list(color or [255, 255, 255])

        clear_unreal(ctx, sim_id=sim_id, prefix=prefix)

        if mode == "path":
            hist = sim["history"]
            step_gap = max(1, len(hist) // max(1, int(max_points)))
            samples = hist[::step_gap][-int(max_points):]
            created: List[str] = []
            for index, (t_sample, y_sample) in enumerate(samples):
                m = MODELS[sim["model"]]
                if m.get("spatial"):
                    pts = m["spatial"](y_sample, sim["params"], t_sample)
                    point = pts[0]
                else:
                    point = [t_sample, float(y_sample[0]), 0.0]
                name = f"{prefix}_{index:04d}"
                result = _send(ctx, "spawn_actor",
                               {"name": name, "type": "PointLight",
                                "location": _to_ue(point, origin, scale)})
                if isinstance(result, dict) and result.get("success") is False:
                    return {"success": False,
                            "message": f"No pude crear {name}: {result.get('message')}",
                            "created": created}
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "Intensity",
                       "property_value": float(intensity)})
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "AttenuationRadius",
                       "property_value": max(1.0, scale * 5.0)})
                created.append(name)
        else:
            created = []
            points = _spatial_points(sim)
            for index, point in enumerate(points):
                name = f"{prefix}_{index:04d}"
                result = _send(ctx, "spawn_actor",
                               {"name": name, "type": "PointLight",
                                "location": _to_ue(point, origin, scale)})
                if isinstance(result, dict) and result.get("success") is False:
                    return {"success": False,
                            "message": f"No pude crear {name}: {result.get('message')}",
                            "created": created}
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "Intensity",
                       "property_value": float(intensity)})
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "LightColor",
                       "property_value": list(color) + [255] * (4 - len(color))})
                created.append(name)

        sim["viz"] = {"prefix": prefix, "mode": mode, "actors": created,
                      "scale": float(scale), "origin": origin}
        return {"success": True, "id": sim_id, "prefix": prefix, "mode": mode,
                "actors_created": len(created), "first": created[:3], "last": created[-3:],
                "t": sim["t"]}

    def update_unreal(ctx: Context, sim_id: str, prefix: Optional[str] = None) -> Dict[str, Any]:
        """Mueve los actores de modo "state" a la posicion actual de la simulacion.

        Args:
            sim_id: simulacion visualizada con to_unreal(mode="state").
            prefix: prefijo usado en to_unreal (opcional si la simulacion lo recuerda).
        """
        try:
            sim = _get(sim_id)
        except KeyError as exc:
            return {"success": False, "message": str(exc)}
        viz = sim.get("viz")
        if not viz:
            return {"success": False,
                    "message": "Esa simulacion no tiene visualizacion; usa to_unreal primero."}
        if viz.get("mode") != "state":
            return {"success": False,
                    "message": "update_unreal solo sirve para mode='state'; en mode='path' "
                               "vuelve a llamar a to_unreal para redibujar la estela."}
        prefix = prefix or viz["prefix"]
        points = _spatial_points(sim)
        actors = viz.get("actors", [])
        moved = 0
        for index, point in enumerate(points[:len(actors)]):
            name = actors[index]
            result = _send(ctx, "set_actor_transform",
                           {"name": name, "location": _to_ue(point, viz["origin"], viz["scale"])})
            if isinstance(result, dict) and (result.get("success") is True
                                             or result.get("transform_set") is not False):
                moved += 1
        return {"success": True, "id": sim_id, "t": sim["t"], "moved": moved,
                "actors": len(actors)}

    def clear_unreal(ctx: Context, sim_id: Optional[str] = None,
                     prefix: Optional[str] = None) -> Dict[str, Any]:
        """Borra los actores creados por to_unreal.

        Args:
            sim_id: simulacion cuyo viz quieres borrar.
            prefix: o directamente el prefijo de nombres a borrar (se busca con find_actors_by_name).
        """
        names: List[str] = []
        if sim_id:
            try:
                sim = _get(sim_id)
            except KeyError as exc:
                return {"success": False, "message": str(exc)}
            viz = sim.get("viz")
            if viz:
                prefix = prefix or viz["prefix"]
                names = list(viz.get("actors", []))
                sim["viz"] = None
        if prefix and not names:
            found = _send(ctx, "find_actors_by_name", {"pattern": prefix})
            names = [a.get("name") for a in found.get("actors", [])
                     if isinstance(a, dict) and str(a.get("name", "")).startswith(prefix)]
        deleted, errors = 0, []
        for name in names:
            result = _send(ctx, "delete_actor", {"name": name})
            if isinstance(result, dict) and result.get("deleted_actor"):
                deleted += 1
            elif isinstance(result, dict) and result.get("success") is False:
                errors.append(result.get("message"))
        return {"success": not errors, "found": len(names), "deleted": deleted,
                "errors": errors}

    ACTIONS = {
        'list_models': list_models,
        'create': create,
        'step': step,
        'run': run,
        'get_state': get_state,
        'set_params': set_params,
        'reset': reset,
        'delete': delete,
        'list': list_sims,
        'calc': calc,
        'to_unreal': to_unreal,
        'update_unreal': update_unreal,
        'clear_unreal': clear_unreal,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_sim",
        description=(
            "Simulaciones cientificas en Unreal (fisica, quimica, biologia y espacial).\n"
            "    \n"
            "    Parametros:\n"
            "      action: nombre de la operacion (ver lista abajo)\n"
            "      params: dict con los argumentos de esa operacion\n"
            "    \n"
            "    Flujo tipico: list_models -> create(model, params) -> run(t_end) ->\n"
            "    get_state(history=True) -> to_unreal(mode='state') -> update_unreal -> clear_unreal.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_sim(ctx: Context, action: str,
                   params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_sim action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_sim '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Sim tools (router) registered: {len(ACTIONS)} acciones, "
                f"{len(MODELS)} modelos, {len(CALC)} calculos")
