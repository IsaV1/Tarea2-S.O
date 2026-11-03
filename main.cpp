#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cstdlib>
#include <time.h>
#include <pthread.h>
#include <math.h>
using namespace std;

class Tablero{

private:
    //Las filas se representan desde arriba hacia abajo
    vector<vector<int>> filas = {

        {0}, 
        {0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0}

    };

    //0 vacio
    //-1 puma
    //1-12 perro

    //Mutex POSIX
    pthread_mutex_t lock_tablero;

    int capturas = 0;

public:
    //Constructor que inicializa el mutex POSIX
    Tablero() {
        pthread_mutex_init(&lock_tablero, NULL);
    }

    //Destructor que destruye el mutex para evitar fugas de recursos en el sistema operativo
    ~Tablero() {
        pthread_mutex_destroy(&lock_tablero);
    }

    //Medida de seguridad para impedir copia de una variable tipo tablero
    Tablero(const Tablero&) = delete; //Evita instanciar un nuevo tablero copiando otro
    Tablero& operator=(const Tablero&) = delete; // Evita copiar un tablero a otro tablero ya instanciado

    void estadoActual(){
        pthread_mutex_lock(&lock_tablero); //Entrada a la sección crítica
        for(int i = 0; i<(int)filas.size(); i++){

            if(i == 0){ //Fila de una sola casilla

                cout<<"    "<<filas[0][0]<<"  "<<endl;

            }else if(i == 1){

                for(int j = 0; j<(int)filas[i].size(); j++){

                    cout<<"  ";
                    cout<<filas[i][j];
                    cout<<" ";

                }
                cout<<endl;

            }else{

                for(int j = 0; j<(int)filas[i].size(); j++){

                    cout<<filas[i][j]<<" ";

                }

                cout<<endl;

            }
        }
        pthread_mutex_unlock(&lock_tablero); //Salida de la sección crítica

    }

    //Validación geométrica pura (no lee el contenido del tablero, por eso no necesita lock)
    //Antes se llamaba movimientoValido, pero las demás funciones llaman a movimientoValidoBase
    bool movimientoValidoBase(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){
        
        //Verificar que existan las posiciones de los argumentos
        // Verificar que existan las filas (índices válidos)
        if (filaOrigen < 0 || filaOrigen >= (int)filas.size() || 
            filaDestino < 0 || filaDestino >= (int)filas.size()) {

            return false;
        }
        //Nota: es necesario castear en int porque .size() entrega una variable de tipo size_t
        // Verificar límites de columnas adaptado
        if(columnaOrigen < 0 || columnaOrigen >= (int)filas[filaOrigen].size() || 
            columnaDestino < 0 || columnaDestino >= (int)filas[filaDestino].size()){

            return false;
        }

        //Verificación de un movimiento valido

        //zona del 5x5

        int diferenciaFila = abs(filaDestino-filaOrigen);  //calcular distancias
        int diferenciaColumna = abs(columnaDestino-columnaOrigen);

        if(filaOrigen >= 2 && filaDestino >= 2){ // Verificar que nos mantenemos aún en el 5x5

            if(diferenciaColumna + diferenciaFila == 1){

                return true;

            }else{

                return false;
            }

        }

        //zona del triangulo

        // punta del tablero hacia fila 1
        if((filaOrigen == 0 && filaDestino == 1)||(filaOrigen == 1 && filaDestino == 0)){

            return true;
        }

        // punta del tablero hasta el centro del 5x5 (0,0) hacia (2,2) y vice versa
        if((filaOrigen == 0 && filaDestino == 2 && columnaDestino == 2)||
        (filaOrigen == 2 && filaDestino == 0 && columnaOrigen == 2)){
        
            return true;
        }

        //movimiento horizontal en la fila 1 (2 casillas solamente)
        if(filaOrigen == 1 && filaDestino == 1){

            if(diferenciaColumna == 1) {

                return true;
            }
        }

        // Conexión fila 1 hacia el 5x5
        if(filaOrigen == 1 && filaDestino == 2){

            if(abs(columnaDestino - (4 * columnaOrigen)) <= 1){

                return true;
            }

        }else if(filaOrigen == 2 && filaDestino == 1){ // Desde 5x5 hacia la fila 1

            if(abs(columnaOrigen - (4 * columnaDestino)) <= 1){

                return true;
            }
        }

        return false;
    }

