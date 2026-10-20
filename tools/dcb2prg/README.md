# dcb2prg — descompilador de .dcb a fuente BennuGD

Recupera código fuente legible a partir de un `.dcb` compilado (o de un ejecutable
con stub). Pensado para recuperar proyectos cuyo `.prg` se ha perdido.

Soporta las dos revisiones del formato:

| Versión | Motor | Palabra |
|---|---|---|
| `0x0800` | BennuGD2 | 64 bits |
| `0x0710` | BennuGD1 | 32 bits |

Se detecta sola. Entre revisiones cambian además el enum `BASETYPE`, la tabla de
mnemónicos (BennuGD1 no tiene tipo `DOUBLE`, así que le faltan esos códigos y los
posteriores se corren) y la posición de los flags de tipo dentro del opcode
(`0xF00` en v1, `0xF000` en v2). Las tablas de v1 están transcritas del repo
oficial <https://github.com/SplinterGU/BennuGD>, no inferidas.

## Uso

    python3 tools/dcb2prg/decompile.py -o juego.prg juego.dcb  # descompilar todo
    python3 tools/dcb2prg/decompile.py -o - juego.dcb NOMBRE_PROC  # sólo un proceso
    python3 tools/dcb2prg/disasm.py    juego.dcb [NOMBRE_PROC] # desensamblado crudo
    python3 tools/dcb2prg/dcb.py       juego.dcb               # resumen del contenedor
    python3 tools/dcb2prg/dcb.py --extract juego.dcb dir/      # volcar assets incrustados
    python3 tools/dcb2prg/decompile.py -o juego2.prg --bgd2 juego.dcb  # y portar a BennuGD2

Si el `.dcb` lleva assets incrustados (gráficos, sonidos, fuentes), el
descompilador avisa y pregunta si extraerlos; `--extraer[=DIR]` lo hace sin
preguntar y `--no-extraer` calla. Sin terminal (un script, un CI) no pregunta:
avisa y sigue. Recuperar el fuente sin los assets deja un proyecto que no
arranca, y es fácil no enterarse de que estaban ahí.

**`bgdi` no se cambia al directorio del `.dcb`**: resuelve las rutas desde el
directorio de trabajo. Como el juego abre sus assets con rutas relativas, un
`.dcb` lanzado desde otro sitio no encuentra nada y **se cierra al arrancar sin
ningún mensaje**. O se ejecuta desde su carpeta, o se usa `jugar.sh <juego.dcb>`,
que hace el `cd` y además avisa si faltan `data/` o `mod/`.

Van a la **carpeta del `.prg`**, no a una subcarpeta: dentro del `.dcb` las rutas
ya son las que usa el juego (`data/adam.fpg`, `mod/...`) y las abre tal cual desde
su directorio de trabajo. Si eso pisara ficheros ya existentes, la pregunta lo
dice y el "no" pasa a ser la respuesta por defecto.

`-o` es **obligatorio**: escribir un fuente de varios MB por stdout sin querer, o
perderlo por olvidar el redirector, es demasiado fácil. Con `-o -` sale por stdout,
para encadenar con una tubería. Ejecutándolo sin argumentos saca la ayuda completa.

Sin dependencias: sólo Python 3 (biblioteca estándar). El `.dcb` se abre con `mmap`,
así que un fichero de cientos de MB no se carga entero en memoria.

El fuente va al fichero de `-o` y el progreso por **stderr**, así que no se mezclan:

    python3 decompile.py -o juego.prg juego.dcb          # progreso en pantalla
    python3 decompile.py -o juego.prg juego.dcb 2> log   # progreso a fichero

La traza sale con color si la terminal lo admite. `NO_COLOR` lo apaga y
`FORCE_COLOR` lo enciende aunque se redirija; al mandarla a un fichero se apaga
sola, así que el log queda en texto limpio.

### Si lo copias a otro sitio

`decompile.py` **no funciona solo**: importa `dcb.py`, `mn.py` y `resolve.py`, y en modo
`--bgd2` además `port_bgd1to2.py` con sus dos `symtab_*.json`. Python busca esos módulos
en la carpeta **del script**, no en el directorio desde el que lo lanzas, así que hay dos
formas correctas de usarlo:

    # (a) dejar la carpeta entera donde esté e invocarlo por su ruta
    cd ~/mi_juego
    python3 /ruta/a/dcb2prg/decompile.py -o juego.prg JUEGO.DCB

    # (b) copiar la carpeta COMPLETA, no sólo decompile.py
    cp -r dcb2prg ~/donde_sea/

Copiar únicamente `decompile.py` da `ModuleNotFoundError: No module named 'dcb'`.
Un enlace simbólico sí vale (conserva la ruta real del script); un `cp` de un solo
fichero, no.

## Qué se recupera

El `.dcb` no es código máquina, sino bytecode de una VM de pila que conserva
casi toda la información simbólica:

- **Nombres**: procesos, funciones, variables globales/locales/privadas/públicas,
  miembros de struct y funciones del sistema (tabla `OSysProcsCodes`).
- **Cadenas**: la tabla de textos completa.
- **Tipos y declaraciones**: tipo base, dimensiones de array y structs anidados.
- **Estructuras de control**: `IF/ELSE`, `WHILE`, `FROM..TO..STEP`, `REPEAT..UNTIL`,
  `SWITCH/CASE` (incluidos rangos `a..b`), `CLONE`, `FRAME`, `&&`/`||` con cortocircuito.
- **Nombres de fichero originales**, si se compiló con `-g`.
- **Los `import`**: la tabla `OImports` indexa la de cadenas y da los módulos.
- **Assets incrustados**: gráficos, sonidos y datos que el compilador empaquetó
  dentro del `.dcb` (`--extract`), con sus rutas originales.

## Qué NO se puede recuperar

Se pierde en la compilación, sin excepción:

- **Comentarios** y formato original.
- **`#define` y constantes**: quedan plegadas a su valor (`ACT_PREV` -> `1`).
- **`#include`**: la división en ficheros se pierde (los `import` sí se
  recuperan, desde la tabla `OImports`).
- **Tipo de retorno de las FUNCTION**: no se almacena por proceso.
- **Valores por defecto de parámetros**.
- El código fuente **no va embebido** ni con `-g`: con debug sólo se guardan los
  nombres de fichero y la tabla línea/columna, y el runtime lee los `.prg` del disco.

## Estado

Los 65 `.dcb` del repositorio (BennuGD2) se descompilan sin fallos. Validado
contra `examples/12-sound/12.2-sound.prg`, cuyo fuente original conservamos:
funciones, `SWITCH` y expresiones se reconstruyen con equivalencia semántica.

