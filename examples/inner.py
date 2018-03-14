import taskloaf as tsk

async def submit(w):
    async def submit2(w):
        print("HI!")
    tsk.cluster(2, submit2)

if __name__ == "__main__":
    tsk.cluster(2, submit)