    bool movimientoValidoPerro(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){
        pthread_mutex_lock(&lock_tablero);
        bool valido = validarPerroSinLock(filaOrigen, columnaOrigen, filaDestino, columnaDestino);
        pthread_mutex_unlock(&lock_tablero);
        return valido;
    }

    bool movimientoValidoPuma(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino) {
        pthread_mutex_lock(&lock_tablero);
        bool captura = false;
        int fMid = -1;
        int cMid = -1;
        bool valido = validarPumaSinLock(filaOrigen, columnaOrigen, filaDestino, columnaDestino, captura, fMid, cMid);
        pthread_mutex_unlock(&lock_tablero);
        return valido;
    }

    //Valida Y mueve dentro de la misma sección crítica
    bool moverPerro(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){
        pthread_mutex_lock(&lock_tablero);

        bool valido = validarPerroSinLock(filaOrigen, columnaOrigen, filaDestino, columnaDestino);
        if (valido) {
            filas[filaDestino][columnaDestino] = filas[filaOrigen][columnaOrigen];
            filas[filaOrigen][columnaOrigen] = 0;
        }

        pthread_mutex_unlock(&lock_tablero);
        return valido;
    }

    //si salta sobre un perro lo elimina y cuenta la captura
    bool moverPuma(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){
        pthread_mutex_lock(&lock_tablero);

        bool captura = false;
        int fMid = -1;
        int cMid = -1;
        bool valido = validarPumaSinLock(filaOrigen, columnaOrigen, filaDestino, columnaDestino, captura, fMid, cMid);

        if (valido) {
            if (captura) {
                filas[fMid][cMid] = 0; //el perro capturado sale del tablero
                capturas++;
            }
            filas[filaDestino][columnaDestino] = -1;
            filas[filaOrigen][columnaOrigen] = 0;
        }

        pthread_mutex_unlock(&lock_tablero);
        return valido;
    }

    bool colocarPieza(int fila, int columna, int pieza){
        if (pieza != -1 && (pieza < 1 || pieza > 12)) {
            return false;
        }

        pthread_mutex_lock(&lock_tablero);

        bool colocada = false;
        if (fila >= 0 && fila < (int)filas.size() &&
            columna >= 0 && columna < (int)filas[fila].size() &&
            filas[fila][columna] == 0) {
            filas[fila][columna] = pieza;
            colocada = true;
        }

        pthread_mutex_unlock(&lock_tablero);
        return colocada;
    }

    int getCapturas(){
        pthread_mutex_lock(&lock_tablero);
        int c = capturas;
        pthread_mutex_unlock(&lock_tablero);
        return c;
    }

    bool pumaPuedeMoverse(){ //Se usara para validar victoria (Puma acorralado por perros)
        pthread_mutex_lock(&lock_tablero); //bloqueamos el recurso

        int filaPuma = -1;//inicializamos en -1, porque 0 es una posición valida
        int columnaPuma = -1;

        //buscar fila y columna del puma (recordar que el valor del puma en el tablero es -1)
        for(int i = 0; i<(int)filas.size(); i++){

            for(int j = 0; j< (int)filas[i].size(); j++){

                if(filas[i][j] == -1){

                    filaPuma = i;
                    columnaPuma = j;

                }

            }

        }

        //Caso borde en caso de que no haya un puma en el tablero

        if(filaPuma == -1 || columnaPuma == -1){
            pthread_mutex_unlock(&lock_tablero);
            
            return false;
        }


        //buscamos por todo el tablero posiciones vacias en las que el puma pueda moverse (con o sin captura)
        for(int i = 0; i<(int)filas.size(); i++){

            for(int j = 0; j< (int)filas[i].size(); j++){

                if(filas[i][j]== 0){ //casilla vacia

                    //valores por defecto para utilizar en la función que viene
                    bool captura = false;
                    int fMid = -1;
                    int cMid = -1;

                    if(validarPumaSinLock(filaPuma, columnaPuma, i, j, captura, fMid, cMid)){
                        //la función misma verifica si es que puede haber captura o no
                    
                        pthread_mutex_unlock(&lock_tablero);
                        return true;

                    }
                }
            }
        }

        pthread_mutex_unlock(&lock_tablero);
        return false;

    }

private:

