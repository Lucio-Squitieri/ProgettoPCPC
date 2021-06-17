#include <math.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//#define N 2500
//#define M 2500
#define T 30
#define BLUE(string) "\033[1;34m" string "\x1b[0m"
#define RED(string) "\033[1;31m" string "\x1b[0m"

void printm(char *matrix, int rank, int rows, int N, int M);
int getUnsatisfiedAgents(int rank, int rows, char *received_matrix, char *unsatisfied, char *temp, int liberi, int world_size, int N, int M);
void calculateInitial(int world_size, int *rowProcess, int *sendcounts, int divisione, int resto, int *displs, int *displs_modifier, int N, int M);
void calculateEmptyPlaces(int world_size, int o, int *emptySendcounts, int *emptyDispls);
void populateTemp(int rank, int rows, char *received_matrix, char *temp, int world_size, int M);
int populateInitialMatrix(char *data, int *emptyPlaces, int N, int M);
int getEmptyPlaces(char *irecv, int *emptyPlaces, int N, int M);

void main(int argc, char *argv[]) {
    int rank, world_size;  // for storing this process' rank, and the number of processes
    int root = 0;          // rank of master or root process
    double start, end;     // start and end time of program's execution
    int M = 0;
    int N = 0;

    MPI_Init(&argc, &argv);
    N = atoi(argv[1]);  //Numero righe matrice
    M = atoi(argv[2]);  //Numero colonne matrice

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Barrier(MPI_COMM_WORLD);

    char *temp = malloc(sizeof(char) * N * M);           //temp matrix to store the received matrix for each of the processor
    char *irecv = malloc(sizeof(char) * N * M);          //matrix used to store the result of gather and then to scatter in the iteraztions > 1
    int *rowProcess = malloc(sizeof(int) * M * N);       //number of row for each process
    int *recvcount = malloc(sizeof(int) * N * M);        //recv counts for gather
    char *data = malloc(sizeof(char) * N * M);           // the matrix to be distributed
    int steps = 10;                                      //number of iterations
    int *sendcounts = malloc(sizeof(int) * world_size);  // array describing how many elements to send to each process
    int *displs = malloc(sizeof(int) * world_size);      // array describing the displacements where each segment begins
    int *index = malloc(sizeof(int) * world_size);
    int *displs_modifier = malloc(sizeof(int) * world_size);  // array used to ignore the extra row in the calculation
    int *emptySendcounts = malloc(sizeof(int) * world_size);  // array describing how many elements to send to each process
    int *emptyDispls = malloc(sizeof(int) * world_size);      // array describing the displacements where each segment begins
    start = MPI_Wtime();
    int divisione = N / world_size;
    int resto = N % world_size;
    resto = resto * M;
    char *received_matrix = malloc(sizeof(char) * N * M);  // buffer where the received data should be stored

    //definition of the struct that defines the agent to move
    typedef struct {
        int newPosition;  //new position of the agent
        int processo;     //process owner of the position
        char tipo;        //type of agent
    } movingAgent;

    MPI_Datatype sendAgent, oldtypes[2];
    int blockcount[2];
    MPI_Aint offset[2];

    offset[0] = 0;
    offset[1] = sizeof(int) * 2;
    oldtypes[0] = MPI_INT;
    oldtypes[1] = MPI_CHAR;
    blockcount[0] = 2;
    blockcount[1] = 1;

    MPI_Type_create_struct(2, blockcount, offset, oldtypes, &sendAgent);
    MPI_Type_commit(&sendAgent);

    // calculate send counts and displacements
    calculateInitial(world_size, rowProcess, sendcounts, divisione, resto, displs, displs_modifier, N, M);

    int current_step = 0;
    while (current_step < steps) {
        int *emptyPlaces = malloc(sizeof(int) * (N * M));  //array containing all the locations of the empty places
        //find out if the current process is the master
        int o = 0;
        if (0 == rank) {
            //populate the matrix with random values SEED
            srand(10);
            if (current_step == 0) {
                //printf("CREO TABELLA INIZIALE\n");
                o = populateInitialMatrix(data, emptyPlaces, N, M);
            } else {
                // printf("USO TABELLA IRECV\n");
                o = getEmptyPlaces(irecv, emptyPlaces, N, M);
                //printm(irecv, 0, N);
            }
        }

        MPI_Bcast(&o, 1, MPI_INT, 0, MPI_COMM_WORLD);
        // calculate send counts and displacements for the empty places
        calculateEmptyPlaces(world_size, o, emptySendcounts, emptyDispls);
        int *received_array = malloc(sizeof(int) * N * M);  //array received wich contains the locatioon of the empty places assigned to the processor

        //SEED SHUFFLE
        //srand(time(NULL));
        for (int i = 0; i < o; i++) {
            size_t j = i + rand() / (RAND_MAX / (o - i) + 1);
            int t = emptyPlaces[j];
            emptyPlaces[j] = emptyPlaces[i];
            emptyPlaces[i] = t;
        }

        MPI_Scatterv(emptyPlaces, emptySendcounts, emptyDispls, MPI_INT, received_array, emptySendcounts[rank], MPI_INT, root, MPI_COMM_WORLD);

        int receive_size = emptySendcounts[rank];
        if (world_size != 1) {
            if (current_step == 0)
                MPI_Scatterv(data, sendcounts, displs, MPI_CHAR, received_matrix, sendcounts[rank], MPI_CHAR, root, MPI_COMM_WORLD);
            else
                MPI_Scatterv(irecv, sendcounts, displs, MPI_CHAR, received_matrix, sendcounts[rank], MPI_CHAR, root, MPI_COMM_WORLD);
        } else if (current_step == 0)
            received_matrix = data;
        else
            received_matrix = irecv;

        //calcolo il numero di righe
        int rows = sendcounts[rank] / M;

        //stampo la matrice ricevuta
        //printm(received_matrix, rank, rows);

        //calculate the soddisfaction
        char *unsatisfied = malloc(sizeof(char) * N * M);  //array containing the agent to move
        int liberi = receive_size;
        int prova = receive_size;

        populateTemp(rank, rows, received_matrix, temp, world_size, M);

        for (int i = 0; i < world_size; i++) {
            index[i] = displs[i] + M;
            index[0] = 0;
            recvcount[i] = rowProcess[i] * M;
        }
        //printf("STEP %d rank %d spazi assegnati %d\n", current_step, rank, liberi);

        //check the treshold for each element of the submatrix received and if the process has k empty places assigned then the first k agent
        //that wants to move are stored in unassigned and the position in the submatrix received becemes empty
        int uns = getUnsatisfiedAgents(rank, rows, received_matrix, unsatisfied, temp, liberi, world_size, N, M);

        int daSpostare = 0;
        movingAgent *agent = malloc(sizeof(movingAgent) * uns);

        int uns1 = uns;
        int *totaluns = malloc(sizeof(int) * world_size);
        totaluns[rank] = uns;
        int *received = malloc(sizeof(int) * world_size);

        //all gather to allow each processor to share the number of unsatisfied agents
        MPI_Allgather(&uns1, 1, MPI_INT, received, 1, MPI_INT,
                      MPI_COMM_WORLD);

        //check for unsatisfied agents
        bool insoddisfatti = false;
        for (int l = 0; l < world_size; l++) {
            if (received[l] > 0) insoddisfatti = true;
        }

        if (insoddisfatti) {
            while (receive_size > 0 && uns > 0) {
                int processo = -1;
                //find the corrsponding process to the empty location assigned
                for (int j = 1; j < world_size; j++) {
                    if (received_array[daSpostare] < index[j] && received_array[daSpostare] >= index[j - 1])
                        processo = j - 1;
                }
                if (processo == -1) {
                    processo = world_size - 1;
                }
                //printf("1 STEP %d rank %d l'elemento %c va spostato nella posizione %d appartenente al processo %d\n", current_step, rank, unsatisfied[i], received_array[i], processo);

                agent[daSpostare].processo = processo;
                agent[daSpostare].newPosition = received_array[daSpostare];
                agent[daSpostare].tipo = unsatisfied[daSpostare];
                //printf("\n  rank%d tipo:%c, POSIZIONE:%d, PROCESSO:%d\n", rank, agent[daSpostare].tipo, agent[daSpostare].newPosition, agent[daSpostare].processo);
                daSpostare++;
                uns--;
                receive_size--;
            }

            for (int i = 0; i < world_size; i++) {
                movingAgent *moving = malloc(sizeof(movingAgent) * daSpostare);
                int k = 0;
                for (int j = 0; j < daSpostare; j++) {
                    if (agent[j].processo == i) {
                        moving[k].newPosition = agent[j].newPosition;
                        moving[k].processo = agent[j].processo;
                        moving[k].tipo = agent[j].tipo;

                        //printf("\n  elementi da inviare rank %d tipo:%c, processo:%d, posizione:%d\n", rank, moving[k].tipo, moving[k].processo, moving[k].newPosition);
                        k++;
                    }
                }
                //invio k per preparare il ricevente
                if (i != rank) {
                    MPI_Request request;
                    MPI_Isend(&k, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &request);
                    if (k > 0) {  //se k>0 eseguo una Isend per l'invio della struttura

                        MPI_Isend(moving, k, sendAgent, i, 1, MPI_COMM_WORLD, &request);
                    }
                } else {
                    if (k > 0) {
                        for (int p = 0; p < k; p++) {
                            //printf("INTERESSE posizione che possiedo in locale %d\n", moving[p].newPosition);
                            int pos = moving[p].newPosition - index[rank];
                            temp[pos] = moving[p].tipo;
                        }
                    }
                }
            }

            MPI_Status status;
            int buf;
            for (int i = 0; i < world_size; i++) {
                if (i != rank) {
                    //Recv of the number of the elements sent
                    MPI_Recv(&buf, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    if (buf > 0) {  //se k>0 recv per la struttura
                        movingAgent *received = malloc(sizeof(movingAgent) * buf);
                        MPI_Recv(received, buf, sendAgent, i, 1, MPI_COMM_WORLD, &status);
                        for (int k = 0; k < buf; k++) {
                            int pos = received[k].newPosition - index[rank];
                            temp[pos] = received[k].tipo;
                        }
                    }
                }
            }
            //gather to get the total matrix after each iteration
            MPI_Gatherv(temp, recvcount[rank], MPI_CHAR, irecv, recvcount, &index[rank], MPI_CHAR, root, MPI_COMM_WORLD);

            //stampo all'ultima iterazione se non ho risolto
            if (rank == root) {
                if (current_step == steps - 1) {
                    printf(" non ho risolto\n");
                    //printm(irecv, 0, N, N, M);
                }
            }
            current_step++;

            //altrimenti stampo allo step in cui mi sono fermato
        } else {
            if (rank == 0) printf("mi sono fermato allo step %d\n", current_step);
            current_step = steps;
            //gather to get the matrix after the last iteration if the matrix is resolved
            MPI_Gatherv(temp, recvcount[rank], MPI_CHAR, irecv, recvcount, &index[rank], MPI_CHAR, root, MPI_COMM_WORLD);

            if (rank == root) {
                printf("ho risolto\n");
                //printm(irecv, 0, N, N, M); STAMPA INIZIALE
            }
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();

    if (rank == root) {
        printf("\n\n Time in s = %f\n", end - start);
    }
    MPI_Finalize();
}

void printm(char *matrix, int rank, int rows, int N, int M) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < M; j++) {
            if (j == 0) printf("| ");
            if (matrix[(i * M) + j] == 'B') {
                printf(BLUE("%c") " ", matrix[(i * M) + j]);
            } else if (matrix[(i * M) + j] == 'R') {
                printf(RED("%c") " ", matrix[(i * M) + j]);
            } else
                printf("%c ", matrix[(i * M) + j]);

            printf("| ");
        }
        printf("\n");
    }

    /* for (int i = 0; i < rows; i++) {
        for (int j = 0; j < M; j++) {
            printf("%c|", matrix[(i * M) + j]);
        }
        printf("\n");
    }
    printf("\n");*/
}

