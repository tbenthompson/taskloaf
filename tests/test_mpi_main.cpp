#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <mpi.h>

int main(int argc, char* const argv[])
{
    MPI_Init(nullptr, nullptr);
    int result = Catch::Session().run( argc, argv );
    MPI_Finalize();
    return result;
}
