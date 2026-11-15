[![✗](https://img.shields.io/badge/Release-v2.0.0-ffb600.svg?style=for-the-badge)](https://github.com/svillanueva22/tpe_tla/releases)

[![✗](https://github.com/svillanueva22/tpe_tla/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/svillanueva22/tpe_tla/actions/workflows/pipeline.yaml)

# Fixtura

Compilador de **Fixtura**, un DSL para declarar torneos de fútbol (ligas, copas y federaciones con calendarios plurianuales), desarrollado con Flex y Bison sobre la base de [Flex-Bison-Compiler](https://github.com/agustin-golmar/Flex-Bison-Compiler). El compilador valida el programa (análisis léxico, sintáctico y semántico) y genera un **documento HTML** autocontenido con el fixture de cada torneo.

La especificación del lenguaje se encuentra en [`doc/Especificación_Fixtura_TLA.pdf`](doc/Especificación_Fixtura_TLA.pdf).

* [Requerimientos](#requerimientos)
* [Configuración](#configuración)
* [Comandos](#comandos)
* [Salida generada](#salida-generada)
* [Casos de prueba](#casos-de-prueba)
* [CI/CD](#cicd)
* [Extensiones recomendadas](#extensiones-recomendadas)

## Requerimientos

* [Docker v28.3.2](https://www.docker.com/)

No se utilizan librerías externas: la salida es HTML + CSS + JavaScript embebido (sin dependencias) y no requiere ningún runtime más allá de un navegador web.

## Configuración

Las siguientes variables de entorno controlan el comportamiento de la aplicación:

| Nombre                | Default | Descripción                                                                                                                                                  |
| :-------------------- | :-----: | :----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | El nombre del entorno activo. Los entornos disponibles son: `Local`, `Development` y `Production`.                                                           |
| `LOG_IGNORED_LEXEMES` | `true`  | Si es `true`, loguea todos los lexemas ignorados por Flex con nivel `DEBUGGING`. Para quitarlos de la consola, usar `false`.                                  |
| `LOGGING_LEVEL`       | `ALL`   | El nivel mínimo de log en consola. De menor a mayor: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` y `CRITICAL`.                                     |

_Docker Compose_ también puede leer las variables desde un archivo `.env` (ver `compose.yaml`).

> [!NOTE]
> Todos los logs se escriben en la salida de error estándar, por lo que la salida estándar contiene únicamente el HTML generado.

## Comandos

### Iniciar

Levanta un contenedor efímero, listo para desarrollar:

```bash
docker compose run --rm compiler
```

Todos los comandos siguientes se ejecutan dentro del contenedor.

### Construir

Construye (o reconstruye) el compilador completo:

```bash
src/main/bash/build.sh
```

### Compilar un programa

```bash
src/main/bash/run.sh <programa> > <salida>.html
```

donde `<programa>` es la ruta al archivo fuente. Por ejemplo:

```bash
src/main/bash/run.sh src/test/c/accept/93-example-afa > afa.html
```

Luego, abrir el archivo `.html` con cualquier navegador. El código de salida es `0` si el programa es correcto, y distinto de `0` si se rechaza (los errores léxicos, sintácticos o semánticos se informan en la salida de error estándar). Para silenciar los logs en la terminal, agregar `LOGGING_LEVEL=ERROR` al inicio del comando.

### Testear

Ejecuta todos los casos de prueba de la carpeta `src/test/c`:

```bash
src/main/bash/test.sh
```

### Detener

Sale del contenedor, destruye los contenedores efímeros y apaga el clúster:

```bash
exit
docker compose down
```

### Docker

| Comando                                 | Descripción                                                  |
| :-------------------------------------- | :----------------------------------------------------------- |
| `docker builder prune --all`            | Elimina todos los builds y la caché de construcción.         |
| `docker compose --progress=plain build` | Fuerza la construcción o reconstrucción de las imágenes.     |
| `docker image prune`                    | Elimina las imágenes huérfanas (_dangling_).                 |
| `docker network prune`                  | Elimina las redes no utilizadas.                             |
| `docker volume prune`                   | Elimina los volúmenes no utilizados.                         |

## Salida generada

El compilador emite un único documento HTML con estilos y un script embebidos. Por cada declaración:

* **Torneo todos contra todos**: fixture por fechas (algoritmo de Berger; con equipo libre si la cantidad es impar, y a una o dos ruedas), y tabla de posiciones con las zonas coloreadas (campeón, ascenso, descenso y clasificación).
* **Eliminación directa**: cuadro de llaves. Si la cantidad de equipos no es potencia de 2, los primeros pasan directo (_bye_).
* **Grupos y eliminación directa**: fixture de la fase de grupos, tabla por grupo y cuadro eliminatorio cruzado (e.g., «1° Grupo A» contra «2° Grupo B»).
* **Federación**: el calendario se evalúa año por año (`every`, `otherwise`, `if`/`else`, ciclos y `host` con años explícitos o relativos) y se genera cada torneo en las temporadas que lo albergan, resolviendo las fechas sin año. Además, se listan las reglas de ascenso y descenso y se aplican entre temporadas consecutivas, rearmando los planteles de cada liga para el año siguiente.

Las fechas de cada jornada se calculan a partir del `schedule` (cada N días, cada N semanas, o ciertos días de la semana). Si el torneo declara `results`, la tabla de posiciones se calcula con ellos (según `points` y `tiebreak`, o los _templates_ usados), y en las llaves avanzan los ganadores (por global y, si hace falta, por penales). Los equipos tomados de otro torneo (`teams from`) se muestran según su posición (e.g., «2° Nacional») hasta que los resultados permiten conocerlos, ya que se definen recién al finalizar ese torneo.

El documento es interactivo: cada partido del fixture y cada llave tiene campos para cargar goles (y penales), precargados con los `results` declarados. Al escribir un resultado se recalculan los puntos y el orden de las tablas (según `points` y `tiebreak`; las zonas se mantienen por posición), al completarse un grupo sus clasificados ocupan su lugar en las llaves, y los ganadores avanzan hasta el campeón. En una federación, al cargar los resultados de una temporada se recalculan los ascensos y descensos y los equipos tomados de otro torneo (`teams from`) para las temporadas siguientes. Los resultados cargados se guardan en el `localStorage` del navegador, y el botón «Restaurar resultados declarados» vuelve a los del programa.

## Casos de prueba

| Carpeta                     | Contenido                                                           |
| :-------------------------- | :------------------------------------------------------------------ |
| `src/test/c/accept`         | Programas válidos: el compilador debe aceptarlos.                   |
| `src/test/c/reject`         | Programas léxica o sintácticamente inválidos: deben rechazarse.     |
| `src/test/c/semantic/reject`| Programas sintácticamente válidos pero semánticamente incorrectos.  |

## CI/CD

Para disparar una integración automática en cada _push_ o PR (_Pull Request_), se debe activar _GitHub Actions_ en la pestaña _Settings_, con la siguiente configuración:

| Clave                                                      | Valor                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

## Extensiones recomendadas

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
