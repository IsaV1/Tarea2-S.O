# Tarea 2 - Torneo de Komikan Concurrente
### Integrantes: Alexander Bravo e Isaías Vásquez

Simulación de un torneo de Komikan en tiempo real (sin turnos) con hilos POSIX. N jugadores se emparejan, ocupan hasta M mesas y juegan partidas donde cada ficha (1 Puma + 12 Perros) es un hilo independiente que mueve con retardos aleatorios. Sin red: los jugadores son hilos locales.

## Requisitos

- Entorno Unix; probado en Linux.
- `g++` con soporte C++17 y `pthread`

## Compilación

```bash
g++ -Wall -Wextra -std=c++17 -pthread -o torneo main.cpp
```

## Ejecución

```bash
./torneo N M [config.txt]
```

| Parámetro    | Descripción                                              |
|--------------|----------------------------------------------------------|
| `N`          | Número de jugadores. Debe ser par y mayor o igual a 2.   |
| `M`          | Número máximo de mesas simultáneas. Mayor o igual a 1.   |
| `config.txt` | (Opcional) Archivo de configuración.                     |

N y M deben ser enteros válidos dentro del rango de `int`. Se rechazan argumentos no numéricos, parcialmente numéricos, fuera de rango, N impar, N < 2, M < 1 y cantidades incorrectas de argumentos. En esos casos el programa termina con código 1.

Ejemplos:

```bash
./torneo 8 2                # 8 jugadores, 2 mesas, valores por defecto
./torneo 8 2 config.txt     # con configuración propia
```

Con `Ctrl+C` se solicita el cierre ordenado del servidor, descrito en la sección «Cierre del servidor».

## Formato de `config.txt`

Números separados por espacios o saltos de línea, en este orden:

```
minMs maxMs seed
filaPuma columnaPuma
fila1 columna1
fila2 columna2
...
fila12 columna12
```

1. `minMs maxMs`: retardo mínimo y máximo (ms) entre movimientos de cada ficha (`0 < minMs < maxMs`).
2. `seed`: semilla base para la aleatoriedad.
3. 13 pares `fila columna`: el primero es el **Puma**; los siguientes 12 son los **Perros 1 a 12**.

**Coordenadas del tablero** (filas de arriba hacia abajo, columnas de izquierda a derecha, desde 0):

| Fila | Casillas (columnas válidas) |
|------|-----------------------------|
| 0    | 1 casilla (`0`) – punta del trono |
| 1    | 2 casillas (`0`–`1`) – trono |
| 2–6  | 5 casillas (`0`–`4`) – cuadrícula 5×5 |

Las posiciones deben estar dentro del tablero y no repetirse. El archivo debe contener exactamente los tres valores iniciales y los 13 pares de posiciones; después del último valor solo se permite espacio en blanco. No se admiten encabezados, comentarios ni datos adicionales. Si el archivo no puede abrirse, está incompleto o contiene datos inválidos, se informa el error y se utilizan todos los valores predeterminados.

**Valores por defecto:** `minMs = 50`, `maxMs = 300`, `seed = 12345`, Puma en `(0,0)`, Perros 1–5 en la fila 6, 6–10 en la fila 5 y 11–12 en la fila 4 (columnas desde 0).

Ejemplo (`config.txt`, equivalente a los valores por defecto):

```
50 300 12345
0 0
6 0
6 1
6 2
6 3
6 4
5 0
5 1
5 2
5 3
5 4
4 0
4 1
```
El ejemplo anterior muestra los valores predeterminados. El archivo `config.txt` incluido en esta entrega utiliza un intervalo personalizado de 100 a 200 ms y mantiene la misma semilla y disposición inicial.

No cambiar el ejemplo de 50-300 ms ni la descripcion de los predeterminados: siguen siendo correctos.

## Reglas implementadas

- **Puma (`-1` en el tablero):** se mueve a casillas adyacentes libres. Captura saltando sobre un perro hacia una casilla vacía. Si tiene una captura disponible, la prioriza. **Gana con 6 o más capturas.**
- **Perros (`1`–`12`):** se mueven a casillas adyacentes libres; no capturan. **Ganan si el Puma queda sin movimientos válidos.**
- No hay turnos: cada ficha duerme un retardo aleatorio en `[minMs, maxMs]` y luego intenta un movimiento. Si dos fichas buscan la misma casilla, solo la primera en tomar el lock la ocupa.

- En la cuadrícula 5×5 se permiten movimientos ortogonales, sin diagonales directas. El trono utiliza las conexiones especiales definidas en la validación del tablero. Las capturas requieren un perro en un nodo intermedio real y dos tramos alineados y equidistantes según la representación geométrica adoptada.

## Salida por consola

Cada movimiento imprime la partida, la pieza, origen, destino, captura (si hay) y la matriz del tablero (`0` vacío, `-1` Puma, `1`–`12` perros). También se informan las parejas formadas, el abandono de jugadores (si ocurre) y el ganador de cada partida.

## Abandono simulado

Cada jugador activo realiza aproximadamente cada 100 ms una comprobación con una probabilidad cercana al 1 % de abandonar, utilizando `rand_r` y una semilla propia.

Si abandona, `abandonarPartida()` registra el término bajo `lock_partida` y asigna la victoria al oponente. Las piezas finalizan y el árbitro espera sus hilos antes de registrar el resultado y liberar la mesa.

Este mecanismo simula el evento de abandono. No detecta automáticamente la muerte o el bloqueo real de un hilo ni recupera un fallo fatal del proceso.

## Cierre del servidor

El manejador de SIGINT únicamente activa una bandera `atomic<bool>`, cuya implementación lock-free se comprueba mediante `static_assert`. El servidor deja de admitir nuevos jugadores y partidas al detectar el cierre.

Las piezas dividen sus retardos en tramos de hasta 100 ms y comprueban entre ellos la solicitud de cierre y el término de la partida. Los árbitros devuelven las mesas adquiridas y los hilos se esperan mediante `pthread_join` antes de destruir los recursos asociados. El tiempo de cierre también depende de la planificación del sistema operativo.