    int validacionDiagonal(int fila, int columna){ //metodo para validar capturas validas del puma en la zona del triangulo

        if(fila == 0){

            return 2;
        }

        if(fila == 1){

            return 1 + (2*columna);

        }

        return columna;
    }
    //lo que hace mas que nada es como estandarizar el triangulo como si fuera el 5x5 para obtener capturas diagonales.
    //en resumen, estandariza las posiciones horizontales del triangulo de arriba del 5x5, para asi calcular las posibles
    //capturas diagonales que puede hacer un puma

    //estas validaciones requieren que el lock ya este tomado
    //el mutex no es recursivo, por eso no vuelven a bloquearlo

    bool validarPerroSinLock(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){

        if (filaOrigen < 0 || filaOrigen >= (int)filas.size() || 
            columnaOrigen < 0 || columnaOrigen >= (int)filas[filaOrigen].size()) {
            return false;
        }
        int pieza = filas[filaOrigen][columnaOrigen];
        if (pieza < 1 || pieza > 12) {
            return false;
        }

        if (filaDestino < 0 || filaDestino >= (int)filas.size() || 
            columnaDestino < 0 || columnaDestino >= (int)filas[filaDestino].size() ||
            filas[filaDestino][columnaDestino] != 0) {
            return false;
        }

        return this->movimientoValidoBase(filaOrigen, columnaOrigen, filaDestino, columnaDestino);
    }

    //si es un salto de captura, dejar en captura = true y la casilla del perro en (fMid, cMid)
    bool validarPumaSinLock(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino, bool& captura, int& fMid, int& cMid) {

        captura = false;
        fMid = -1;
        cMid = -1;

        if (filaOrigen < 0 || filaOrigen >= (int)filas.size() || 
            columnaOrigen < 0 || columnaOrigen >= (int)filas[filaOrigen].size()) {
            return false;
        }
        if (filas[filaOrigen][columnaOrigen] != -1) {
            return false;
        }

        if (filaDestino < 0 || filaDestino >= (int)filas.size() || 
            columnaDestino < 0 || columnaDestino >= (int)filas[filaDestino].size() ||
            filas[filaDestino][columnaDestino] != 0) {
            return false;
        }

        if (this->movimientoValidoBase(filaOrigen, columnaOrigen, filaDestino, columnaDestino)) {
            return true;
        } 

        //salto de captura sobre un Perro
        //calculamos en donde deberia estar el perro, usando la estandarización del triangulo mencionado en validacionDiagonal
        int horizontalOrigen = validacionDiagonal(filaOrigen, columnaOrigen);
        int horizontalDestino = validacionDiagonal(filaDestino, columnaDestino);

        //verificar que el salto del puma sobre el perro tenga una posición entre medio
        if((filaOrigen + filaDestino) % 2 != 0 ||(horizontalOrigen + horizontalDestino) % 2 != 0){

            return false;

        }

        //Posición real del perro (sin estandarización)
        int filaMedia = (filaOrigen + filaDestino) / 2;
        int horizontalMedia = (horizontalOrigen + horizontalDestino) / 2;
        int columnaMedia = horizontalMedia;

        //verificar indice de la columna con su posición horizontal
        if(filaMedia == 0){

            columnaMedia = 0;

        }else if(filaMedia==1){

            //Si estamos en la fila 1, solo hay dos casillas (pos. horizontales)
            if(horizontalMedia!= 1 && horizontalMedia != 3){

                return false;

            }

            columnaMedia = (horizontalMedia - 1) / 2;
        }

        //verificando que exista la posición de entremedio
        if(filaMedia >= 0 && filaMedia < (int)filas.size() && columnaMedia >= 0 && columnaMedia < (int)filas[filaMedia].size()){

            //verificar que haya un perro en esa posición del tablero (1 a 12) y que el movimiento sea valido
            if(filas[filaMedia][columnaMedia] >=1 && filas[filaMedia][columnaMedia] <= 12&&
            movimientoValidoBase(filaOrigen,columnaOrigen,filaMedia,columnaMedia) &&
            movimientoValidoBase(filaMedia,columnaMedia,filaDestino,columnaDestino)){

                //registramos la captura y la posición en donde estaba el perro
                captura = true;
                fMid = filaMedia;
                cMid = columnaMedia;

                return true;

            }
        }

        return false;

    }

