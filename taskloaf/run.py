import taskloaf.worker
import taskloaf.promise
import taskloaf.allocator
import taskloaf.refcounting
import taskloaf.ref
import contextlib

def add_plugins(worker):
    block_root_path = '/dev/shm/taskloaf_' + str(worker.addr) + '_'
    block_manager = worker.exit_stack.enter_context(contextlib.closing(
        taskloaf.allocator.BlockManager(block_root_path)
    ))
    worker.allocator = worker.exit_stack.enter_context(contextlib.closing(
        taskloaf.allocator.ShmemAllocator(block_manager)
    ))
    worker.ref_manager = taskloaf.refcounting.RefManager(worker)
    worker.object_cache = dict()
    # worker.remote_shmem = taskloaf.allocator.RemoteShmemRepo(worker.exit_stack)
    taskloaf.ref.setup_protocol(worker)
    taskloaf.promise.setup_protocol(worker)
    return worker

@contextlib.contextmanager
def null_comm_worker():
    try:
        with taskloaf.worker.Worker(taskloaf.worker.NullComm()) as worker:
            add_plugins(worker)
            yield worker
    finally:
        pass

def run(coro):
    async def wrapper(w):
        result = await coro(w)
        taskloaf.worker.shutdown(w)
        return result

    with null_comm_worker() as worker:
        return worker.start(wrapper)
