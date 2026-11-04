#include <iostream>
#include <vector>
#include <utility>
#include <cstdlib>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <semaphore.h> //para poder usar semaforos
#include <fstream> //Lectura txt
using namespace std;


pthread_mutex_t lock_cout = PTHREAD_MUTEX_INITIALIZER; //Es mas estetico, pero existe una condición de carrera
//al momento de imprimir los movimientos del tablero, no afecta el funcionamiento, solo en como se imprime en consola


struct Configuracion{ //estructura y valores default para las configuraciones de la partida

    int minMs = 50;
    int maxMs = 300;
    unsigned int seed = 12345;
    int filas[13];
    int columnas[13];
    //pos 0 para puma: -1
    //pos 1-12 para perros del 1-12
};

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
        asignarGanador(); //Verificar si quiza los perros ya acorralaron al puma

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

        pthread_mutex_lock(&lock_cout); //lock para evitar solapamiento al imprimir mensajes en cnsola

        cout << "[Partida " << idPartida << "] ";

        if(idPieza == -1){
            cout << "Puma";
        }else{
            cout << "Perro " << idPieza;
        }

        cout<<" (" << filaOrigen << "," << columnaOrigen << ") -> ("<< filaDestino << "," << columnaDestino << ")";

        if(resultado == 2){

            cout << " CAPTURA al perro " << idCapturado << " (capturas: " << tablero.getCapturas() << ")";
            
        }

        cout<<endl;

        tablero.estadoActual(); //mostramos el tablero

        if(asignarGanador()){

            cout<<"[Partida "<<idPartida<<"] FIN. Ganador: jugador "<<ganador<<(ganador == idJugadorPuma ? " (Puma)" : " (Perros)")<<endl;

        }

        pthread_mutex_unlock(&lock_cout); // desbloqueamos el mutex para la impresión de mensajes por consola
        pthread_mutex_unlock(&lock_partida);
        return true;
    }

    void mostrarTablero(){

        tablero.estadoActual();

    }

    bool cancelarPartida(){ //cancelamos una partida en caso de un error interno 

        pthread_mutex_lock(&lock_partida);

        if(terminado){ //La partida ya habia terminado, por lo que no es necesario cancelarla

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        terminado = true;
        ganador = -1; //ningun ganador

        pthread_mutex_unlock(&lock_partida);
        return true;

    }

    
};

struct ArgsArbitro{

    Partida* partida;
    int minMs;
    int maxMs;
    unsigned int seed; //semilla propia para los hilos
    sem_t* mesas;//se comparte el recurso de mesas entre los arbitros para poder realizar partidas
    //puntero porque todos los arbitros deben apuntar al mismo semaforo
    Configuracion* config;
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

        //en caso de que el hijo siga dormida cuando termino la partida
        if(a -> partida -> getTerminado()){

            break;

        }

        if(!a->partida->listarDestinos(a->idPieza, destinos, destinosCaptura)){break;}

        if(destinos.empty()){continue;}

        const vector<pair<int,int>>& opciones = (a->idPieza == -1 && !destinosCaptura.empty()) ? destinosCaptura : destinos;
        pair<int,int> elegido = opciones[rand_r(&a->seed) % opciones.size()];

        a->partida->moverPorId(a->idPieza, elegido.first, elegido.second);

    }

    return NULL;
}