int getUnsatisfiedAgents(int rank, int rows, char *received_matrix, char *unsatisfied, char *temp, int liberi, int world_size, int N, int M) {
    int uns = 0;
    if (rank == 0) {
        for (int i = 0; i < rows - 1; i++) {
            int simili = 0;
            int vicini = 0;
            for (int j = 0; j < M; j++) {
                if (received_matrix[(i * M) + j] != ' ') {
                    if (i > 0) {  // TODO SE HO UNA RIGA SOPRA
                        vicini += 2;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + j]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[(i * M) + j]) simili++;
                        //controllo se ci sono elementi a sinistra
                        if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        } else {
                            vicini += 6;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        }
                    } else {  //TODO SE NON E' PRESENTE UNA RIGA SUPERIORE
                        vicini += 1;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) simili++;
                        //controllo se ci sono elementi a sinistra
                        if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                            vicini += 2;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                            vicini += 2;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                        } else {
                            vicini += 4;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;

                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        }
                    }
                } else
                    simili = -1;

                float soddisfazione;
                if (vicini > 0)
                    soddisfazione = (100 / vicini) * simili;
                else
                    soddisfazione = 100;

                if (soddisfazione < T && received_matrix[(i * M) + j] != ' ') {
                    unsatisfied[uns] = received_matrix[(i * M) + j];
                    uns++;
                    if (liberi > 0) {
                        temp[(i * M) + j] = ' ';
                        liberi--;
                    }
                }

                simili = 0;
                vicini = 0;
            }
        }
        return uns;
        //printm(temp, rank, 1);
    } else if (rank == world_size - 1) {
        for (int i = 1; i < rows; i++) {
            int simili = 0;
            int vicini = 0;
            for (int j = 0; j < M; j++) {
                if (received_matrix[(i * M) + j] != ' ') {
                    if (i != rows - 1) {  //TODO se  ho una riga inferiore
                        vicini += 2;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + j]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) simili++;
                        if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                        } else {
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        }
                    } else {
                        vicini += 1;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) simili++;
                        if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                            vicini += 2;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                            vicini += 2;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                        } else {
                            vicini += 4;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                        }
                    }
                } else
                    simili = -1;
                float soddisfazione;
                if (vicini > 0)
                    soddisfazione = (100 / vicini) * simili;
                else
                    soddisfazione = 100;

                if (soddisfazione < T && received_matrix[(i * M) + j] != ' ') {
                    unsatisfied[uns] = received_matrix[(i * M) + j];
                    uns++;
                    if (liberi > 0) {
                        temp[((i - 1) * M) + j] = ' ';
                        liberi--;
                    }
                }
                simili = 0;
                vicini = 0;
            }
        }
        //printm(temp, rank, 1);
        return uns;
    } else {
        for (int i = 1; i < rows - 1; i++) {
            int simili = 0;
            int vicini = 0;
            for (int j = 0; j < M; j++) {
                if (received_matrix[(i * M) + j] != ' ') {
                    vicini += 2;
                    if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) simili++;
                    if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + j]) simili++;
                    if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                        vicini += 3;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                    } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                        vicini += 3;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                    } else {
                        vicini += 6;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) simili++;
                        if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) simili++;
                    }
                } else
                    simili = -1;
                float soddisfazione;
                if (vicini > 0)
                    soddisfazione = (100 / vicini) * simili;
                else
                    soddisfazione = 100;

                if (soddisfazione < T && received_matrix[(i * M) + j] != ' ') {
                    unsatisfied[uns] = received_matrix[(i * M) + j];
                    uns++;
                    if (liberi > 0) {
                        temp[((i - 1) * M) + j] = ' ';
                        liberi--;
                    }
                }
                vicini = 0;
                simili = 0;
            }
        }
        // printm(temp, rank, 1);
        return uns;
    }
}

