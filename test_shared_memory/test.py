# -*- coding: utf-8 -*-

import multiprocessing
from multiprocessing import shared_memory
import time

def reader():
    shm = shared_memory.SharedMemory(name='shared_memory_example')
    buffer = shm.buf

    for _ in range(10):
        msg = buffer[:1024].tobytes().decode('utf-8').rstrip('\x00')
        print("Reader: {}".format(msg))

        time.sleep(1)

    # 关闭共享内存段
    shm.close()

if __name__ == "__main__":
    reader()