void* hiloArbitro(void* arg){

    ArgsArbitro* a = (ArgsArbitro*)arg;

    //Los arbitros manejaran las mesas con los jugadores, por lo que debe esperar a que alguna mesa este disponible

    if(sem_wait(a -> mesas)!= 0){

        //se podria usar solo sem_wait(a -> mesas), pero existe la posibilidad de que falle

        cout<<"Error al esperar una mesa disponible" << endl;

        a -> partida -> cancelarPartida();
        return NULL;

    }

    //Colocamos las 13 piezas usando el struct de config
    for(int i = 0; i<13; i++){

        int idPieza;

        if(i == 0){

            idPieza = -1; //Puma

        }else{

            idPieza = i; //Perros

        }

        if(!a->partida->colocarPieza(a -> config -> filas[i] , a -> config -> columnas[i] , idPieza ) ){ //Se coloca la pieza y luego se valida

            cout<<"Error al colocar la pieza " << idPieza << endl;
            a -> partida -> cancelarPartida();
            sem_post(a->mesas);
            return NULL;

        }
    }



    //como ya se pusieron todas las fichas en el tablero de manera exitosa, iniciamos la partida

    if(! a -> partida -> iniciarPartida()){ //Primero iniciamos la partida, y luego se comprueba la condición si es verdadera o falsa

        cout<< "Error al iniciar la partida"<<endl;
        a -> partida -> cancelarPartida();
        sem_post(a->mesas); //liberamos el recurso
        return NULL;

    }


    pthread_t hilos[13];//hilos de las fichas
    ArgsPieza args[13];//estructuras de las fichas
    int cont = 0;//contador

    for(int i = 0; i<13; i++){
        //Creamos las 13 piezas, cada pieza se asigna dentro de una posición de args[13]

        args[i].partida = a -> partida;

        if(i == 0){ //La pieza 0 es el puma
            args[i].idPieza = -1;
        }else{ // las demas piezas (del 1 al 12), se le asigna su id
            args[i].idPieza = i;
        }

        args[i].minMs = a->minMs;
        args[i].maxMs = a->maxMs;

        args[i].seed = a->seed + (unsigned int)i; //asignamos una semilla diferente para cada ficha

    }

    for(int i = 0; i < 13;i++){ //creamos los 13 hilos

        if(pthread_create(&hilos[i],NULL, hiloPieza, &args[i])!= 0){ // !=0 si da error

            cout<<"Error al crear el hilo de la pieza "<<i << endl;
            a->partida->cancelarPartida(); //cancelamos la partida en caso de haber un error en la creación de hilos
  
            break;
        }

        cont++;
    }

    for(int i = 0; i < cont; i++){ //El arbitro no termina hasta que todas las demas piezas hayan terminado
        
        pthread_join(hilos[i], NULL);

    }
    //Una vez terminaron los 13 hilos, el arbitro revisa el ganador del atributo partida

    //recordar que existe la posibilidad de que la partida se cancele, en ese caso el id del ganador es -1
    int ganador = a->partida->getGanador();

    if(ganador ==-1){//La partida fue cancelada y no tiene ganador

        cout<<"Partida cancelada" << endl;

    }else{//La partida termino normalmente

        cout<<"Partida finalizada, ganador id: "<< ganador<< endl;

    }

    sem_post(a -> mesas); //Liberamos la mesa para que otros arbitros puedan acceder
    return NULL;
}

