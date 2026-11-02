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
        for(int i = 0; i<filas.size(); i++){

            if(i == 0){ //Fila de una sola casilla

                cout<<"    "<<filas[0][0]<<"  "<<endl;

            }else if(i == 1){

                for(int j = 0; j<filas[i].size(); j++){

                    cout<<"  ";
                    cout<<filas[i][j];
                    cout<<" ";

                }
                cout<<endl;

            }else{

                for(int j = 0; j<filas[i].size(); j++){

                    cout<<filas[i][j]<<" ";

                }

                cout<<endl;

            }
        }
        pthread_mutex_unlock(&lock_tablero); //Salida de la sección crítica

    }

    bool movimientoValido(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){
        
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

    bool movimientoValidoPerro(){}
    bool movimientoValidoPuma(){}

};

int main(){

    Tablero test;

}