Probado también sobre un juego real de BennuGD1 de 320 MB: 357 procesos y
169.000 líneas, con menos del 0,3 % de líneas marcadas como dudosas.

El formato de BennuGD1 se dedujo primero del propio binario y después se
contrastó con `core/include/{dcb,typedef_st,pslang}.h` del repo oficial:
cabecera, `BASETYPE` y tamaños de estructura coincidían; la tabla de mnemónicos
tenía 12 códigos mal en el rango 0x63-0x78 y los flags de tipo estaban en el
nibble equivocado, y ambas cosas se corrigieron desde el fuente.

Como comprobación independiente, los 65.364 saltos del juego de prueba caen
todos en frontera de instrucción y no queda ningún opcode sin reconocer.

Probado además sobre un segundo juego comercial de BennuGD1 (*5 Leaps*, 586
procesos, extraído del `.exe` que lo lleva incrustado): descompila y **recompila
entero sin un solo error**, con 549 de 586 procesos (93,7 %) de bytecode
idéntico, parecido medio 0,998, y **cero** diferencias en operandos de variable,
en destinos de salto, en accesos a globales, en cadenas y en valores iniciales.
Ese juego fue el que sacó a la luz casi todo lo de la sección siguiente.

La salida es una **reconstrucción**, no el fichero original: requiere repasar
los `import`, las constantes y algún salto no estructurado marcado con `//`.

### Cosas del dialecto que hay que reconstruir a mano (no están en el `.dcb`)

El `.dcb` guarda **menos tipo** del que la sintaxis exige, y hay que deducirlo:

- **Variables de tipo proceso.** `otro.mivarpublica` sólo compila si `otro` se
  declaró con el nombre de un proceso como tipo (`TORRETA otro;`): es lo único
  que le da a `bgdc` el varspace de las públicas ajenas (`c_data.c:551` y `:906`).
  En el `.dcb` esa variable es un `INT` a secas y el enlace no viaja. Se deduce
  del proceso del que se asigna la variable y, si no lo hay, del único proceso
  cuyas públicas cubren todos los offsets y tipos que se usan.
- **Las `PUBLIC` van en la declaración adelantada**, no en el cuerpo: es el
  único sitio donde son visibles antes de que el proceso aparezca, y declararlas
  dos veces es un error. Por eso se emite `DECLARE PROCESS x() PUBLIC ... END END`.
- **Declaraciones adelantadas por los parámetros.** Si un proceso se llama antes
  de definirse, `bgdc` fija el tipo de cada parámetro por los argumentos de esa
  llamada y luego choca con el declarado (`c_main.c:1083`). Se declara por
  delante todo proceso con algún parámetro de tipo no entero.
- **`LOCAL` del programa.** Las locales que no son del motor necesitan su bloque
  `LOCAL`, o `otro.saved_status` no compila.
- **Punteros a struct encadenados.** `lista.ultimo.siguiente` son tres opcodes
  (`INDEX`, `PTR`, `INDEX`) y el tipo hay que arrastrarlo a mano; sin eso salía
  `*(lista.ultimo + 28)`. Un puntero sin indexar apunta a su primer campo, que
  el compilador se ahorra: `*motor` es en realidad `motor.<primer campo>`. Y un
  puntero doble se escribe `(*p).campo`, con `tipo_N ** p` en la firma.
- **Structs pasados por parámetro.** El compilador pasa la dirección, que es la
  del primer escalar: `mitorreta.arma` y `mitorreta.arma.velocidad` son el mismo
  offset. Hay que quitar exactamente el camino del primer escalar **del tipo que
  espera el parámetro**, ni más ni menos.
- **Dirección de struct = `&` en el fuente.** Un `INDEX` a un campo que es un
  struct embebido, sin `PTR` detrás, es la *dirección* de ese struct:
  `&nodo.evento == evento`, `nave.lpath = &map_mapa.lpath[i]`. Sin el `&` bgdc
  responde "Data type not accepted here" o mete un `PTR` de más. Ojo a no
  confundirlo con un puntero a struct leído, que ya *es* una dirección y no
  lleva `&`.
- **Procesos declarados y nunca definidos.** 17 de los 586 de *5 Leaps* no
  tienen ni una instrucción en el `.dcb`. Darles un cuerpo vacío les mete un
  `END` que el original no tiene: sólo se emite su declaración adelantada.
- **Qué es y qué no es el `init` de un `FOR`.** El init de verdad se compila
  como expresión-sentencia y acaba en `LET; PTR; POP`; una sentencia normal de
  antes del bucle acaba en `LETNP`. Tragársela como init cambia el programa. Si
  no hay init, `FOR (; cond; paso)` es sintaxis válida.
- **El `DEFAULT` acaba donde acaba el `SWITCH`**, y eso lo dice el `JUMP` con
  que cierran los casos, no el final del bloque de fuera. Delimitarlo mal se
  tragaba las sentencias de después del switch y corría todos los saltos
  siguientes: 6 procesos con saltos mal se arreglaron con esto.
- **Operandos que son profundidad de pila.** `A2STR n` y las conversiones
  convierten el elemento a profundidad `n`, no la cima. Y `STRI2CHR` y `STR2A`
  no son conversiones de un operando: consumen dos.

### Módulos de terceros

Un `.dcb` que importe un módulo que no viene con BennuGD1 (`mod_steam` y
similares) no se puede recompilar, y sin recompilar no hay verificación. Para
eso, el propio `.dcb` sirve de fuente: lleva la tabla de sysprocs con el nombre
y los tipos de parámetro de cada función.

    python3 make_stub_modules.py --dcb <fichero.dcb> <modulo> <dir_salida> \
                                 <fuentes_bennugd1>

Entran en el stub las funciones del `.dcb` que no exporte ningún módulo de las
fuentes. El tipo de retorno no viaja en el `.dcb`: se declara `INT`.

## Ficheros

| Fichero | Cometido |
|---|---|
| `dcb.py` | Parser del contenedor (cabecera, tablas de ID, cadenas, varspaces, procesos) |
| `mn.py` | Tabla de mnemónicos de la VM (espejo de `core/include/pslang.h`) |
| `resolve.py` | Offset de byte -> nombre de variable (structs y arrays incluidos) |
| `disasm.py` | Desensamblador con nombres resueltos |
| `decompile.py` | Reconstrucción de expresiones y flujo de control |
| `extract_dcb.py` | Saca el `.dcb` que va incrustado dentro de un ejecutable |
| `build_sysprocs_fenix.py`, `build_vars_fenix.py` | Tablas de Fenix, desde sus fuentes |
| `construir_fenix.sh` | Compila el `fxc` y el `fxi` de Fenix |
| `verificar_casos.sh` | Round-trip a una carpeta entera de casos de prueba |

