# Primeros pasos {#start}

Pragma Chess guarda tus partidas en **bases de datos** y te permite estudiarlas en el tablero con un motor, un libro de aperturas y tus propias notas.

En cada inicio se abre la **ventana de bienvenida** sobre la ventana principal: lo que Pragma Chess sabe hacer, y luego tus **proyectos** y tus **bases de datos**, leídos de sus carpetas (un archivo que añadas allí aparece). Haz clic en un proyecto para abrirlo, en una base de datos para abrirla en un proyecto nuevo, o en **Nuevo proyecto**. Marca **No mostrar esta ventana al iniciar** para empezar directamente; **Ayuda ▸ Bienvenida…** la vuelve a abrir cuando quieras.

La ventana tiene el **tablero** en el centro y cuatro paneles a su alrededor:

- **Partidas**, abajo: el árbol de la base de datos abierta y la lista de sus partidas.
- **Jugadas**: las jugadas de la partida del tablero.
- **Árbol de aperturas**: las jugadas del libro de aperturas para la posición del tablero.
- **Motor**: la evaluación, la mejor línea y la apertura en la que está la partida.

La **barra de herramientas** tiene Sincronizar ahora, Guardar proyecto, Nueva partida, Nuevo entrenamiento, Nueva partida en línea, Entrar en el lobby (la copa de martini) y tres iconos para elegir el libro de aperturas, el motor y la base de datos en uso.

Todo lo que ves — base de datos, partida, jugada, paneles — es un **proyecto**: vuelve tal como lo dejaste la próxima vez que inicies Pragma Chess.

Tus archivos están en la carpeta Pragma dentro de tu carpeta de ajedrez (por ejemplo `Ajedrez/Pragma` en tu carpeta personal), con `Databases`, `Projects` y `Books`. El primer inicio pone allí una base de datos de partidas clásicas.

**Opciones ▸ Ajustes personales…** dice quién eres: tu nombre, tu año de nacimiento y tu ID FIDE. Tu nombre va en tu lado de una partida nueva, de una partida de Preparar posición (en el lado de abajo del tablero) y de una partida de entrenamiento (en el color que elegiste), a menos que la base de datos abierta ya te conozca: gana un jugador marcado como Yo con **¿Quién es?**. Hasta que des un nombre, Pragma Chess te da uno, el de un campeón con tres cifras, como *Spassky007*, distinto para cada uno, para que el lobby no se llene de jugadores con el mismo nombre: lo encuentras escrito aquí, para conservarlo o cambiarlo. **Estilo del tablero** elige juntos los colores de las casillas y las piezas: Pragma Classic (casillas marrones y las piezas de los libros de ajedrez), Lichess Alpha (el tablero verde y las piezas Alpha de lichess.org) o Classic Book (el diagrama de un libro de ajedrez impreso: papel antiguo, las casillas oscuras rayadas en tinta, un margen de papel alrededor de cada pieza que esté sobre ellas, y sin coordenadas); todos los tableros cambian en cuanto pulsas Aceptar. Estos ajustes se guardan en `.pragma-chess.conf`, un archivo YAML de tu carpeta Pragma, que la sincronización lleva a tus otros ordenadores aunque esté oculto; si se cambia en dos ordenadores a la vez, gana el más reciente.

**Opciones ▸ Ajustes de carpetas…** los pone en otro lugar de este ordenador: la propia carpeta Pragma, o solo las bases de datos, los proyectos, los libros o los nombres de aperturas. Deja una carpeta vacía para mantenerla en su lugar habitual. Los archivos que ya están allí no se mueven, y las carpetas nuevas se usan la próxima vez que inicies Pragma Chess. La sincronización mantiene iguales en tus otros ordenadores solo los archivos que están dentro de la carpeta Pragma.

Una pregunta, una idea, algo que enseñar: **Ayuda ▸ Preguntas e ideas…** abre las Discussions del proyecto en GitHub. Algo que no funciona: **Ayuda ▸ Informar de un problema…**.

# Bases de datos {#databases}

Una base de datos es un archivo `.pdb` que contiene partidas. Hay una base de datos abierta cada vez; su nombre está en la descripción emergente del icono de base de datos de la barra de herramientas. En tu gestor de archivos una base de datos tiene el icono de documento de tu sistema con una base de datos y un peón en la página; al abrirla se abre Pragma Chess con la base de datos en el proyecto que dejaste abierto.

- **Base de datos ▸ Nueva base de datos…** crea una vacía en la carpeta Databases.
- **Base de datos ▸ Abrir base de datos…** abre un archivo de cualquier lugar.
- **Base de datos ▸ Cambiar base de datos** muestra las bases de datos de la carpeta: elige una para abrirla. El icono de base de datos de la barra de herramientas despliega la misma lista.
- **Base de datos ▸ Ajustes de la base de datos…** (también desde el menú contextual de la base de datos, arriba del árbol) edita el nombre y la descripción, dice si la base de datos es una colección de partidas, un libro de aperturas o **Problemas y entrenamiento**, y tiene **Optimizar base de datos**. En una base de datos de Problemas y entrenamiento (como Endgame Training y Tactics Training) la lista de partidas no muestra las jugadas, para que la solución quede oculta, y una partida abierta desde ella empieza en **Modo entrenamiento**: el tablero se gira hacia el bando que mueve, que es el tuyo, y el motor juega con el otro. Cuando una partida no nombra a los jugadores, la lista y el tablero muestran tu nombre, el de **Ajustes personales**, en el bando que mueve y *Pragma Coach* en el otro: nombres provisionales, que no se escriben en la base de datos, y la lista también ordena por ellos. Del mismo modo, allí la columna Evento se llama **Theme** (tema) y muestra el tema de cada problema, el más significativo de los que le da lichess: *Ataque doble*, *Mate en 2*, *Final de torre*. Una base de datos con nombre se muestra con él, y el archivo entre paréntesis detrás — *Mis partidas (games.pdb)* — en **Cambiar base de datos** y arriba del árbol, donde el nombre va en negrita.
- **Base de datos ▸ Guardar base de datos como…** escribe una copia.
- **Base de datos ▸ Cambiar base de datos ▸ Mostrar carpeta de bases de datos** abre la carpeta en el gestor de archivos.

Los cambios en una base de datos se escriben a medida que los haces: no hay nada que guardar a mano.

Pragma Chess incluye dos bases de datos para entrenar, añadidas a la carpeta Databases: **Endgame Training** — los finales teóricos clásicos (los mates elementales, la oposición, Lucena, Philidor…) con su objetivo, y más de dos mil problemas de finales — y **Tactics Training**, problemas de veinte temas, de fáciles a difíciles. Los problemas vienen de la base de datos de problemas de lichess.org (libre, CC0); cada uno empieza una vez que el rival ha movido, y te toca a ti encontrar la respuesta: juégalo en **Modo entrenamiento**, o recorre la solución jugada a jugada. Las encuentras en el árbol bajo **Finales** y **Táctica**. Una base de datos de entrenamiento que borres no se vuelve a añadir.

# La lista de partidas {#games-list}

La lista muestra las partidas de la base de datos abierta, una por fila. Haz doble clic en una partida para ponerla en el tablero.

- Haz clic en el título de una columna para **ordenar** por ella; haz clic de nuevo para invertir el orden.
- Arrastra el título de una columna para **mover** la columna.
- Haz clic derecho en el título de una columna para **ocultar** esa columna, o para **mostrar** una oculta. Cada base de datos recuerda sus propias columnas.
- La última columna, **Línea**, muestra cómo empieza la partida; se corta con “…” donde acaba la columna.

