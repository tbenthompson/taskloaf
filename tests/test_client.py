import asyncio
import multiprocessing
import zmq
import cloudpickle
import taskloaf

def test_client_server():
    bind_addr = 'tcp://127.0.0.1:5555'

    def server_proc():
        async def server(w):
            with w.comm.ctx.socket(zmq.REP) as new_task_socket:
                new_task_socket.bind(bind_addr)
                while not w.stop:
                    if new_task_socket.poll(timeout = 0) == 0:
                        await asyncio.sleep(0)
                    task_bytes = new_task_socket.recv()
                    task = cloudpickle.loads(task_bytes)
                    out = await w.wait_for_work(task)
                    out_bytes = cloudpickle.dumps(out)
                    new_task_socket.send(out_bytes)
        taskloaf.cluster(2, server)

    def client_proc():
        ctx = zmq.Context()
        def launch_job(f):
            with ctx.socket(zmq.REQ) as send_socket:
                send_socket.connect(bind_addr)
                task_bytes = cloudpickle.dumps(f)
                send_socket.send(task_bytes)
                out_bytes = send_socket.recv()
                out = cloudpickle.loads(out_bytes)
                return out

        for i in range(10):
            launch_job(lambda w: print("HI" + str(i)))
            print(launch_job(lambda w: 'abc'))
        print(launch_job(taskloaf.worker.shutdown))

    p_server = multiprocessing.Process(target = server_proc)
    p_server.start()

    p_client = multiprocessing.Process(target = client_proc)
    p_client.start()
    p_client.join()

    p_server.join()