El formato está descrito en `core/include/dcb.h`; el lector de referencia es
`core/bgdrtm/dcbr.c` y el escritor `core/bgdc/dcbw.c`.

### Juegos repartidos como un solo `.exe`

BennuGD y Fenix pueden pegar el `.dcb` detrás del intérprete para repartir el
juego como un ejecutable suelto:

    python3 extract_dcb.py <ejecutable> [salida.dcb]

Hay dos formas de la cola —`"DCB Stub\x1a\r\n\0"` + offset de 4 bytes (16 bytes en
total), y `"DCB Stub\x1a\r\n"` + offset de 8 (20 bytes, BennuGD de 64)— y además
un ejecutable puede llevarlo sin cola ninguna. Se prueban las dos y, si no hay,
se busca la cabecera `dcb\r\n\x1f`. La versión que sale se comprueba contra las
conocidas para no escribir basura. Con esto se sacó el `.dcb` de *5Leaps.exe*.

## De dónde viene y a dónde va

La herramienta nombra siempre los dos extremos, porque no son intercambiables:
un `.dcb` de Fenix y uno de BennuGD1 son los dos de 32 bits y no tienen los
mismos opcodes, y descompilar a BennuGD1 no es lo mismo que portar a BennuGD2.

    == leyendo ruckman.dcb ==
       origen    : Fenix 0x0200 (32 bits, palabra de 4 bytes)
       ...
       dialecto  : Fenix (el mismo, sin portar)
       fichero   : ruckman.prg

El fuente que sale lo dice en su cabecera (`// origen: Fenix 0x0200   destino:
Fenix`), y todo lo demás va en consecuencia: quién declara las globales del
motor, cómo se llama la función que repone sus valores iniciales
(`fenix_init_globals`, `bgd1_init_globals`, `bgd2_init_globals`), y de qué motor
es el ancho de 32 bits de las variables que van a disco. `disasm.py` encabeza el
listado con el motor y la versión, que es de donde sale la tabla de opcodes.

Con `--bgd2` el destino pasa a ser BennuGD2 y se dice; desde un `.dcb` de Fenix
además avisa de que el portador está hecho para BennuGD1 y hay que repasarlo.

`verificar_roundtrip.py` avisa si se le pasa un compilador que no es el del
motor del `.dcb`: recompilar un fichero de Fenix con el `bgdc` de BennuGD no da
nada comparable.

## Portar DESDE Fenix (`--bgd1`, `--bgd2`)

Fenix lo lleva todo dentro del intérprete: **no tiene módulos ni `import`**, y
por eso sus `.dcb` traen `NImports = 0`. Al portarlo a BennuGD hay que averiguar
qué módulos hacen falta a partir de lo que el programa usa, o no compila nada.
Ojo: no basta con mirar las funciones — `timer` lo declara `mod_timers`, y sin
ese `import` falla aunque no se llame a ninguna de sus funciones.

    decompile.py --bgd1 -o juego.prg juego.dcb     # Fenix -> BennuGD1
    decompile.py --bgd2 -o juego.prg juego.dcb     # Fenix -> BennuGD2

Antes de escribir nada se comprueba, contra la tabla de símbolos del destino,
todo lo que el programa le pide al motor —funciones del sistema, globales y
locales, **sólo las que toca de verdad**— y se avisa de tres cosas distintas:

- lo que **no existe** en el destino;
- lo que existe pero **cambia de firma** (que exista `map_put` no sirve si
  `put` de Fenix lleva 5 parámetros y el de BennuGD2 lleva 6, 14, 15 o 21);
- las variables del motor que **el destino no declara**, que pasan a declararse
  en el propio programa.

Si no hay nada de eso, lo dice: *«todo lo que usa tiene equivalente: el portado
es completo»*. Si lo hay, avisa de que **el fuente que sale no compila tal cual**
y lo repite como comentario en la cabecera del `.prg`.

### Cómo salió con *Ruckman*

| Destino | Resultado |
|---|---|
| **BennuGD1** | Completo. Las 33 funciones existen; sólo faltaban los 10 `import`. **Compila con el `bgdc` de BennuGD1.** |
| **BennuGD2** | Incompleto, y lo avisa: 5 funciones sin equivalente (`PUT`, `PUT_SCREEN`, `SET_TEXT_COLOR`, `SET_WAV_VOLUME`, `XPUT`) y 1 que cambia de firma (`MAP_PUT`). No compila sin repasarlo. |

De ahí el consejo que da la herramienta: **de Fenix a BennuGD2 conviene pasar
primero por BennuGD1**, que va directo.

Combinaciones que no tienen sentido y se rechazan: `--bgd1` junto con `--bgd2`,
y `--bgd1` sobre un `.dcb` de BennuGD2 (enteros de 64 bits y funciones que en
BennuGD1 no existen).

## Fenix, el antecesor

BennuGD es un fork de Fenix, y sus `.dcb` (versiones `0x0100`-`0x0500`) también
se leen. Diferencias que hubo que resolver, todas contrastadas con las fuentes
de Fenix:

| | Fenix | BennuGD1 |
|---|---|---|
| Campos por proceso | 13 (`0x0200`), 19 (`0x0500`) o 20 (`0x0600`) | 21 |
| Variables `PUBLIC` | no existen en `0x0200` | sí |
| `MAX_TYPECHUNKS` | 6 | 8 |
| Tabla de ficheros | `Name[55]` fijo, 64 bytes | empaquetada (`SName`, nombre variable) |
| Códigos de opcode | los suyos | 39 corridos: BennuGD metió `JTFALSE`/`JTTRUE` en `0x1E`-`0x1F` |
| Tabla de sysprocs | **no va en el `.dcb`**, códigos fijos del intérprete | va en el `.dcb` |
| `BASETYPE`, flags de tipo | idénticos | idénticos |

Cuál de las tres disposiciones de proceso usa un fichero se prueba por orden de
versión pero **midiendo**: se queda la que da un reparto coherente (el código de
cada proceso cabe en el fichero y empieza después de la propia tabla).

### Versiones de Fenix

Están soportadas las dos que hemos visto, y las tablas son **por versión**,
porque cambian:

