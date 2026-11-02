#include <iostream>
#include <vector>
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
        pthread_mutex_lock(&lock_tablero); // Sección crítica protegida con POSIX
        //Verificar que existan las posiciones de los argumentos

        // Verificar que existan las filas (índices válidos)
        if (filaOrigen < 0 || filaOrigen >= (int)filas.size() || 
            filaDestino < 0 || filaDestino >= (int)filas.size()) {
            pthread_mutex_unlock(&lock_tablero);
            return false;
        }

        // Verificar límites de columnas adaptado
        if (columnaOrigen < 0 || columnaOrigen >= (int)filas[filaOrigen].size() || 
            columnaDestino < 0 || columnaDestino >= (int)filas[filaDestino].size()) {
            pthread_mutex_unlock(&lock_tablero);
            return false;
        }

        //continuar








        pthread_mutex_unlock(&lock_tablero);
        return true;
    }

};

int main(){

    Tablero test;
    test.estadoActual();

    return 0;
}