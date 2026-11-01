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

public:
    // No es necesario hacerle un constructor al vector (se inicializan al hacer el tablero)

    void estadoActual(){

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

    }

    bool movimientoValido(int filaOrigen, int columnaOrigen, int filaDestino, int columnaDestino){

        //Verificar que existan las posiciones de los argumentos

        if((filaOrigen < 0 || filaOrigen > 6) || (columnaOrigen < 0 || columnaOrigen > 5) || (filaDestino < 0 || filaDestino > 6) || (columnaDestino < 0 || columnaDestino > 5)){
            return false;
        }

        //continuar











    }

};

int main(){

    Tablero test;
    test.estadoActual();

    return 0;
}