| | 0.86g (`0x0500`) | 0.93 (`0x0600`) |
|---|---|---|
| Campos por proceso | 19 | 20 (añade `ONEXIT`) |
| Opcodes | los suyos | **los de BennuGD1**: ya metió `JTFALSE`/`JTTRUE` en `0x1E`-`0x1F` |
| Funciones del sistema | 256 | 277, y **20 códigos cambian de nombre** |
| Locales | 52 | 54, y **25 offsets se corren** (metió `palette` por medio) |
| Globales | 835 | las mismas |

Por eso las tablas se guardan con la versión en el nombre
(`sysprocs_fenix_0500.json`, `vars_fenix_0600.json`...) y se elige la más
antigua que sea `>= ` la del fichero: hacia atrás sólo se pierden cosas del
final, así que ésa es la que menos se equivoca. Los generadores sacan la versión
de `include/dcb.h` y nombran el fichero solos.

El listado de fuentes también cambia (la 0.93 cambió `mmx_scale2x.c` por
`scaler_*.c`), así que `construir_fenix.sh` compila **lo que haya** en vez de
una lista fija. Además la 0.93 escribe `TARGET_Linux` con L mayúscula, necesita
`-DVERSION` (la ponía autotools) y enlaza con SDL y con Xlib.

Como Fenix no guarda la tabla de funciones del sistema ni las variables del
motor, se sacan de sus fuentes:

    python3 build_sysprocs_fenix.py <fuentes_de_fenix>   # -> sysprocs_fenix.json
    python3 build_vars_fenix.py     <fuentes_de_fenix>   # -> vars_fenix.json

El segundo interpreta `locals_def` y `globals_def` de `fxc/src/main_div.c`, que
están escritas en el propio lenguaje, y calcula el reparto de offsets.

### Lo que no se puede recuperar de un `.dcb` de Fenix

Los juegos se compilaban **sin depuración**, y entonces `NID`, `NGloVars`,
`NLocVars` y `NPriVars` valen 0: el fichero no lleva un solo nombre. Los
procesos salen como `proc_388` y las variables del programa como
`unresolved_global_1888`. Eso no es un límite de la herramienta: el nombre no se
guardó en ningún sitio.

Lo que sí se reconstruye:

- **Variables del motor** (`x`, `y`, `graph`, `file`, `mouse`, `joy`, `scroll`,
  `m7`...), por offset, desde `vars_fenix.json`.
- **Funciones del sistema**, por código, desde `sysprocs_fenix.json`.
- **Parámetros**, que son las primeras privadas.
- **Dimensiones de los arrays**, deducidas del bytecode: el operando de `ARRAY`
  es el paso en bytes de la dimensión indexada, y el cociente entre dos pasos
  consecutivos da el número de elementos de la de dentro. La de más afuera se
  estima con el hueco hasta la siguiente variable usada. En *Ruckman* sale
  `INT tabla[50][21][14]`, que son 51×22×15×4 = 67320 bytes: exactamente los que
  hay hasta la siguiente global. 

### Probado con

*Ruckman* (un Pac-Man de 2009 para GP2X, Fenix `0x0200`): 38 procesos, 14.264
instrucciones, **0 opcodes desconocidos y los 1.155 saltos caen todos en
frontera de instrucción**. Descompila con 0 instrucciones sin traducir y 0
expresiones sin reconstruir. Hasta se recupera el nombre del fuente original,
que va en la tabla `OSourceFiles`: `fenixDevKit\fenix\pacman\editor\ruckman11.prg`.

### Round-trip de Fenix, y ejecutarlo

Sí lo hay: `construir_fenix.sh` compila el `fxc` **y el `fxi`** de Fenix desde
sus fuentes.
Cuatro cosas que cuestan de encontrar:

- `-m32` — Fenix es de 32 bits;
- `-DTARGET_linux` — es lo que define `__stdcall`, sin él `fnx_loadlib.h` no compila;
- `-fcommon` — el código es anterior a gcc 10 y tiene definiciones tentativas
  repetidas (`debug` en `main.c` y en `c_main.c`);
- la `libz` de 32 bits se enlaza por ruta, porque no suele estar el enlace `.so`.

Y dos más solo para el intérprete:

- `-fgnu89-inline` — el `_inline` de `g_draw.c` con semántica C99 no emite el
  cuerpo de la función: 20 símbolos sin definir al enlazar;
- `img_png.c` usa la API de libpng 1.2 (`png_ptr->jmpbuf`, `info_ptr->palette`),
  que ya no existe. Se parchea sobre una copia, sin tocar las fuentes.

`verificar_roundtrip.py` acepta el `fxc` igual que el `bgdc` (se da cuenta de
que no tiene `-o` y de que escribe el `.dcb` junto al fuente).

Sobre un programa compilado **con** nombres, el round-trip da **100 %**: mismo
tamaño de código, mismos opcodes, mismos operandos, mismos saltos.

Sobre *Ruckman*, que va sin nombres, el descompilado **recompila entero, arranca
y se juega**: el `.dcb` reconstruido dibuja su pantalla de título y su menú. 29
de los 38 procesos salen con el bytecode idéntico. Los operandos no pueden
coincidir: al no guardarse la tabla de variables, las globales se reconstruyen y
acaban en otros offsets. Los 9 restantes son procesos donde la forma deducida de
algún array no coincide con la original.

Lo que hizo falta para que un `.dcb` de Fenix vuelva a compilar:

- **Fenix no tiene `goto`**, así que el modo plano con etiquetas —que en BennuGD
  es la red de seguridad fiel— allí no vale. Hubo que estructurar el 100 % del
  flujo: reconstruir el `REPEAT ... UNTIL` (antes se soltaba un `UNTIL` suelto
  sin bucle) y dejar que un bucle que empieza fuera del bloque que se está
  emitiendo se envuelva igual. Eso último **solo** se hace en Fenix: en BennuGD
  forzar la estructura se inventaba un `if/else` que no estaba.
- El `BREAK` de un `REPEAT` se confundía con un `ELSE`: el guardián que los
  distingue solo miraba saltos atrás incondicionales, y el cierre de un repeat
  es condicional.
- El cuerpo del programa se reconoce por `NParams == -1`, no por llamarse
  `MAIN` — en un `.dcb` sin nombres no hay ningún `MAIN`. Ojo: ese mismo
  centinela lo llevan también los procesos solo declarados (5 de los 586 de
  *5 Leaps*), así que solo cuenta si se llama `MAIN` o si no hay tabla de
  identificadores.

Dos avisos sobre el `fxi` que sale de estas fuentes (0.86g), que **no** es el que
usaba Ruckman (una 0.84 portada a GP2X):

- solo abre `.dcb` de versión 5, así que el original de *Ruckman* (`0x0200`) lo
  rechaza; el recompilado, que ya es `0x0500`, sí;
