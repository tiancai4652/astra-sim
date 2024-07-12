#include "astra-sim/system/AstraNetworkAPI.hh"
#include "astra-sim/system/Sys.hh"
#include "ns3/applications-module.h"
// #include "ns3/core-module.h"
// #include "ns3/csma-module.h"
// #include "ns3/internet-module.h"
// #include "ns3/network-module.h"
#include "entry.h"
#include <execinfo.h>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdio.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <iostream>

using namespace std;
// using namespace ns3;

// std::vector<string> workloads{"microAllReduce.txt", "microAllToAll.txt"};
// std::vector<std::vector<int>> physical_dims{{8, 4}, {8, 4}};
std::vector<string> workloads{"microAllReduce.txt"};
std::vector<std::vector<int>> physical_dims{{2,1}};

queue<struct task1> workerQueue;
unsigned long long tempcnt = 999;
unsigned long long cnt = 0;
struct sim_event {
  void *buffer;
  uint64_t count;
  int type;
  int dst;
  int tag;
  string fnType;
};
class ASTRASimNetwork : public AstraSim::AstraNetworkAPI {
private:
  int npu_offset;

public:
  queue<sim_event> sim_event_queue;
  ASTRASimNetwork(int rank, int npu_offset) : AstraNetworkAPI(rank) {
    this->npu_offset = npu_offset;
  }
  ~ASTRASimNetwork() {}
  int sim_comm_size(AstraSim::sim_comm comm, int *size) { return 0; }
  int sim_finish() {
    for (auto it = nodeHash.begin(); it != nodeHash.end(); it++) {
      pair<int, int> p = it->first;
      if (p.second == 0) {
        cout << "All data sent from node " << p.first << " is " << it->second
             << "\n";
      } else {
        cout << "All data received by node " << p.first << " is " << it->second
             << "\n";
      }
    }
    exit(0);
    return 0;
  }
  double sim_time_resolution() { return 0; }
  int sim_init(AstraSim::AstraMemoryAPI *MEM) { return 0; }
  AstraSim::timespec_t sim_get_time() {
    // AstraSim::timespec_t timeSpec;
    // timeSpec.time_val = Simulator::Now().GetNanoSeconds();
    // return timeSpec;
    // to do
    AstraSim::timespec_t timeSpec;
    timeSpec.time_val = 0;
    return timeSpec;
  }
  virtual void sim_schedule(AstraSim::timespec_t delta,
                            void (*fun_ptr)(void *fun_arg), void *fun_arg) {
    task1 t;
    t.type = 2;
    t.fun_arg = fun_arg;
    t.msg_handler = fun_ptr;
    t.schTime = delta.time_val;
    // to do
    // Simulator::Schedule(NanoSeconds(t.schTime), t.msg_handler, t.fun_arg);
    printf("sim_schedule: %f\n", delta.time_val);
    return;
  }
  virtual int sim_send(void *buffer,   // not yet used
                       uint64_t count, // number of bytes to be send
                       int type,       // not yet used
                       int dst,
                       int tag,                        // not yet used
                       AstraSim::sim_request *request, // not yet used
                       void (*msg_handler)(void *fun_arg), void *fun_arg) {
    dst += npu_offset;
    // if(rank==0 && dst == 1 && cnt == 0){
    //	cout<<“rank 0 and destination 1 sim_send test\n”;
    // }
    task1 t;
    t.src = rank;
    t.dest = dst;
    t.count = count;
    t.type = 0;
    t.fun_arg = fun_arg;
    t.msg_handler = msg_handler;
    sentHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
    SendFlow(rank, dst, count, msg_handler, fun_arg, tag);
    printf("sim_send: %f\n", 0.0);
    return 0;
  }
  virtual int sim_recv(void *buffer, uint64_t count, int type, int src, int tag,
                       AstraSim::sim_request *request,
                       void (*msg_handler)(void *fun_arg), void *fun_arg) {
    src += npu_offset;
    task1 t;
    t.src = src;
    t.dest = rank;
    t.count = count;
    t.type = 1;
    t.fun_arg = fun_arg;
    t.msg_handler = msg_handler;
    if (recvHash.find(make_pair(tag, make_pair(t.src, t.dest))) !=
        recvHash.end()) {
      int count = recvHash[make_pair(tag, make_pair(t.src, t.dest))];
      if (count == t.count) {
        recvHash.erase(make_pair(tag, make_pair(t.src, t.dest)));
        t.msg_handler(t.fun_arg);
      } else if (count > t.count) {
        recvHash[make_pair(tag, make_pair(t.src, t.dest))] = count - t.count;
        t.msg_handler(t.fun_arg);
      } else {
        recvHash.erase(make_pair(tag, make_pair(t.src, t.dest)));
        t.count -= count;
        expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
      }
    } else {
      if (expeRecvHash.find(make_pair(tag, make_pair(t.src, t.dest))) ==
          expeRecvHash.end()) {
        expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
      } else {
        int expecount =
            expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))].count;
        t.count += expecount;
        expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
      }
    }
    return 0;
  }
  void handleEvent(int dst, int cnt) {
    // cout<<"event handled\n";
  }
};



