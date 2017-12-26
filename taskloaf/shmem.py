import os
import mmap
import uuid
import contextlib

def get_size_from_fd(fd):
    return os.fstat(fd).st_size

def mmap_full_file(fd):
    return mmap.mmap(fd, get_size_from_fd(fd))

class Shmem:
    def __init__(self, filename):
        self.setup(filename)

    def setup(self, filename):
        self.filename = filename
        self.file = open(self.filename, 'r+b')
        self.mem = mmap_full_file(self.file.fileno())

    def __getstate__(self):
        return self.filename

    def __setstate__(self, newstate):
        self.setup(newstate)

    def __del__(self):
        #TODO: This isn't great and may leak file descriptors because python
        # __del__ methods are not a good resource management mechanism
        self.file.close()

# Note: File descriptors vs filenames
# After creating a shared memory block, we need to be able to reference that
# block of memory and share it between processes. We could remove the file
# immediately so we don't leave cruft hanging around on the filesystem,
# and do just fine sharing the file descriptor with other processes. This
# approach relies on sending file descriptors over UNIX domain sockets. This
# sounds annoying and requires some extra code. It's probably the right
# solution so it'll happen eventually.
# The alternate approach is simply to share a filename. This has the
# disadvantage that the file will remain on the filesystem if the process dies
# without running the corresponding clean up code.

@contextlib.contextmanager
def alloc_shmem(obj):
    unique_filename = str(uuid.uuid4())
    filename = os.path.join('/dev/shm', unique_filename)
    try:
        with open(filename, 'w+b') as f:

            if type(obj) is int:
                # Create a sparse file of the proper size.
                f.seek(obj - 1)
                f.write(bytes(1))
                f.seek(0)
            else:
                f.write(obj)
                f.seek(0)
        yield Shmem(filename)
    finally:
        os.remove(filename)