- su `set_wav_volume(sample, vol)` castea `sample` a `Mix_Chunk *` sin
  comprobarlo, y *Ruckman* llama `set_wav_volume(-1, ...)` — está así en el
  bytecode original. Con esta versión eso es un segfault seguro. Para probar hay
  que poner un `if (sample > 0)` en `fxi/src/sound.c`.

## Portar de BennuGD1 a BennuGD2 (`--bgd2`)

Con `--bgd2` la salida sale ya traducida al dialecto de BennuGD2: `import`
correctos, funciones renombradas y globales reubicadas.

**Nada está inventado.** `build_symtab.py` recorre los `*_exports.h` de cada
árbol de fuentes y extrae lo que los módulos publican al lenguaje (macro `FUNC`,
`DLCONSTANT`, `globals_def`, `locals_def`). Un renombrado sólo se aplica si el
nombre destino **existe de verdad** en la tabla de BennuGD2; si además la aridad
no encaja, se traduce pero se marca para revisar. Lo que no tiene equivalente se
deja con su nombre original y un TODO, en vez de traducirlo a ojo.

Para regenerar las tablas:

    python3 build_symtab.py /ruta/a/BennuGD   symtab_bgd1.json   # SplinterGU/BennuGD
    python3 build_symtab.py /ruta/a/BennuGD2  symtab_bgd2.json

Los `.json` van versionados, así que el portado funciona sin tener los dos
árboles a mano.

### Resultado sobre el juego de prueba

De 100 funciones del sistema usadas, 57 conservan el nombre y 32 se renombran
automáticamente (`load_fpg`->`fpg_load`, `play_wav`->`sound_play`,
`file_exists`->`fexists`...). Las globales sueltas que pasaron a struct se
reubican (`sound_freq`->`sound.freq`, `full_screen`->`screen.fullscreen`).

Quedan 18 símbolos sin equivalente, y la mayoría no es un descuido sino un
cambio de arquitectura: BennuGD2 eliminó las paletas de 8 bits (todo el grupo
`pal_*`, `load_pal`, la local `palette`) y el pintado por regiones
(`restore_type`, `dump_type`). Eso hay que rehacerlo a mano.

`--bgd2` deja la lista completa como cabecera del fichero generado.

## Estado del portado real (juego BennuGD1 de 170k líneas)

**Compila y se ejecuta.** El `.prg` portado pasa por el `bgdc` de BennuGD2 sin
errores y el juego arranca, carga sus FPG y llega a la intro animada.

Llegar ahí exigió corregir 21 fallos del descompilador, casi todos destapados
por el compilador o por el propio juego en ejecución:

- `INDEX` sobre un valor es una suma constante, no un acceso a miembro.
- `CLONE` lleva un offset de código, no un índice de proceso.
- El acceso remoto (`father.x`) indexa las LOCALES, no las privadas.
- Los `'+'` de los ParamTypes son argumentos que **genera el compilador**.
- En arrays multidimensionales se emitía una dimensión de más.
- Cada `CASE` cierra con su propio `END`; 326 bucles `LOOP` se perdían.
- Firmas MIXTAS: primeros parámetros locales reservadas sin tipo, resto con tipo.
- Los arrays como parámetro van por puntero, no con dimensiones.
- BennuGD1 permitía variable y proceso con el mismo nombre.
- **`BREAK` perdidos**: "cond; JFALSE t; JUMP fuera; t: ..." dentro de un bucle
  no es un if/else sino un `BREAK` en la rama THEN. Se confundían, la rama
  salía vacía y el bucle nunca cortaba. Se distinguen porque el supuesto ELSE
  termina saltando hacia atrás (el cierre del bucle). Eran 31 de 42.
- **x/y/z/size**: enteras en BennuGD1 y DOUBLE en BennuGD2. El código original
  asume truncación (`y = pos_float; ... IF (y == 780)`), así que hay que
  truncar en cada escritura o el juego se queda esperando una igualdad que no
  se cumple nunca.
- **Valores iniciales**: el `.dcb` guarda el contenido inicial de globales,
  privadas y públicas. Ignorarlo dejaba el juego con todas sus tablas y rutas a
  cero (`RUTA_PAL` vacío hacía que no encontrase ningún fichero).
- **Punteros**: los parámetros `'P'` reciben una dirección y hay que escribir
  `&var`. Sin el `&` la función nativa escribía en una dirección inventada y el
  runtime petaba. La clase de parámetro se toma de la firma de **destino**,
  porque al renombrar puede cambiar (`WRITE_INT` usa `P`, `WRITE_VAR` usa `V`).

### Por qué hacen falta los `DECLARE`

BennuGD2 cambió las locales `x`, `y`, `z` y `size` de INT a **DOUBLE**. Si un
proceso se llama antes de declararse, `bgdc` fija el tipo del parámetro por los
argumentos y luego choca con el tipo real de la local
(`core/bgdc/c_main.c:918`), y el error aparece en la firma de un proceso que
puede estar 50.000 líneas más allá. Por eso `--bgd2` da tipo explícito a esos
parámetros **y** emite un `DECLARE PROCESS` de cada proceso: las dos cosas son
necesarias.

### Lo que queda fuera del alcance de la herramienta

- Las paletas de 8 bits (`pal_*`, `load_pal`) no existen en BennuGD2: se generan
  stubs con TODO y hay que rehacer esa parte.
- Los ficheros `.pal` del juego no estaban en el `.dcb` (contenido externo).
- Comprobaciones de integridad que leen píxeles esperando color de 16 bits
  fallan siempre en BennuGD2 y llaman a `exit()`.

## Paletas: intentado y revertido

`--bgd2` puede generar una emulación de la API de paletas de BennuGD1 sobre
`MAP_GET_PALETTE` / `MAP_SET_PALETTE`, pero sobre el motor tal cual no
funciona, por tres motivos comprobados uno a uno:

1. `map_get_palette` y `map_set_palette` tienen todo su cuerpo dentro de
   `#ifdef USE_SDL2`, y ese símbolo no se define en ningún sitio del build:
   se compilan como `return 0` (7 bytes) sin tocar nada.
2. Con el backend SDL2_GPU, `bitmap_new` hace `gr->surface = NULL` tras subir
   la textura, así que la superficie indexada -lo único que permite
   recolorear- se destruye al cargar.
3. Ninguna comprueba que `bitmap_get` devolviera algo: con un gráfico
   inexistente desreferencian NULL y el runtime peta.

