import os
import mmap

class Shmem:
    def __init__(self, filename):
        self.filename = filename
        self.fd = os.open(self.filename, os.O_RDWR)
        self.mem = mmap.mmap(self.fd, os.path.getsize(self.filename))

    def __del__(self):
        os.close(self.fd)

class OwnedShmem:
    def __init__(self, name, writeable):
        self.name = name
        self.writeable = writeable

    def __enter__(self):
        self.filename = os.path.join('/dev/shm', self.name)
        with open(self.filename, 'wb') as f:
            f.write(self.writeable)
            f.seek(0, 2)
            self.size = f.tell()
        del self.writeable
        return Shmem(self.filename)

    def __exit__(self, *args):
        os.remove(self.filename)
        pass
