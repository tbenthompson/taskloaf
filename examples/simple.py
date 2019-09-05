import taskloaf as tsk


async def f():
    print("FFFF")
    # await tsk.task(lambda w: print("hI"))
    # raise Exception("HI")
    return 123


if __name__ == "__main__":
    r = tsk.run(f)
    print(r)
    assert r == 123

    # with tsk.zmq_cluster(10) as cluster:
    #     with tsk.zmq_client(cluster) as client:
    #         import time

    #         time.sleep(5.0)
    #         client.update_cluster()
    #         print(len(client.worker_names))
    #         # subs = []
    #         # for i in range(10):
    #         #     async def g(i = i):
    #         #         return i
    #         #     idx = i % len(client.worker_names)
    #         #     to = client.worker_names[idx]
    #         #     subs.append(client.submit(g, to = to))
    #         # for s in subs[::-1]:
    #         #     print(client.wait(s))