Haz clic derecho en un jugador para decir **quién es**: tú, un amigo o un rival. El árbol muestra entonces esos jugadores bajo Yo, Amigo y Rival, tu nombre aparece en negrita en la lista, y una partida en la que juegas se abre con el tablero girado hacia tu lado.

Haz clic derecho en una partida para moverla a la **papelera**.

**Mover partida…**, en el mismo menú, lleva la partida a otra base de datos: una de tu carpeta Databases, o una nueva (elige **Nueva base de datos…** y ponle un nombre). Con varias partidas seleccionadas (Mayús o Ctrl y un clic) las mueve todas. Cada partida llega entera, con sus variantes y comentarios, y solo entonces sale de esta base de datos. Marca **Abrir la base de datos al terminar** para ir allí después.

Para cambiar los jugadores, el evento, la fecha o el resultado de la partida del tablero, haz clic en el encabezado que hay sobre el tablero.

# El árbol de la base de datos {#database-tree}

A la izquierda de la lista de partidas, el árbol muestra lo que contiene la base de datos abierta. Selecciona un nodo y la lista muestra solo esas partidas; selecciona la propia base de datos para verlas todas. El árbol se queda como lo dejas: la próxima vez que se abra la base de datos, aquí o después de cerrar Pragma Chess, estarán abiertos los mismos nodos y seleccionado el mismo.

- **Tablero ▸ Posición**: las partidas en las que aparece la posición del tablero, sea cual sea el orden de las jugadas.
- **Tablero ▸ Variante**: las partidas que empiezan exactamente con las jugadas hechas en el tablero.
- **Yo**, **Amigo**, **Rival**: los jugadores que identificaste con “¿Quién es?”.
- **ECO**: las partidas por código de apertura.
- **Torneo/evento** y **Año**.
- **Ritmo de juego**: los ritmos a los que se jugaron las partidas, del más rápido — *Blitz 3+2*, *Rápidas 10+5*, *Clásicas 90+30*, *Por correspondencia 1 día*. Las partidas de lichess.org y chess.com traen el suyo; para cualquier otra partida, haz clic en los nombres de los jugadores sobre el tablero y escríbelo en **Ritmo de juego**, como lo dicen los jugadores (*3+2*: tres minutos, más dos segundos por jugada).
- **Estudio**: los estudios de los que vienen las partidas — un estudio de lichess conectado como fuente, o partidas cuyo PGN lleva las etiquetas `StudyName` y `ChapterName` —, cada uno con sus capítulos en el orden del estudio. Selecciona un estudio para todas sus partidas, o un capítulo para las suyas.
- **Fuente**: las partidas que vinieron de lichess.org, chess.com o torneionline.com.
- **Finales**: las partidas que empiezan desde un final, por familia — *Mates elementales*, *Finales de peones*, *Finales de torre*… — y, dentro de cada una, por material: *R+T+P contra R+T*, el bando más fuerte primero.
- **Táctica**: los problemas por tema — *Clavada*, *Enfilada*, *Ataque doble*, *Ataque a la descubierta*, *Desviación*…
- **Papelera**: las partidas que has tirado.

Posición y Variante siguen al tablero: recorre una partida y sus recuentos cambian. Haz doble clic en un nodo con una sola partida y esa partida se abre.

# El tablero {#board}

Mueve una pieza arrastrándola, o haciendo clic en ella y luego en su casilla. Un peón que llega a la última fila pregunta a qué pieza coronar.

- **Izquierda** y **Derecha** van una jugada atrás y adelante, **Inicio** y **Fin** al principio y al final. Hacer clic en una jugada del panel Jugadas lleva allí.
- **Ver ▸ Girar el tablero** (Ctrl+R) da la vuelta al tablero; **Ver ▸ Mostrar coordenadas** muestra u oculta las letras y los números.
- **Opciones ▸ Ajustes gráficos…** elige la **Apariencia** — Seguir el sistema (la opción por defecto), o siempre Claro o siempre Oscuro, diga lo que diga el sistema —, dónde se muestran las piezas capturadas, si mostrar a quién le toca jugar y si una jugada se oye: **Sonido al mover una pieza** reproduce el golpe de una pieza que se posa en el tablero para tus jugadas, las del motor en el entrenamiento y las de tu rival en línea (las del motor y las del rival cuando llegan a su casilla). Estos ajustes se quedan en este ordenador: cada uno puede adaptarse a su propio escritorio.
- Cuando la posición del tablero es **jaque mate**, el borde del tablero se vuelve rojo.

**Partida ▸ Nueva partida** (Ctrl+Shift+N) empieza una partida para introducirla jugada a jugada. **Partida ▸ Guardar partida en la base de datos** la guarda en la base de datos abierta; **Partida ▸ Guardar partida en otra base de datos…** la guarda en una base de datos que eliges, que no se abre.

Si haces una jugada que no es la siguiente de la partida, se te pregunta qué es: **Insertar como variante** conserva la línea de la partida, y la nueva aparece en el panel Jugadas bajo la jugada que sustituye; **Reemplazar línea principal** (o **Reemplazar línea**, en una variante) borra las jugadas que la siguen y continúa con las tuyas. Durante el entrenamiento o el juego en línea no se pregunta nada: una jugada distinta es una variante. Una partida guardada en la base de datos se guarda en el acto, variantes incluidas.

**Partida ▸ Preparar posición…** abre un tablero en el que dibujar una posición: elige una pieza a la derecha y haz clic en las casillas para colocarla (un clic sobre la misma pieza la quita), arrastra una pieza para moverla a cualquier lugar del tablero (si la sueltas fuera del tablero, vuelve a su sitio), haz clic derecho en una casilla para vaciarla. Indica el bando que mueve, el enroque, la captura al paso y el número de jugada, o escribe una FEN; **Posición inicial**, **Vaciar tablero** y **Girar el tablero** ayudan. Aceptar solo se activa para una posición desde la que puede empezar una partida, y el texto bajo el tablero dice qué falla. La posición empieza una partida nueva al final del capítulo, así que la partida en la que estabas se queda como está. Una partida en línea en curso se conserva o se abandona antes.

**Editar ▸ Copiar** pone en el portapapeles las jugadas, la partida en PGN, la posición en FEN (Ctrl+Shift+C), la línea del motor o la explicación. **Editar ▸ Pegar** toma lo que hay en el portapapeles: **Pegar FEN** (Ctrl+Shift+V) prepara la posición, **Pegar línea** empieza una partida nueva con las jugadas (PGN, jugadas con o sin números, o UCI), y **Pegar línea desde la posición actual** (Ctrl+Alt+V) hace las jugadas desde la posición del tablero, como si las jugaras tú: al final de la partida se añaden, en otro punto se convierten en una variante.

**Editar ▸ Apuntes…** guarda cosas bajo un nombre: cada cajón tiene un nombre y su contenido — jugadas, una variante, una posición FEN, una nota. **Nuevo cajón** añade uno, **Eliminar** quita el seleccionado; Aceptar guarda los cambios, siempre que cada cajón tenga nombre y no haya dos con el mismo. Los cajones son tuyos, se guardan con tus ajustes personales, así que son los mismos en todos los ordenadores sincronizados. Pronto podrás meter y sacar cosas rápidamente.

# Jugadas y anotaciones {#moves}

El panel Jugadas muestra la partida del tablero, con las piezas dibujadas como figuras. Haz clic en una jugada para ir a ella.

La línea principal es una tabla, una jugada por celda: haz clic en la celda para ir a la jugada. Las **variantes** ocupan una fila propia bajo la jugada que sustituyen, más pequeñas, como texto; las variantes de una variante la siguen entre paréntesis. Haz clic en una jugada de una variante para seguir esa línea: las flechas avanzan entonces por ella, y hacer clic en una jugada de la línea principal te devuelve a ella. Al principio de una variante, jugar la jugada de la línea principal vuelve a la línea principal, y otra jugada empieza una variante hermana. Las variantes se guardan con la partida y viajan en el PGN que copias.

