/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/delegates/gpu/cl/cl_command_queue.h"

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "tensorflow/lite/delegates/gpu/cl/cl_device.h"
#include "tensorflow/lite/delegates/gpu/cl/cl_event.h"
#include "tensorflow/lite/delegates/gpu/cl/util.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/common/types.h"

#include "tensorflow/lite/minimal_logging.h"

namespace tflite {
namespace gpu {
namespace cl {

CLCommandQueue::CLCommandQueue(cl_command_queue queue, bool has_ownership)
    : queue_(queue), has_ownership_(has_ownership) {}

CLCommandQueue::CLCommandQueue(CLCommandQueue&& queue)
    : queue_(queue.queue_), has_ownership_(queue.has_ownership_) {
  queue.queue_ = nullptr;
}

CLCommandQueue& CLCommandQueue::operator=(CLCommandQueue&& queue) {
  if (this != &queue) {
    Release();
    std::swap(queue_, queue.queue_);
    has_ownership_ = queue.has_ownership_;
  }
  return *this;
}

CLCommandQueue::~CLCommandQueue() { Release(); }

void CLCommandQueue::Release() {
  if (has_ownership_ && queue_) {
    clReleaseCommandQueue(queue_);
    queue_ = nullptr;
  }
}

absl::Status CLCommandQueue::Dispatch(const CLKernel& kernel,
                                      const int3& work_groups_count,
                                      const int3& work_group_size,
                                      CLEvent* event) {
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw In Dispatch...");

  std::array<size_t, 3> local;
  std::array<size_t, 3> global;
  for (int i = 0; i < 3; ++i) {
    local[i] = work_group_size[i];
    global[i] = work_groups_count[i] * work_group_size[i];
  }
  cl_event resulting_event;
  const int error_code = clEnqueueNDRangeKernel(
      queue_, kernel.kernel(), 3, nullptr, global.data(), local.data(), 0,
      nullptr, event ? &resulting_event : nullptr);
  if (event) {
    *event = CLEvent(resulting_event);
  }
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(
        absl::StrCat("Failed to clEnqueueNDRangeKernel - ",
                     CLErrorCodeToString(error_code)));
  }
  return absl::OkStatus();
}

absl::Status CLCommandQueue::Dispatch(const CLKernel& kernel,
                                      const int3& work_groups_count,
                                      const int3& work_group_size) {
  
  //author:fu
  // auto start = std::chrono::high_resolution_clock::now();

  return Dispatch(kernel, work_groups_count, work_group_size, nullptr);

  // auto end = std::chrono::high_resolution_clock::now();
  // std::chrono::duration<double,std::ratio<1,1>> ds = end - start;
  // std::chrono::milliseconds d = std::chrono::duration_cast< std::chrono::milliseconds >( ds );
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw run kernel time = %f ms, ",d.count());
  // std::chrono::duration<double,std::ratio<1,1000000>> duration_mcs=std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1,1000000>>> (end-start);  
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw run kernel time = %f us, ",duration_mcs.count());

}

absl::Status CLCommandQueue::EnqueueEvent(CLEvent* event) {
  cl_event resulting_event;
  const int error_code = clEnqueueMarker(queue_, &resulting_event);
  *event = CLEvent(resulting_event);
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(absl::StrCat("Failed to clEnqueueMarker - ",
                                           CLErrorCodeToString(error_code)));
  }
  return absl::OkStatus();
}

absl::Status CLCommandQueue::EnqueueWriteImage(cl_mem memory, int3 region,
                                               const void* data, bool async) {
  const size_t origin[] = {0, 0, 0};
  const size_t r[] = {static_cast<size_t>(region.x),
                      static_cast<size_t>(region.y),
                      static_cast<size_t>(region.z)};
  const cl_bool blocking = async ? CL_FALSE : CL_TRUE;
  auto error_code = clEnqueueWriteImage(queue_, memory, blocking, origin, r, 0,
                                        0, data, 0, nullptr, nullptr);
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(
        absl::StrCat("Failed to upload data to GPU (clEnqueueWriteImage) - ",
                     CLErrorCodeToString(error_code)));
  }

  return absl::OkStatus();
}

absl::Status CLCommandQueue::EnqueueReadImage(cl_mem memory, int3 region,
                                              void* data, bool async) {
  const size_t origin[] = {0, 0, 0};
  const size_t r[] = {static_cast<size_t>(region.x),
                      static_cast<size_t>(region.y),
                      static_cast<size_t>(region.z)};
  const cl_bool blocking = async ? CL_FALSE : CL_TRUE;
  auto error_code = clEnqueueReadImage(queue_, memory, blocking, origin, r, 0,
                                       0, data, 0, nullptr, nullptr);
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(
        absl::StrCat("Failed to read data from GPU (clEnqueueReadImage) - ",
                     CLErrorCodeToString(error_code)));
  }

  return absl::OkStatus();
}

