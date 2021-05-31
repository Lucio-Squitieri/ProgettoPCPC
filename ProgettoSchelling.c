#include <math.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define N 5000
#define M 5000
#define T 30
#define BLUE(string) "\033[1;34m" string "\x1b[0m"
#define RED(string) "\033[1;31m" string "\x1b[0m"

void printm(char *matrix, int rank, int rows);

void main(int argc, char *argv[]) {
    int rank, world_size;  // for storing this process' rank, and the number of processes
    int root = 0;
    double start, end;  // start and end time of program's execution
    int sum = 0;        // Sum of counts. Used to calculate displacements
    int emptySum = 0;
    char *temp = malloc(sizeof(char) * (N * M));
    char *irecv = malloc(sizeof(char) * (N * M));
    int *rowProcess = malloc(sizeof(int) * N);
    int *panacea = malloc(sizeof(int) * (N * M));
    char *data = malloc(sizeof(char) * (N * M));  // the matrix to be distributed
    int emptyResto;
    int emptyDivisione;
    int steps = 10;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    MPI_Barrier(MPI_COMM_WORLD);

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

    char *received_matrix = (char *)malloc(sizeof(char) * (N * M));
    ;  // buffer where the received data should be stored

    // calculate send counts and displacements
    typedef struct {
        int newPosition;
        int processo;
        char tipo;
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
        }

        else {
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
    int cristo = 0;
    while (cristo < steps) {
        int *emptyPlaces = malloc(sizeof(int) * (N * M));
        //find out if the current process is the master
        int o = 0;
        if (0 == rank) {
            // print calculated send counts and displacements for each process (for correctness and testing)
            for (int i = 0; i < world_size; i++) {
                //printf("sendcounts[%d] = %d\tdispls[%d] = %d\n", i, sendcounts[i], i, displs[i]);
            }
            //populate the matrix with random values
            srand(10);
            if (cristo == 0) {
                //printf("CREO TABELLA INIZIALE\n");
                for (int i = 0; i < N; i++) {
                    for (int j = 0; j < M; j++) {
                        int r = rand() % 3;
                        if (r == 0)
                            data[(i * M) + j] = 'R';
                        else if (r == 1)
                            data[(i * M) + j] = 'B';
                        else if (r == 2) {
                            data[(i * M) + j] = ' ';
                            emptyPlaces[o] = (i * M) + j;
                            o++;
                        }
                    }
                }

                //printm(data, 0, N);
            } else {
                o = 0;
                // printf("USO TABELLA IRECV\n");
                for (int i = 0; i < N; i++)
                    for (int j = 0; j < M; j++)
                        if (irecv[(i * M) + j] == ' ') {
                            emptyPlaces[o] = (i * M) + j;
                            //printf("posizione che dovrebbe stare in diocane:%d\n", emptyPlaces[o]);
                            o++;
                        }
                //printm(irecv, 0, N);
            }
        }

        MPI_Bcast(&o, 1, MPI_INT, 0, MPI_COMM_WORLD);

        emptyResto = o % world_size;
        emptyDivisione = o / world_size;
        emptySum = 0;
        for (int i = 0; i < world_size; i++) {
            emptySendcounts[i] = emptyDivisione;
            if (emptyResto > 0) {
                emptySendcounts[i] += 1;
                emptyResto--;
            }

            emptyDispls[i] = emptySum;
            emptySum += emptySendcounts[i];
        }
        int *received_array = malloc(sizeof(int) * (N * M));

        srand(time(NULL));
        for (int i = 0; i < o; i++) {
            size_t j = i + rand() / (RAND_MAX / (o - i) + 1);
            int t = emptyPlaces[j];
            emptyPlaces[j] = emptyPlaces[i];
            emptyPlaces[i] = t;
        }
        MPI_Scatterv(emptyPlaces, emptySendcounts, emptyDispls, MPI_INT, received_array, emptySendcounts[rank], MPI_INT, root, MPI_COMM_WORLD);

        int receive_size = emptySendcounts[rank];

        if (cristo == 0)
            MPI_Scatterv(data, sendcounts, displs, MPI_CHAR, received_matrix, sendcounts[rank], MPI_CHAR, root, MPI_COMM_WORLD);
        else
            MPI_Scatterv(irecv, sendcounts, displs, MPI_CHAR, received_matrix, sendcounts[rank], MPI_CHAR, root, MPI_COMM_WORLD);
        //calcolo il numero di righe
        int rows = sendcounts[rank] / M;

        //stampo la matrice ricevuta
        //printm(received_matrix, rank, rows);

        //printf("ricezione rank:%d dim:%d\n", rank, receive_size);

        //calculate the soddisfaction
        char *unsatisfied = malloc(sizeof(char) * (N * M));
        int uns = 0;
        int liberi = receive_size;
        int prova = receive_size;

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
                    // printf("%c ", temp[i][j]);
                }
                //printf("\n");
            }
        } else {
            for (int i = 0, g = 1; g < rows - 1; i++, g++) {
                for (int j = 0; j < M; j++) {
                    temp[(i * M) + j] = received_matrix[(g * M) + j];
                    //printf("%c ", temp[i][j]);
                }
                //  printf("\n");
            }
            //printf("\n");
        }

        for (int i = 0; i < world_size; i++) {
            index[i] = displs[i] + M;
            index[0] = 0;
            panacea[i] = rowProcess[i] * M;
        }
        //printf("STEP %d rank %d spazi assegnati %d\n", cristo, rank, liberi);
        if (rank == 0) {
            //int uns = 0;
            for (int i = 0; i < rows - 1; i++) {
                int r = 0;
                int vicini = 0;
                for (int j = 0; j < M; j++) {
                    if (received_matrix[(i * M) + j] != ' ') {
                        if (i > 0) {  // TODO SE HO UNA RIGA SOPRA
                            vicini += 2;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + j]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + j]) r++;
                            //controllo se ci sono elementi a sinistra
                            if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                                vicini += 3;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                                vicini += 3;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            } else {
                                vicini += 6;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            }
                        } else {  //TODO SE NON E' PRESENTE UNA RIGA SUPERIORE
                            vicini += 1;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) r++;
                            //controllo se ci sono elementi a sinistra
                            if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                                vicini += 2;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                                vicini += 2;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                            } else {
                                vicini += 4;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;

                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            }
                        }
                    } else
                        r = -1;

                    float soddisfazione;
                    if (vicini > 0)
                        soddisfazione = (100 / vicini) * r;
                    else
                        soddisfazione = 100;

                    if (soddisfazione < T && received_matrix[(i * M) + j] != ' ') {
                        //printf("STEP %d gente:%c rank %d:soddisfazione riga %d elemento %d: %d, soddisfazione:%0.1f\n", cristo, received_matrix[i][j], rank, i, j, r, soddisfazione);
                        unsatisfied[uns] = received_matrix[(i * M) + j];
                        uns++;
                        if (liberi > 0) {
                            temp[(i * M) + j] = ' ';

                            //printf("STEP %d gente:%c rank %d:soddisfazione riga %d elemento %d: %d, soddisfazione:%0.1f\n", cristo, received_matrix[i][j], rank, i, j, r, soddisfazione);
                            liberi--;
                        }
                    }

                    r = 0;
                    vicini = 0;
                }
            }
            //printm(temp, rank, 1);
        } else if (rank == world_size - 1) {
            for (int i = 1; i < rows; i++) {
                int r = 0;
                int vicini = 0;
                for (int j = 0; j < M; j++) {
                    if (received_matrix[(i * M) + j] != ' ') {
                        if (i != rows - 1) {  //TODO se  ho una riga inferiore
                            vicini += 2;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + j]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) r++;
                            if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                                vicini += 3;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                                vicini += 3;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                            } else {
                                vicini += 3;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            }
                        } else {
                            vicini += 1;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) r++;
                            if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                                vicini += 2;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                                vicini += 2;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                            } else {
                                vicini += 4;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                                if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                            }
                        }
                    } else
                        r = -1;
                    float soddisfazione;
                    if (vicini > 0)
                        soddisfazione = (100 / vicini) * r;
                    else
                        soddisfazione = 100;

                    if (soddisfazione < T && received_matrix[(i * M) + j] != ' ') {
                        // printf("STEP %d gente:%c rank %d:soddisfazione riga %d elemento %d: %d, soddisfazione:%0.1f\n", cristo, received_matrix[i][j], rank, i - 1, j, r, soddisfazione);
                        unsatisfied[uns] = received_matrix[(i * M) + j];
                        uns++;
                        if (liberi > 0) {
                            temp[((i - 1) * M) + j] = ' ';

                            // printf("STEP %d gente:%c rank %d:soddisfazione riga %d elemento %d: %d, soddisfazione:%0.1f\n", cristo, received_matrix[i][j], rank, i - 1, j, r, soddisfazione);

                            liberi--;
                        }
                    }
                    r = 0;
                    vicini = 0;
                }
            }
            //printm(temp, rank, 1);
        } else {
            for (int i = 1; i < rows - 1; i++) {
                int r = 0;
                int vicini = 0;
                //int uns = 0;
                for (int j = 0; j < M; j++) {
                    if (received_matrix[(i * M) + j] != ' ') {
                        vicini += 2;
                        if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + j]) r++;
                        if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + j]) r++;
                        if (j == 0) {  //CI SONO ELEMENTI SOLO A DESTRA
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                        } else if (j == M - 1) {  //CI SONO ELEMENTI SOLO A SINISTRA
                            vicini += 3;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                        } else {
                            vicini += 6;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j - 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j - 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j - 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i - 1) * M) + (j + 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[((i + 1) * M) + (j + 1)]) r++;
                            if (received_matrix[(i * M) + j] == received_matrix[(i * M) + (j + 1)]) r++;
                        }
                    } else
                        r = -1;
                    float soddisfazione;
                    if (vicini > 0)
                        soddisfazione = (100 / vicini) * r;
                    else
                        soddisfazione = 100;

                    if (soddisfazione < T && received_matrix[(i * M) + j] != ' ') {
                        //printf("STEP %d gente:%c rank %d:soddisfazione riga %d elemento %d: %d, soddisfazione:%0.1f\n", cristo, received_matrix[i][j], rank, i - 1, j, r, soddisfazione);
                        unsatisfied[uns] = received_matrix[(i * M) + j];
                        uns++;
                        if (liberi > 0) {
                            temp[((i - 1) * M) + j] = ' ';
                            // printf("STEP %d gente:%c rank %d:soddisfazione riga %d elemento %d: %d, soddisfazione:%0.1f\n", cristo, received_matrix[i][j], rank, i - 1, j, r, soddisfazione);

                            liberi--;
                        }
                    }
                    vicini = 0;
                    r = 0;
                }
            }
            // printm(temp, rank, 1);
        }

        //calcolo il displacemnte in base al numero di righe e non al numero di elementi

        int daSpostare = 0;
        movingAgent *agent = malloc(sizeof(movingAgent) * uns);

        int uns1 = uns;
        int totaluns[world_size];
        totaluns[rank] = uns;
        int received[world_size];
        MPI_Allgather(&uns1, 1, MPI_INT, received, 1, MPI_INT,
                      MPI_COMM_WORLD);

        bool insoddisfatti = false;

        for (int l = 0; l < world_size; l++) {
            if (received[l] > 0) insoddisfatti = true;
        }

        if (insoddisfatti) {
            //if (rank == 0) printf("boolean= %d\n", insoddisfatti);
            //int prova = receive_size;
            while (receive_size > 0 && uns > 0) {
                //printf("mannagg a maronn rank %d  elementi insoddisfatti %d e elementi che posso spostare %d\n", rank, uns, prova);
                int processo = -1;

                for (int j = 1; j < world_size; j++) {
                    if (received_array[daSpostare] < index[j] && received_array[daSpostare] >= index[j - 1])
                        processo = j - 1;
                }
                if (processo == -1) {
                    processo = world_size - 1;
                }
                //printf("1 STEP %d rank %d l'elemento %c va spostato nella posizione %d appartenente al processo %d\n", cristo, rank, unsatisfied[i], received_array[i], processo);

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
                                  //printf("MITTENTE rank %d : devo inviare al processo %d il valore %d \n", rank, i, k);
                        MPI_Isend(moving, k, sendAgent, i, 1, MPI_COMM_WORLD, &request);
                    }
                } else {
                    if (k > 0) {
                        for (int p = 0; p < k; p++) {
                            //printf("INTERESSE posizione che possiedo in locale %d\n", moving[p].newPosition);
                            int pos = moving[p].newPosition - index[rank];
                            int riga = (pos / M);
                            int colonna = pos % M;
                            // printf("MITTENTE  in locale rank %d l'agente va inserito alla riga %d colonna %d, la posizione è: %d\n", rank, riga, colonna, moving[p].newPosition);
                            //if (temp[riga][colonna] != ' ') printf("SOVRASCRIVO in locale lar posizione %d con riga %d e colonna %d \n", pos, riga, colonna);
                            temp[pos] = moving[p].tipo;
                        }
                    }
                }

                //spero di aver inviato
                //printf("rank %d : devo inviare al processo %d il valore %c da mettere nella posizione %d perchè la pos. appartiene al processo %d\n", rank, i, moving[p].tipo, moving[p].newPosition, moving[p].processo);
            }

            //Recv per k
            MPI_Status status;
            MPI_Request request;
            int v = 0;
            int buf;
            for (int i = 0; i < world_size; i++) {
                if (i != rank) {
                    MPI_Irecv(&buf, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &request);
                    MPI_Wait(&request, &status);
                    //printf("RICEVENTE 1 rank %d e ho ricevuto k= %d dal processo %d\n", rank, buf, i);
                    if (buf > 0) {  //se k>0 recv per la struttura
                        //printf("RICEVENTE rank %d e ho ricevuto k= %d dal processo %d\n", rank, buf, i);
                        movingAgent received[buf];
                        v = 100;
                        MPI_Irecv(received, buf, sendAgent, i, 1, MPI_COMM_WORLD, &request);
                        MPI_Wait(&request, &status);
                        for (int k = 0; k < buf; k++) {
                            //printf(" INTERESSE RICEVENTE rank %d e ho ricevuto la struttura con il tipo:%c, da mettere in posizione:%d, dal processo %d\n", rank, received[k].tipo, received[k].newPosition, i);
                            int pos = received[k].newPosition - index[rank];
                            int riga = (pos / M);
                            int colonna = pos % M;
                            //printf("RICEVENTE rank %d l'agente va inserito alla riga %d colonna %d,la posizione è: %d\n", rank, riga, colonna, received[k].newPosition);
                            //if (temp[riga][colonna] != ' ') printf("SOVRASCRIVO in remoto la posizione %d con riga %d e colonna %d \n", pos, riga, colonna);
                            temp[pos] = received[k].tipo;
                        }
                    }
                }
            }

            MPI_Gatherv(temp, panacea[rank], MPI_CHAR, irecv, panacea, &index[rank], MPI_CHAR, root, MPI_COMM_WORLD);

            if (rank == root) {
                if (cristo == steps - 1) {
                    printf(" non ho risolto\n");
                    // printm(irecv, 0, N);
                }
            }
            cristo++;
        } else {
            if (rank == 0) printf("mi sono fermato allo step %d\n", cristo);
            cristo = steps;

            MPI_Gatherv(temp, panacea[rank], MPI_CHAR, irecv, panacea, &index[rank], MPI_CHAR, root, MPI_COMM_WORLD);

            if (rank == root) {
                printf("ho risolto\n");
                //printm(irecv, 0, N);
            }
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();

    free(sendcounts);
    free(displs);
    if (rank == root) {
        printf("\n\n Time in ms = %f\n", end - start);
    }
    MPI_Finalize();
}

void printm(char *matrix, int rank, int rows) {
    /*for (int i = 0; i < rows; i++) {
        for (int j = 0; j < M; j++) {
            if (j == 0) printf("| ");
            if (matrix[i][j] == 'B') {
                printf(BLUE("%c") " ", matrix[i][j]);
            } else if (matrix[i][j] == 'R') {
                printf(RED("%c") " ", matrix[i][j]);
            } else
                printf("%c ", matrix[i][j]);

            printf("| ");
        }
        printf("\n");
    }
*/
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < M; j++) {
            printf("%c|", matrix[(i * M) + j]);
        }
        printf("\n");
    }
    printf("\n");
}