Parchear el motor (conservar la superficie de los gráficos de 8 bits,
regenerar la textura GPU y añadir guardas de NULL) hace que la paleta se lea y
el cambio persista, pero ROMPIO el juego en pruebas reales (sin audio y con la
velocidad alterada), asi que se revirtio. Queda pendiente rehacerlo aislado,
sin tocar la ruta de dibujado compartida.

## Verificación objetiva: round-trip con BennuGD1

El único control que no depende de jugar al juego:

    original.dcb -> decompile.py -> .prg -> bgdc de BennuGD1 -> .dcb'

y comparar las dos tablas. Vale para cualquier juego de BennuGD1.

    python3 verificar_roundtrip.py juego.dcb /ruta/al/bgdc

Montarlo requiere dos cosas:

1. **El `bgdc` de BennuGD1.** Se compila del repo oficial con autotools; hay
   que enlazar `-lcrypto` (el `DES_ecb_encrypt` que busca ya no está en
   `libssl`). Sólo hace falta `bgdc`; que `bgdi` no enlace da igual.
2. **Módulos stub** (`make_stub_modules.py`). `bgdc` hace dlopen de cada módulo
   importado y sólo lee sus TABLAS DE DATOS: nombres de función, tipos de
   parámetro, constantes y las declaraciones de globales/locales. No llama a
   ninguna implementación, así que basta con `.so` que expongan esas tablas y
   no hace falta SDL 1.2 ni el resto de dependencias del motor original.

   Dos detalles que cuestan de encontrar: los símbolos van **prefijados con el
   nombre del módulo** (`mod_sound_globals_def`), porque `_dlibaddr()` compone
   `<fichero_sin_extension>_<simbolo>`; y `bgdc` busca los `.so` en
   `<directorio_del_ejecutable>/../modules/`, no en el directorio actual.

### Que compile NO demuestra nada

El descompilado puede compilar perfectamente y no ser el mismo programa. Dos
casos reales, los dos encontrados sobre `pointers.prg`:

- un `RETURN;` de más al final del proceso (el `END` que cierra todo proceso es
  implícito, no hay que escribirlo) — compila igual y el `.dcb` sale más grande;
- `PROCESS MAIN()` donde el fuente era un programa suelto `BEGIN ... END`. Se
  distinguen por `NParams`: el programa suelto lleva el centinela `-1`, la
  cabecera explícita deja 0. Mismo bytecode, distinto `.dcb`.

Ninguno de los dos lo ve un "compila sin errores". Por eso está
`verificar_casos.sh`, que pasa el round-trip completo a una carpeta de casos:

    ./verificar_casos.sh <carpeta_con_prg> <ruta_del_bgdc>

Sirve con el `bgdc` de BennuGD1 o con el de BennuGD2 — `verificar_roundtrip.py`
lee las dos revisiones del formato. Con los dos arreglos de arriba, `cast` y
`pointers` recompilan a un `.dcb` **idéntico byte a byte** salvo la hora de
compilación.

### La etiqueta de un `case` es una expresión, no una lista de opcodes

El sondeo que reconoce la cadena de `case` de un `SWITCH` funcionaba con una
lista de opcodes PERMITIDOS. Cualquier cosa que no estuviera en ella hacía que
el sondeo se rindiera, no encontrase el `JNOCASE`, y el switch entero cayera a
la rama `DEFAULT` con un `// ?? JNOCASE` dentro: se perdían todos los casos
menos uno.

Lo delató `case get_value():` — una llamada a función como etiqueta, con `CALL`
fuera de la lista. Ahora es al revés: una lista de lo que la DESCARTA (flujo de
control y sentencias), así que entra cualquier expresión. Las seis formas del
dialecto se reconstruyen: `case f():`, `case 0:`, `case 1..4:`, `case 7,8,9:`,
`case (i>5)*i:` y `default:`.

### Cadenas que el original no usa

Un `.dcb` puede llevar literales que **nada** referencia: ni el bytecode los
empuja ni aparecen como valor inicial de ninguna variable. En *5 Leaps* son 42
seguidos (rótulos de un panel de estadísticas). No hay fuente que los regenere
—no hay dónde escribirlos— así que contarlos como pérdida es una falsa alarma:
el verificador los lista aparte y no falla por ellos.

Ojo al detectarlos: **sólo el `PUSH` lleva un índice de cadena**. En los demás
opcodes con tipo de cadena (`PRIVATE.string`, `GET_PRIV.string`...) el operando
es un *offset de variable*, y confundirlo con un índice daba por vivas
justo las cadenas cuyo offset era múltiplo de 4.

### Resultado sobre el juego de prueba (170k líneas)

Recompila entero. **357 de 357 procesos**; locales, varspaces, `SLocal` e
imports coinciden exactamente. Las globales salen +32, que son justo los
`UNRESOLVED_*` que aún no resuelvo.

El tamaño de código por proceso no coincide al byte, pero todas las diferencias
son negativas y la mayoría se explican: varios procesos llevan al final una
tirada de `PUSH` que nadie consume (57 seguidos en `MAIN`), relleno que el
compilador no regenera.

Este control ya encontró un fallo real: los literales enteros de operaciones
float se reinterpretaban como bits IEEE y salían denormales (`10000` ->
`1.4013e-41`). BennuGD1 no empuja floats como bits crudos, usa `INT2FLOAT`.

### Resultado actual (tras corregir valores iniciales y A2STR/STR2A)

Con el `bgdc`/`bgdi` de 32 bits construidos por `construir_bennugd1_32.sh`:

| tabla        | original | recompilado |
|--------------|---------:|------------:|
| `NProcs`     |      357 |     358 (*) |
| `NLocVars`   |       28 |          28 |
| `NVarSpaces` |       16 |          16 |
| `SLocal`     |      220 |         220 |
| `NImports`   |       31 |          31 |
| `NGloVars`   |     1190 |        1190 |
| `NStrings`   |      613 |         613 |

(*) el proceso de más es `bgd1_init_globals()`, que inyectamos nosotros.

Parecido medio del flujo de opcodes: **0.988**; instrucciones sin traducir: **0**.

Y la prueba de comportamiento: ejecutando el `.dcb` original y el recompilado
con el mismo estado, la **secuencia ordenada de 1999 aperturas de fichero
(1035 assets distintos) es idéntica**.

#### Tres fallos que encontró este control

1. **Los valores iniciales sólo se emitían al portar a BennuGD2.** En modo
   BennuGD1 se perdían los de globales, privadas y públicas: 613 valores a
   cero. Se veía en `RUTA_PAL`/`RUTAX`, que sólo se asignan dentro de
   condicionales, así que el juego buscaba las paletas en `//galsia.pal` en vez
   de en `palettes/enemies/galsia.pal` y moría al primer intento.

