#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <mpi.h>

int main(int argc, char* const argv[])
{
    MPI_Init(nullptr, nullptr);
    int result = doctest::Context(argc, argv).run();
    MPI_Finalize();
    return result;
}