    //Busca una pieza por su ID. REQUIERE el lock tomado.
    bool buscarSinLock(int idPieza, int& fila, int& columna){

        if(idPieza != -1 && (idPieza < 1 || idPieza > 12)){ //ID inválido (evita "encontrar" casillas vacías con 0)

            return false;

        }

        for(int i = 0; i < (int)filas.size(); i++){

            for(int j = 0; j < (int)filas[i].size(); j++){

                if(filas[i][j] == idPieza){

                    fila = i;
                    columna = j;
                    return true;

                }

            }

        }

        return false;
    }

public:

    bool verificarPiezas(){ // Verificamos que el tablero tenga las 13 piezas (12 perros y 1 puma)

        pthread_mutex_lock(&lock_tablero);

        vector<bool> piezas(13, false); //Pos 0 para el puma, de la 1 hasta la 12 para los perros
        int cantidad = 0;

        for(int i = 0; i< (int)filas.size(); i++){

            for(int j = 0; j< (int)filas[i].size(); j++){

                int pieza = filas[i][j]; //verifica que valor hay en la casilla

                if(pieza == 0){//casilla vacia

                    continue;

                }
                
                //comprobar fuera de rango

                if(pieza < -1 || pieza > 12){

                    pthread_mutex_unlock(&lock_tablero);
                    return false;

                }

                //puma
                if(pieza == -1){

                    if(piezas[0]){ //comprobar que no se repita puma

                        pthread_mutex_unlock(&lock_tablero);
                        return false;

                    }


                    piezas[0] = true; //asignamos el puma a la pos 0
                    cantidad++;
                    continue; 

                }

                // Comprobar perros repetidos
                if(piezas[pieza]){

                    pthread_mutex_unlock(&lock_tablero);
                    return false;

                }

                piezas[pieza] = true;
                cantidad++;

            }

        }

        if(cantidad < 13){

            pthread_mutex_unlock(&lock_tablero);
            return false;

        }

        pthread_mutex_unlock(&lock_tablero);
        return true;
    }

    bool buscarPieza(int idPieza, int& fila, int& columna){

        pthread_mutex_lock(&lock_tablero);
        bool encontrada = buscarSinLock(idPieza, fila, columna);
        pthread_mutex_unlock(&lock_tablero);
        return encontrada;

    }

    //Lista, en UNA sola sección crítica, todos los destinos válidos de la pieza idPieza (-1 puma, 1-12 perro).
    //destinosCaptura es el subconjunto de destinos que implican captura (solo para el puma).
    //Devuelve false si la pieza ya no está en el tablero (por ejemplo, un perro capturado).
    bool listarDestinos(int idPieza, vector<pair<int,int>>& destinos, vector<pair<int,int>>& destinosCaptura){

        destinos.clear();
        destinosCaptura.clear();

        pthread_mutex_lock(&lock_tablero);

        int filaPieza = -1;
        int columnaPieza = -1;

        if(!buscarSinLock(idPieza, filaPieza, columnaPieza)){

            pthread_mutex_unlock(&lock_tablero);
            return false;

        }

        for(int i = 0; i < (int)filas.size(); i++){

            for(int j = 0; j < (int)filas[i].size(); j++){

                if(filas[i][j] != 0){ //solo casillas vacías

                    continue;

                }

                if(idPieza == -1){

                    bool captura = false;
                    int fMid = -1;
                    int cMid = -1;

                    if(validarPumaSinLock(filaPieza, columnaPieza, i, j, captura, fMid, cMid)){

                        destinos.push_back(make_pair(i, j));

                        if(captura){

                            destinosCaptura.push_back(make_pair(i, j));

                        }
                    }

                }else if(validarPerroSinLock(filaPieza, columnaPieza, i, j)){

                    destinos.push_back(make_pair(i, j));

                }
            }
        }

        pthread_mutex_unlock(&lock_tablero);
        return true;
    }

