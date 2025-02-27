// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "paddle/fluid/distributed/collective/Types.h"
#include "paddle/fluid/eager/api/utils/tensor_utils.h"

#include "paddle/fluid/framework/tensor.h"
#include "paddle/fluid/framework/variable.h"
#include "paddle/fluid/platform/enforce.h"

constexpr auto kWaitTimeout = std::chrono::milliseconds(0);

namespace paddle {
namespace distributed {

constexpr int IGNORE_ID = -1;
using Tensor = paddle::experimental::Tensor;

enum class CommType : std::uint8_t {
  BROADCAST = 0,
  ALLREDUCE = 1,
  ALLREDUCE_SPARSE = 2,  // TODO(shenliang03): to support sparse in allreduce
  REDUCE = 3,
  ALLGATHER = 4,
  GATHER = 5,
  SCATTER = 6,
  REDUCE_SCATTER = 7,
  ALLTOALL = 8,
  SEND = 9,
  RECV = 10,
  BARRIER = 11,
  UNKNOWN = 100,
};

class ProcessGroup {
 public:
  class Task {
   public:
    Task(int rank, const std::vector<phi::DenseTensor>& inputTensors,
         CommType opType = CommType::UNKNOWN);

    virtual ~Task();
    virtual bool IsCompleted();
    virtual bool Wait(std::chrono::milliseconds timeout = kWaitTimeout);
    virtual void Synchronize();

   protected:
    const int rank_;
    CommType comm_type_;
    std::mutex mutex_;
    bool is_completed_ = false;
  };

  explicit ProcessGroup(int rank, int size, int gid);
  virtual ~ProcessGroup() {}

  int GetRank() const { return rank_; }

  int GetSize() const { return size_; }

  virtual const std::string GetBackendName() const = 0;

  virtual std::shared_ptr<ProcessGroup::Task> AllReduce(
      std::vector<phi::DenseTensor>& /* input tensors */,   // NOLINT
      std::vector<phi::DenseTensor>& /* output tensors */,  // NOLINT
      const AllreduceOptions& = AllreduceOptions()) {
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support allreduce", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> Broadcast(
      std::vector<phi::DenseTensor>& /* input tensors */,   // NOLINT
      std::vector<phi::DenseTensor>& /* output tensors */,  // NOLINT
      const BroadcastOptions& = BroadcastOptions()) {
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support broadcast", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> Barrier(
      const BarrierOptions& = BarrierOptions()) {
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support barrier", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> Send(
      std::vector<phi::DenseTensor>&, int) {  // NOLINT
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support send", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> Recv(
      std::vector<phi::DenseTensor>& tensors, int) {  // NOLINT
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support receive", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> AllGather(
      std::vector<phi::DenseTensor>&,    // NOLINT
      std::vector<phi::DenseTensor>&) {  // NOLINT
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support AllGather", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> AllToAll(
      std::vector<phi::DenseTensor>&,    // NOLINT
      std::vector<phi::DenseTensor>&) {  // NOLINT
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support AllToAll", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> Reduce(
      std::vector<phi::DenseTensor>&,  // NOLINT
      std::vector<phi::DenseTensor>&,  // NOLINT
      const ReduceOptions& opts) {
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support Reduce", GetBackendName()));
  }

  virtual std::shared_ptr<ProcessGroup::Task> Scatter(
      std::vector<phi::DenseTensor>&,  // NOLINT
      std::vector<phi::DenseTensor>&,  // NOLINT
      const ScatterOptions&) {         // NOLINT
    PADDLE_THROW(platform::errors::InvalidArgument(
        "ProcessGroup%s does not support Scatter", GetBackendName()));
  }

 protected:
  const int rank_;
  const int size_;
  const int gid_;
};

class ProcessGroupMapFromGid {
 public:
  bool has(int gid) {
    auto it = map_.find(gid);
    return it != map_.end();
  }

  void insert(int gid, ProcessGroup* pg) {
    // TODO(sandyhouse): address ut and uncomment the following codes
    // PADDLE_ENFORCE_EQ(has(gid), false,
    //                   platform::errors::PreconditionNotMet(
    //                       "The process group with id %d doesnot exist.",
    //                       gid));
    map_[gid] = pg;
  }

  ProcessGroup* get(int gid) {
    // TODO(sandyhouse): address ut and uncomment the following codes
    // PADDLE_ENFORCE_EQ(has(gid), true,
    //                   platform::errors::PreconditionNotMet(
    //                       "The process group with id %d doesnot exist.",
    //                       gid));
    return map_.find(gid)->second;
  }

  static std::shared_ptr<ProcessGroupMapFromGid> getInstance() {
    static auto s_instance = std::make_shared<ProcessGroupMapFromGid>();
    return s_instance;
  }

  ProcessGroupMapFromGid() = default;
  ~ProcessGroupMapFromGid() = default;

 private:
  std::unordered_map<int, ProcessGroup*> map_;
};

}  //  namespace distributed
}  //  namespace paddle
