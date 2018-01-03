import taskloaf as tsk

async def submit(w):
    n = await tsk.task(w, lambda w: 10, to = 1)
    pr = tsk.when_all(
        [tsk.task(w, lambda w, i = i: i, to = i % 2) for i in range(n)]
    ).then(lambda w, x: sum(x))
    return (await pr)

if __name__ == "__main__":
    result = tsk.cluster(2, submit)

    from mpi4py import MPI
    if MPI.COMM_WORLD.Get_rank() == 0:
        print(result)
        assert(result == 45)
