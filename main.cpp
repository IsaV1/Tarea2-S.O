#include <iostream>
#include <vector>
#include <utility>
#include <cstdlib>
#include <time.h>
#include <pthread.h>
#include <semaphore.h> //para poder usar semaforos
#include <fstream> //Lectura txt
#include <signal.h> //biblioteca de C para manejar señales del sistema operativo
#include <cerrno> //para revisar errores al convertir numeros (verificar N y M)
#include <climits> //para verificar que no superemos el maximo de un int (verificar N y M)
#include <atomic> //para poder compartir la señal de cierre entre todos los hilos
#include <new> //para poder comprobar si falla la reserva de memoria
#include <stdexcept> //para poder lanzar errores desde los constructores y no usar exit(1) y que no se cierre todo el programa
using namespace std;


pthread_mutex_t lock_cout = PTHREAD_MUTEX_INITIALIZER; //Es mas estetico, pero existe una condición de carrera
//al momento de imprimir los movimientos del tablero, no afecta el funcionamiento, solo en como se imprime en consola, 
//asi que para evitar las condiciones de carrera se crea este mutex

atomic<bool> detenerServidor(false); //indica si se solicito detener el servidor con Ctrl+C (SIGINT)
//verificamos que la variable atomica no necesite bloqueos internos, ya que se modifica desde SIGINT
static_assert(atomic<bool>::is_always_lock_free, "Error con la variable atomica");


void manejarSIGINT(int señal){

    (void)señal; //para evitar el warning de parametro sin utilizar cuando compilamos

    detenerServidor.store(true); //avisamos a los demas hilos que se solicito cerrar el servidor

}

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

        //verificamos que el mutex del tablero se haya iniciado correctamente
        if(pthread_mutex_init(&lock_tablero, NULL) != 0){

            cout<<"Error al iniciar el mutex del tablero"<<endl;
            throw runtime_error("Error al iniciar el mutex del tablero"); //no podemos seguir usando el tablero si su mutex fallo

        }

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
    int jugadorAbandono; //-1 por defecto

    pthread_mutex_t lock_partida;

