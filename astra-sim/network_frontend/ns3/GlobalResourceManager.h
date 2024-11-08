// GlobalResourceManager.h
#ifndef GLOBAL_RESOURCE_MANAGER_H
#define GLOBAL_RESOURCE_MANAGER_H

#include <semaphore.h>
#include <map>
#include <vector>
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "entry.h"



// 假设MadronaMsg和task1已经在其他地方定义
  class MadronaMsg {
   public:
    int type;
    int event_id;
    int time;
    int src;
    int dst;
    int size;
    int port;
    // 为 std::cout 添加输出流重载
    friend std::ostream& operator<<(std::ostream& os, const MadronaMsg& msg) {
      return os << "MadronaMsg{type: " << msg.type
                << ", event_id: " << msg.event_id << ", time: " << msg.time
                << ", src: " << msg.src << ", dst: " << msg.dst
                << ", port: " << msg.port << ", size: " << msg.size << "}";
    }
  };



class GlobalResourceManager {
public:
    static int shm_fd;
    static void* addr;
    static int* header;
    static sem_t* semaphore_a;
    static sem_t* semaphore_b;
    static MadronaMsg* data;

    // enent_id - task
    static std::map<int, struct task1> commTaskHash;
    static int event_id;
    // set shared memory size, less than 100 default.
    static int numMessages;


inline static bool comm_init() {
    int size = sizeof(MadronaMsg) * numMessages + sizeof(int);
    printf("size:%hd\n", size);
    shm_fd = shm_open("myshm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
      perror("shm_open");
      return false;
    }

    if (ftruncate(shm_fd, size) == -1) {
      perror("ftruncate");
      close(shm_fd);
      return false;
    }

    addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
      perror("mmap");
      close(shm_fd);
      return false;
    }
    header = (int*)addr;
    semaphore_a = sem_open("semA", O_CREAT, 0666, 0);
    semaphore_b = sem_open("semB", O_CREAT, 0666, 0);
    return true;
  }

inline static void comm_send_wait_callback(
      int event_id,
      int time,
      int type,
      int src ,
      int dst ,
      int size ,
      int port ) {
    *header = 1;
    data = (MadronaMsg*)(header + 1); // jump count location.
    data[0].type = type;
    data[0].event_id = event_id;
    data[0].time = time;
    data[0].src = src;
    data[0].dst = dst;
    data[0].size = size;
    data[0].port = port;

    sem_post(semaphore_a);
    sem_wait(semaphore_b);

    // only hanle one element event.
    if (commTaskHash.find(data[0].event_id) != commTaskHash.end()) {
      task1 t = commTaskHash[data[0].event_id];
      commTaskHash.erase(data[0].event_id);
      if (t.type == 0) {
        qp_finish(t.src, t.dest, data[0].port, data[0].size);
      } else {
        t.msg_handler(t.fun_arg);
      }
    }
  }

inline static MadronaMsg comm_send_wait_immediately(
      int event_id,
      int time,
      int type,
      int src ,
      int dst ,
      int size ,
      int port) {
    *header = numMessages;
    data = (MadronaMsg*)(header + 1); // jump count location.
    data[0].type = type;
    data[0].event_id = event_id;
    data[0].time = time;
    data[0].src = src;
    data[0].dst = dst;
    data[0].size = size;
    data[0].port = port;

    // std::cout << "size: " << size << std::endl;
    // std::cout << "AstraSim: Data written to Madrona: " << data[0] << std::endl;

    sem_post(semaphore_a);
    sem_wait(semaphore_b);
    // std::cout << "AstraSim: Received from Madrona: " << data[0] << std::endl;
    return data[0];
  }


inline static void comm_close() {
    munmap(data, sizeof(MadronaMsg) * numMessages);
    munmap(header, sizeof(int));
    shm_unlink("myshm");
    sem_close(semaphore_a);
    sem_close(semaphore_b);
    sem_unlink("semA");
    sem_unlink("semB");
    close(shm_fd);
  }


};

// 静态变量初始化
int GlobalResourceManager::shm_fd = -1;
void* GlobalResourceManager::addr = nullptr;
int* GlobalResourceManager::header = nullptr;
sem_t* GlobalResourceManager::semaphore_a = nullptr;
sem_t* GlobalResourceManager::semaphore_b = nullptr;
MadronaMsg* GlobalResourceManager::data = nullptr;
std::map<int, task1> GlobalResourceManager::commTaskHash = {};
int GlobalResourceManager::event_id = 0;
int GlobalResourceManager::numMessages = 1;

#endif // GLOBAL_RESOURCE_MANAGER_H