    //Mueve la pieza idPieza a su destino. La posición ACTUAL de la pieza se busca dentro del mismo lock
    //con el que se valida y se mueve, así un hilo nunca puede mover una pieza que no es la suya,
    //aunque la posición que conocía ya esté desactualizada.
    //Devuelve: 0 = movimiento inválido, 1 = movido, 2 = movido con captura.
    //Entrega además la posición de origen y el ID del perro capturado (0 si no hubo captura).
    int moverPorId(int idPieza, int filaDestino, int columnaDestino, int& filaOrigen, int& columnaOrigen, int& idCapturado){

        filaOrigen = -1;
        columnaOrigen = -1;
        idCapturado = 0;

        pthread_mutex_lock(&lock_tablero);

        if(!buscarSinLock(idPieza, filaOrigen, columnaOrigen)){

            pthread_mutex_unlock(&lock_tablero);
            return 0;

        }

        int resultado = 0;

        if(idPieza == -1){

            bool captura = false;
            int fMid = -1;
            int cMid = -1;

            if(validarPumaSinLock(filaOrigen, columnaOrigen, filaDestino, columnaDestino, captura, fMid, cMid)){

                if(captura){

                    idCapturado = filas[fMid][cMid];
                    filas[fMid][cMid] = 0;
                    capturas++;
                    resultado = 2;

                }else{

                    resultado = 1;

                }

                filas[filaDestino][columnaDestino] = -1;
                filas[filaOrigen][columnaOrigen] = 0;
            }

        }else if(validarPerroSinLock(filaOrigen, columnaOrigen, filaDestino, columnaDestino)){

            filas[filaDestino][columnaDestino] = idPieza;
            filas[filaOrigen][columnaOrigen] = 0;
            resultado = 1;

        }

        pthread_mutex_unlock(&lock_tablero);
        return resultado;
    }

};

class Partida{

private:

    int idPartida;
    int idJugadorPuma;
    int idJugadorPerros;
    bool terminado; //true si la partida termino
    int ganador; // se inicializara en -1 (cuando termine la partida se le asigna el id del jugador puma o perro)
    Tablero tablero;
    bool iniciado;

    pthread_mutex_t lock_partida;

public:
    // constructor
    Partida(int id, int idJP, int idJPe) : idPartida(id), idJugadorPuma(idJP), idJugadorPerros(idJPe), 
    terminado(false), ganador(-1), iniciado(false)
    {

        pthread_mutex_init(&lock_partida, NULL);

    }

    //Destructor que destruye el mutex para evitar fugas de recursos en el sistema operativo
    ~Partida() {
        pthread_mutex_destroy(&lock_partida);
    }

    Partida(const Partida&) = delete; // lo mismo que en tablero, para no copiar una variable partida
    Partida& operator=(const Partida&) = delete;

