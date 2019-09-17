import asyncio
from contextlib import ExitStack, closing

from .refcounting import RefManager
from .allocator import RemoteShmemRepo, BlockManager, ShmemAllocator


class Context:
    def __init__(self, messenger):
        self.messenger = messenger
        self.name = self.messenger.name
        self.next_id = 0

        self.protocol = messenger.protocol

        # TODO: move elsewhere?
        def handle_new_work(args):
            self.executor.add_work(args)

        self.protocol.add_msg_type("WORK", handler=handle_new_work)

        self.ref_manager = RefManager(self.protocol)
        self.setup_object_protocol()
        self.setup_promise_protocol()

    def __enter__(self):
        self.exit_stack = ExitStack()

        # TODO: configurable!
        block_root_path = "/dev/shm/taskloaf_"
        local_root_path = block_root_path + str(self.name)
        self.remote_shmem = self.exit_stack.enter_context(
            closing(RemoteShmemRepo(block_root_path))
        )
        self.block_manager = self.exit_stack.enter_context(
            closing(BlockManager(local_root_path))
        )
        self.allocator = self.exit_stack.enter_context(
            closing(ShmemAllocator(self.block_manager))
        )
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

    def setup_object_protocol(self):
        from .object_ref import (
            ObjectMsg,
            handle_remote_get,
            handle_remote_put,
            handle_ref_work,
        )

        self.object_cache = dict()
        self.protocol.add_msg_type(
            "REMOTEGET", serializer=ObjectMsg, handler=handle_remote_get
        )
        self.protocol.add_msg_type(
            "REMOTEPUT", serializer=ObjectMsg, handler=handle_remote_put
        )
        self.protocol.add_msg_type(
            "REFWORK", serializer=ObjectMsg, handler=handle_ref_work
        )

    def setup_promise_protocol(self):
        from .promise import (
            TaskMsg,
            task_handler,
            set_result_handler,
            await_handler,
        )

        self.promises = dict()
        self.protocol.add_msg_type(
            "TASK", serializer=TaskMsg, handler=task_handler
        )
        self.protocol.add_msg_type(
            "SETRESULT", serializer=TaskMsg, handler=set_result_handler
        )
        self.protocol.add_msg_type(
            "AWAIT", serializer=TaskMsg, handler=await_handler
        )

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def get_memory_used():
        import os
        import psutil

        process = psutil.Process(os.getpid())
        return process.memory_info().rss

    def get_all_names(self):
        return [self.name] + list(self.messenger.endpts.keys())

    async def wait_for_workers(self, n_workers):
        # TODO: what if there will never be n_workers available?
        while len(self.messenger.endpts.keys()) < n_workers - 1:
            await asyncio.sleep(0)
        return self.get_all_names()
