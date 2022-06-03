#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h>
#define DEBUG 0
#define NUM_MAX 20

void randomatrix(int *matrix,int row, int col)
{
	srand(time(NULL));
    int i;
    for (i = 0; i < (row * col); i++) {
		*(matrix + i) = rand()% (NUM_MAX + 1) + 0;
    }

}
int matrix_get_cell(int *matrix,int rows, int cols, int x, int y)
{
    int valor_lineal;
    valor_lineal = (y * cols + x);
    return matrix[valor_lineal];
}

int matrix_set_cell(int *matrix,int rows,int cols, int x, int y, int val)
{
    int valor_lineal;
    valor_lineal = (y * cols + x);
    matrix[valor_lineal] = val;
    return 0;
}

int main(int argc, char **argv) 
{
	double time_spent = 0.0;
	
	int dimension = atoi(argv[1]);
	
	int localid, numprocs, namelen, rv;
	double startwtime, endwtime;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int partitions, firstrow, lastrow, rowstodo;
    int *resultrow;
    int elem1, elem2, suma;
    int completerows;
    MPI_Status status;
    int x, y, i, k;
	
	//Generar matrices de manera mas optima
  	int* matrixA = NULL;
  	int* matrixB = NULL;
  	int* result = NULL;
  	
  	// Inicializar mpi
    MPI_Init(&argc, &argv);
  	
	/* determinar numero total de procesos y cual
	somos, asi como en que procesador (nodo) estamos
	corriendo. */
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &localid);
    MPI_Get_processor_name(processor_name, &namelen);
  	
  	/* En MPI el proceso inicial (padre) tiene rank
       de 0, esta es la sección donde el proceso padre
       realiza su trabajo */
    if (localid == 0) 
	{
		int i,j;
		

		if(!dimension)
		{
			printf("Por favor, introduzca el tamaï¿½o de las matrices cuadradas: \n");
	  		scanf("%d", &dimension);
		}
	
		matrixA = malloc(dimension * dimension * sizeof(int));
		matrixB = malloc(dimension * dimension * sizeof(int));
		result = malloc(dimension * dimension * sizeof(int));
		if (matrixA == NULL || matrixB == NULL || result == NULL ){
		    MPI_Finalize();
		    if (matrixA!=NULL) free(matrixA);
		    if (matrixB!=NULL) free(matrixB);
		    if (result!=NULL) free(result);
		    exit(1);
		}
		
		randomatrix(matrixA, dimension, dimension);
		randomatrix(matrixB, dimension, dimension);

		/*Determinar el momento de inicio de ejecución,
		   aquí medimos cuando comenzamos realmente a
		   hacer los cálculos */
		startwtime = MPI_Wtime();


		// mi registro de cuales rows ya estan
		// completos
		completerows = 0;

		/* el proceso con rank 0 transmite las dos
	   matrices a los demás, esto se hace por medio
	   de un broadcast de MPI a todos los miembros de
	   mi comunicador (MPI_COMM_WORLD) */
		rv = MPI_Bcast(matrixA,dimension * dimension,MPI_INT, 0, MPI_COMM_WORLD);
		//printf("Root, Broadcast said %d\n", rv);
		rv = MPI_Bcast(matrixB,dimension * dimension,MPI_INT, 0, MPI_COMM_WORLD);
		//printf("Root, Broadcast said %d\n", rv);

		// asignar un row temporal para resultados
		resultrow = malloc((dimension + 1) * sizeof(int));
		
		// esperar a tener todos los resultados completos
		while (completerows < dimension) {
		    /* Esta llamada espera a recibir un renglón
		       ya resuelto, de cualquier proceso
		       hijo. Nótese el parametro MPI_ANY_SOURCE
		       que indica que se admiten valores de
		       cualquier proceso. */
		    MPI_Recv(resultrow,dimension + 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
		    //printf("recibido renglon %d de %d\n",resultrow[0], status.MPI_SOURCE);
		    completerows++;
		    // Este ciclo "pone" el renglon recibido en
		    // mi matriz de resultado
		    for (i = 0; i < dimension; i++) 
			{
				matrix_set_cell(result,dimension,dimension,i,resultrow[0],resultrow[i + 1]);
		    }
		}
		// terminamos el cálculo! anotar tiempo al
		// terminar..
		endwtime = MPI_Wtime();
		
		time_spent = endwtime - startwtime;
	
 		printf("%d;%f\n" , dimension, time_spent);
 	
		if (matrixA!=NULL) free(matrixA);
		if (matrixB!=NULL) free(matrixB);
		if (result!=NULL) free(result);

    } // AQUI termina la ejecucion del proceso padre
    else 
	{
		
		/* Aqui comienza lo que ejecuten procesos con
	   rank distinto de cero, es decir los procesos
	   hijo.  Asigno espacio para dos matrices.. */
		matrixA = malloc(dimension * dimension * sizeof(int));
		matrixB = malloc(dimension * dimension * sizeof(int));
		if (matrixA == NULL || matrixB == NULL){
		    MPI_Finalize();
		    if (matrixA!=NULL) free(matrixA);
		    if (matrixB!=NULL) free(matrixB);
		    exit(1);
		}

		/* En MPI, si mi rank no es 0, una llamada al
	  	broadcast (notar el parametro 4 que es de 0)
	   	indica recibir el broadcast del proceso con ese
	   	rank. */
		rv = MPI_Bcast(matrixA,dimension * dimension,MPI_INT, 0, MPI_COMM_WORLD);
		rv = MPI_Bcast(matrixB,dimension * dimension,MPI_INT, 0, MPI_COMM_WORLD);

		/*de acuerdo a las dimensiones de las matrices
	   	y al numero de procesos, calcular numero de
	   	particiones, renglones por particion, el
	   	primer renglon que tiene que resolver este
	  	 proceso, y el ultimo renglon.  */
		partitions = numprocs - 1;
		rowstodo = (int) (dimension / partitions);
		firstrow = rowstodo * (localid - 1);
		lastrow = firstrow + rowstodo - 1;

		// el ultimo proceso amplia su limite para
		// tomar los huerfanitos
		if (localid == numprocs - 1) {
		    lastrow = lastrow + (dimension % partitions);
		}

		// asignar un row temporal
		resultrow = malloc((dimension + 1) * sizeof(int));

		// calcular cada row del grupo que me toca.
		for (i = firstrow; i <= lastrow; i++) 
		{
		    /* printf ("proceso %d[%s] haciendo row
		       %d\n",localid,processor_name,i); */
		    /* el elemento que estoy calculando esta en
		       y,x entonces lo que va a variar va a ser
		       la x porque la y es fija por renglon */
		    y = i;
		    for (x = 0; x < dimension; x++) 
			{
				suma = 0;
				for (k = 0; k < dimension; k++) 
				{
				    elem1 = matrix_get_cell(matrixA,
							    dimension,
							    dimension, k, y);
				    elem2 = matrix_get_cell(matrixB,
							    dimension,
							    dimension, x, k);
				    suma += elem1 * elem2;
				}
				resultrow[x + 1] = suma;
	    	}
	    	// ya tengo calculado el renglon ahora se
	    	// lo tengo que mandar al proceso padre
		    resultrow[0] = i;
		    MPI_Send(resultrow,dimension + 1,MPI_INT, 0, 1, MPI_COMM_WORLD);
		    printf("%d",resultrow);
		}
	    if (matrixA!=NULL) free(matrixA);
	    if (matrixB!=NULL) free(matrixB);
	    if (result!=NULL) free(result);	
    } // termina seccion de calculo proceso hijo

    // Terminamos la sesión de MPI
    MPI_Finalize();

    return 0;
}