    bool colocarPieza(int fila, int columna, int pieza){//recordar: -1 puma, 1 - 12 perros

        pthread_mutex_lock(&lock_partida);

        if(iniciado||terminado){ //verificar que la partida aun no comienze o que no haya terminado

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        bool resultado = tablero.colocarPieza(fila, columna, pieza);//reutilizamos función hecha en tablero

        pthread_mutex_unlock(&lock_partida);
        return resultado;

    }

    bool terminarPartida(int idGanador){

        pthread_mutex_lock(&lock_partida);

        if(terminado){
            
            pthread_mutex_unlock(&lock_partida);
            return false;
        }

        //verificar que el idGanador entregado corresponda a alguno de los 2 jugadores
        if((idGanador != idJugadorPerros) && (idGanador != idJugadorPuma)){

            pthread_mutex_unlock(&lock_partida);
            return false;
        }

        ganador = idGanador;
        terminado = true;

        pthread_mutex_unlock(&lock_partida);
        return true;
    }

    //Para ganadorPuma y ganadorPerros no es necesario un mutex, ya que las mismas funciones ya implementadas
    //en el tablero aseguran consultas atomicas

    bool ganadorPuma(){

        if(tablero.getCapturas() >= 6){

            return true;
        }

        return false;
    }

    bool ganadorPerros(){

        if(tablero.pumaPuedeMoverse()){

            return false;

        }

        return true; //puma ya no puede moverse
    }

    
    bool moverPuma(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){

        pthread_mutex_lock(&lock_partida);

        //caso borde para no permitir movimiento si es que la partida ya termino

        if(!iniciado||terminado){ //verificar partida en curso

            pthread_mutex_unlock(&lock_partida);
            return false;
        }

        //Mover el puma utilizando la función del tablero

        bool movimientoExitoso = tablero.moverPuma(filaOrigen, columnaOrigen,filaDestino,columnaDestino);

        if(movimientoExitoso){ //Verificamos, que una vez se haya movido la pieza, revisamos si el puma tiene 6 capturas.
            asignarGanador();
        }

        pthread_mutex_unlock(&lock_partida);

        return movimientoExitoso;
    }

    bool moverPerro(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){

        pthread_mutex_lock(&lock_partida);

        //verificar si la partida termino

        if(!iniciado||terminado){//verificar partida en curso

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        //mover perro usando la función del tablero
        bool movimientoExitoso = tablero.moverPerro(filaOrigen, columnaOrigen,filaDestino,columnaDestino);
    
        if(movimientoExitoso){ //Verificamos, que una vez se haya movido la pieza, revisamos si el puma tiene movimientos validos
            asignarGanador();
        }

        pthread_mutex_unlock(&lock_partida);
        return movimientoExitoso;
    }

    bool getTerminado(){

        pthread_mutex_lock(&lock_partida);
        bool resultado = terminado;
        pthread_mutex_unlock(&lock_partida);

        return resultado;

    }

    int getGanador(){

        pthread_mutex_lock(&lock_partida);
        int resultado = ganador; 
        pthread_mutex_unlock(&lock_partida);

        return resultado;
    }

    bool iniciarPartida(){

        pthread_mutex_lock(&lock_partida);

        //verificar que la partida no haya iniciado o este terminada
        if(iniciado || terminado){

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        //comprobar que el tablero tenga todas sus piezas

        if(!tablero.verificarPiezas()){

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        iniciado = true;
        pthread_mutex_unlock(&lock_partida);
        return true;

    }

private:

    bool asignarGanador(){

        //preimero revisar si es que ya esta terminada la partida de antes
        if(terminado){

            return false;
        }

        //verifiar si gano el puma

        if(ganadorPuma()){

            ganador = idJugadorPuma;
            terminado = true;

            return true;
        }

        //verificar si gano el perro

        if(ganadorPerros()){

            ganador = idJugadorPerros;
            terminado = true;

            return true;
        }

        return false;
    }

public:

    bool buscarPieza(int idPieza, int& fila, int& columna){

        return tablero.buscarPieza(idPieza, fila, columna); //buscamos pieza del tablero utilizando metodo de tablero

    }

    bool listarDestinos(int idPieza, vector<pair<int,int>>& destinos, vector<pair<int,int>>& destinosCaptura){

        return tablero.listarDestinos(idPieza, destinos, destinosCaptura);

    }

    bool moverPorId(int idPieza, int filaDestino, int columnaDestino){

        pthread_mutex_lock(&lock_partida);

        if(!iniciado||terminado){

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        int filaOrigen = -1;
        int columnaOrigen = -1;
        int idCapturado = 0;

        int resultado = tablero.moverPorId(idPieza, filaDestino, columnaDestino, filaOrigen, columnaOrigen, idCapturado);

        if(resultado == 0){

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        string nombre = (idPieza == -1) ? "Puma" : "Perro " + to_string(idPieza);

        cout<<"[Partida "<<idPartida<<"] "<<nombre<<" ("<<filaOrigen<<","<<columnaOrigen<<") -> ("
            <<filaDestino<<","<<columnaDestino<<")";

        if(resultado == 2){

            cout<<" CAPTURA al perro "<<idCapturado<<" (capturas: "<<tablero.getCapturas()<<")";

        }

        cout<<endl;

        if(resultado == 2){

            tablero.estadoActual(); //mostramos el tablero en cada captura

        }

        if(asignarGanador()){

            cout<<"[Partida "<<idPartida<<"] FIN. Ganador: jugador "<<ganador
                <<(ganador == idJugadorPuma ? " (Puma)" : " (Perros)")<<endl;

        }

        pthread_mutex_unlock(&lock_partida);
        return true;
    }

    void mostrarTablero(){

        tablero.estadoActual();

    }

};

struct ArgsPieza{
    Partida* partida;
    int idPieza;
    int minMs;
    int maxMs;
    unsigned int seed; //semilla propia de cada hilo (rand_r es seguro entre hilos)
};

static void dormirMs(int ms){
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void* hiloPieza(void* arg){

    ArgsPieza* a = (ArgsPieza*)arg;

    vector<pair<int,int>> destinos;
    vector<pair<int,int>> destinosCaptura;

    while(!a->partida->getTerminado()){

        //retardo aleatorio entre minMs y maxMs
        int espera = a->minMs + (int)(rand_r(&a->seed) % (unsigned int)(a->maxMs - a->minMs + 1));
        dormirMs(espera);
.
        if(!a->partida->listarDestinos(a->idPieza, destinos, destinosCaptura)){break;}

        if(destinos.empty()){continue;}

        const vector<pair<int,int>>& opciones = (a->idPieza == -1 && !destinosCaptura.empty()) ? destinosCaptura : destinos;
        pair<int,int> elegido = opciones[rand_r(&a->seed) % opciones.size()];

        a->partida->moverPorId(a->idPieza, elegido.first, elegido.second);

    }

    return NULL;
}


//pruebaaa
int main(int argc, char** argv){

    int minMs = 50;
    int maxMs = 300;
    unsigned int seed = 12345;

    if(argc >= 3){

        minMs = atoi(argv[1]);
        maxMs = atoi(argv[2]);

    }

    if(argc >= 4){

        seed = (unsigned int)atoi(argv[3]);

    }

    if(minMs < 0 || maxMs < minMs){

        cerr<<"Uso: "<<argv[0]<<" [minMs maxMs [seed]]"<<endl;
        return 1;

    }

    Partida partida(1, 100, 200); //id partida, id jugador puma, id jugador perros

    //Disposición inicial por defecto: puma en la punta, perros en las filas 6, 5 y 4
    if(!partida.colocarPieza(0, 0, -1)){

        cerr<<"No se pudo colocar el puma"<<endl;
        return 1;

    }

    int idPerro = 1;

    for(int f = 6; f >= 4 && idPerro <= 12; f--){

        for(int c = 0; c < 5 && idPerro <= 12; c++){

            if(!partida.colocarPieza(f, c, idPerro)){

                cerr<<"No se pudo colocar el perro "<<idPerro<<endl;
                return 1;

            }

            idPerro++;
        }
    }

    if(!partida.iniciarPartida()){

        cerr<<"No se pudo iniciar la partida (piezas incompletas)"<<endl;
        return 1;

    }

    cout<<"Tablero inicial:"<<endl;
    partida.mostrarTablero();

    pthread_t hilos[13];
    ArgsPieza args[13];
    bool creado[13];

    for(int k = 0; k < 13; k++){

        creado[k] = false;
        args[k].partida = &partida;
        args[k].idPieza = (k == 0) ? -1 : k; //hilo 0 = puma, hilos (1-12) = perros
        args[k].minMs = minMs;
        args[k].maxMs = maxMs;
        args[k].seed = seed + (unsigned int)k; //semilla distinta por hilo, reproducible

    }

    bool falloCreacion = false;

    for(int k = 0; k < 13; k++){

        if(pthread_create(&hilos[k], NULL, hiloPieza, &args[k]) != 0){

            cerr<<"Error al crear el hilo "<<k<<endl;
            falloCreacion = true;
            break;

        }

        creado[k] = true;
    }

    if(falloCreacion){

        partida.terminarPartida(200); //detiene a los hilos ya creados

    }

    for(int k = 0; k < 13; k++){

        if(creado[k]){

            pthread_join(hilos[k], NULL);

        }
    }

    cout<<"Tablero final:"<<endl;
    partida.mostrarTablero();

    int g = partida.getGanador();
    cout<<"Ganador: jugador "<<g<<(g == 100 ? " (Puma)" : " (Perros)")<<endl;

    return falloCreacion ? 1 : 0;
}