int main(int argc, char** argv){ //no borrar main o cambiar lo principal

    if(argc < 3 || argc > 4){ //verificar ejecución correcta del programa

        cout<<"Ejecución incorrecta del programa"<<endl;
        return 1;

    }

    //obtener nuestros N jugadores y M mesas
    int N = atoi(argv[1]); 
    int M = atoi(argv[2]);

    //Verificar que existan al menos 2 jugadores y que sea de a pares, y que exista al menos 1 mesa
    if( (N < 2) || (N%2 != 0) || (M < 1)){

        cout<<"Parametros de jugadores invalido (menos de 2 jugadores o jugadores impares)";
        cout<<" o cantidad de mesas invalida (menor que 1 mesa)"<<endl;
        return 1;

    }

    Configuracion configuracion;
    //Asignamos las posiciones predeterminadas para cada ficha en el struct de configuracion

    //posición predeterminada puma

    configuracion.filas[0] = 0;//trono como posición predeterminada
    configuracion.columnas[0] = 0;

    for(int i =1; i<=12; i++){ // se reutiliza logica de hiloArbitro en el posicionamiento por defecto de los perros

        if(i<=5){

            configuracion.filas[i] = 6; //ultima fila
            configuracion.columnas[i] = i - 1; //izq a der

        }else if(i<= 10){

            configuracion.filas[i] = 5; //penultima fila
            configuracion.columnas[i] = i - 6; //izq a der

        }else{

            configuracion.filas[i] = 4; //antepenultima fila
            configuracion.columnas[i] = i - 11; // izq a der

        }
    }
    //una vez establecida las posiciones por defectos, se intenta leer el archivo, en caso de fallar se usan las posiciones por defecto

    if(argc == 4){ //verificamos si se añadio un txt

        ifstream archivo (argv[3]); //txt esta en la pos 4 al ejecutar programa

        if(! archivo.is_open()){

            cout<<"Error al abrir archivo" <<endl;
            //simplemente se usaran las posiciones y semilla por defecto
        }else{

            Configuracion temp = configuracion; //se hace un temp en caso de que el txt no respete formato sugerido

            archivo >> temp.minMs>> temp.maxMs>>temp.seed; //se guarda en temp los primeros 3 valores, que son los dos de tiempo y la semilla

            for(int i= 0; i< 13; i++){

                archivo >>temp.filas[i] >>temp.columnas[i]; //obtenemos las posiciones de cada ficha, recordar pos 0 para puma, 1-12 perros

            }

            bool validar = !archivo.fail(); //verificamos tipo de valores correctos en el txt

            //verificar que las posiciones otorgadas en el txt, sean validas dentro del tablero
            for(int i = 0; (i< 13) && validar; i++){ // se continua el for mientras validar sea verdadero

                int fila = temp.filas[i];
                int columna = temp.columnas[i];

                if(fila < 0 || fila > 6 || columna < 0 || (fila == 0 && columna > 0) || (fila == 1&& columna > 1) || (fila >=2 &&columna > 4) ){
 
                    validar = false; //posición invalida en el txt

                }

                for(int j = 0; (j < i ) && validar ; j++){ //verificar que no se repitan posiciones de las fichas

                    if(fila == temp.filas[j] && columna == temp.columnas[j]){

                        validar = false;

                    }
                }

            }

            if(! validar || temp.minMs<= 0 || temp.maxMs <= temp.minMs){ //verificar tiempos validos

                    cout<< "Archivo txt invalido. Se utilizaran posiciones, tiempos y semilla predeterminados "<<endl;

                }else {

                    configuracion = temp; //Se pasaron todas las verificaciones, por lo que se cargan las nuevas configs

                }

        }

    }
    cout<<"SERVIDOR INICIADO"<<endl;
    cout<<"Jugadores:" <<N<<endl;
    cout<<"Mesas disponibles:" <<M <<endl;
    
    sem_t mesas; //Como se indica la existencia de M mesas, las mesas pasarian a ser un recurso limitado, por lo que se usara semaforos

    if(sem_init(&mesas, 0, M) != 0){//Verificar que el semaforo se inicie bien (En caso de exito, ya se inicializa dentro del if)

        cout<< "Error al iniciar  semaforo de mesas"<< endl;
        return 1;

    }

    int cantidadPartidas = N/2;

    
    vector<Partida*> partidas(cantidadPartidas); //Vector de punteros de partidas, con tamaño de cantidadPartidas, que de momento no apuntan a nada
    vector<pthread_t> arbitros(cantidadPartidas);// Vector de donde se almacenaran los hilos de los arbitros
    vector<ArgsArbitro> argsArbitros(cantidadPartidas);//Almacena las estructuras de los Arbitros, de tamaño la cantidad de partidas


    int contArbitros = 0; //contar la cantidad de arbitros creados nos servira para saber si se crearon todos los arbitros correctamente
    //SI alguno no se creo, igual podremos esperar a los que si se crearon sin cerrar todo el programa

    //Creación de partidas

    for(int i = 0; i<cantidadPartidas; i++){

        partidas[i] = new Partida(i + 1 , 2*i + 1, 2*i + 2); //recordar el new para punteros de objetos
        //parametro 1: id partida, parametro 2: idJugadorPuma, parametro 3: idJugadorPerros
        //id partida parte desde 1

        argsArbitros[i].partida = partidas[i]; //Asignar una partida a un arbitro
        argsArbitros[i].mesas = &mesas; //todos los arbitros comparten el mismo semaforo

        argsArbitros[i].config = &configuracion; //todos los arbitros compparten la misma config

        argsArbitros[i].minMs = configuracion.minMs; //Retardo minimo de las piezas
        argsArbitros[i].maxMs = configuracion.maxMs; //Retardo maximo de las piezas
        argsArbitros[i].seed = configuracion.seed + (unsigned int)(i * 13); //Semilla distinta para cada partida
        // el i*13 nos ayuda a que en caso de tener 2 arbitros con la misma semilla, igual generar semillas diferentes entre los 2 (13 piezas)

        if(pthread_create(&arbitros[i], NULL, hiloArbitro, &argsArbitros[i]) != 0){ // !=0 si da error

            cout<<"Error al crear el arbitro "<<i << endl;

            break;
        }

        contArbitros++;
    }

    //Esperamos a que terminen todos los arbitros creados
    for(int i = 0; i<contArbitros; i++){

        pthread_join(arbitros[i], NULL);

    }

    //Liberamos la memoria de todas las partidas creadas
    for(int i = 0; i < cantidadPartidas; i++){

        delete partidas[i];

    }


    sem_destroy(&mesas);
    return 0;
}