Una partida escrita a mano puede tener una **jugada nula**, escrita `--`: el bando que mueve pasa, cuando no se conocía la jugada o para mostrar un plan (ChessBase y PGN la escriben así). Se muestra y se copia como `--`, las flechas pasan por ella como por cualquier otra jugada, y el motor analiza la posición que la sigue. Explicar no tiene nada que decir de una jugada nula; nunca se juega en el entrenamiento, en línea ni en el lobby.

Haz clic derecho en una jugada para ver su menú:

- **Copiar ▸ Copiar jugada** y **Copiar ▸ Copiar línea hasta aquí** ponen en el portapapeles la jugada, o la partida hasta ella.
- **Anotaciones** muestra los símbolos con lo que significa cada uno.
- **Variantes**: en una jugada de una variante, **Promover variante** la convierte en la línea de la que cuelga (las jugadas que sustituía pasan a ser su variante, con las suyas) y **Eliminar variante** la quita; en cualquier jugada, **Eliminar desde aquí** la quita junto con las jugadas que la siguen en su línea, con sus variantes. Eliminar pregunta antes: no se puede deshacer.

Una jugada puede llevar un juicio sobre la jugada y una valoración de la posición:

- `!!` jugada brillante, `!` buena jugada, `!?` jugada interesante, `?!` jugada dudosa, `?` error, `??` error grave, `□` jugada única.
- `+−` las blancas están ganando, `±` las blancas están mejor, `⩲` las blancas están ligeramente mejor, `=` igualdad, `∞` posición incierta, `⩱` `∓` `−+` lo mismo para las negras.

Elige el símbolo que ya tiene una jugada para quitarlo, o **Sin anotación** para borrarlos todos. Las anotaciones se guardan con la partida en el acto y se escriben cuando copias la partida en PGN.

Los **comentarios** — el texto entre las jugadas de una partida PGN o de un estudio de lichess — se muestran en cursiva: bajo su jugada en la línea principal, entre las jugadas en una variante. Las órdenes que los programas ponen en los comentarios (una evaluación `[%eval 0.18]`, un reloj, flechas) se conservan con la partida pero no se muestran. Los comentarios se guardan en la base de datos con la partida y se vuelven a escribir cuando la partida se escribe en PGN.

Haz doble clic en un comentario para escribir en él, ahí mismo, como se escribe un párrafo (Esc, Ctrl+Enter o un clic en otro lugar terminan; si se vacía, desaparece). Haz clic derecho en una jugada para **Añadir comentario** o **Editar comentario**, y en un comentario para **Editar comentario** y **Eliminar comentario**. Las órdenes que contiene se quedan como estaban. Las jugadas escritas en un comentario — "4.d4 cxd4 era mejor", "luego Dg5" — son enlaces: haz clic en una y su línea se juega en el tablero como variante de la partida (o se toma la variante que ya es), y después se sigue con las flechas como cualquier otra. Una jugada con número empieza desde esa jugada de la línea; una sin número continúa desde el comentario, o sustituye a la jugada comentada. Los círculos y las flechas que un estudio de lichess dibuja en las casillas (`[%csl …]`, `[%cal …]` en el comentario) se dibujan en el tablero, en sus colores, mientras se muestra la posición de su comentario.

# Explicar {#explain}

**Explicar** muestra en el tablero por qué la última jugada es buena o mala. Pulsa el botón entre las flechas bajo el tablero, o la tecla **E**.

El motor mira la posición antes y después de la jugada y dibuja flechas:

- flechas **rojas**: hay material a punto de caer, y las piezas que se pierden se rodean con un círculo;
- flechas **verdes**: las ideas de tu bando, las jugadas que justifican la evaluación;
- flechas **azules**: las jugadas del otro bando — la respuesta que marca la diferencia cuando todavía no se pierde nada, o su plan; *tu bando* es el que juegas en el entrenamiento, y si no el que está abajo en el tablero;
- flechas **rojas discontinuas**: una amenaza, una pieza que queda atacada — "5.Dxf3 ataca la torre de a8", o la que tu jugada no ha atendido, "5…Dh4+ deja atacada la torre de a8: 7.Dxa8";
- una flecha **verde discontinua**: la mejor jugada que tenías en su lugar. Cuando mueve la misma pieza que moviste — así que la casilla de la que sale está ahora vacía, porque esa pieza se fue a otro sitio —, la pieza se dibuja pequeña y tenue donde debería haber llegado.

Un mate forzado se reproduce en el tablero, dentro de un marco rojo. El panel Motor lo dice con palabras, por ejemplo “Error grave (+0.3 → −2.9). Las negras ganan un caballo”.

Explicar lee el análisis que se está ejecutando en el panel Motor: la explicación aparece en cuanto el motor ha buscado a suficiente profundidad, y se afina a medida que profundiza — las flechas solo cambian cuando una línea nueva se mantiene durante un rato. La jugada se juzga respecto a la posición anterior: si esa posición nunca se analizó (llegaste directamente a la jugada, o volviste a abrir Pragma Chess en ella), el motor la mira primero, un momento, y luego sigue con la posición del tablero. Hasta la primera respuesta el borde del tablero respira; se vuelve azul cuando la explicación está lista. Explicar trata de una jugada: ir a otra jugada lo desactiva, y vuelves a pedirlo. Si lo cierras con Explicar activado, Pragma Chess se abre con él activado, en la misma jugada.

**Editar ▸ Copiar ▸ Explicación** copia el texto.

# Descubrir las funciones {#discovery}

Pragma Chess señala lo que puede ayudar, cuando puede ayudar: un bocadillo sobre el control, la primera vez. Aparece como mucho **una vez por sesión**, y **No volver a mostrar esto** en el bocadillo lo guarda para siempre.

- **Explicar** se señala tras una jugada hecha en el tablero que parece una prueba: una que el motor juzga un error — con el análisis activado, o cuando ya conoce ambas posiciones — o una fuera de las jugadas que ofrece el Árbol de aperturas, fuera de las aperturas que conoce el libro. No en el entrenamiento, que tiene su tutor, ni jugando en línea, ni mientras Explicar ya está activado.

# Motores {#engine}

**Motor ▸ Análisis** (Ctrl+E) activa y desactiva el análisis de la posición del tablero. El panel Motor muestra la puntuación — siempre desde el lado de las blancas: positiva es buena para las blancas —, la profundidad y la mejor línea. La barra junto al tablero muestra la misma puntuación. Haz clic en la puntuación para mostrarla de otra forma, por turnos: absoluta (desde el lado de las blancas, como la da el motor), para el bando que mueve, las posibilidades del bando que mueve, y el juicio que imprime un libro de ajedrez (=, ⩲, ±, +− y sus opuestos, desde el lado de las blancas). El punto delante del valor es su unidad: mitad blanco y mitad negro para el lado de las blancas, el color del bando que mueve para el suyo; los símbolos del libro no llevan ninguno. Las posibilidades son la puntuación esperada en porcentaje: lo que ese bando se lleva de media de una posición así, contando 1 una victoria y ½ unas tablas — 50 % es igualdad, un peón de más alrededor de 59 %, tres peones alrededor de 75 % (la curva que usa lichess, y la barra junto al tablero). La descripción emergente da las cuatro a la vez; la elección se guarda con el proyecto. Junto a la puntuación, en el mismo recuadro, el curso de la partida: un punto por cada jugada cuya evaluación se conoce (lo que el motor encontró allí, o las evaluaciones de la propia partida, como las escribe lichess), unidos por una línea, sobre la línea central de una partida equilibrada. La altura crece con la ventaja, pero más despacio que ella: medio peón ya se nota, un peón está cerca de la mitad, dos están a menos del doble de distancia, diez y un mate llegan al borde. La ventaja de las blancas va hacia el lado de las blancas del tablero, como en la barra, así que el gráfico gira con el tablero; la jugada del tablero está marcada. Haz clic en un punto para ir a esa jugada. Veinte jugadas llenan el ancho; una partida más larga se comprime en él, nunca se desplaza.

