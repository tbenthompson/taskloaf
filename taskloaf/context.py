import logging
import structlog
from contextlib import ExitStack, closing

from .refcounting import RefManager
from .allocator import RemoteShmemRepo, BlockManager, ShmemAllocator
from .protocol import Protocol
from .executor import Executor

class Context:
    def __init__(self, comm, cfg):
        self.name = comm.addr
        self.cfg = cfg
        self.next_id = 0

        log_name = 'taskloaf.context' + str(self.name)
        self.log = structlog.wrap_logger(logging.getLogger(log_name))

        self.comm = comm
        self.protocol = Protocol()
        self.setup_object_protocol()
        self.setup_promise_protocol()

        def recv_fnc():
            return self.comm.recv()
        # TODO: Make Executor into a context manager
        self.executor = Executor(recv_fnc, cfg, self.log)

    def __enter__(self):
        self.exit_stack = ExitStack()

        block_root_path = self.cfg.get('block_root_path', '/dev/shm/taskloaf_')
        local_root_path = block_root_path + str(self.name)
        self.remote_shmem = self.exit_stack.enter_context(closing(
            RemoteShmemRepo(block_root_path)
        ))
        self.block_manager = self.exit_stack.enter_context(closing(
            BlockManager(local_root_path)
        ))
        self.allocator = self.exit_stack.enter_context(closing(
            ShmemAllocator(self.block_manager)
        ))
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def setup_object_protocol(self):
        from .object_ref import ObjectMsg, handle_remote_get, \
            handle_remote_put, handle_ref_work
        self.object_cache = dict()
        self.protocol.add_msg_type(
            'REMOTEGET', type = ObjectMsg, handler = handle_remote_get
        )
        self.protocol.add_msg_type(
            'REMOTEPUT', type = ObjectMsg, handler = handle_remote_put
        )
        self.protocol.add_msg_type(
            'REFWORK', type = ObjectMsg, handler = handle_ref_work
        )

    def setup_promise_protocol(self):
        from .promise import TaskMsg, task_handler, set_result_handler, \
            await_handler
        self.promises = dict()
        self.protocol.add_msg_type(
            'TASK', type = TaskMsg, handler = task_handler
        )
        self.protocol.add_msg_type(
            'SETRESULT', type = TaskMsg, handler = set_result_handler
        )
        self.protocol.add_msg_type(
            'AWAIT', type = TaskMsg, handler = await_handler
        )

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def get_memory_used():
        import os
        import psutil
        process = psutil.Process(os.getpid())
        return process.memory_info().rss