public:
    // constructor
    Partida(int id, int idJP, int idJPe) : idPartida(id), idJugadorPuma(idJP), idJugadorPerros(idJPe), 
    terminado(false), ganador(-1), iniciado(false), jugadorAbandono(-1)
    {
        //verificamos que el mutex de la partida se inicie correctamente
        if(pthread_mutex_init(&lock_partida, NULL) != 0){

            cout<<"Error al iniciar el mutex de la partida"<<endl;
            throw runtime_error("Error al iniciar el mutex de la partida"); //no podemos continuar si falla el mutex

        }

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

    bool getIniciado(){

        //mutex porque existenn condiciones de carrera
        pthread_mutex_lock(&lock_partida);
        bool resultado = iniciado;
        pthread_mutex_unlock(&lock_partida);

        return resultado;
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

    bool cancelarPartida(){ //cancelamos una partida en caso de un error interno o un SIGINT

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

    bool abandonarPartida(int idJugador){ //metodo para terminar una partida y darle la victoria al jugador opuesto

        pthread_mutex_lock(&lock_partida); //necesario debido a las race conditions

        if(terminado){ //La partida ya termino anteriormente, por lo que la partida no se puede abandonar ya

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        //condiciones para ver quien abandono y asignar ganador por defecto

        if(idJugador == idJugadorPuma){

            ganador = idJugadorPerros;

        }else if(idJugador == idJugadorPerros){

            ganador = idJugadorPuma;

        }else{ //idjugador no pertenece a la partida

            pthread_mutex_unlock(&lock_partida);
            return false;

        }

        jugadorAbandono = idJugador;
        terminado = true;
        pthread_mutex_unlock(&lock_partida);
        return true;
    }

    int getJugadorAbandono(){

        pthread_mutex_lock(&lock_partida);
        int resultado = jugadorAbandono;
        pthread_mutex_unlock(&lock_partida);

        return resultado;
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


struct ColaJugadores{ //struct para el emparejamiento de los jugadores que vayan llegando

    vector<int> esperando; //Jugadores que aun no tienen oponente
    vector<pair<int,int>> parejas; //vector de pares con los jugadores que ya tienen oponente
    pthread_mutex_t lock_cola; //necesario ya que las modificaciones a los vectores tienen condiciones de carrera
    pthread_cond_t cond_parejas; //variable de condicion para avisar cuando se forme una nueva pareja
    //permite que main inicie las partidas sin esperar a que lleguen todos los jugadores (siempre y cuando hayan mesas disponibles)
    vector<Partida*> partidasJugador; //Para saber a qu partida esta metida cada jugador
    bool matchmakingCompletado = false; //servira para saber si quedan partidas por armar

};

struct ArgsJugador{

    int idJugador;
    ColaJugadores* cola; //puntero de colajugadores que todos compartiran
    unsigned int seed; //Usamos seed para crear un % de que un jugador abandone la partida

};

static void dormirMs(int ms){
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void* hiloJugador(void* arg){

    ArgsJugador* a = (ArgsJugador*)arg;

    pthread_mutex_lock (&a -> cola -> lock_cola); //bloqueamos el recurso dentro de la propiedad cola, que corresponde a los vectores de colaJugadores


    //verificamos si se hizo un SIGINT antes de empezar a añadir jugadores a los vectores de cola
    if(detenerServidor){

        pthread_mutex_unlock( &a -> cola -> lock_cola);//liberamos el mutex recien adquirido si se cancelo el servidor

        return NULL; //terminar hilo
    }


    if( a -> cola -> esperando.empty() ){ //verificamos el caso de que no haya ningun jugador esperando en la cola
        //Si no hay ningun jugador, añadimos el jugador que acaba de llegar a la cola esperando

        a -> cola -> esperando.push_back(a -> idJugador);

    }else{ //Si hay un jugador esperando un openente en la cola

        int oponente = a -> cola -> esperando.front(); //obtonemos el id del jugador esperando
        //hacemos la pareja
        a -> cola -> parejas.push_back( make_pair (oponente, a -> idJugador) );
        //sacamos al jugador que estaba esperando en el vector cola
        a -> cola -> esperando.erase( a -> cola -> esperando.begin() );
        //para erase es necesario usar begin()


        pthread_cond_broadcast( &a -> cola -> cond_parejas);//avisamos a todos los hilos que esten esperando cambios en los vectores de cola
        //es necesario el broadcast debido a la posibilidad de un deadlock si el main y los jugadores quedan esperando la misma variable
    }

    //esperamos hasta que el jugador tenga una partida asignada, recordar que por defecto su valor es nullptr
    //tambien existe la posibilidad de que un error aparte como la creación del arbitro, por eso se añade una segunda condición
    while( a -> cola -> partidasJugador[ a -> idJugador ] == nullptr && !( a -> cola -> matchmakingCompletado )){
 
        pthread_cond_wait(&a->cola->cond_parejas, &a->cola->lock_cola);
    
    }

    Partida* partida = a -> cola ->partidasJugador[a -> idJugador]; //obtenemos la partida a la que esta asociada el jugador

    pthread_mutex_unlock(&a -> cola -> lock_cola); //desbloqueamos el recurso
    //si nunca se le asigno una partida a un jugador tendra partida == nullptr, lo que significa que hubo un error o nunca tuvo oponente

    if(partida == nullptr){

        return NULL;

    }

    //esta parte es para la situación en la que un jugador podria abandonar la partida
    //considerando que no hay jugadores reales interactuando, por lo que la unica otra forma que un jugador
    //abandone una partida es que exista algun fallo en el hilo o una probabilidad aleatoria como la siguiente
    while(! partida->getTerminado() ){

        dormirMs(100); //cada 100ms el hilo jugador verificara si la partida comenzo

        if( (partida -> getIniciado() ) && ( !partida->getTerminado() ) && ( !detenerServidor ) ){
            //si la partida comenzo y el servidor no esta detenido se entra a la condición
            //y solo se puede abandonar si la partida comenzo, sigue activa y no hay ninguna señal SIGINT

            //existe un 1% aprox a que se cumpla la condición, si se cumple, el jugador abandonara la partida
            //otorgandole la victoria al oponente (al encontrar un numero dividido por 100)
            if(rand_r(&a->seed) % 100 == 0){

                partida->abandonarPartida(a->idJugador);

            }
        }
    }

    return NULL;

}

void* hiloPieza(void* arg){

    ArgsPieza* a = (ArgsPieza*)arg;

    vector<pair<int,int>> destinos;
    vector<pair<int,int>> destinosCaptura;

    while(!a->partida->getTerminado()){

        //verificamos en cada iteriación si se dio una señal SIGINT

        if(detenerServidor){

            a -> partida -> cancelarPartida(); //actualizamos la partida como terminada para que finalizen los hilos de piezas

            break; //terminamos la ejecución por completo del hilo

        }

        //retardo aleatorio entre minMs y maxMs y ademas para evitar desbordamiento de int usamos 1U
        //calculamos el rango de tiempos sin provocar desbordamientos
        unsigned int rango = (unsigned int)(a->maxMs - a->minMs) + 1U;
        int espera = a->minMs + (int)(rand_r(&a->seed) % rango);

        int tiempoRestante = espera;

        //en vez de dormir todo el tiempo de una sola vez, vamos durmiendo de a 100ms
        //asi si la partida termina o llega un SIGINT, el hilo puede salir sin esperar tanto
        while(tiempoRestante > 0 && !detenerServidor && !a -> partida -> getTerminado()){

            int dormir = tiempoRestante;

            if(dormir > 100){
                dormir = 100;
            }

            dormirMs(dormir);
            tiempoRestante -= dormir;

        }

        //si se dio un SIGINT mientras el hilo estaba durmiendo
        if(detenerServidor){

            a -> partida -> cancelarPartida(); //cancelamos la partida para que los demas hilos tambien terminen

            break;

        }


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

    //en caso de que el haya un SIGINT y el arbitro este esperando una mesa a que se desocupe
    if(detenerServidor ){ 
 
        a -> partida -> cancelarPartida(); //actualizamos variable

        return NULL;//terminamos el hilo del arbitro
    }

    //Los arbitros manejaran las mesas con los jugadores, por lo que debe esperar a que alguna mesa este disponible

    if(sem_wait(a -> mesas)!= 0){

        //se podria usar solo sem_wait(a -> mesas), pero existe la posibilidad de que falle

        pthread_mutex_lock(&lock_cout);
        cout<<"Error al esperar una mesa disponible" << endl;
        pthread_mutex_unlock(&lock_cout);

        a -> partida -> cancelarPartida();
        return NULL;

    }

    //caso de SIGINT cuando un arbitro este haya tomado una mesa

    if(detenerServidor || a -> partida -> getTerminado()){//tambien verificar si la partida se cancelo anteriormente para devolver la mesa

        a -> partida -> cancelarPartida(); //actualizamos variables

        sem_post( a-> mesas ); //se devuelve la mesa que reservo el arbitro

        return NULL;//terminamos el hilo del arbitro
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

            pthread_mutex_lock(&lock_cout);
            cout<<"Error al colocar la pieza " << idPieza << endl;
            pthread_mutex_unlock(&lock_cout);
            a -> partida -> cancelarPartida();
            sem_post(a->mesas);
            return NULL;

        }
    }



    //como ya se pusieron todas las fichas en el tablero de manera exitosa, iniciamos la partida

    if(! a -> partida -> iniciarPartida()){ //Primero iniciamos la partida, y luego se comprueba la condición si es verdadera o falsa

        pthread_mutex_lock(&lock_cout);
        cout<< "Error al iniciar la partida"<<endl;
        pthread_mutex_unlock(&lock_cout);

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

            pthread_mutex_lock(&lock_cout);
            cout<<"Error al crear el hilo de la pieza "<<i << endl;
            pthread_mutex_unlock(&lock_cout);

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
    int jugadorAbandono = a -> partida -> getJugadorAbandono();
    //Si un jugador abandono, el ganador de la partida sera su oponente

    pthread_mutex_lock(&lock_cout); //bloqueamos la consola para que los mensajes del arbitro no se mezclen con los tableros

    if(ganador == -1){

        cout<<"Partida cancelada" << endl;

    }else if(jugadorAbandono != -1 ){

        cout<<"Partida finalizada por abandono del jugador: " <<jugadorAbandono <<endl;
        cout<<"El ganador es jugador id: " <<ganador <<endl;

    }else{

        cout<<"Partida finalizada, ganador id: "<< ganador<< endl;

    }

    pthread_mutex_unlock(&lock_cout); //liberamos la consola

    sem_post(a -> mesas); //Liberamos la mesa para que otros arbitros puedan acceder
    return NULL;
}

int main(int argc, char** argv){ //no borrar main o cambiar lo principal

    signal(SIGINT, manejarSIGINT); //para el manejo del ctrl+c


    if(argc < 3 || argc > 4){ //verificar ejecución correcta del programa

        cout<<"Ejecución incorrecta del programa"<<endl;
        return 1;

    }

    //obtener nuestros N jugadores y M mesas usamos strtol para verificar que realmente sean numeros validos

    char* finN;
    char* finM;

    errno = 0;
    long numN = strtol(argv[1], &finN, 10);

    bool errorN = (errno != 0 || *finN != '\0' || numN < 2 || numN > INT_MAX || numN % 2 != 0);

    errno = 0;
    long numM = strtol(argv[2], &finM, 10);

    bool errorM = (errno != 0 || *finM != '\0' || numM < 1 || numM > INT_MAX);

    //verificamos que los jugadores sean pares y que exista al menos una mesa tambien que no se hayan ingresado letras o numeros demasiado grandes
    if(errorN || errorM){

        cout<<"Parametros de jugadores o mesas invalidos"<<endl;
        return 1;

    }

    int N = (int)numN;
    int M = (int)numM;


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

            //verificamos que no haya contenido adicional despues de leer todos los valores del txt
            if(validar){

                char extra;

                if(archivo >> extra){ //si podemos leer algo mas, significa que habia contenido sobrante

                    validar = false;
                }
            }

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

    ColaJugadores cola;

    //intentamos reservar la memoria para guardar las partidas de los jugadores
    try{

        cola.partidasJugador.resize(N + 1, nullptr);//nullptr, ya que aun ningun jugador esta asociado a ninguna partida

        //reservamos memoria antes de crear los hilos de jugadores
        cola.esperando.reserve(1);
        cola.parejas.reserve(N / 2);


    }catch(...){

        cout<<"Error al reservar memoria para los jugadores"<<endl;

        sem_destroy(&mesas); //liberamos el semaforo que ya habiamos creado
        return 1;

    }

    //iniciamos el mutex de la cola y verificamos si hubo algun error
    if(pthread_mutex_init(&cola.lock_cola, NULL) != 0){

        cout<<"Error al iniciar el mutex de la cola"<<endl;
        sem_destroy(&mesas); //liberamos el semaforo que ya habiamos creado
        return 1;

    }

    //hacemos lo mismo con la variable de condicion
    if(pthread_cond_init(&cola.cond_parejas, NULL) != 0){

        cout<<"Error al iniciar la variable de condicion"<<endl;

        pthread_mutex_destroy(&cola.lock_cola); //liberamos el mutex que ya se habia creado
        sem_destroy(&mesas);

        return 1;

    }

    vector<pthread_t> jugadores;//vector de hilos
    vector<ArgsJugador> argsJugadores;//almacenamos los argumentos de los jugadores

    //intentamos reservar memoria para los hilos de jugadores y sus argumentos
    try{

        jugadores.resize(N);
        argsJugadores.resize(N);

    }catch(...){

        cout<<"Error al reservar memoria para los hilos de jugadores"<<endl;

        //liberamos los recursos que ya habiamos iniciado
        pthread_cond_destroy(&cola.cond_parejas);
        pthread_mutex_destroy(&cola.lock_cola);
        sem_destroy(&mesas);

        return 1;

    }


    int contJugadores = 0; //contador para los jugadores que si se alcazaron a crear antes de un posible error

    for(int i = 0; i<N; i++){ //ciclo con N iteraciones correcspontiente a la cantidad de jugadores

        //verificamos al principio de cada iteración si hubo una señal SIGINT

        if(detenerServidor){

            break; //paramos de hacer nuevos jugadores

        }

        //Asignamos a cada jugador su argumento propio
        argsJugadores[i].idJugador = i + 1; // comenzamos los id desde el 1
        argsJugadores[i].cola = &cola;
        argsJugadores[i].seed = configuracion.seed + 1000 + (unsigned int)i;  //semilla distinta para cada jugador, separada de las semillas de las piezas



        //hilo propio para cada jugador y luego se verifica
        if(pthread_create(&jugadores[i], NULL, hiloJugador, &argsJugadores[i]) != 0){ //!= si da error

            cout << "Error al crear el hilo del jugador " << i + 1 << endl;
            break;

        }

        contJugadores++;

    }

    int cantidadPartidas = contJugadores / 2; //cantidad de parejas con los jugadores que si lograron crearse de manera correcta

    
    vector<Partida*> partidas; //Vector de punteros de partidas, que de momento no apuntan a nada
    vector<pthread_t> arbitros;// Vector de donde se almacenaran los hilos de los arbitros
    vector<ArgsArbitro> argsArbitros;//Almacena las estructuras de los Arbitros

    //intentamos reservar memoria para las partidas y sus arbitros
    try{

        partidas.resize(cantidadPartidas, nullptr);
        arbitros.resize(cantidadPartidas);
        argsArbitros.resize(cantidadPartidas);

    }catch(...){

        cout<<"Error al reservar memoria para las partidas"<<endl;

        //avisamos a los jugadores que no se crearan mas partidas
        pthread_mutex_lock(&cola.lock_cola);

        cola.matchmakingCompletado = true;
        pthread_cond_broadcast(&cola.cond_parejas);

        pthread_mutex_unlock(&cola.lock_cola);

        //esperamos a que todos los jugadores terminen antes de liberar recursos
        for(int i = 0; i < contJugadores; i++){

            pthread_join(jugadores[i], NULL);

        }

        pthread_cond_destroy(&cola.cond_parejas);
        pthread_mutex_destroy(&cola.lock_cola);
        sem_destroy(&mesas);

        return 1;

    }    

    //int contArbitros = 0; //contar la cantidad de arbitros creados nos servira para saber si se crearon todos los arbitros correctamente,
    //SI alguno no se creo, igual podremos esperar a los que si se crearon sin cerrar todo el programa

    //Creación de partidas

    for(int i = 0; i<cantidadPartidas; i++){

        pthread_mutex_lock(&cola.lock_cola); //bloqueamos la cola para revisar cuantas parejas hay disponibles para crear partidas
        //existen condiciones de carrera al revisar el vector de colas, por lo que es necesario el mutex

        while( i >= (int)cola.parejas.size() && !detenerServidor ){ // si i es mayor o igual que el tamaño del vector, significa que aun no se forma una nueva pareja
            //para crear una nueva partida, por lo que se espera hasta que exista una nueva pareja
            //tambien importante verificar que no se haya solicitado la detención del servidor

            struct timespec limite; //almacenamos el tiempo limite de espera

            clock_gettime(CLOCK_REALTIME, &limite); //obtenemos la hora actual

            limite.tv_sec += 1; //permitimos esperar como maximo un segundo

            pthread_cond_timedwait(&cola.cond_parejas, &cola.lock_cola, &limite);
            //al usar cond, podemos desbloquear el mutex, para que cuando exista una modificación, despierte y vuelva a adquirir el mutex
            //y asi continuar con la ejecución


            //desbloqueamos temporalmente el mutex y esperamos una pareja o hasta que se cumpla el tiempo

            //la logica del bloque, es que si solo se usa wait, existe la posibilidad de que el hilo haya entrado en espera
            //y que despues se de la señal SIGINT, pero el hilo seguira esperando, asi que nuca cancelara su ejecuion, por eso con timedwait
            //el hilo despertara cada cierto tiempo para verificar ese estado

        }

        if(detenerServidor){ //verificamos si justo se solicito detención despues de encontrar un par para una partida

            pthread_mutex_unlock(&cola.lock_cola); //liberamos el mutex antes de salir del ciclo

            break; //dejamos de crear nuevas partidas

        }
        
        //obtenemos la pareja
        int jugadorPuma = cola.parejas[i].first;
        int jugadorPerros = cola.parejas[i].second;

        //desbloqueamos el recurso de colas
        pthread_mutex_unlock(&cola.lock_cola);

        //imprimimos la nueva pareja que se acaba de formar, existen race conditions asi que se bloquea el recurso temporalmente
        pthread_mutex_lock(&lock_cout);

        cout<<"Nueva pareja formada." <<endl;
        cout<<"Pareja: " << i+1 <<", JugadorPuma: "<<jugadorPuma << ", JugadorPerro: "<< jugadorPerros <<endl;

        pthread_mutex_unlock(&lock_cout);

        //intentamos crear la partida, verificando si ocurre algun error de memoria
        try{

            partidas[i] = new Partida(i + 1, jugadorPuma, jugadorPerros); //recordar new para objeto de punteros

        }catch(...){

            partidas[i] = nullptr; //si falla la partida no apuntara a nada y en la siguiente condición se verifica

        }

        //verificamos si se pudo reservar memoria para la partida
        if(partidas[i] == nullptr){

            pthread_mutex_lock(&lock_cout);
            cout<<"Error al crear la partida"<<endl;
            pthread_mutex_unlock(&lock_cout);

            break; //dejamos de crear partidas nuevas

        }


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

            pthread_mutex_lock(&lock_cout); //evitamos que el mensaje de error se mezcle con los movimientos de otras partidas
            cout<<"Error al crear el arbitro "<<i +1<< endl;
            pthread_mutex_unlock(&lock_cout);

            delete partidas[i]; //liberamos la partida que no pudo ejecutarse

            partidas[i] = nullptr; //para dejar con un puntero por defecto

            continue;//seguimos intentando crear nuevos arbitros
        }

        pthread_mutex_lock(&cola.lock_cola);//lock para acceder a los vectores de cola

        cola.partidasJugador[jugadorPuma] = partidas[i];//asignamos su partida al par de jugadores correspondiente
        cola.partidasJugador[jugadorPerros] = partidas[i];


        pthread_cond_broadcast(&cola.cond_parejas); //avisamos que hay jugadores con partidas asignadas
        //usamos broadcast con el cond ppara despertar a todos los hilos que estaban esperando esta condición

        pthread_mutex_unlock(&cola.lock_cola);



    }

    //Ahora verificamos si hubo una cancelación del torneo antes de que terminen todas las partidas

    if(detenerServidor){

        //buscamos todas las partidas que se crearon para cancelarlas
        for(int i = 0; i<cantidadPartidas; i++){

            //verificamos que en la posición i, exista una partida
            if(partidas[i] != nullptr){

                partidas[i] -> cancelarPartida(); //cancelamos la partida para que finalicen todos los hilos

            }

        }

    }


    //indicamos que ya se intentaron crear todas las partidas (si falla alguna, simplemente no se reintenta, asi que se pierde esa partida)
    //y quedan ambos jugadores sin jugar
    pthread_mutex_lock(&cola.lock_cola); //como accedemos a una zona con race conditions es necesario los bloqueos

    cola.matchmakingCompletado = true; //actualizamos a true, indicando que ya se terminaron de crear todas las partidas, incluso si alguna falla

    pthread_cond_broadcast(&cola.cond_parejas); //despertamos a los jugadores que aun siguen esperando

    pthread_mutex_unlock(&cola.lock_cola);

    //Esperamos a que todos los hilos de jugadores terminen de ejecutarse
    //en este caso al esperar, significa que ya se formaron las parejas de los hilos que si se crearon
    //con posibilidad de que un jugador quede sin oponente en caso de que falle la creacion de una cantidad impar de hilos,
    for(int i = 0; i < contJugadores; i++){ //se usa contJugadores, ya que no hay certeza total de que todos los N jugadores se hayan creado correctamente

        pthread_join(jugadores[i], NULL);

    }


    //Esperamos a que terminen todos los arbitros creados
    for(int i = 0; i<cantidadPartidas; i++){

        if(partidas[i]!=nullptr){//las partidas que se crearon correctamente esperaran a que terminen todos los arbitros de ejecutarse

            pthread_join(arbitros[i], NULL);

        }

    }

    //Liberamos la memoria de todas las partidas creadas
    for(int i = 0; i < cantidadPartidas; i++){

        delete partidas[i];

    }

    //liberación de recursos
    sem_destroy(&mesas);
    pthread_mutex_destroy(&cola.lock_cola);
    pthread_cond_destroy(&cola.cond_parejas); // destruimos la variable de condicion al finalizar el torneo

    if(detenerServidor){ //verificamos si se cancelo el torneo antes de terminar de cerrar el programa, para asi imprimir que se cancelo

        cout<<"SIGINT detectado, servidor detenido "<<endl;

    }

    return 0;
}