2. **`STR2A` se descartaba como si fuera una conversión implícita**, pero
   asigna por sí misma (destino en `[-2]`, cadena en `[-1]`, sin `LET`):
   cada `SAVEGAME.PERFIL_NOMBRE1 = PERFIL_NOMBRE[0];` desaparecía del fuente.
   Su simétrica `A2STR` dejaba en la pila la dirección del primer elemento, y
   salía `NAME_R1 = SAVEGAME.NAME_R1[0];` (un byte) en vez del array entero.

3. **Los `UNRESOLVED_GLOBAL_*` no eran variables.** Son el descriptor de tipos
   que el compilador genera para `load`/`save` (viven en la zona del espacio
   global que va más allá de las variables declaradas). `bgdc` los regenera
   solo, así que basta con no declarar los que el código no usa; eso dejó
   `NGloVars` clavado.

   Medido sobre el juego: **32 offsets, 32 usos, y los 32 los consume `save` (9)
   o `load` (23)**; ninguno otro. Por eso el contador sigue apareciendo en el
   resumen: si en otro juego alguno de esos offsets *sí* aparece en el código,
   entonces sí es una variable que no supimos resolver, y hay que mirarlo.

#### Nota sobre `MAIN`

El bloque de `MAIN` en el `.dcb` original ocupa 120 words pero sólo 6 son
alcanzables (`PROC MENU; PUSH 100; FRAME; END`); el resto son datos que `bgdc`
no regenera. Comparar `SCode` de `MAIN` da un falso positivo.

#### Hay que compilar contra SDL 1.2 AUTENTICA, no contra sdl12-compat

Ubuntu 24.04 ya no empaqueta la SDL 1.2 de verdad: `libsdl1.2debian` es la
version **1.2.68**, que es `sdl12-compat` (SDL 1.2 emulada sobre SDL2) con el
nombre heredado. La SDL 1.2 autentica acabo en la 1.2.15.

Compilar BennuGD1 contra la emulacion produce **dos fallos que no son del motor
ni del descompilador**, y que desaparecen solos al usar la SDL de verdad:

| sintoma | con sdl12-compat | con SDL 1.2 real |
|---------|------------------|------------------|
| `set_icon()` -> SIGSEGV en `gr_set_icon` | 10/30 arranques buenos | **8/8** |
| escalado (`scale_mode`) al rehacer el modo de video | imagen sin escalar en la esquina | **correcta** |

El primero es porque `SDL_CreateRGBSurfaceFrom()` devuelve NULL en la emulacion
y `gr_set_icon()` no lo comprueba. El segundo se ve con un `.prg` de 20 lineas,
sin el juego de por medio: con la emulacion, cambiar `scale_mode` y volver a
llamar a `set_mode()` deja la imagen a 1:1 arriba a la izquierda; con SDL 1.2
real el resultado es identico al del arranque limpio.

`construir_bennugd1_32.sh` compila SDL 1.2.15 desde
<https://github.com/libsdl-org/SDL-1.2> y **no parchea el motor**.

#### La escala del juego

`scale_resolution` (formato `ancho * 10000 + alto`) se puede fijar tambien por
variable de entorno, y tiene prioridad sobre lo que ponga el programa:

    SCALE_RESOLUTION=9600720 bgdi juego.dcb    # 960x720


## Verificar el portado a BennuGD2

    python3 verificar_portado.py original.dcb portado.dcb [PROCESO]

Compara el `.dcb` original de BennuGD1 con el `.dcb` del portado. A diferencia
de la comparación de assets (que sólo ve el código que llega a ejecutarse), ésta
recorre **todos** los procesos.

Los dos motores no comparten tabla de opcodes, así que antes se normaliza lo que
es consecuencia esperada del portado y no un fallo: los sufijos de tipo (v2 tiene
DOUBLE y v1 no), las conversiones que v2 inserta sola, las llamadas (un sysproc
de v1 puede acabar siendo una función del programa en v2, por los shims) y el
`END` final de más que emite el `bgdc` de BennuGD2.

Da dos números:

- **Flujo completo**: incluye las expresiones. Baja bastante y no es alarmante,
  porque el portador reescribe `X -= 50` como `X = (INT)(X - 50)` — x/y/z eran
  enteras en BennuGD1 y son reales en BennuGD2, y el código de v1 depende de que
  trunque. Eso cambia la forma del bytecode sin cambiar el comportamiento.
- **Esqueleto de control**: sólo saltos, llamadas, bucles y salidas. Es inmune a
  cómo se reescriban las expresiones y es lo que decide si el programa toma las
  mismas decisiones. **Éste es el número que hay que mirar.**

### Resultado sobre el juego de prueba

| medida | valor |
|---|---|
| procesos no portados | **0** de 357 |
| esqueleto de control, media | **0.9914** |
| procesos con control idéntico | **230** de 357 |
| procesos con control >= 0.99 | 288 |
| flujo completo, media | 0.9111 |

Como referencia, el mismo juego recompilado con el `bgdc` de **BennuGD1** da 0.9880
de flujo completo: esa es la cota que marca el descompilador, y la diferencia con
el 0.9111 del portado es la reescritura de expresiones, no pérdida de código.

Aviso para quien toque esto: **no se puede truncar el flujo en el primer `END`**
dándolo por final de proceso. `END` aparece legítimamente a mitad; en este juego
`ENEMIGO` tiene 171.807 opcodes después de su primer `END`, con 17.071 de control.

### Paletas: intercambio por shader (fase visual)

BennuGD2 entrega los gráficos ya convertidos a RGBA, así que no hay índices de
paleta que intercambiar. La solución, sin tocar el motor:

1. `load_fpg` guarda además la paleta ORIGINAL del fichero (`BGD1_BASE[file]`).
2. Cada paleta se vuelca a un gráfico de 256x1 (`map_new` + `map_put_pixel`).
3. Un fragment shader recibe las dos paletas como texturas, busca el color de
   entrada en la original y saca el de la misma posición en la destino.
4. Tras cada asignación a `PALETTE` se sincroniza `shader`/`shader_params`.
   Tiene que ir **en línea**: una `FUNCTION` no puede tocar las locales de quien
   la llama. `PALETTE` es la única local de BennuGD1 con efecto de render, y en
   BennuGD2 `shader` también es local por proceso, así que mapean una a una.

Dos detalles que cuestan de encontrar:

- Los nombres de atributo/uniform que exige el motor son **`bgd_Vertex`,
  `bgd_TexCoord`, `bgd_Color`, `bgd_ModelViewProjectionMatrix`** (ver
  `shader_create()` en `modules/libbggfx/g_shaders.c`), **no** los `gpu_*` por
  defecto de SDL_gpu: con los `gpu_*` el shader compila sin error y no pinta nada.
