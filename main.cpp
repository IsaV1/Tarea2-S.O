#include <iostream>
#include <vector>
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
 
private:
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
 
    //si es un salto de captura, deja captura = true y la casilla del perro en (fMid, cMid)
    bool validarPumaSinLock(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino,
                            bool& captura, int& fMid, int& cMid) {
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
        int fm = (filaOrigen + filaDestino) / 2;
        int cm = (columnaOrigen + columnaDestino) / 2;
 
        //verificamos que la casilla intermedia este dentro de los límites
        if (fm >= 0 && fm < (int)filas.size() && 
            cm >= 0 && cm < (int)filas[fm].size()) {
            
            int piezaMedia = filas[fm][cm];
            if (piezaMedia >= 1 && piezaMedia <= 12 && 
                this->movimientoValidoBase(filaOrigen, columnaOrigen, fm, cm) && 
                this->movimientoValidoBase(fm, cm, filaDestino, columnaDestino)) {
                captura = true;
                fMid = fm;
                cMid = cm;
                return true;
            }
        }
 
        return false;
    }
 
};


// Pruebassss

struct IntentoPerro {
    Tablero* tablero;
    int fo, co, fd, cd;
    bool resultado;
};
 
void* hiloMoverPerro(void* arg) {
    IntentoPerro* a = (IntentoPerro*)arg;
    a->resultado = a->tablero->moverPerro(a->fo, a->co, a->fd, a->cd);
    return NULL;
}
 
int main(){
 
    Tablero t;
    t.colocarPieza(3, 0, -1); //puma
    t.colocarPieza(3, 1, 1); //perro 1 que va a ser capturado
    t.colocarPieza(4, 4, 2); //perro 2
 
    cout << "Tablero inicial:" << endl;
    t.estadoActual();
 
    cout << "Perro 2 (4,4)->(4,3): " << (t.moverPerro(4, 4, 4, 3) ? "OK" : "FALLA") << endl;
    cout << "Perro 2 (4,4)->(4,3) otra vez (debe ser invalido): " << (t.moverPerro(4, 4, 4, 3) ? "FALLA" : "OK") << endl;
    cout << "Puma salta (3,0)->(3,2): " << (t.moverPuma(3, 0, 3, 2) ? "OK" : "FALLA") << endl;
    cout << "Capturas: " << t.getCapturas() << " (esperado 1)" << endl;
    t.estadoActual();
 
    //Carrera: dos perros disputan la misma casilla, solo uno debe ganarla
    int errores = 0;
    for (int i = 0; i < 2000; i++) {
        Tablero r;
        r.colocarPieza(3, 1, 1);
        r.colocarPieza(3, 3, 2);
 
        IntentoPerro a = {&r, 3, 1, 3, 2, false};
        IntentoPerro b = {&r, 3, 3, 3, 2, false};
 
        pthread_t ha, hb;
        pthread_create(&ha, NULL, hiloMoverPerro, &a);
        pthread_create(&hb, NULL, hiloMoverPerro, &b);
        pthread_join(ha, NULL);
        pthread_join(hb, NULL);
 
        if ((a.resultado ? 1 : 0) + (b.resultado ? 1 : 0) != 1) {
            errores++;
        }
    }
    cout << "Carreras con resultado incorrecto: " << errores << " de 2000 (esperado 0)" << endl;
 
    return 0;
}