void calculateInitial(int world_size, int *rowProcess, int *sendcounts, int divisione, int resto, int *displs, int *displs_modifier, int N, int M) {
    int sum = 0;  // Sum of counts. Used to calculate displacements
    for (int i = 0; i < world_size; i++) {
        rowProcess[i] = 0;
        if (i == 0) {
            sendcounts[i] = M;
            displs_modifier[i] = 0;
            for (int j = 0; j < divisione; j++) {
                rowProcess[i] = rowProcess[i] + 1;
                sendcounts[i] += M;
                displs_modifier[i] += M;
                if (resto > 0) {
                    rowProcess[i] = rowProcess[i] + 1;
                    displs_modifier[i] += M;
                    sendcounts[i] += M;
                    resto -= M;
                }

                displs[i] = sum;
            }
            displs[i] += sum;
            sum = sum + displs_modifier[i];
        }

        else if (i == world_size - 1) {
            displs_modifier[i] = 0;
            sendcounts[i] = M;
            for (int j = 0; j < divisione; j++) {
                rowProcess[i] = rowProcess[i] + 1;
                sendcounts[i] += M;
                displs_modifier[i] += M;
                if (resto > 0) {
                    rowProcess[i] = rowProcess[i] + 1;
                    displs_modifier[i] += M;
                    sendcounts[i] += M;
                    resto -= M;
                }
                displs[i] = sum - M;
            }
            sum = sum + displs_modifier[i];
        } else {
            displs_modifier[i] = 0;
            sendcounts[i] = 2 * M;
            for (int j = 0; j < divisione; j++) {
                rowProcess[i] = rowProcess[i] + 1;
                sendcounts[i] += M;
                displs_modifier[i] += M;
                if (resto > 0) {
                    rowProcess[i] = rowProcess[i] + 1;
                    displs_modifier[i] += M;
                    sendcounts[i] += M;
                    resto -= M;
                }
                displs[i] = sum - M;
            }
            sum = sum + displs_modifier[i];
        }
    }
}

