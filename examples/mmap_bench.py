import os
import sys
import time
import mmap
import numpy as np

# High performance interprocess shared memory in python
# Pointers can't be shared between processes
# So, --> mmap!
# Use shm_open or /dev/shm for tmpfs backing the mmap
# Write a simple allocator
# Oops! We need to align memory allocations
# TODO: Why does MAP_PRIVATE not have this perf problem?
# Using huge pages makes the MAP_SHARED version equivalent to the MAP_PRIVATE or malloc version
#TODO: Try map_populate
#TODO: Try transparent huge pages

# The story of shared memory in python
# Zero copy data formats

#https://jvns.ca/blog/2014/05/12/computers-are-fast/
#https://jvns.ca/blog/2014/05/13/profiling-with-perf/
#http://www.brendangregg.com/perf.html
#https://en.wikipedia.org/wiki/Translation_lookaside_buffer
#https://stackoverflow.com/questions/45972/mmap-vs-reading-blocks
#https://paolozaino.wordpress.com/2016/10/02/how-to-force-any-linux-application-to-use-hugepages-without-modifying-the-source-code/
# https://xkcd.com/1205/
# TODO: using likwid to OBSERVE what's wrong!
# https://github.com/RRZE-HPC/likwid/wiki

# This paper discusses all this stuff!
#http://www.mcs.anl.gov/~balaji/pubs/2015/ppmm/ppmm15.stencil.pdf

# transparent huge pages are enabled which is massively reducing the TLB misses and page-faults for both MAP_PRIVATE and malloc-ed (underlain by MAP_PRIVATE, probably). but transparent huge pages can't be used for file backed mmap. So, when I start using mmap and /dev/shm, I'm now using 4kb pages instead of 2mb pages and everything gets slow as bananas. There really isn't any way to use true IPC shared memory with transparent huge pages because using MAP_ANONYMOUS makes the memory visible only to the current process and any forked children.

# perf stat -e dTLB-load-misses,iTLB-load-misses,page-faults python mmap_bench.py shmem 10
# To enable/disable transparent hugepages
# sudo sh -c 'echo "never" > /sys/kernel/mm/transparent_hugepage/enabled'

n_iters = int(sys.argv[2])

def make_problem():
    n = int(2e7)
    x = np.random.rand(n)
    y = np.empty(n)
    idxs = np.random.randint(0, n, n)
    return x, y, idxs

def direct(x, y, idxs):
    for i in range(n_iters):
        start = time.time()
        y[:] = x[idxs] ** 2
        print('no shm square', time.time() - start)

def round_up_to_pagesize(nbytes):
    page_size = 2097152
    n_pages = np.ceil(nbytes / page_size)
    alloc_bytes = int(n_pages * page_size)
    return alloc_bytes

def shmem(x, y, idxs, hugepages = False):

    if hugepages:
        filename = '/mnt/hugepages/data'
    else:
        filename = '/dev/shm/data'

    MAP_GROWSDOWN =0x00100         # Stack-like segment.  */
    MAP_DENYWRITE =0x00800         # ETXTBSY */
    MAP_EXECUTABLE=0x01000         # Mark it as an executable.  */
    MAP_LOCKED    =0x02000         # Lock the mapping.  */
    MAP_NORESERVE =0x04000         # Don't check for reservations.  */
    MAP_POPULATE  =0x08000         # Populate (prefault) pagetables.  */
    MAP_NONBLOCK  =0x10000         # Do not block on IO.  */
    MAP_STACK     =0x20000         # Allocation is for a stack.  */
    MAP_HUGETLB   =0x40000         # Create huge page mapping.  */


    nalloc = round_up_to_pagesize(x.nbytes + y.nbytes)
    with open(filename, 'w+b') as f:
        os.ftruncate(f.fileno(), nalloc)
        mmap_file = mmap.mmap(f.fileno(), nalloc)
    # mmap_file = mmap.mmap(-1, nalloc, flags = MAP_HUGETLB | mmap.MAP_SHARED | mmap.MAP_ANONYMOUS)
    mem = memoryview(mmap_file)

    mem[:x.nbytes] = x.data.cast('B')
    mem[x.nbytes:(x.nbytes + y.nbytes)] = y.data.cast('B')

    x_shm = np.frombuffer(mem[:x.nbytes], dtype = np.float64)
    y_shm = np.frombuffer(mem[x.nbytes:(x.nbytes + y.nbytes)], dtype = np.float64)

    for i in range(n_iters):
        start = time.time()
        y_shm[:] = x_shm[idxs] ** 2
        print('shm square', time.time() - start)

    del mem, x_shm, y_shm
    mmap_file.close()
    f.close()
    os.remove(filename)

def main():
    p = make_problem()
    if sys.argv[1] == 'shmemhuge':
        shmem(*p, True)
    elif sys.argv[1] == 'shmem':
        shmem(*p, False)
    elif sys.argv[1] == 'sysv':
        sysv(*p)
    elif sys.argv[1] == 'direct':
        direct(*p)

if __name__ == "__main__":
    main()
