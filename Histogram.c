#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include <limits.h>

void ReadFile(int POINTS[], int *endingBAR, int numberOfPoints)
{
    FILE *ptr;
    ptr = fopen("/shared/dataset.txt", "r");

    if (NULL == ptr)
        printf("file can't be opened \n");
    int i = 0;
    char str[50];
    int x = 0;
    *endingBAR = 0;
    while (fgets(str, 50, ptr) != NULL)
    {

        POINTS[i] = atoi(str);
        if (POINTS[i] > *endingBAR)
            *endingBAR = POINTS[i];
        i = i + 1;
    }
    for (; i < numberOfPoints; i++)
        POINTS[i] = INT_MIN;
    fclose(ptr);
}
void Print(int numberOfBars, int GLOBAL_COUNTS[], float range[], int np)
{

    int i, j = 0;
    int FinalCount[numberOfBars];
    for (j = 0; j < numberOfBars; j = j + 1)
        FinalCount[j] = 0;

#pragma omp parallel shared(numberOfBars, GLOBAL_COUNTS, range, np) private(i, j)
    {
#pragma omp for schedule(static) // hence each thread take one bar and collecting
        for (i = 0; i < numberOfBars; i++)
        {
            // #pragma omp for schedule(static) // hence each thread take one element and collecting
            for (j = 0; j < np * numberOfBars; j = j + numberOfBars) // np * numberOfBars size global count
            {
#pragma omp critical
                {

                    FinalCount[i] += GLOBAL_COUNTS[j + i];
                    // printf("thread NUMBER %d  ,FInalCOunt IS %d , COUNT IS %d\n", omp_get_thread_num(), FinalCount[i], GLOBAL_COUNTS[i + j]);
                }
            }
        }
    }
    printf(" The Range Start with %0.3f,end with %0.3f ,with count %d \n ", 0.0, range[0], FinalCount[0]);

    for (j = 1; j < numberOfBars; j = j + 1)
        printf(" The Range Start with %0.3f,end with %0.3f ,with count %d  \n ", range[j] - range[0], range[j], FinalCount[j]);
}
void RunThreads(int localPoints[], int localSize, int Count[], float range[], int numberOfBars)
{
    int i, j = 0;
#pragma omp parallel shared(localPoints, localSize, Count, range,numberOfBars) private(i, j)
    {
#pragma omp for schedule(static)
        for (i = 0; i < localSize; i++)
        {
            for (j = 0; j < numberOfBars + 1; j++)
            {
                if (localPoints[i] == INT_MIN)
                    break;
                if (localPoints[i] <= range[j])
                {
#pragma omp critical
                    {
                        Count[j]++;
                        // printf("And thread NUMBER %d  ,POINT IS %d , COUNT IS %d\n", omp_get_thread_num(), localPoints[i], Count[j]);
                    }
                    break;
                }
            }
        }
    }
}
int main(int argc, char *argv[])
{

    int i, j = 0;
    int endingBar;
    int numberOfBars;
    int numberOfTherads;
    int pid, np, localSize;
    int numberOfPoints;
    int numberOfProcesses;
    int *POINTS = NULL;
    int localPoints[1000];
    int GLOBAL_COUNTS[100];
    for (i = 0; i < 100; i++)
        GLOBAL_COUNTS[i] = 0;
    MPI_Status status;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &np);

    ////////////////////////////////Master Process  ////////////////////////////////////////////
    if (pid == 0)
    {
        printf("Enter Number OF BARS: ");
        scanf("%d", &numberOfBars);
        MPI_Bcast(&numberOfBars, 1, MPI_INT, 0, MPI_COMM_WORLD);

        printf("Enter Number OF POINTS: ");
        scanf("%d", &numberOfPoints);

        if (numberOfPoints % np != 0)
            numberOfPoints += np - (numberOfPoints % np);

        POINTS = malloc(sizeof(int) * (numberOfPoints));

        localSize = numberOfPoints / (np /*- 1*/); // Master process  inculding with me

        MPI_Bcast(&localSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

        for (i = 0; i < localSize; i++)
            localPoints[i] = 0;

        //========================== REEAD FILE==========================
        ReadFile(POINTS, &endingBar, numberOfPoints);

        MPI_Bcast(&endingBar, 1, MPI_INT, 0, MPI_COMM_WORLD);

        //==========================END REEAD FILE==========================

        MPI_Scatter(POINTS, localSize, MPI_INT, localPoints, localSize, MPI_INT, 0, MPI_COMM_WORLD);
        float range[numberOfBars];
        int Count[numberOfBars];
        for (i = 1; i < numberOfBars + 1; i++)
            range[i - 1] = (((float)endingBar / (float)numberOfBars)) * (i);

        for (i = 0; i < numberOfBars; i++)
            Count[i] = 0;

        RunThreads(localPoints, localSize, Count, range, numberOfBars);

        MPI_Gather(Count, numberOfBars, MPI_INT, GLOBAL_COUNTS, numberOfBars, MPI_INT, 0, MPI_COMM_WORLD);
        free(POINTS);

        Print(numberOfBars, GLOBAL_COUNTS, range, np);
    }
    else
    {

        MPI_Bcast(&numberOfBars, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&localSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&endingBar, 1, MPI_INT, 0, MPI_COMM_WORLD);

        MPI_Scatter(POINTS, localSize, MPI_INT, localPoints, localSize, MPI_INT, 0, MPI_COMM_WORLD);

        float range[numberOfBars];
        int Count[numberOfBars];
        for (i = 1; i < numberOfBars + 1; i++)
            range[i - 1] = ((float)endingBar / (float)numberOfBars) * (i);
        for (i = 0; i < numberOfBars; i++)
            Count[i] = 0;
        RunThreads(localPoints, localSize, Count, range, numberOfBars);

        MPI_Gather(Count, numberOfBars, MPI_INT, GLOBAL_COUNTS, numberOfBars, MPI_INT, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}