**Para ver adónde lleva la línea**, mantén pulsado el ojo junto a Detener: mientras lo mantienes, el tablero muestra la posición al final de la línea del motor, como si se hubieran hecho todas sus jugadas, y sigue al motor: cuando el motor encuentra otra línea, el tablero pasa a donde termina la nueva. Suéltalo y vuelve tu posición. Sobre esa posición, flechas violetas muestran los planes que contiene la línea: cómo viajó una pieza para llegar allí (la ruta de un caballo, una torre que se eleva, la dama que cambia de flanco), el rey que marcha en un final, un peón que corre o que rompe. Una flecha que gira pasa por las casillas donde la pieza se detuvo. Los cambios y las jugadas forzadas por un jaque no son planes, así que una línea hecha solo de ellos no muestra flechas.

Pragma Chess incluye Stockfish, y funciona con cualquier motor UCI.

- **Motor ▸ Cambiar motor** elige entre los motores de este ordenador. El icono de motor de la barra de herramientas despliega la misma lista.
- **Motor ▸ Gestionar motores…** añade, edita y quita motores. **Detectar motores** encuentra los que están instalados en el ordenador; **Usar este motor** cambia al seleccionado.
- **Potencia de cálculo**, en la misma ventana, es cuánto del ordenador puede usar el motor mientras analiza, desde **Mínima** (3 % del procesador) hasta **Total** (todo, sin límite); **Media**, 10 %, es donde empieza cada motor. El motor es igual de fuerte en cada nivel, solo más lento: recibe menos hilos, una prioridad más baja y, en Linux y Windows, un tope a su parte del procesador, así los ventiladores se quedan tranquilos y el resto del ordenador queda libre. Los **hilos**, si los fijas, mandan sobre los del nivel.

**IA**: el botón con las estrellitas, a la izquierda del ojo, abre un chat con ChatGPT sobre la posición del tablero, pagado con tu propio plan de ChatGPT, sin clave de API. La primera vez despliega **Iniciar sesión con ChatGPT**: el navegador muestra la página de OpenAI y, una vez dentro, el botón pasa a ser un interruptor que enciende y apaga el chat. El chat ocupa el panel entre la línea del motor y la apertura; cada pregunta se envía con la posición, las jugadas que llevaron a ella y la evaluación del motor. Con el botón derecho sobre el botón está **Cerrar sesión de ChatGPT**; si se pierde la conexión, el botón te pide iniciar sesión de nuevo. La sesión se queda en este ordenador y nunca se sincroniza.

El motor en uso forma parte del proyecto.

# Entrenamiento {#training}

**Partida ▸ Nuevo entrenamiento…** (Ctrl+Shift+T) empieza una partida contra el motor. Elige Blancas, Negras o Al azar y pulsa Empezar.

Mientras te toca mover, el motor oculta su mejor línea y muestra solo la puntuación. Cuando has movido, responde por sí solo, despacio, para que veas su jugada.

Mientras la posición esté en el **libro de aperturas**, el motor responde con una jugada del libro, eligiendo cada una tan a menudo como dice su peso: ajusta los pesos en el Árbol de aperturas para entrenarte contra las líneas que quieras, tantas veces como quieras. Fuera del libro juega su propia mejor jugada.

El **tutor** vigila tus jugadas. Cuando una es una **imprecisión**, un **error**, un **error grave** o una **oportunidad perdida**, el motor no responde; el borde del tablero se vuelve rojo, el panel Motor dice qué ha pasado y ofrece:

- **Deshacer jugada**: volver a la posición y probar otra jugada;
- **Explicar**: el mismo Explicar que el botón bajo el tablero, que se activa y desactiva junto con él: muestra en el tablero por qué la jugada es un error;
- **Ignorar**: conservar la jugada, y el motor responde.

Puedes volver atrás en una partida de entrenamiento y hacer de nuevo una jugada, incluso donde la partida continúa después: una jugada que haces en el tablero es tu turno, así que el motor responde y el tutor la juzga (una respuesta distinta de la que ya había empieza una variante). Recorrer las jugadas con las flechas es solo mirar.

El jaque mate o el ahogado terminan la partida, que se guarda en la base de datos abierta.

Marca **Recordar para esta sesión** en la ventana Nuevo entrenamiento y el botón de la barra de herramientas empezará los siguientes entrenamientos con la misma elección, sin preguntar. El menú siempre pregunta.

**Motor ▸ Modo entrenamiento** activa o desactiva el entrenamiento para la partida del tablero. Abrir una partida desde la lista de partidas lo desactiva. Si cierras Pragma Chess mientras entrenas, vuelve a empezar en entrenamiento.

# Jugar en línea {#online}

**Partida ▸ Nueva partida en línea…** juega una partida contra una persona en lichess.org o en el Free Internet Chess Server, freechess.org. Lichess se conecta a través de su propia página de inicio de sesión en tu navegador; freechess.org pide en Pragma Chess el nombre y la contraseña de un jugador registrado, o te deja jugar como invitado — partidas no puntuables con un nombre que el servidor da en cada sesión. Una partida en freechess.org dura lo que dura la sesión: si cierras Pragma Chess durante ella, no puede volver a seguirla.

La ventana muestra las **plataformas a las que estás conectado**, cada una con la cuenta con la que juegas. **Conectar plataforma…** pregunta qué tipo de plataforma, abre su propia página de inicio de sesión en tu navegador y trae aquí la conexión; **Desconectar** quita una. Las conexiones son tuyas en este ordenador, se guardan con tus ajustes, nunca en un proyecto. Elige la conexión, el **reloj** (minutos e incremento), el color y si la partida es **puntuable**, y luego **Buscar un rival**.

Mientras Pragma Chess busca un rival y mientras juegas, está en **modo de juego en línea**, marcado en **Motor ▸ Modo de juego en línea** (en gris si no: el juego empieza desde **Partida ▸ Nueva partida en línea…**; elegirlo mientras está activo lo detiene): el motor, Explicar y el Modo entrenamiento están desactivados y no se pueden activar, y el Árbol de aperturas se queda donde lo pusiste pero no muestra jugadas, y su primera fila dice por qué — eres tú contra tu rival. El panel Motor muestra los dos relojes, como el reloj digital de un torneo: cada uno con su rey para su color y el tiempo en cifras grandes sobre una pantalla oscura, en rojo por debajo de veinte segundos y con décimas por debajo de diez; a la izquierda el color que está arriba en el tablero (gira el tablero y cambian de lugar), el que corre rodeado. Debajo, **Ofrecer tablas** y **Abandonar** (que pregunta antes): cuando tu rival ofrece tablas el primero pasa a ser **Aceptar tablas**, y mientras tu propia oferta espera dice **Tablas ofrecidas**; hacer una jugada rechaza una oferta. Ambos desaparecen cuando termina la partida. Debajo de los botones, a quién le toca mover. Tus jugadas van a la plataforma a medida que las haces; las de tu rival se deslizan sobre el tablero. Solo se puede jugar en la posición actual: puedes mirar jugadas anteriores, y volver al final para mover. Si Pragma Chess se cierra durante una partida, se vuelve a conectar a ella en el siguiente inicio y la partida sigue donde está (el reloj siguió corriendo en la plataforma); una partida que terminó entretanto se guarda con su resultado. Puedes cambiar de base de datos mientras juegas: la partida se queda en el tablero y, cuando termina, se guarda en la base de datos que esté abierta entonces.