// set shared memory size,less than 100 default.
int numMessages=10;

class MadronaMsg{
public:
int type;
int event_id;
int time;
int src;
int dst;
int size;
// 为 std::cout 添加输出流重载
    friend std::ostream& operator<<(std::ostream& os, const MadronaMsg& msg) {
        return os << "MadronaMsg{type: " << msg.type
                  << ", event_id: " << msg.event_id
                  << ", time: " << msg.time
                  << ", src: " << msg.src
                  << ", dst: " << msg.dst
                  << ", size: " << msg.size << "}";
    }
};

void comm() {

    int size = sizeof(MadronaMsg) * numMessages + sizeof(int);
    printf("size:%hd\n", size); 
    int shm_fd = shm_open("myshm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return;
    }

    if (ftruncate(shm_fd, size) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return;
    }

    // 映射整个内存区域
    void* addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return;
    }

    // 访问头部信息（消息数量）
    int* header = (int*) addr;
    *header = numMessages;

    // 访问 MadronaMsg 数组
    MadronaMsg* data = (MadronaMsg*) (header + 1);  // 利用指针偏移，跳过一个 int 的位置
    for (int i = 0; i < numMessages; i++) {
        data[i].type = i;
        data[i].event_id = i + 100;
        data[i].time = i * 1000;
        data[i].src = i * 10;
        data[i].dst = i * 20;
        data[i].size = i * 30;
    }

    std::cout << "size: " << size << std::endl;
    std::cout << "C++: Data written to shared memory: " << data[0] << std::endl;


    sem_t* semaphore_a = sem_open("semA", O_CREAT, 0666, 0);
    sem_t* semaphore_b = sem_open("semB", O_CREAT, 0666, 0);
    sem_post(semaphore_a);
    sem_wait(semaphore_b);
    std::cout << "C++: Received from Python: " << data[0] << std::endl;

    munmap(data, sizeof(MadronaMsg) * numMessages);
    munmap(header, sizeof(int));
    shm_unlink("myshm");
    sem_close(semaphore_a);
    sem_close(semaphore_b);
    sem_unlink("semA");
    sem_unlink("semB");
    close(shm_fd);
}




int main(int argc, char *argv[]) {

  printf("Start....\n");
  float comm_scale = 1;

  CommandLine cmd;
  cmd.AddValue("commscale", "Communication Scale", comm_scale);
  cmd.Parse(argc, argv);

  assert(workloads.size() == physical_dims.size());
  int num_gpus = 0;
  for (auto &a : physical_dims) {
    int job_npus = 1;
    for (auto &dim : a) {
      job_npus *= dim;
    }
    num_gpus += job_npus;
  }

  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  std::vector<ASTRASimNetwork *> networks(num_gpus, nullptr);
  std::vector<AstraSim::Sys *> systems(num_gpus, nullptr);

  int npu_offset = 0;
  for (int i = 0; i < physical_dims.size(); i++) {
    std::vector<int> queues_per_dim(physical_dims[i].size(), 1);
    // determining the appropriate system input file
    std::string system_input;
    if (physical_dims[i].size() == 1) {
      system_input = "sample_1D_switch_sys.txt";
    } else if (physical_dims[i].size() == 2) {
      system_input = "sample_2D_switch_sys.txt";
    } else if (physical_dims[i].size() == 3) {
      system_input = "sample_3D_switch_sys.txt";
    }
    // initializing the net and sys layers
    int job_npus = 1;
    for (auto dim : physical_dims[i]) {
      job_npus *= dim;
    }
    for (int j = 0; j < job_npus; j++) {
      networks[j + npu_offset] =
          new ASTRASimNetwork(j + npu_offset, npu_offset);
      systems[j + npu_offset] = new AstraSim::Sys(
          networks[j + npu_offset], // AstraNetworkAPI
          nullptr,                  // AstraMemoryAPI
          j,                        // id
          npu_offset,               // npu ofsset in a multi-job scenario
          1,                        // num_passes
          physical_dims[i],         // dimensions
          queues_per_dim,           // queues per corresponding dimension
          "../../../../../astra-sim/inputs/system/" +
              system_input, // system configuration
          "../../../../../astra-sim/inputs/workload/" +
              workloads[i], // DLRM_HybridParallel.txt, //
                            // Resnet50_DataParallel.txt, // workload
                            // configuration
          comm_scale, // communication scale
          1,          // computation scale
          1,          // injection scale
          1,
          0,                  // total_stat_rows and stat_row
          "scratch/results/", // stat file path
          "test1",            // run name
          true,               // separate_log
          false               // randezvous protocol
      );
    }
    npu_offset += job_npus;
  }
  // main1(argc, argv);
  // pass number of nodes
  for (int i = 0; i < num_gpus; i++) {
    systems[i]->workload->fire();
  }

  // to do
  // Simulator::Run();
  // // Simulator::Stop(TimeStep (0x7fffffffffffffffLL));
  // Simulator::Stop(Seconds(2000000000));
  // Simulator::Destroy();
  printf("Start Comm....\n");
  comm();
  printf("End....\n");
  
  return 0;
}