absl::Status CLCommandQueue::EnqueueWriteBuffer(cl_mem memory,
                                                size_t size_in_bytes,
                                                const void* data, bool async) {
  //author:fu
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw In EnqueueWriteBuffer...");

  // const cl_bool blocking = async ? CL_FALSE : CL_TRUE;
  
  //author:fu
  const cl_bool blocking = CL_TRUE;

  auto error_code = clEnqueueWriteBuffer(
      queue_, memory, blocking, 0, size_in_bytes, data, 0, nullptr, nullptr);
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(
        absl::StrCat("Failed to upload data to GPU (clEnqueueWriteBuffer) - ",
                     CLErrorCodeToString(error_code)));
  }
  return absl::OkStatus();
}

absl::Status CLCommandQueue::EnqueueReadBuffer(cl_mem memory,
                                               size_t size_in_bytes, void* data,
                                               bool async) {
  //author:fu
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"async=%d",async);
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw In EnqueueReadBuffer...");

  // const cl_bool blocking = async ? CL_FALSE : CL_TRUE;

  //author:fu
  const cl_bool blocking = CL_TRUE;
  // auto start = std::chrono::high_resolution_clock::now();

  auto error_code = clEnqueueReadBuffer(
      queue_, memory, blocking, 0, size_in_bytes, data, 0, nullptr, nullptr);
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(
        absl::StrCat("Failed to read data from GPU (clEnqueueReadBuffer) - ",
                     CLErrorCodeToString(error_code)));
  }

  // auto end = std::chrono::high_resolution_clock::now();
  // std::chrono::duration<double,std::ratio<1,1>> ds = end - start;
  // std::chrono::milliseconds d = std::chrono::duration_cast< std::chrono::milliseconds >( ds );
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw clEnqueueReadBuffer time = %f ms, ",d.count());
  // std::chrono::duration<double,std::ratio<1,1000000>> duration_mcs=std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1,1000000>>> (end-start);  
  // TFLITE_LOG_PROD(TFLITE_LOG_INFO,"fsw clEnqueueReadBuffer time = %f us, ",duration_mcs.count());

  return absl::OkStatus();
}

absl::Status CLCommandQueue::WaitForCompletion() {
  auto error_code = clFinish(queue_);
  if (error_code != CL_SUCCESS) {
    return absl::UnknownError(
        absl::StrCat("Failed to clFinish - ", CLErrorCodeToString(error_code)));
  }
  return absl::OkStatus();
}

ProfilingCommandQueue::ProfilingCommandQueue(cl_command_queue queue)
    : CLCommandQueue(queue, true) {
  events_.reserve(128);
}

ProfilingCommandQueue::ProfilingCommandQueue(ProfilingCommandQueue&& queue)
    : CLCommandQueue(std::move(queue)),
      events_(std::move(queue.events_)),
      number_of_dispatches_(std::move(queue.number_of_dispatches_)),
      current_label_(std::move(queue.current_label_)) {}

ProfilingCommandQueue& ProfilingCommandQueue::operator=(
    ProfilingCommandQueue&& queue) {
  if (this != &queue) {
    events_ = std::move(queue.events_);
    number_of_dispatches_ = std::move(queue.number_of_dispatches_);
    current_label_ = std::move(queue.current_label_);
    CLCommandQueue::operator=(std::move(queue));
  }
  return *this;
}

void ProfilingCommandQueue::SetEventsLabel(const std::string& name) {
  current_label_ = name;
}

void ProfilingCommandQueue::ResetMeasurements() {
  events_.clear();
  number_of_dispatches_.clear();
}

absl::Status ProfilingCommandQueue::Dispatch(const CLKernel& kernel,
                                             const int3& work_groups_count,
                                             const int3& work_group_size) {
  events_.push_back(CLEvent());
  number_of_dispatches_.push_back(1);
  RETURN_IF_ERROR(CLCommandQueue::Dispatch(kernel, work_groups_count,
                                           work_group_size,
                                           &events_[events_.size() - 1]));
  events_.back().SetName(current_label_);
  return absl::OkStatus();
}

absl::Status ProfilingCommandQueue::DispatchNTimes(
    const CLKernel& kernel, const int3& work_groups_count,
    const int3& work_group_size, int n, int flush_period) {
  number_of_dispatches_.push_back(n);
  if (n == 1) {
    events_.push_back(CLEvent());
    RETURN_IF_ERROR(CLCommandQueue::Dispatch(kernel, work_groups_count,
                                             work_group_size,
                                             &events_[events_.size() - 1]));
    events_.back().SetName(current_label_);
  } else {
    events_.push_back(CLEvent());
    events_.push_back(CLEvent());
    RETURN_IF_ERROR(CLCommandQueue::Dispatch(kernel, work_groups_count,
                                             work_group_size,
                                             &events_[events_.size() - 2]));
    for (int i = 1; i < n - 1; ++i) {
      RETURN_IF_ERROR(
          CLCommandQueue::Dispatch(kernel, work_groups_count, work_group_size));
      if (flush_period && i % flush_period == 0) {
        clFlush(queue_);
      }
    }
    RETURN_IF_ERROR(CLCommandQueue::Dispatch(kernel, work_groups_count,
                                             work_group_size,
                                             &events_[events_.size() - 1]));
    clFlush(queue_);
    events_[events_.size() - 2].SetName(current_label_);
    events_[events_.size() - 1].SetName(current_label_);
  }
  return absl::OkStatus();
}

