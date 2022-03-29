#include "./vectorProd.h"

void checkSizeCSendNext(uint64_t *currentIdx, uint64_t sizeC, uint16_t rank, MPI_Comm comm)
{
    // don't overrun work if m < size
    if (*currentIdx < sizeC)
    {
        // cout << "sending row: " << curIdx << ", to rank: " << i << endl;
        MPI_Send(currentIdx, 1, MPI_UINT64_T, rank, 0, comm);
        *currentIdx += 1;
    }
}

/**
 * @brief 2D Matrix dot product with 2D Matrix.
 *
 * @param A The left matrix
 * @param m The row count of the left matrix
 * @param n The column count of the left matrix
 * @param B The right matrix
 * @param p The column count of the right matrix
 * @param comm The current MPI_Comm
 * @return The pointer to the result
 */
double *matrixProductRowByRow(double *A, uint64_t m, uint64_t n, double *B, uint64_t p, MPI_Comm comm)
{
    int rank, commSize, sendBuf, flag = 0;
    const uint64_t sizeC = p * m;

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &commSize);
    MPI_Datatype vectorCalcType = mpiDatatypeVectorCalcType();

    double *C = NULL;
    vectorCalcStruct vecBuf;

    if (rank == 0)
    {
        MPI_Request request;
        MPI_Status status;
        uint64_t returnCounter = 0;
        uint64_t curIdx = 0;
        C = new double[sizeC];

        while (1)
        {
            MPI_Irecv(&vecBuf, 1, vectorCalcType, MPI_ANY_SOURCE, 0, comm, &request);

            // On initial send, start workers
            if (!curIdx)
            {
                for (size_t i = 1; i < commSize; i++)
                {
                    checkSizeCSendNext(&curIdx, sizeC, i, comm);
                }
            }

            // Kill process when calculated
            if (returnCounter >= sizeC)
            {
                // cout << "returnCounter: " << returnCounter << endl;
                // cout << "sending poison pill" << endl;
                sendBuf = POISON_PILL;
                for (size_t i = 1; i < commSize; i++)
                {
                    MPI_Send(&sendBuf, 1, MPI_UINT64_T, i, 0, comm);
                }
                break;
            }

            while (1)
            {
                MPI_Test(&request, &flag, &status);

                if (flag)
                {
                    returnCounter++;
                    // cout << "returnCounter: " << returnCounter << endl;

                    uint64_t idx = vecBuf.idx;
                    uint16_t recRank = status.MPI_SOURCE;
                    double recValue = vecBuf.value;

                    C[idx] = recValue;

                    // cout << "value: " << recValue << ", from rank: " << recRank << ", idx: " << idx << endl;

                    checkSizeCSendNext(&curIdx, sizeC, recRank, comm);
                    break;
                };
            }
        }
    }
    else
    {
        int recBuf;

        while (1)
        {
            MPI_Recv(&recBuf, 1, MPI_UINT64_T, MPI_ANY_SOURCE, 0, comm, MPI_STATUS_IGNORE);

            if (recBuf == POISON_PILL)
            {
                break;
            }

            vecBuf.idx = recBuf;
            vecBuf.value = 0;

#ifndef MAKE_TEST
            if (vecBuf.idx == 3)
            {
                cout << "rank: " << rank << ", idx: " << vecBuf.idx << endl;
            }
#endif

            // First Matrix: MxN
            // Second Matrix: NxP
            // Result : MxP
            for (size_t i = 0; i < n; i++)
            {
                // The first values of A are on the first row (e.g., idx/p == 0), indexed by i % n.
                // Use the columns of B (i * b) and incrememt by the idx % p.
                vecBuf.value += A[(vecBuf.idx / p) * n + i % n] * B[i * p + vecBuf.idx % p];
            }

            MPI_Send(&vecBuf, 1, vectorCalcType, 0, 0, comm);
        }
    }

    // cout << "rank: " << rank << " exiting" << endl;

    return C;
}

/**
 * @brief 2D Matrix - 2D Matrix product. It is assumed that matrix A is m x n and
 *        matrix B is n x p. Result is a m x p matrix stored in array form.
 *
 * @param A The left matrix
 * @param m The row count of the left matrix
 * @param n The column count of the left matrix
 * @param B The right matrix
 * @param p The column count of the right matrix
 * @param comm The current MPI_Comm
 * @return The pointer to the result
 */
double *matrixProduct(double *A, uint64_t m, uint64_t n, double *B, uint64_t p, MPI_Comm comm)
{
    // The fastest in benchmark so far
    return matrixProductRowByRow(A, m, n, B, p, comm);
}