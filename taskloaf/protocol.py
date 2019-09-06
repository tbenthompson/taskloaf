import cloudpickle
import capnp  # noqa
import taskloaf.message_capnp

import logging

log = logging.getLogger(__name__)


class CloudPickleMsg:
    @staticmethod
    def serialize(args):
        msg = taskloaf.message_capnp.Message.new_message()
        msg.arbitrary = cloudpickle.dumps(args)
        return msg

    @staticmethod
    def deserialize(msg):
        return cloudpickle.loads(msg.arbitrary)


class Protocol:
    def __init__(self):
        self.msg_types = []
        self.printer = print

    def add_msg_type(self, name, *, serializer=CloudPickleMsg, handler=None):

        type_code = len(self.msg_types)
        setattr(self, name, type_code)
        self.msg_types.append((serializer, handler, name))
        return type_code

    def encode(self, source_name, type_code, *args):
        serializer = self.msg_types[type_code][0]
        m = serializer.serialize(*args)
        m.typeCode = type_code
        m.sourceName = source_name
        return memoryview(m.to_bytes())

    def handle(self, msg_buf, *args, **kwargs):
        msg = taskloaf.message_capnp.Message.from_bytes(msg_buf)
        serializer, handler, _ = self.msg_types[msg.typeCode]
        data = serializer.deserialize(msg)
        log.info(
            f"handling from {msg.sourceName}"
            f"a {self.get_name(msg.typeCode)} with data: {str(data)}"
        )
        handler(data, *args, **kwargs)

    def get_name(self, type_code):
        return self.msg_types[type_code][2]