void calculateEmptyPlaces(int world_size, int o, int *emptySendcounts, int *emptyDispls) {
    int emptyResto = o % world_size;      //remainder of the division for empty places
    int emptyDivisione = o / world_size;  //reult of the division for empty places
    int emptySum = 0;                     // Sum of counts. Used to calculate displacements of empty places to use to move elements
    for (int i = 0; i < world_size; i++) {
        emptySendcounts[i] = emptyDivisione;
        if (emptyResto > 0) {
            emptySendcounts[i] += 1;
            emptyResto--;
        }

        emptyDispls[i] = emptySum;
        emptySum += emptySendcounts[i];
    }
}
void populateTemp(int rank, int rows, char *received_matrix, char *temp, int world_size, int M) {
    if (rank == 0) {
        for (int i = 0, g = 0; i < rows - 1; i++, g++) {
            for (int j = 0; j < M; j++) {
                temp[(i * M) + j] = received_matrix[(g * M) + j];
                // printf("%c ", temp[i][j]);
            }
            // printf("\n");
        }
        //printf("\n");
    } else if (rank == world_size - 1) {
        for (int i = 0, g = 1; g < rows; i++, g++) {
            for (int j = 0; j < M; j++) {
                temp[(i * M) + j] = received_matrix[(g * M) + j];
                // printf("%c ", temp[i][j]); STAMPA FINALE
            }
            //printf("\n");
        }
    } else {
        for (int i = 0, g = 1; g < rows - 1; i++, g++) {
            for (int j = 0; j < M; j++) {
                temp[(i * M) + j] = received_matrix[(g * M) + j];
                //printf("%c ", temp[i][j]); STAMPA FINALE
            }
            //  printf("\n");
        }
        //printf("\n");
    }
}

int populateInitialMatrix(char *data, int *emptyPlaces, int N, int M) {
    int o = 0;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            int simili = rand() % 3;
            if (simili == 0)
                data[(i * M) + j] = 'R';
            else if (simili == 1)
                data[(i * M) + j] = 'B';
            else if (simili == 2) {
                data[(i * M) + j] = ' ';
                emptyPlaces[o] = (i * M) + j;
                o++;
            }
        }
    }
    //printm(data, 0, N, N, M);
    return o;
}

int getEmptyPlaces(char *irecv, int *emptyPlaces, int N, int M) {
    int o = 0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < M; j++)
            if (irecv[(i * M) + j] == ' ') {
                emptyPlaces[o] = (i * M) + j;
                o++;
            }
    return o;
}
