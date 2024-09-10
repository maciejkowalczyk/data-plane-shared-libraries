// Copyright 2024 Google LLC
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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");
ABSL_FLAG(std::vector<std::string>, mounts,
          std::vector<std::string>({"/lib", "/lib64"}),
          "Mounts containing dependencies needed by the binary");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::byob::LoadRequest;

bool ConnectToPath(int fd, std::string_view socket_name) {
  sockaddr_un sa;
  ::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  ::strncpy(sa.sun_path, socket_name.data(), sizeof(sa.sun_path));
  return ::connect(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)) == 0;
}
struct WorkerImplArg {
  absl::Span<const std::string> mounts;
  std::string_view pivot_root_dir;
  std::string_view socket_name;
  std::string_view code_token;
  std::string_view binary_path;
};

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  PCHECK(fd != -1);
  if (!ConnectToPath(fd, worker_impl_arg.socket_name)) {
    PLOG(INFO) << "connect() to " << worker_impl_arg.socket_name << "failed";
    return -1;
  }
  PCHECK(::write(fd, worker_impl_arg.code_token.data(), 36) == 36);

  // Set up restricted filesystem for worker using pivot_root
  // pivot_root doesn't work under an MS_SHARED mount point.
  // https://man7.org/linux/man-pages/man2/pivot_root.2.html.
  PCHECK(::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == 0)
      << "Failed to mount /";
  for (const std::string& mount : worker_impl_arg.mounts) {
    const std::string target =
        absl::StrCat(worker_impl_arg.pivot_root_dir, mount);
    CHECK(std::filesystem::create_directories(target));
    PCHECK(::mount(mount.c_str(), target.c_str(), nullptr, MS_BIND, nullptr) ==
           0)
        << "Failed to mount " << mount;
  }

  // MS_REC needed here to get other mounts (/lib, /lib64 etc)
  PCHECK(::mount(worker_impl_arg.pivot_root_dir.data(),
                 worker_impl_arg.pivot_root_dir.data(), "bind",
                 MS_REC | MS_BIND, nullptr) == 0)
      << "Failed to mount " << worker_impl_arg.pivot_root_dir;
  PCHECK(::mount(worker_impl_arg.pivot_root_dir.data(),
                 worker_impl_arg.pivot_root_dir.data(), "bind",
                 MS_REC | MS_SLAVE, nullptr) == 0)
      << "Failed to mount " << worker_impl_arg.pivot_root_dir;
  {
    const std::string pivot_dir =
        absl::StrCat(worker_impl_arg.pivot_root_dir, "/pivot");
    CHECK(std::filesystem::create_directories(pivot_dir));
    PCHECK(::syscall(SYS_pivot_root, worker_impl_arg.pivot_root_dir.data(),
                     pivot_dir.c_str()) == 0);
  }
  PCHECK(::chdir("/") == 0);
  PCHECK(::umount2("/pivot", MNT_DETACH) == 0);
  for (const std::string& mount : worker_impl_arg.mounts) {
    PCHECK(::mount(mount.c_str(), mount.c_str(), nullptr, MS_REMOUNT | MS_BIND,
                   nullptr) == 0)
        << "Failed to mount " << mount;
  }
  {
    const std::string binary_dir =
        std::filesystem::path(worker_impl_arg.binary_path).parent_path();
    PCHECK(::mount(binary_dir.c_str(), binary_dir.c_str(), nullptr,
                   MS_REMOUNT | MS_BIND, nullptr) == 0);
  }

  // Exec binary.
  const std::string connection_fd = [fd] {
    const int connection_fd = ::dup(fd);
    PCHECK(connection_fd != -1);
    return absl::StrCat(connection_fd);
  }();
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), connection_fd.c_str(), nullptr);
  PLOG(FATAL) << "execl() failed";
}
struct PidAndPivotRootDir {
  int pid;
  std::string pivot_root_dir;
};

PidAndPivotRootDir ConnectSendCloneAndExec(absl::Span<const std::string> mounts,
                                           std::string_view socket_name,
                                           std::string_view code_token,
                                           std::string_view binary_path) {
  char tmp_file[] = "/tmp/roma_app_server_XXXXXX";
  const char* pivot_root_dir = ::mkdtemp(tmp_file);
  PCHECK(pivot_root_dir != nullptr);
  WorkerImplArg worker_impl_arg{
      .mounts = mounts,
      .pivot_root_dir = pivot_root_dir,
      .socket_name = socket_name,
      .code_token = code_token,
      .binary_path = binary_path,
  };

  // Explicitly 16-byte align the stack for aarch64. Otherwise, `clone` may hang
  // or the process may receive SIGBUS (depending on the size of the stack
  // before this function call). Overprovisions stack by at most 15 bytes (of
  // 2^10 bytes) where unneeded.
  // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
  alignas(16) char stack[1 << 20];
  const pid_t pid =
      ::clone(WorkerImpl, stack + sizeof(stack),
              CLONE_VM | CLONE_VFORK | CLONE_NEWIPC | CLONE_NEWPID | SIGCHLD |
                  CLONE_NEWUTS | CLONE_NEWNS,
              &worker_impl_arg);
  PCHECK(pid != -1);
  return PidAndPivotRootDir{
      .pid = pid,
      .pivot_root_dir = std::string(pivot_root_dir),
  };
}
}  // namespace

