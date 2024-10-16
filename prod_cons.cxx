/*
DESCRIPCION: Implementacion del problema de PRODUCTOR-CONSUMIDOR

AUTOR: Martín Alejandro Fernandez - Ingenieria en Computacion - UNT-FACET
*/

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
/* 
    DEFINICION DE UN ARCHIVO LOCAL COMO RECURSO NO COMPARTIDO QUE SERA 
    ACCEDIDO POR EL PRODUCTOR Y EL CONSUMIDOR 
*/
#define BUFFER "./buffer"
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
// SEMUN ESTA DEFINIDA EN <sys/sem.h>
#else
union semun {                          
    int val;                             // valor   para SETVAL
    struct semid_ds *buf;                // buffer  para IPC_STAT, IPC_SET
    unsigned short int *array;           // arreglo para GETALL, SETALL
    struct seminfo *__buf;               // buffer  para IPC_INFO
};
#endif
using namespace std;
/* 
    EL USUARIO DEBE INGRESAR UN ENTERO ENTRE 1 y 10 QUE REPRESENTA
    LOS SEGUNDOS QUE CADA PROCESO ESPERARA ENTRE LECTURAS Y/O ESCRITURAS
*/
int main(int argc, char *argv[ ]) {
    FILE           *fptr;
    
    // ESTRUCTURA REPRESENTAR LAS OPERACIONES SOBRE LOS SEMAFOROS
    static struct sembuf acquire = {0, -1, SEM_UNDO}, // ADQUIRIR SEMAFORO  
                         release = {0,  1, SEM_UNDO}; // LIBERAR SEMAFORO
    pid_t           c_pid;  // PID DEL CONSUMIDOR
    key_t           ipc_key; // DEFINICION DE LA KEY PARA GENERAR UN SEMAFORO

    //ARREGLO QUE INICIALIZARA LOS VALORES DE LOS SEMAFOROS
    static unsigned short   start_val[2] = {1, 0};
    int             semid, producer = 0, i, n, p_sleep, c_sleep;
    union semun     arg;
    
    // ENUMERACION DE CONSTANTES PARA INDICAR CUAL SEMAFORO SE ESTA UTILIZANDO
    enum { READ, MADE };
    
    // CONTROL DE DATOS INGRESADOS POR EL USUARIO DESDE LA SHELL
    if (argc != 2) {
	    cerr << argv[0] <<  " sleep_time" << endl;
        return 1;
    }
    // GENERACION DE KEY PARA CREAR EL RECURSO IPC DE TIPO SEMAFORO
    ipc_key = ftok(".", 'S');

    //SE ESTABLECE QUE EL PRODUCTOR EJECUTA PRIMERO. PARA CREAR UN ARREGLO DE SEMAFOROS 
    if ((semid=semget(ipc_key, 2, IPC_CREAT|IPC_EXCL|0660)) != -1) {
	producer = 1;
	arg.array = start_val;

    // INICIALIZA LOS SEMAFOROS CON LOS VALORES DE START_VAL
    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl--productor--inicializacion");
        return 2;
    }
    // SI EL PROCESO QUE EJECUTA ES EL CONSUMIDOR, EL SEMAFORO YA ESTA CREADO, SOLO ACCEDE
    } 
    else if ((semid = semget(ipc_key, 2, 0)) == -1) {
        perror("semget--consumidor--obteniendo semaforo");
        return 3;
    }
    
    cout << (producer==1 ? "Productor" : "Consumidor" ) << " comenzando" << endl;
    switch (producer) {
        // CODIGO CORRESPONDIENTE AL PROCESO PRODUCTOR
	    case 1:                             
    	    p_sleep = atoi(argv[1]);
            // GENERACION DE UN NUMERO ALEATORIO EN BASE AL PID DEL PRODUCTOR COMO SEMILLA
            srand((unsigned) getpid());
            // PRODUCTOR GENERA 10 NUMEROS ALEATORIOS QUE SERAN LEIDOS POR EL CONSUMIDOR
            for (i = 0; i < 10; i++) {
                // PRODUCTOR SE BLOQUEA (SLEEP) LA CANTIDAD DE SEGUNDOS INDICADA
        	    sleep(p_sleep);
                // GENERA UN NUMERO ALEATORIO ENTRE 1 Y 99
        	    n = rand() % 99 + 1;
        	    cout << "PRODUCTOR GENERO EL NUMERO [" << n <<"]" << endl;
                // EL PRODUCTOR INTENTARA OPERAR EN EL PRIMER SEMAFORO DEL ARREGLO
                acquire.sem_num = READ;
                // EL PRODUCTOR INTENTA DECREMENTAR EL PRIMER SEMAFORO. SI ESTE ES CERO, EL RECURSO ESTA SIENDO ACCEDIDO POR EL CONSUMIDOR Y SEMOP DEVUELVE -1
            	if (semop(semid, &acquire, 1) == -1) {
            	    perror("semop -productor- esperando al consumidor para leer numero");
            	    return 4;
            	}
                //ABRE EL RECURSO PARA ESCRITURA (ES UN ARCHIVO)
        	    if ((fptr = fopen(BUFFER, "w")) == NULL ){
                    perror(BUFFER);
                    return 5;
        	    }                 
                // >>> INICIO:  SECCION CRITICA <<<
        	    fprintf(fptr, "%d\n", n);
        	    fclose(fptr);
                // >>> FINAL:  SECCION CRITICA <<<
                // EL PRODUCTOR INTENTARA ACCEDER AL SEGUNDO SEMAFORO
        	    release.sem_num = MADE;
                cout << "PRODUCTOR DEPOSITO EL NUMERO [" << n <<"]" << endl;
                // EL PRODUCTOR INCREMENTA EL SEGUNDO SEMAFORO PARA INDICAR AL CONSUMIDOR QUE EL NUMERO YA ESTA DISPONIBLE PARA LECTURA
        	    if (semop(semid, &release, 1) == -1) {
                    perror("semop -productor- indicando nuevo numero generado");
            	    return 6;
        	    }
            }
            // EL PRODUCTOR ESPERA 5 SEGUNDOS PARA QUE EL ULTIMO VALOR GENERADO SEA LEIDO
            sleep(5);
            // EL PRODUCTOR BORRA O REMUEVE EL ARREGLO DE SEMAFOROS GENERADO 
            if (semctl(semid, 0, IPC_RMID, 0) == -1) {
        	    perror("semctl productor-");
        	    return 7;
            }
            cout << "SEMAFORO REMOVIDO POR PRODUCTOR" << endl;
            break;
        // CODIGO CORRESPONDIENTE AL PROCESO CONSUMIDOR
	    case 0:                             
            c_sleep = atoi(argv[1]);
            c_pid = getpid();
            while (1) {
                // EL CONSUMIDOR SE BLOQUEA LA CANTIDAD DE SEGUNDOS INDICADA
                sleep(c_sleep);
                // EL CONSUMIDOR INTENTARA OPERAR EN EL SEGUNDO SEMAFORO DEL ARREGLO
        	    acquire.sem_num = MADE;
                // EL CONSUMIDOR INTENTA DECREMENTAR EL SEGUNDO SEMAFORO. SI ESTE ES CERO, EL RECURSO ESTA SIENDO ACCEDIDO POR EL PRODUCTOR Y SEMOP DEVUELVE -1
                if (semop(semid, &acquire, 1) == -1) {
            	    perror("semop -consumidor- esperando por nuevo numero generado");
            	    return 8;
        	    }
                // ABRE EL RECURSO PARA LECTURA (ES UN ARCHIVO)
        	    if ( (fptr = fopen(BUFFER, "r")) == NULL ){
        	        perror(BUFFER);
        	        return 9;
    		    }                      
                // >>> INICIO:  SECCION CRITICA <<<
                fptr = fopen(BUFFER, "r");
        	    fscanf(fptr, "%d", &n);
    		    fclose(fptr);
                // >>> FINAL:  SECCION CRITICA <<<
                // EL CONSUMIDOR INTENTARA OPERAR EN EL PRIMER SEMAFORO
        	    release.sem_num = READ;
                // EL CONSUMIDOR INCREMENTA EL PRIMER SEMAFORO PARA INDICAR AL PRODUCTOR QUE NUMERO GENERADO FUE LEIDO.
        	    if (semop(semid, &release, 1) == -1) {
        	        perror("semop -consumidor- indicando que el numero fue leido");
            	    return 10;
		        }
        	    cout << "LECTOR CONSUME EL NUMERO [" << n <<"], " << c_pid << endl;
    	    }
            break;
        default:
        break;
        }
    return 0;
}

