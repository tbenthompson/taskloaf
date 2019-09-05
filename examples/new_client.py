import taskloaf as tsk


def main():
    with tsk.zmq_client((tsk.localhost, 5755)) as c:

        async def f():
            print("SENDING")
            return 123

        print(c.wait(c.submit(f)))


if __name__ == "__main__":
    main()
