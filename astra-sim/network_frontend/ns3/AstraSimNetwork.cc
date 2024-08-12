#include "astra-sim/system/AstraNetworkAPI.hh"
#include "astra-sim/system/Sys.hh"
// #include "ns3/applications-module.h"
// #include "ns3/core-module.h"
// #include "ns3/csma-module.h"
// #include "ns3/internet-module.h"
// #include "ns3/network-module.h"
#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "entry.h"
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>

using namespace std;
// using namespace ns3;

// std::vector<string> workloads{"microAllReduce.txt", "microAllToAll.txt"};
// std::vector<std::vector<int>> physical_dims{{8, 4}, {8, 4}};
std::vector<string> workloads{"microAllReduce.txt"};
std::vector<std::vector<int>> physical_dims{{2, 1}};

// add for madrona
// enent_id - task
map<int, struct task1> commTaskHash;
int event_id = 0;
// set shared memory size,less than 100 default.
int numMessages = 1;

queue<struct task1> workerQueue;
unsigned long long tempcnt = 999;
unsigned long long cnt = 0;



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

  // void comm() {
  //   int size = sizeof(MadronaMsg) * numMessages + sizeof(int);
  //   printf("size:%hd\n", size);
  //   int shm_fd = shm_open("myshm", O_CREAT | O_RDWR, 0666);
  //   if (shm_fd == -1) {
  //     perror("shm_open");
  //     return;
  //   }

  //   if (ftruncate(shm_fd, size) == -1) {
  //     perror("ftruncate");
  //     close(shm_fd);
  //     return;
  //   }

  //   void* addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  //   if (addr == MAP_FAILED) {
  //     perror("mmap");
  //     close(shm_fd);
  //     return;
  //   }

  //   // msg count
  //   int* header = (int*)addr;
  //   *header = numMessages;

  //   MadronaMsg* data = (MadronaMsg*)(header + 1); // jump count location.
  //   for (int i = 0; i < numMessages; i++) {
  //     data[i].type = i;
  //     data[i].event_id = i + 100;
  //     data[i].time = i * 1000;
  //     data[i].src = i * 10;
  //     data[i].dst = i * 20;
  //     data[i].size = i * 30;
  //   }

  //   std::cout << "size: " << size << std::endl;
  //   std::cout << "C++: Data written to shared memory: " << data[0] << std::endl;

  //   sem_t* semaphore_a = sem_open("semA", O_CREAT, 0666, 0);
  //   sem_t* semaphore_b = sem_open("semB", O_CREAT, 0666, 0);
  //   sem_post(semaphore_a);
  //   sem_wait(semaphore_b);
  //   std::cout << "C++: Received from Python: " << data[0] << std::endl;

  //   munmap(data, sizeof(MadronaMsg) * numMessages);
  //   munmap(header, sizeof(int));
  //   shm_unlink("myshm");
  //   sem_close(semaphore_a);
  //   sem_close(semaphore_b);
  //   sem_unlink("semA");
  //   sem_unlink("semB");
  //   close(shm_fd);
  // }

  int shm_fd;
  void* addr;
  int* header;
  sem_t* semaphore_a;
  sem_t* semaphore_b;
  MadronaMsg* data;
  bool comm_init() {
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

  void comm_send_wait_callback(
      int event_id,
      int time,
      int type,
      int src = 0,
      int dst = 0,
      int size = 0,
      int port = 0) {
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

    if (commTaskHash.find(data[0].event_id) != commTaskHash.end()) {
      task1 t = commTaskHash[data[0].event_id];
      commTaskHash.erase(data[0].event_id);
      t.msg_handler(t.fun_arg);
    }
  }

  MadronaMsg comm_send_wait_immediately(
      int event_id,
      int time,
      int type,
      int src = 0,
      int dst = 0,
      int size = 0,
      int port=0) {
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

  void comm_close() {
    munmap(data, sizeof(MadronaMsg) * numMessages);
    munmap(header, sizeof(int));
    shm_unlink("myshm");
    sem_close(semaphore_a);
    sem_close(semaphore_b);
    sem_unlink("semA");
    sem_unlink("semB");
    close(shm_fd);
  }


struct sim_event {
  void* buffer;
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
  int sim_comm_size(AstraSim::sim_comm comm, int* size) {
    return 0;
  }
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
    // todo
    comm_close();
    exit(0);
    return 0;
  }
  double sim_time_resolution() {
    return 0;
  }
  int sim_init(AstraSim::AstraMemoryAPI* MEM) {
    return 0;
  }
  AstraSim::timespec_t sim_get_time() {
    // AstraSim::timespec_t timeSpec;
    // timeSpec.time_val = Simulator::Now().GetNanoSeconds();
    // return timeSpec;
    // to do

    // AstraSim::timespec_t timeSpec;
    // timeSpec.time_val = 0;
    // return timeSpec;
    task1 t;
    t.type = 3;
    event_id++;
    commTaskHash[event_id] = t;
    MadronaMsg msg = comm_send_wait_immediately(event_id, 0, 3);
    AstraSim::timespec_t timeSpec;
    timeSpec.time_val = msg.time;
    return timeSpec;
  }
  virtual void sim_schedule(
      AstraSim::timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg) {
     
    task1 t;
    t.type = 2;
    t.fun_arg = fun_arg;
    t.msg_handler = fun_ptr;
    t.schTime = delta.time_val;
    // to do
    // Simulator::Schedule(NanoSeconds(t.schTime), t.msg_handler, t.fun_arg);
    event_id++;
    commTaskHash[event_id] = t;
    comm_send_wait_callback(event_id, 0, 2);
    printf("sim_schedule: %f\n", delta.time_val);
    return;
  }
  virtual int sim_send(
      void* buffer, // not yet used
      uint64_t count, // number of bytes to be send
      int type, // not yet used
      int dst,
      int tag, // not yet used
      AstraSim::sim_request* request, // not yet used
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) {
    
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
    // to do
    // SendFlow(rank, dst, count, msg_handler, fun_arg, tag);
    event_id++;
    commTaskHash[event_id] = t;

    // entry.cc
    uint32_t port = portNumber[rank][dst]++; // get a new port number
    sender_src_port_map[make_pair(port, make_pair(rank, dst))] = tag;
    int pg = 3, dport = 100;
    flow_input.idx++;


    comm_send_wait_callback(event_id, 0, 0, rank, dst, count,port);
    printf("sim_send: %f\n", 0.0);
    return 0;
  }
  virtual int sim_recv(
      void* buffer,
      uint64_t count,
      int type,
      int src,
      int tag,
      AstraSim::sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) {
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

int main(int argc, char* argv[]) {
  numMessages = 1;
  printf("Start Comm....\n");
  comm_init();
  printf("End....\n");

  printf("Start....\n");
  float comm_scale = 1;

  CommandLine cmd;
  cmd.AddValue("commscale", "Communication Scale", comm_scale);
  cmd.Parse(argc, argv);

  assert(workloads.size() == physical_dims.size());
  int num_gpus = 0;
  for (auto& a : physical_dims) {
    int job_npus = 1;
    for (auto& dim : a) {
      job_npus *= dim;
    }
    num_gpus += job_npus;
  }

  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  std::vector<ASTRASimNetwork*> networks(num_gpus, nullptr);
  std::vector<AstraSim::Sys*> systems(num_gpus, nullptr);

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
          nullptr, // AstraMemoryAPI
          j, // id
          npu_offset, // npu ofsset in a multi-job scenario
          1, // num_passes
          physical_dims[i], // dimensions
          queues_per_dim, // queues per corresponding dimension
          "../../../../../astra-sim/inputs/system/" +
              system_input, // system configuration
          "../../../../../astra-sim/inputs/workload/" +
              workloads[i], // DLRM_HybridParallel.txt, //
                            // Resnet50_DataParallel.txt, // workload
                            // configuration
          comm_scale, // communication scale
          1, // computation scale
          1, // injection scale
          1,
          0, // total_stat_rows and stat_row
          "scratch/results/", // stat file path
          "test1", // run name
          true, // separate_log
          false // randezvous protocol
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
  // printf("Start Comm....\n");
  // comm();
  // printf("End....\n");

  //to do
  //push madrona run next frame
  //event type == -100
  while(true)
  {
    event_id++;
    int type=-100;
    comm_send_wait_callback(event_id, 0, type, 0, 0, 0);
  }


  return 0;
}
