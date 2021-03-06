#include "utils/cluster.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

namespace singa {

Cluster* Cluster::Setup(int job, const SingaProto& singaConf,
                        const ClusterProto& clusterConf) {
  Singleton<Cluster>::Instance()->Init(job, singaConf, clusterConf);
  return Singleton<Cluster>::Instance();
}

Cluster* Cluster::Get() {
  if (!Singleton<Cluster>::Instance()->nprocs_) {
    LOG(ERROR) << "The first call to Get should "
               << "provide the sys/model conf path";
  }
  return Singleton<Cluster>::Instance();
}

void Cluster::Register(int pid, const std::string& endpoint) {
  procs_id_ = cluster_rt_->RegistProc(endpoint, pid);
  CHECK_GE(procs_id_, 0);
  CHECK_LT(procs_id_, nprocs());
  LOG(ERROR) << "proc #" << procs_id_ << " -> " << endpoint
             << " (pid = " << pid << ")";
}

void Cluster::Init(int job, const SingaProto& singaConf,
                   const ClusterProto& clusterConf) {
  cluster_ = clusterConf;
  singa_ = singaConf;
  SetupFolders(clusterConf);
  if (server_worker_separate())
    nprocs_ = nworker_procs() + nserver_procs();
  else
    nprocs_ = std::max(nworker_procs(), nserver_procs());

  // locate the process id of every worker/server
  int ngrps = cluster_.nworker_groups();
  int grp_size = cluster_.nworkers_per_group();
  int procs = 0;
  for (int i = 0; i < ngrps; ++i) {
    for (int j = 0; j < grp_size; ++j) {
      procs = (i * grp_size + j) / cluster_.nworkers_per_procs();
      procs_ids_[Hash(i, j, kWorkerLayer)] = procs;
      procs_ids_[Hash(i, j, kWorkerParam)] = procs;
    }
  }
  int offset = cluster_.server_worker_separate() ? procs + 1 : 0;
  ngrps = cluster_.nserver_groups();
  grp_size = cluster_.nservers_per_group();
  for (int i = 0; i < ngrps; ++i) {
    for (int j = 0; j < grp_size; ++j) {
      procs_ids_[Hash(i, j, kServer)] =
          (i * grp_size + j) / cluster_.nservers_per_procs() + offset;
    }
  }
  cluster_rt_ = new ClusterRuntime(singa_.zookeeper_host(), job);
  cluster_rt_->Init();
  hostip_ = GetHostIP();
}

void Cluster::SetupFolders(const ClusterProto &cluster) {
  // create visulization folder
  mkdir(vis_folder().c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  // create checkpoint folder
  mkdir(checkpoint_folder().c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int Cluster::Hash(int gid, int id, int flag) {
  int ret = -1;
  if (flag == kServer) {
    ret = (flag * cluster_.nserver_groups() + gid)
          * cluster_.nservers_per_group() + id;
  } else {
    ret = (flag * cluster_.nworker_groups() + gid)
          * cluster_.nworkers_per_group() + id;
  }
  return ret;
}

}  // namespace singa