Marca **Recordar para esta sesión** y el botón Nueva partida en línea de la barra de herramientas busca un rival con las mismas elecciones sin preguntar; el menú siempre pregunta.

lichess.org solo permite a aplicaciones como Pragma Chess partidas rápidas o más lentas — al menos 8 minutos, contando 40 jugadas de incremento, como 10+0 o 10+5 —: un ritmo más rápido se marca **Demasiado rápida para lichess.org** y no se puede pedir; freechess.org admite cualquier ritmo.

**Partida ▸ Nueva partida en línea…** durante una partida empieza una nueva partida en línea, **Partida ▸ Nueva partida** pregunta si quieres una nueva partida en línea (como Nueva partida en línea de la barra de herramientas) o una partida nueva para analizar — con **Recordar para esta sesión** no vuelve a preguntar hasta que se cierre Pragma Chess —, y **Partida ▸ Nuevo entrenamiento…** una partida contra el motor; cada uno pregunta antes si **seguir jugando** la partida actual o **abandonarla** (si todavía se está buscando un rival, la búsqueda simplemente se detiene). Elegir **Motor ▸ Modo de juego en línea** mientras está activo detiene la búsqueda, o abandona la partida después de preguntar. Cuando la partida termina — jaque mate, abandono, tiempo, tablas — se escribe el resultado y la partida se guarda en la base de datos abierta, con los jugadores, sus puntuaciones Elo y un enlace a la partida. Si la conexión se pierde en medio de una partida en línea en lichess.org, o Pragma Chess se cierra y se vuelve a abrir, vuelve a conectar con la partida por sí solo y sigues donde está — a pocos segundos, mientras lichess tenga la partida.

Mientras juegas, la puntuación y la evolución están ocultas: no dicen nada hasta que acaba la partida. Después Pragma Chess evalúa con el motor la partida que acabas de jugar, jugada por jugada, durante medio minuto — **Calculando…** junto al ojo dice cuánto le falta: la evolución junto a la puntuación del panel Motor se completa en seguida, y se vuelve más precisa a medida que llegan búsquedas más profundas. Cuando todas las jugadas están evaluadas, el recuadro de la puntuación — mientras el motor no analiza — muestra la **fuerza que mostró cada bando en esta partida**, un Elo estimado tras el punto de su color, en el orden del tablero: sale de cuánto perdió cada jugada frente a la evaluación del motor (pasa el puntero por encima para ver la pérdida media y la precisión). Con una sola partida es aproximado, y una partida corta se queda cerca de la de un jugador medio. Al elegir otro motor, la partida se vuelve a evaluar con él, desde el principio.

**Partida ▸ Entrar en el lobby…** abre el lobby de torneos sin reloj, jugados con otros usuarios de Pragma Chess en una red entre pares: no hay ningún servidor nuestro, cada ordenador guarda una copia del lobby y comprueba cada jugada, y unos relés públicos guardan las jugadas para quien no está. Cada sala es un torneo de cuatro jugadores, en el que todos juegan contra todos dos veces, una con cada color; empieza en cuanto se sientan dos jugadores, y el lobby siempre mantiene al menos dos salas con un asiento libre, ofreciendo una **sala nueva** cuando hay menos. Cada sala lleva el nombre de un tema de ajedrez y de un campeón, como La fortaleza de Capablanca; las llenas aparecen después de las demás, y sus partidas se pueden seguir. **Entrar en la sala** muestra su clasificación — puesto, victorias, tablas, derrotas y puntos, 1 por victoria y ½ por tablas, y luego los asientos libres —, sus partidas y, en un tablero pequeño, la posición de la partida que selecciones. **Tomar asiento** te sienta en un asiento libre: tus partidas con todos los que están allí aparecen en negrita, y las salas en las que te sientas van primero en el lobby. Juegas como mucho en dos torneos a la vez: un tercer asiento espera hasta que uno de los tuyos termine — todos los asientos ocupados y todas las partidas acabadas —, y la sala lo dice. Quien queda primero en un torneo gana una medalla, un punto amarillo detrás de su nombre allí donde el lobby lo muestre, para siempre: los jugadores empatados en el primer puesto reciben una cada uno, y la descripción emergente dice cuántos torneos ha ganado un jugador. Una sala con partidas que esperan tu jugada tiene **Jugar ahora** en la columna Te toca: con una partida te lleva directamente a ella, con varias pregunta cuál, por rival y color. En una sala, las partidas que esperan tu jugada están arriba de la lista, luego tus otras partidas, luego el resto. **Jugar tu jugada** lleva la partida al tablero, vista desde tu lado y en la jugada que hay que hacer, con la sala como evento; si la partida del tablero no está guardada en ningún sitio, primero se te pregunta si guardarla, dejarla ir o quedarte donde estás, y el lobby se aparta (Partida ▸ Entrar en el lobby… lo devuelve tal como estaba). La partida está entonces en **Modo lobby**, marcado en **Motor ▸ Modo lobby**: no se desactiva nada — el juego por correspondencia admite el motor y la preparación — y el panel Motor dice en qué punto está la partida, con dos botones. **Enviar jugada** envía la jugada que sigue al punto en que está la partida. **Enviar plan** la envía junto con las respuestas que has preparado: sigue jugando, moviendo también las piezas de tu rival, y pon sus otras respuestas posibles como variantes, cada una seguida de tu respuesta; cuando tu rival haga una de ellas, tu respuesta se juega en el acto. Tu rival también puede haber preparado respuestas: después de enviar ves las jugadas que han hecho los dos planes, hasta que a alguien le toque pensar. Un plan tiene una respuesta para cada posición, la de la línea; otra jugada tuya en una variante se deja fuera, y el panel lo dice. Cerrar Pragma Chess lo conserva todo: se vuelve a abrir en la partida del lobby, en Modo lobby, en la jugada que estabas mirando, con el plan que estabas preparando. Tu plan nunca se envía a tu rival: tu ordenador lo guarda y lo juega cuando llega su jugada, así que solo responde mientras Pragma Chess esté abierto en uno de tus ordenadores. Pragma Chess no te molesta mientras está abierto: ningún mensaje te avisa de que alguien ha movido. La ventana del lobby es el único lugar que lo hace, mientras está abierta: sigue el lobby en directo, y una partida en la que acaba de tocarte mover brilla un momento, con su sala en la lista. La parte de abajo del lobby, y la barra de estado mientras el lobby está activo, dicen cómo estás conectado — cuántos relés y cuántos ordenadores de otros jugadores —, o que no lo estás: lo que envíes entonces sale en cuanto te conectes. En el lobby eres una clave, que firma tus jugadas: está en **Opciones ▸ Ajustes personales…**, y por seguridad no se sincroniza, así que para jugar como tú desde otro ordenador cópiala allí a mano.

# Libros de aperturas {#books}

Un libro de aperturas es un archivo `.bin` de Polyglot: las jugadas conocidas en cada posición, cada una con un peso. El icono de libro de la barra de herramientas despliega los libros de tu carpeta Books.

- **Libro ▸ Nuevo libro…** y **Libro ▸ Abrir libro…** crean un libro o abren uno de cualquier lugar.
- **Libro ▸ Cambiar libro** muestra los libros de la carpeta: elige uno para usarlo, o **Ningún libro** para trabajar sin ninguno. **Mostrar carpeta de libros**, al final, abre la carpeta en el gestor de archivos.

