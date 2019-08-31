import logging
import structlog
from contextlib import ExitStack, closing

from .refcounting import RefManager
from .allocator import RemoteShmemRepo, BlockManager, ShmemAllocator
from .protocol import Protocol

class Context:
    def __init__(self, name, comm, cfg):
        self.name = name
        self.cfg = cfg
        self.next_id = 0

        log_name = 'taskloaf.context' + str(self.name)
        self.log = structlog.wrap_logger(logging.getLogger(log_name))

        self.comm = comm
        self.protocol = Protocol()

        #TODO: move elsewhere?
        def handle_new_work(args):
            self.executor.work.append(args)
        self.protocol.add_msg_type('WORK', handler = handle_new_work)

        # JOIN = send a message asking to MEET
        #        receiver sends a MEET reply
        #
        # MEET = send a message with tuples of form (name, hostname, port) for
        #        all friends receiver then MEETs all new friends (possibly
        #        including the one they received the MEET from)
        #
        # A worker will JOIN then receive the MEET replies and MEET all the new
        # friends.
        #
        # A client will JOIN then receive the MEET replies and JOIN all the new
        # friends. As a result, a client learns about the whole cluster but the
        # cluster doesn't know about the client or send the client info. Is
        # this a good idea? Then how to send info out to the client? By
        # hostname/port?

        #TODO: move elsewhere?
        #TODO: send my info and all my friends info to a new acquiantance
        # once they receive it, "MEET" all new friends, reply with my info
        # to sender
        # TODO: could be optimized with capnp
        def handle_join(args):

        self.protocol.add_msg_type('JOIN', handler = handle_join)
        def handle_meet(args):
            new_friend = args[0]
            their_friends = args[1]
            self.comm.add_friend(
        self.protocol.add_msg_type('MEET', handler = handle_meet)

        self.setup_object_protocol()
        self.setup_promise_protocol()

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

    def poll_fnc(self):
        msg = self.comm.recv()
        # print(msg)
        if msg is not None:
            m, args = self.protocol.decode(self, memoryview(msg))
            self.cur_msg = m
            self.protocol.handle(self, m.typeCode, args)
            self.cur_msg = None

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