- Nunca dejar `shader` puesto con `shader_params` nulo: hay que comprobarlo y
  poner `shader = NULL` si no hay paleta válida.

### `fade()`: no es una equivalencia directa

En BennuGD1 `fade(r,g,b,speed)` ajustaba el **brillo de la paleta**: r,g,b van de
0 a 200 y **100 es el brillo normal**, así que `fade(100,100,100,v)` significa
"no cambies nada". En BennuGD2 `fade()` pinta un **overlay de color con alpha**.

Traducirlo como `alpha = 255 - media` (lo que hacía la primera versión del shim)
pintaba un velo gris al 61% sobre toda la pantalla de forma permanente. La
traducción correcta: por debajo de 100 se oscurece con un velo negro, por encima
se aclara con uno blanco, y en 100 el alpha es 0. La duración también cambia:
`speed` era un incremento por frame sobre 100 pasos, o sea unos `4000/speed` ms.

## Literales float: el fallo más caro de encontrar

BennuGD1 empuja un literal float con un **`PUSH` SIN TIPO**: el sufijo `.float`
va en la OPERACIÓN que lo consume, no en el `PUSH`. Hay que reinterpretar esos
bits como IEEE-754.

Pero un literal ENTERO usado en contexto float lleva un `INT2FLOAT` explícito, y
ése no se debe reinterpretar (hacerlo convertía `10000` en el denormal
`1.4013e-41`, que además BennuGD ni siquiera sabe parsear porque sale en
notación científica).

El detalle que lo hace difícil: **el operando de `INT2FLOAT` es la PROFUNDIDAD en
la pila del valor afectado, no siempre la cima**. En

    PUSH 1
    GET_PRIV TIPO
    PUSH 1045220557        ; bits de 0.2
    GET_PRIV TIPO
    PUSH 1028443341        ; bits de 0.05
    INT2FLOAT 1            ; convierte TIPO, que esta UN NIVEL POR DEBAJO
    MUL.float

la conversión no toca el literal de arriba. Marcar como "ya convertido" el
elemento de la cima rompe el caso; hay que usar el operando como índice.

Medido sobre el juego de prueba: 110 `PUSH` crudos consumidos por operaciones
`.float` frente a 19 con `INT2FLOAT` delante.

Qué se veía cuando esto estaba mal: `TREN_SPEED -= 1036831949` en vez de `-= 0.1`
y `INTRO_Y = 1063675494` en vez de `= 0.9`, así que **los gráficos de la intro
(el tren, el fondo con el villano) se iban de la pantalla al primer frame**. Ni
la comparación de assets ni la de opcodes lo detectan: los ficheros se cargan y
el flujo de control es idéntico; sólo cambian los valores.

## Fallos que aparecieron al probar con otros `.dcb`

Reproducidos con un programa de prueba compilado con los dos motores, no con el
juego grande. Los tres primeros salian con `.dcb` de **BennuGD2**.

### `&&`/`||` mal reconstruidos en `.dcb` de BennuGD2

Salia esto:

    IF ((D[a][b] == 255 && 1 || D[a][b] == 254) && 1 || D[a][b] == 0)
    IF (D[a][b] >= 1 && (? && D[a][b] < mejor) && (? && ...))
    // ?? JUMP 70

El reconocimiento del cortocircuito estaba **restringido a BennuGD1**
(`if self.v1:`), y en BennuGD2 caia por una rama que fabricaba `A && ?`. Los dos
motores usan el mismo idioma: `JTFALSE` deja el valor en la pila y el `AND`/`OR`
posterior combina los operandos; el `||` añade el atajo `POP; PUSH 1; JUMP`.
Saltarlo y dejar que el `AND`/`OR` haga su trabajo da la expresion correcta.
Aplicarlo tambien a v2 **mejoro de paso el round-trip de BennuGD1**: 0.988 -> 0.992.

### Campos de struct tras un indice dinamico

    paths[i].len = 0;        ->   PATHS[I].MOVES[80] = 0;      (mal)

El compilador emite `GLOBAL base; ARRAY <stride>; INDEX <offset del campo>`, y el
`INDEX` se estaba tratando como indice de array (dividiendo por el tamaño del
elemento) en vez de como desplazamiento a campo. Ahora la expresion de direccion
arrastra el tipo (`chunks`, `members`, nivel) y el `INDEX` resuelve el camino real.

### `import` no se emitia con `.dcb` de BennuGD2

Los modulos se filtraban por el prefijo `mod_`, pero BennuGD2 los llama
`libmod_*`: no se emitia ningun `import` y el fuente no compilaba. Ahora se
aceptan los dos prefijos; el resto (`libbggfx`, `libbgload`...) son librerias
internas y siguen anotadas como dependencias.

Con esto, el round-trip de un `.dcb` de BennuGD2 da **1.0000** en las dos metricas.

### `RETURN;` de mas

El bytecode termina todo proceso con `END`, que se traduce como `RETURN;`. Si el
cuerpo ya acababa en un `RETURN` explicito, ese segundo sobraba.

### Lo que NO se puede recuperar

Las constantes (`#define`, `_TARGET`, `false`...) no estan en el `.dcb`: quedan
plegadas a su valor. Es inherente al formato, no un fallo.

## Codificación de las cadenas: cp850, no latin-1

El `.dcb` guarda las cadenas en la **página de códigos DOS (cp850)**, y el `bgdc`
de BennuGD convierte el fuente (latin-1) a cp850 al compilar. Medido con un
fuente de prueba:

| byte en el fuente | byte en el `.dcb` |
|---|---|
| `0xD1` (`Ñ` en latin-1) | `0xA5` |
| `0xF1` (`ñ` en latin-1) | `0xA4` |

Leer las cadenas como latin-1 y volver a escribirlas hacía que el byte pasara
por la conversión **una segunda vez**: `", FELIZ CUMPLEAÑOS !"` volvía a
compilarse como otra cosa. Y si además la salida se escribía en UTF-8 (lo que
hace `print()` con la configuración regional habitual), cada byte alto se
convertía en dos y la corrupción era doble.

La cadena hay que leerla como **cp850** y escribir el `.prg` en **latin-1**. Con
eso, las 613 cadenas del juego de prueba coinciden con las del original salvo la
fecha y la hora de compilación, que el propio juego incrusta.

Esto importa más de lo que parece: no es sólo texto en pantalla, también afecta a
cualquier comparación de cadenas del juego que lleve acentos.