El panel **Árbol de aperturas** muestra, para la posición del tablero, cada jugada del libro con su parte del peso, el nombre de la apertura a la que lleva y cómo fueron después las partidas de la base de datos abierta (partidas, y luego victorias de las blancas / tablas / victorias de las negras). Haz clic en una jugada para hacerla; la primera fila deshace la última jugada.

**Tu repertorio**: haz clic derecho en una jugada del Árbol de aperturas y elige **Añadir al repertorio**. Las jugadas del repertorio aparecen primero, en negrita. La marca se guarda en el archivo del libro y los demás programas la ignoran.

**Pesos**: el mismo menú tiene **Ajustar peso**, con +5 %, +10 %, +25 %, +50 %, +100 %, lo mismo hacia abajo y **Peso cero**. El porcentaje es de la parte de la propia jugada, y la suma de la posición siempre sigue siendo 100 %: lo que una jugada gana lo ceden las demás jugadas en proporción a lo que tienen — las de más peso, más — y lo que pierde vuelve a ellas del mismo modo. Una jugada al 0 %, o con menos del 1 %, no puede crecer en un porcentaje de sí misma, así que un aumento primero toma un 1 % de las demás y crece desde ahí; una disminución de una jugada al 0 % no hace nada. Peso cero da toda la parte de la jugada a las otras jugadas que tienen algo. Los cambios se escriben en el libro en uso. La lista se vuelve a ordenar, y la jugada que cambiaste brilla un momento y se marca, a la izquierda de su peso, con ↑ si subió, ↓ si bajó, = si se quedó igual; la marca dura hasta que dejas la posición.

# Nombres de aperturas {#opening-names}

El panel Motor da el nombre de la apertura en la que está la partida, y el Árbol de aperturas dice adónde lleva cada jugada.

Los nombres vienen de una base de datos de líneas con nombre. **Opciones ▸ Cambiar nombres de aperturas** elige cuál: inglés, italiano o ninguno. Hasta que elijas, los nombres siguen el idioma de la interfaz.

Una base de datos de nombres es una base de datos normal de tipo Libro de aperturas: puedes abrirla y añadir tus propias líneas, poniendo a cada una el nombre en el campo Evento y el código en ECO.

# Fuentes de partidas {#sources}

Una fuente trae tus partidas de un sitio web a la base de datos abierta y las mantiene al día.

**Base de datos ▸ Conectar fuente…** añade una:

- **lichess.org**: las partidas públicas de la cuenta; iniciar sesión es opcional y las descarga más rápido;
- **chess.com**: tu nombre de usuario;
- **torneionline.com**: tu número FIDE o FSI, para las partidas de los torneos que jugaste.
- **Archivos de ChessBase**: una base de datos de ChessBase en este ordenador: el formato clásico (`.cbh` y sus archivos) o el de ChessBase 17 y posteriores (`.2cbh` y sus archivos). Elige el archivo `.cbh` o `.2cbh`: sus partidas se copian (desde un `.2cbh` con sus variantes, los comentarios en tu idioma cuando vienen en varios, los símbolos, las casillas de colores y las flechas y las evaluaciones del motor), los archivos se quedan donde están, y las partidas que se les añadan más tarde llegan en la siguiente sincronización. En otro ordenador el archivo no está: la sincronización lo dice y ofrece ignorar la fuente en ese ordenador; *Base de datos ▸ Gestionar fuentes… ▸ Editar…* vuelve a elegir el archivo, y **Usar en este ordenador** deja de ignorarla.
- **Lichess Study**: un estudio de lichess.org. Pega la dirección de su página (o de uno de sus capítulos) en **Dirección del estudio**: cada capítulo se convierte en una partida de la base de datos, con sus comentarios y variantes, y sus etiquetas `StudyName`, `ChapterName` y `ChapterURL` dicen qué estudio y qué capítulo es (los capítulos del estudio no son los capítulos de un proyecto). Un capítulo cambiado en lichess.org sustituye a su partida en la siguiente sincronización, a menos que la partida también haya cambiado aquí: entonces se conservan las dos. Un estudio público no necesita cuenta; para uno privado, **Iniciar sesión con lichess.org…** con una cuenta que pueda verlo. Un estudio puede ser público para mirarlo y aun así no poderse descargar: su autor elige quién puede exportarlo (**Share & export** en los ajustes del estudio en lichess.org), y solo esas personas pueden conectarlo. Por ahora el estudio solo se lee: la escritura llegará.
- **Archivo PGN**: un archivo `.pgn` de este ordenador, sincronizado con la base de datos. Elígelo con **Examinar…**, o crea uno vacío con **Nuevo archivo…**, y luego la **Dirección**: **Lectura y escritura** (las partidas del archivo entran en la base de datos y las de la base de datos van al archivo, así que una partida añadida o cambiada en un lado llega al otro), **Solo lectura** (el archivo nunca se escribe) o **Solo escritura** (todas las partidas de la base de datos van al archivo; las demás partidas del archivo se quedan fuera de la base de datos). Una partida que guardas, cambias o anotas se escribe en el archivo unos segundos después. Nada de lo que se quita en un lado se quita en el otro, y una partida cambiada en ambos lados entre dos sincronizaciones se conserva dos veces. Pragma Chess marca cada partida que vincula a la base de datos con una etiqueta `PragmaUid`, y guarda un índice junto al archivo en un archivo oculto, para que un archivo que no ha cambiado no se vuelva a leer.

Las fuentes se leen cuando se abre la base de datos y cada veinte minutos. Una partida nunca se importa dos veces. **Base de datos ▸ Gestionar fuentes…** sincroniza una fuente ahora, la cambia, vuelve a iniciar sesión o la quita; las partidas ya importadas se quedan.

El nodo **Fuente** del árbol muestra las partidas de cada fuente.

# Papelera {#trash}

Haz clic derecho en una partida de la lista y elige **Mover partida a la papelera**. La partida sale de todas las listas y ya no se busca. Para trabajar con varias partidas a la vez, selecciónalas con **Shift** y las teclas de flecha (o un clic), o con **Ctrl** y un clic: el menú contextual entonces las mueve, las restaura o las elimina todas; **¿Quién es?** solo se ofrece para una partida.

El nodo **Papelera**, el último del árbol, muestra las partidas tiradas: **Recientes**, tiradas en los últimos siete días, y **Antiguas**. Allí puedes **Restaurar partida** o **Eliminar partida…**.

Eliminar todavía no reduce el archivo. **Base de datos ▸ Ajustes de la base de datos… ▸ Optimizar base de datos** quita definitivamente las partidas eliminadas y compacta el archivo. Hasta entonces no se pierde nada.

# Convertir un archivo PGN {#convert}

**Herramientas ▸ Convertir ▸ De PGN a base de datos Pragma…** crea una base de datos nueva con las partidas de un archivo PGN, por grande que sea: elige el **Archivo PGN** y dónde va la **Base de datos** (la carpeta Databases, con el nombre del archivo, a menos que elijas otro lugar con **Examinar…**), y luego **Convertir**. El archivo se lee por partes y sus partidas en todos los núcleos del ordenador menos uno; la ventana dice cuántas partidas han entrado y a qué velocidad va, y entretanto puedes seguir trabajando.

El archivo PGN no se modifica, y no se conecta nada a él: para mantener sincronizados una base de datos y un archivo PGN, usa en su lugar **Base de datos ▸ Conectar fuente… ▸ Archivo PGN**. Las entradas del archivo que no son una partida con jugadas se dejan fuera, y la ventana dice cuántas. **Cancelar** se detiene y no deja ninguna base de datos; una vez terminada, **Abrir base de datos** la abre.

# Proyectos {#projects}