int main(int argc, char** argv) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  const std::string socket_name = absl::GetFlag(FLAGS_socket_name);
  std::vector<std::string> mounts = absl::GetFlag(FLAGS_mounts);
  const std::filesystem::path progdir =
      std::filesystem::temp_directory_path() /
      ToString(google::scp::core::common::Uuid::GenerateUuid());
  CHECK(std::filesystem::create_directories(progdir));
  mounts.push_back(progdir);
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  PCHECK(fd != -1);
  PCHECK(ConnectToPath(fd, socket_name));
  struct UdfInstanceMetadata {
    std::string pivot_root_dir;
    std::string code_token;
    std::string binary_path;
  };

  absl::Mutex mu;
  absl::flat_hash_map<int, UdfInstanceMetadata> pid_to_udf;  // Guarded by mu.
  bool shutdown = false;                                     // Guarded by mu.
  std::thread reloader([&socket_name, &mounts, &mu, &pid_to_udf, &shutdown] {
    {
      auto fn = [&] {
        mu.AssertReaderHeld();
        return !pid_to_udf.empty() || shutdown;
      };
      absl::MutexLock lock(&mu);

      // Wait until at least one worker has been created before reloading.
      mu.Await(absl::Condition(&fn));
      if (shutdown) {
        return;
      }
    }
    while (true) {
      UdfInstanceMetadata udf;
      {
        int status;

        // Wait for any worker to end.
        const int pid = ::waitpid(-1, &status, /*options=*/0);
        PCHECK(pid != -1);
        absl::MutexLock lock(&mu);
        const auto it = pid_to_udf.find(pid);
        if (it == pid_to_udf.end()) {
          LOG(ERROR) << "waitpid() returned unknown pid=" << pid;
          continue;
        }
        udf = std::move(it->second);
        pid_to_udf.erase(it);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
          CHECK(std::filesystem::remove_all(udf.pivot_root_dir));
          break;
        }
      }
      // Start a new worker with the same UDF as the most-recently ended worker.
      PidAndPivotRootDir pid_and_pivot_root_dir = ConnectSendCloneAndExec(
          mounts, socket_name, udf.code_token, udf.binary_path);
      CHECK(std::filesystem::remove_all(udf.pivot_root_dir));
      udf.pivot_root_dir = std::move(pid_and_pivot_root_dir.pivot_root_dir);
      absl::MutexLock lock(&mu);
      CHECK(pid_to_udf.insert({pid_and_pivot_root_dir.pid, std::move(udf)})
                .second);
    }
  });
  FileInputStream input(fd);
  while (true) {
    LoadRequest request;
    if (!ParseDelimitedFromZeroCopyStream(&request, &input, nullptr)) {
      break;
    }
    const std::filesystem::path binary_path =
        progdir / ToString(google::scp::core::common::Uuid::GenerateUuid());
    {
      std::ofstream ofs(binary_path, std::ios::binary);
      ofs.write(request.binary_content().c_str(),
                request.binary_content().size());
    }
    std::filesystem::permissions(binary_path,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::owner_read);
    for (int i = 0; i < request.n_workers() - 1; ++i) {
      PidAndPivotRootDir pid_and_pivot_root_dir = ConnectSendCloneAndExec(
          mounts, socket_name, request.code_token(), binary_path.native());
      UdfInstanceMetadata udf{
          .pivot_root_dir = std::move(pid_and_pivot_root_dir.pivot_root_dir),
          .code_token = request.code_token(),
          .binary_path = binary_path,
      };
      absl::MutexLock lock(&mu);
      pid_to_udf[pid_and_pivot_root_dir.pid] = std::move(udf);
    }

    // Start n-th worker out of loop.
    PidAndPivotRootDir pid_and_pivot_root_dir = ConnectSendCloneAndExec(
        mounts, socket_name, request.code_token(), binary_path.native());
    UdfInstanceMetadata udf{
        .pivot_root_dir = std::move(pid_and_pivot_root_dir.pivot_root_dir),
        .code_token = std::move(*request.mutable_code_token()),
        .binary_path = binary_path,
    };
    absl::MutexLock lock(&mu);
    pid_to_udf[pid_and_pivot_root_dir.pid] = std::move(udf);
  }
  {
    absl::MutexLock lock(&mu);
    shutdown = true;
  }
  reloader.join();
  ::close(fd);

  // Kill extant workers before exit.
  for (const auto& [pid, udf] : pid_to_udf) {
    if (::kill(pid, SIGKILL) == -1) {
      PLOG(ERROR) << "kill(" << pid << ", SIGKILL)";
    }
    if (::waitpid(pid, nullptr, /*options=*/0) == -1) {
      PLOG(ERROR) << "waitpid(" << pid << ", nullptr, 0)";
    }
    if (std::error_code ec;
        !std::filesystem::remove_all(udf.pivot_root_dir, ec)) {
      LOG(ERROR) << "Failed to remove " << udf.pivot_root_dir << ": " << ec;
    }
  }
  if (std::error_code ec; !std::filesystem::remove_all(progdir, ec)) {
    LOG(ERROR) << "Failed to remove " << progdir << ": " << ec;
  }
  return 0;
}