ProfilingInfo ProfilingCommandQueue::GetProfilingInfo() const {
  ProfilingInfo result;
  result.dispatches.resize(number_of_dispatches_.size());
  int events_counter = 0;
  for (int i = 0; i < number_of_dispatches_.size(); ++i) {
    result.dispatches[i].label = events_[events_counter].GetName();
    if (number_of_dispatches_[i] == 1) {
      result.dispatches[i].duration =
          absl::Nanoseconds(events_[events_counter].GetEventTimeNs());
      events_counter += 1;
    } else {
      result.dispatches[i].duration =
          absl::Nanoseconds(events_[events_counter + 1].GetFinishedTimeNs() -
                            events_[events_counter].GetStartedTimeNs()) /
          number_of_dispatches_[i];
      events_counter += 2;
    }
  }
  return result;
}

absl::Status ProfilingCommandQueue::GetBestWorkGroupIndex(
    const CLKernel& kernel, const GpuInfo& gpu_info,
    const std::vector<int3>& work_groups_count,
    const std::vector<int3>& work_group_sizes, int* index) {
  // Some Adreno 3xx can have wrong numbers for some events
  const bool possible_bug_with_events =
      gpu_info.IsAdreno() && gpu_info.adreno_info.IsAdreno3xx();
  events_.resize(work_group_sizes.size());
  for (int i = 0; i < work_group_sizes.size(); ++i) {
    RETURN_IF_ERROR(CLCommandQueue::Dispatch(kernel, work_groups_count[i],
                                             work_group_sizes[i], &events_[i]));

    // reducing the speed of memory leak on Mali for some kernels
    if (gpu_info.IsMali() && i % 8 == 7) {
      events_[i - 7].Wait();
    }
    if (possible_bug_with_events) {
      // We are trying to increase probability for correct result.
      RETURN_IF_ERROR(WaitForCompletion());
    }
  }

  RETURN_IF_ERROR(WaitForCompletion());

  // To release memory of some kernel pool on Mali.
  if (gpu_info.IsMali()) {
    RETURN_IF_ERROR(kernel.ReInit());
  }

  int minimum_index = 0;
  double minimum_time = std::numeric_limits<double>::max();
  if (possible_bug_with_events) {  // we will try to cut out suspicious results
    double average_time = 0.0;
    int average_samples_count = 0;
    for (int i = 0; i < work_group_sizes.size(); ++i) {
      if (events_[i].GetEventTimeMs() < 100 * 1000) {  // 100 sec
        average_time += events_[i].GetEventTimeMs();
        average_samples_count++;
      }
    }
    average_time /= average_samples_count;
    for (int i = 0; i < work_group_sizes.size(); ++i) {
      double time = events_[i].GetEventTimeMs();
      if (time < minimum_time && time >= 0.1 * average_time) {
        minimum_index = i;
        minimum_time = time;
      }
    }
  } else {
    for (int i = 0; i < work_group_sizes.size(); ++i) {
      double time = events_[i].GetEventTimeMs();
      if (time < minimum_time) {
        minimum_index = i;
        minimum_time = time;
      }
    }
  }

  *index = minimum_index;

  return absl::OkStatus();
}

absl::Status CreateCLCommandQueue(const CLDevice& device,
                                  const CLContext& context,
                                  CLCommandQueue* result) {
  int error_code;
  cl_command_queue queue =
      clCreateCommandQueue(context.context(), device.id(), 0, &error_code);
  if (!queue) {
    return absl::UnknownError(
        absl::StrCat("Failed to create a command queue - ",
                     CLErrorCodeToString(error_code)));
  }
  *result = CLCommandQueue(queue, true);
  return absl::OkStatus();
}

double ProfilingCommandQueue::GetQueueExecutionTimeMs() const {
  const uint64_t start = events_.front().GetStartedTimeNs();
  const uint64_t end = events_.back().GetFinishedTimeNs();
  const uint64_t time_ns = (end - start);

  return static_cast<double>(time_ns) / 1000000.0;
}

double ProfilingCommandQueue::GetSumOfEventsTimeMs() const {
  double sum = 0.0;
  for (int i = 0; i < events_.size(); ++i) {
    sum += events_[i].GetEventTimeMs();
  }
  return sum;
}

absl::Status CreateProfilingCommandQueue(const CLDevice& device,
                                         const CLContext& context,
                                         ProfilingCommandQueue* result) {
  int error_code;
  cl_command_queue queue = clCreateCommandQueue(
      context.context(), device.id(), CL_QUEUE_PROFILING_ENABLE, &error_code);
  if (!queue) {
    return absl::UnknownError(
        absl::StrCat("Failed to create a command queue - ",
                     CLErrorCodeToString(error_code)));
  }

  *result = ProfilingCommandQueue(queue);
  return absl::OkStatus();
}

}  // namespace cl
}  // namespace gpu
}  // namespace tflite