Un proyecto es lo que estás mirando: la base de datos, la partida y la jugada, el lado desde el que se ve el tablero, el motor, los paneles y si estás entrenando. La barra de título muestra su nombre — el del archivo, o el que se da en **Archivo ▸ Información del proyecto…** — con un asterisco cuando tiene cambios sin guardar y, en cuanto el proyecto tiene capítulos, el capítulo abierto (desaparece cuando el proyecto vuelve a no tener capítulos): *Aperturas* - La italiana - Pragma Chess*.

- **Archivo ▸ Nuevo proyecto** conserva lo que ves — base de datos, motor, paneles — y empieza una partida nueva y vacía, con las blancas abajo.
- **Archivo ▸ Abrir proyecto…** y **Archivo ▸ Abrir proyecto reciente** abren un archivo `.pch`. En tu gestor de archivos un proyecto tiene el icono de documento de tu sistema con un peón en la página, y al abrirlo se abre Pragma Chess.
- **Archivo ▸ Guardar proyecto** y **Archivo ▸ Guardar proyecto como…** lo guardan.

No tienes que guardar: Pragma Chess se vuelve a abrir tal como lo cerraste.

Los paneles se pueden redimensionar y cerrar; el menú **Ver** (Jugadas, Árbol de aperturas, Motor, Lista de partidas) los vuelve a mostrar, y **Ver ▸ Restablecer la disposición de los paneles** los devuelve a donde empiezan.

**Archivo ▸ Información del proyecto…** muestra el archivo del proyecto, su nombre, una descripción, el autor, los contactos y la edición (una línea cada uno, guardados en el proyecto), sus idiomas y si es de solo lectura. Cambiarlos es la excepción, no la regla: los campos se abren bloqueados, y el botón **Editar**, con su candado, los desbloquea; el nombre del archivo no se cambia allí (eso lo hace **Guardar proyecto como…**). El **?** al final de cada campo dice para qué sirve.

**Solo lectura**, en **Archivo ▸ Información del proyecto…**, protege un proyecto de cambios hechos sin pensar: sus capítulos, títulos, párrafos, comentarios y variantes no se pueden cambiar, y no se guarda — el tablero se puede seguir explorando, y la barra de título dice *(solo lectura)*. Desmárcalo para cambiar el proyecto. Los proyectos que vienen con Pragma Chess son de solo lectura.

# Capítulos y párrafos {#chapters}

Un proyecto es una colección de **capítulos**, como lo es un estudio o un libro de ajedrez, y un capítulo contiene partidas una detrás de otra, con texto entre sus jugadas. La lista de jugadas muestra el capítulo entero: una línea clara marca dónde empieza cada partida, y su numeración vuelve a empezar; haz clic en una jugada de otra partida y el tablero va allí.

Haz clic derecho en la lista de jugadas — en una jugada, en un párrafo o en cualquier lugar, incluso sin jugadas — para:

- **Insertar ▸ Párrafo**: un párrafo después de la jugada (o donde esté el tablero), escrito ahí mismo en la lista de jugadas. Escribe como en un libro: el texto se justifica, y cada línea nueva empieza un párrafo nuevo, con sangría. **Esc**, **Ctrl+Enter** o un clic en otro lugar terminan; un párrafo que se deja vacío desaparece. Haz doble clic en un párrafo (o en un título, o en un subtítulo) para volver a escribir en él — un solo clic lo deja en paz; **Editar párrafo**, **Mover** y **Eliminar párrafo** están en su menú: Mover lo lleva **Al principio** o **Al final** de la partida, o lo hace **Subir** y **Bajar** media jugada cada vez — después de la jugada de las blancas, Bajar lo pone después de la de las negras, que vuelve a la fila de las blancas. Los párrafos van en la línea principal.
- **Insertar ▸ Título** e **Insertar ▸ Subtítulo**: un encabezado donde iría un párrafo, para dividir un capítulo como lo hace un libro: un título en negrita y centrado, un subtítulo en negrita, un poco más pequeño y también centrado. Se escriben, se mueven y se eliminan como los párrafos. Un título o un subtítulo es una sola línea: **Enter** lo confirma.
- **Mover**: mueve lo que has pulsado. En un título, un subtítulo o un párrafo mueve eso, como arriba; en una jugada, o en cualquier otro lugar, mueve la partida entera **Al principio** o **Al final** del capítulo, o la hace **Subir** y **Bajar** más allá de la partida de arriba o de abajo — las separaciones entre partidas van con ellas; el tablero se queda en la partida que muestra.
- **Insertar ▸ Separación de partidas**: una partida nueva desde la posición inicial, con la numeración empezando otra vez desde 1, justo debajo de la partida en la que hiciste clic derecho — siempre, incluso varias seguidas. Una partida sin jugadas todavía conserva su lugar en la lista, con la celda de la primera jugada en tono tenue ("1. …"): haz clic en ella para ir a esa partida. Se queda hasta que la eliminas.
- Haz clic derecho en cualquier lugar de una partida — una jugada, su primera celda, su texto — para **Eliminar partida**, o **Eliminar línea** cuando no empieza desde la jugada 1. Haz clic derecho en el número de la primera jugada de una partida para **Cambiar número de jugada…**: una línea desde una posición preparada puede empezar desde la jugada 12 en lugar de la 1 (es el número de jugada de la posición inicial, como lo escribe PGN).
- Haz clic derecho en la línea de una separación de partidas: **Eliminar separación de partidas** la quita cuando no se ha introducido nada después; cuando la partida que la sigue tiene jugadas, **Eliminar partida siguiente** quita la separación y esa partida (una partida de la base de datos se queda allí; se te pregunta antes cuando se perderían jugadas que no están guardadas en ningún sitio). Los títulos, subtítulos y párrafos se eliminan del mismo modo, desde su propio menú.

Nueva partida, Nuevo entrenamiento, Preparar posición, las acciones de pegar y las partidas en línea añaden todas su partida al final del capítulo, y lo mismo hace una partida abierta desde la lista de partidas (una que el capítulo ya tiene simplemente se muestra). Las partidas guardadas en una base de datos se guardan allí a medida que cambian; las demás, y todos los párrafos, se guardan con el proyecto.

El menú **Archivo** tiene los capítulos: **Nuevo capítulo…**, **Cambiar capítulo** para elegir el abierto, y al final de su lista **Gestionar capítulos…** para reordenarlos (arrastrando, o con Subir y Bajar), renombrarlos, añadirlos y eliminarlos. **Información del proyecto…** da al proyecto un nombre propio.

Un proyecto puede ser **multilingüe**: marca **Proyecto multilingüe** en **Archivo ▸ Información del proyecto…** y elige el **Idioma de los textos**, English o Italiano. El proyecto sigue siendo uno — los mismos capítulos, partidas y jugadas —, pero su nombre, los títulos de los capítulos, los títulos, subtítulos y párrafos se escriben en cada idioma por separado: lo que escribes va al idioma elegido, y un texto que aún no está escrito en él se muestra en inglés, o en el otro idioma, para que puedas traducirlo ahí mismo. Un proyecto multilingüe siempre se abre en el idioma de la interfaz. Un proyecto en un solo idioma, en cambio, **declara** su idioma, en el mismo **Idioma de los textos**: sus textos se escriben en él, sea cual sea el idioma de la interfaz, y elegir otro declara los mismos textos en ese idioma, sin traducirlos. Un proyecto nuevo toma el idioma de la interfaz. Así puedes usar Pragma Chess en español y cuidar los textos de un proyecto en inglés.

Pragma Chess incluye proyectos propios, en la carpeta Projects desde el primer inicio, para ver lo que puede ser un proyecto: ábrelos con **Archivo ▸ Abrir proyecto…**. **Rook Endgames** recorre los finales de torre que todo jugador encuentra, **Pawn Endgames** las ideas a las que se reduce todo final — la regla del cuadrado, la oposición, las casillas clave, el peón de torre, el peón pasado alejado, la ruptura —, y **Fischer – Spassky 1972** comenta la sexta partida del match de Reikiavik; todos en inglés e italiano.

