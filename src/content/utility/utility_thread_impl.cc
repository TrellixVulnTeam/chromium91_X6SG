// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/debug/crash_logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/browser_exposed_utility_interfaces.h"
#include "content/utility/services.h"
#include "content/utility/utility_blink_platform_with_sandbox_support_impl.h"
#include "content/utility/utility_service_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"

namespace content {

namespace {

class ServiceBinderImpl {
 public:
  explicit ServiceBinderImpl(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
      : main_thread_task_runner_(std::move(main_thread_task_runner)) {}
  ~ServiceBinderImpl() = default;

  void BindServiceInterface(mojo::GenericPendingReceiver* receiver) {
    // Set a crash key so utility process crash reports indicate which service
    // was running in the process.
    static auto* service_name_crash_key = base::debug::AllocateCrashKeyString(
        "service-name", base::debug::CrashKeySize::Size32);
    const std::string& service_name = receiver->interface_name().value();
    base::debug::SetCrashKeyString(service_name_crash_key, service_name);

    // Traces should also indicate the service name.
    auto* trace_log = base::trace_event::TraceLog::GetInstance();
    if (trace_log->IsProcessNameEmpty())
      trace_log->set_process_name("Service: " + service_name);

    // Ensure the ServiceFactory is (lazily) initialized.
    if (!io_thread_services_) {
      io_thread_services_ = std::make_unique<mojo::ServiceFactory>();
      RegisterIOThreadServices(*io_thread_services_);
    }

    // Note that this is balanced by `termination_callback` below, which is
    // always eventually run as long as the process does not begin shutting
    // down beforehand.
    ++num_service_instances_;

    auto termination_callback =
        base::BindOnce(&ServiceBinderImpl::OnServiceTerminated,
                       weak_ptr_factory_.GetWeakPtr());
    if (io_thread_services_->CanRunService(*receiver)) {
      io_thread_services_->RunService(std::move(*receiver),
                                      std::move(termination_callback));
      return;
    }

    termination_callback =
        base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                       base::ThreadTaskRunnerHandle::Get(), FROM_HERE,
                       std::move(termination_callback));
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceBinderImpl::TryRunMainThreadService,
                       std::move(*receiver), std::move(termination_callback)));
  }

  static base::Optional<ServiceBinderImpl>& GetInstanceStorage() {
    static base::NoDestructor<base::Optional<ServiceBinderImpl>> storage;
    return *storage;
  }

 private:
  static void TryRunMainThreadService(mojo::GenericPendingReceiver receiver,
                                      base::OnceClosure termination_callback) {
    // NOTE: UtilityThreadImpl is the only defined subclass of UtilityThread, so
    // this cast is safe.
    auto* thread = static_cast<UtilityThreadImpl*>(UtilityThread::Get());
    thread->HandleServiceRequest(std::move(receiver),
                                 std::move(termination_callback));
  }

  void OnServiceTerminated() {
    if (--num_service_instances_ > 0)
      return;

    // There are no more services running in this process. Time to terminate.
    //
    // First ensure that shutdown also tears down |this|. This is necessary to
    // support multiple tests in the same test suite using out-of-process
    // services via the InProcessUtilityThreadHelper, and it must be done on the
    // current thread to avoid data races.
    auto main_thread_task_runner = main_thread_task_runner_;
    GetInstanceStorage().reset();
    main_thread_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&ServiceBinderImpl::ShutDownProcess));
  }

  static void ShutDownProcess() {
    UtilityThread::Get()->ReleaseProcess();
  }

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  // Tracks the number of service instances currently running (or pending
  // creation) in this process. When the number transitions from non-zero to
  // zero, the process will self-terminate.
  int num_service_instances_ = 0;

  // Handles service requests for services that must run on the IO thread.
  std::unique_ptr<mojo::ServiceFactory> io_thread_services_;

  base::WeakPtrFactory<ServiceBinderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceBinderImpl);
};

ChildThreadImpl::Options::ServiceBinder GetServiceBinder() {
  auto& storage = ServiceBinderImpl::GetInstanceStorage();
  // NOTE: This may already be initialized from a previous call if we're in
  // single-process mode.
  if (!storage)
    storage.emplace(base::ThreadTaskRunnerHandle::Get());
  return base::BindRepeating(&ServiceBinderImpl::BindServiceInterface,
                             base::Unretained(&storage.value()));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure),
                      ChildThreadImpl::Options::Builder()
                          .ServiceBinder(GetServiceBinder())
                          .ExposesInterfacesToBrowser()
                          .Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(base::DoNothing(),
                      ChildThreadImpl::Options::Builder()
                          .InBrowserProcess(params)
                          .ServiceBinder(GetServiceBinder())
                          .ExposesInterfacesToBrowser()
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() = default;

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
}

void UtilityThreadImpl::ReleaseProcess() {
  // Ensure all main-thread services are destroyed before releasing the process.
  // This limits the risk of services incorrectly attempting to post
  // shutdown-blocking tasks once shutdown has already begun.
  main_thread_services_.reset();

  if (!IsInBrowserProcess()) {
    ChildProcess::current()->ReleaseProcess();
    return;
  }

  // Close the channel to cause the UtilityProcessHost to be deleted. We need to
  // take a different code path than the multi-process case because that case
  // depends on the child process going away to close the channel, but that
  // can't happen when we're in single process mode.
  channel()->Close();
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/false);
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
void UtilityThreadImpl::EnsureBlinkInitializedWithSandboxSupport() {
  EnsureBlinkInitializedInternal(/*sandbox_support=*/true);
}
#endif

void UtilityThreadImpl::HandleServiceRequest(
    mojo::GenericPendingReceiver receiver,
    base::OnceClosure termination_callback) {
  if (!main_thread_services_) {
    main_thread_services_ = std::make_unique<mojo::ServiceFactory>();
    RegisterMainThreadServices(*main_thread_services_);
  }

  if (main_thread_services_->CanRunService(receiver)) {
    main_thread_services_->RunService(std::move(receiver),
                                      std::move(termination_callback));
    return;
  }

  DLOG(ERROR) << "Cannot run unknown service: " << *receiver.interface_name();
  std::move(termination_callback).Run();
}

void UtilityThreadImpl::EnsureBlinkInitializedInternal(bool sandbox_support) {
  if (blink_platform_impl_)
    return;

  // We can only initialize Blink on one thread, and in single process mode
  // we run the utility thread on a separate thread. This means that if any
  // code needs Blink initialized in the utility process, they need to have
  // another path to support single process mode.
  if (IsInBrowserProcess())
    return;

  blink_platform_impl_ =
      sandbox_support
          ? std::make_unique<UtilityBlinkPlatformWithSandboxSupportImpl>()
          : std::make_unique<blink::Platform>();
  blink::Platform::CreateMainThreadAndInitialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  ChildProcess::current()->AddRefProcess();

  GetContentClient()->utility()->UtilityThreadStarted();

  // NOTE: Do not add new interfaces directly within this method. Instead,
  // modify the definition of |ExposeUtilityInterfacesToBrowser()| to ensure
  // security review coverage.
  mojo::BinderMap binders;
  content::ExposeUtilityInterfacesToBrowser(&binders);
  ExposeInterfacesToBrowser(std::move(binders));

  service_factory_ = std::make_unique<UtilityServiceFactory>();
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return GetContentClient()->utility()->OnMessageReceived(msg);
}

void UtilityThreadImpl::RunServiceDeprecated(
    const std::string& service_name,
    mojo::ScopedMessagePipeHandle service_pipe) {
  DCHECK(service_factory_);
  service_factory_->RunService(service_name, std::move(service_pipe));
}

}  // namespace content
