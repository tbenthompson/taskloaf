from mpi4py import MPI
def mpi_procs(n):
    n_procs_available = MPI.COMM_WORLD.Get_size()
    def decorator(test_fnc):
        test_fnc.n_procs = n
        return test_fnc
    return decorator