Un proyecto nuevo y vacío no tiene capítulos: **Cambiar capítulo** muestra *(Sin capítulo)*, en gris, y una partida abierta desde la lista de partidas, o una nueva, ocupa el lugar de la del tablero. Las jugadas que haces en el tablero todavía no son un capítulo: cuando no están guardadas en ningún sitio, primero se te pregunta si **Guardar** la partida en la base de datos abierta o dejarla ir (**No guardar**). En cuanto pones algo — una separación de partidas, un párrafo, un título — se convierte en el primer capítulo, con todo lo que hay; **Nuevo capítulo…** hace lo mismo. Elimina todos los capítulos en **Gestionar capítulos…** y el proyecto vuelve a no tener capítulos, con el tablero vacío. Y si el capítulo vino por sí solo, eliminar lo que lo creó — la separación, el párrafo, el título — también devuelve el proyecto a no tener capítulos; un capítulo creado con **Nuevo capítulo…**, o renombrado, se queda.

# Sincronización {#sync}

La sincronización mantiene tu carpeta Pragma — bases de datos y proyectos — igual en varios ordenadores, a través de una carpeta en un servidor.

**Opciones ▸ Ajustes de sincronización…** la configura: un servidor **FTP** (también con TLS), un servidor **WebDAV** o un **Repositorio Git**. **Probar conexión** lo comprueba.

**Archivo ▸ Sincronizar ahora** (Ctrl+Y, también el primer botón de la barra de herramientas) lo hace todo en orden: lee las fuentes, guarda el proyecto e intercambia los archivos con el servidor. **Sincronizar antes de cerrar** lo hace cada vez que sales.

La sincronización no borra nada por sí sola: una base de datos que falta en un lado se copia allí, y una base de datos cambiada en dos ordenadores se fusiona partida a partida. Para deshacerte de una partida usa la papelera, que los demás ordenadores siguen.

Cada ordenador recuerda lo que sincronizó por última vez en un archivo oculto de su carpeta Pragma, `.pragma-chess.mine`, que nunca va al servidor. Así puede distinguir un archivo que borraste a mano — en el gestor de archivos, por ejemplo — de uno que todavía tiene que recibir: en la siguiente sincronización pregunta si **Eliminar en todas partes**, **Restaurar** o **Preguntarme más tarde** (se vuelve a preguntar la próxima vez que inicies Pragma Chess). Eliminado en todas partes, el archivo se quita del servidor, y cada uno de los demás ordenadores mueve su copia a la papelera en su siguiente sincronización. Un archivo que alguien cambió entretanto en otro ordenador simplemente vuelve.

Los archivos que pertenecen solo al servidor — el README y el LICENSE de un repositorio Git, por ejemplo — se indican en un archivo **`.pragmaignore`** en la raíz de la carpeta del servidor, uno por línea, escritos como en `.gitignore` (`*.tmp`, `Notes/`, `# un comentario`). Se quedan en el servidor: nunca se copian a tus ordenadores, una copia que un ordenador recibió antes lo abandona en la siguiente sincronización (una que cambiaste allí se queda, pero ya no se sincroniza), y no se envía nada con ese nombre desde tus ordenadores.

**Gestionar archivos…**, en los Ajustes de sincronización, muestra los archivos de la carpeta del servidor. Selecciona algunos y pulsa **Eliminar…** para hacer limpieza: después de que confirmes, se eliminan de todos los dispositivos sincronizados, este ordenador incluido (a la papelera), así que dejan de viajar entre tus ordenadores.

Con un repositorio Git cada sincronización que cambia archivos hace un commit, con el nombre de esos archivos ("Update Databases/Games.pdb; add Projects/Study.pch"): una sincronización que solo recibe, o no encuentra nada nuevo, deja el historial en paz.

**Opciones ▸ Conectar app móvil…** muestra un código para escanear con Pragma Chess en tu teléfono, que entonces guarda una copia de tus bases de datos.

Cuando eliminas una base de datos en el teléfono, el ordenador te pregunta en su siguiente sincronización con el teléfono si eliminarla aquí también o conservarla: **Conservarla** la deja en el ordenador, **Preguntarme más tarde** vuelve a preguntar la próxima vez que inicies Pragma Chess, y **Eliminar en todas partes…** te avisa primero de que la base de datos se eliminará de todos los dispositivos sincronizados — va a la papelera en este ordenador, se quita de la carpeta de sincronización del servidor, y los demás ordenadores que se sincronizan con ella eliminan su copia. En cualquier caso el teléfono no la vuelve a recibir.

# Extensiones {#extensions}

**Herramientas ▸ Gestionar extensiones…** instala motores y bases de datos con un clic. A la izquierda están los **proveedores**, que los ofrecen; en el centro lo que ofrece el proveedor elegido, para buscar por nombre y filtrar por tipo; a la derecha el que eliges, con lo que es, cuánto ocupa, de dónde viene, e **Instalar** y **Quitar**.

Instalar lo descarga y lo pone en la carpeta propia de Pragma Chess, y un motor se une a **Motor ▸ Gestionar motores…** y **Motor ▸ Cambiar motor**, listo para usar: no se toca nada más de tu ordenador, y no se pide ninguna contraseña. **Quitar** lo vuelve a quitar, y si el motor estaba en uso, el incluido ocupa su lugar. Una versión más nueva ofrecida por el proveedor muestra **Actualizar**. **Motor ▸ Instalar nuevo motor…** y **Base de datos ▸ Instalar nueva base de datos…** abren la misma ventana en el registro de En Croissant, mostrando solo motores o solo bases de datos.

El primer proveedor es el registro de En Croissant, otro programa de ajedrez libre: sus motores (Stockfish, RubiChess, Komodo, Dragon) se descargan de las propias páginas de sus autores, para tu sistema y tu procesador. Sus bases de datos aparecen en la lista pero todavía no se pueden instalar: están en el formato propio de En Croissant.

# Actualizaciones {#updates}

Una vez al día, poco después de iniciarse, Pragma Chess pregunta a GitHub el número de su última versión, y nada más: no se envía ningún dato sobre ti ni sobre tu ordenador. Cuando sale una más nueva, la barra de estado lo dice un momento y el menú Ayuda ofrece **Descargar Pragma Chess …**, que abre su página de descarga. **Ayuda ▸ Buscar actualizaciones…** pregunta en el acto; **Ayuda ▸ Buscar actualizaciones al iniciar** desactiva la pregunta diaria.

# Idioma {#language}

**Opciones ▸ Cambiar idioma** elige el idioma de la interfaz. Se aplica la próxima vez que se inicie Pragma Chess. Esta guía y los nombres de aperturas lo siguen.

# Atajos de teclado {#shortcuts}

**Recorrer una partida**

- Izquierda, Derecha — jugada anterior y siguiente
- Inicio, Fin — primera y última jugada
- E — Explicar
- Ctrl+E — iniciar o detener el análisis
- Ctrl+R — girar el tablero

**Partidas**

- Ctrl+Shift+N — nueva partida
- Ctrl+Shift+T — nuevo entrenamiento

**Portapapeles**

- Ctrl+Alt+C — copiar las jugadas hasta la posición actual
- Ctrl+Shift+C — copiar la posición en FEN
- Ctrl+Shift+V — pegar una FEN

**Proyectos y sincronización**

- Ctrl+N, Ctrl+O, Ctrl+S — nuevo, abrir y guardar proyecto
- Ctrl+Y — sincronizar ahora
- F1 — esta guía
