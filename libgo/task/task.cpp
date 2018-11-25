#include "task.h"
#include "../common/config.h"
#include <iostream>
#include <string.h>
#include <string>
#include <algorithm>
#include "../debug/listener.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/ref.h"

namespace co
{

const char* GetTaskStateName(TaskState state)
{
    switch (state) {
    case TaskState::runnable:
        return "Runnable";
    case TaskState::block:
        return "Block";
    case TaskState::done:
        return "Done";
    default:
        return "Unkown";
    }
}
#define __DEF_2Name(x) case SchedulerEvent::x: return #x
const char* GetSchedulerEventName(SchedulerEvent event)
{
    switch (event) {
    __DEF_2Name(uninitialized);
    __DEF_2Name(fatal);
    __DEF_2Name(swapIn);
    __DEF_2Name(swapOut_runnable);
    __DEF_2Name(swapOut_block);
    __DEF_2Name(swapOut_done);
    __DEF_2Name(suspend);
    __DEF_2Name(wakeup);
    __DEF_2Name(addIntoNewQueue);
    __DEF_2Name(addIntoRunnableQueue);
    __DEF_2Name(addIntoWaitQueue);
    __DEF_2Name(addIntoStealList);
    __DEF_2Name(popRunnableQueue);
    __DEF_2Name(pushRunnableQueue);
    default:
        return "Unkown";
    }
}

void Task::Run()
{
    auto call_fn = [this]() {
#if ENABLE_DEBUGGER
        if (Listener::GetTaskListener()) {
            Listener::GetTaskListener()->onStart(this->id_);
        }
#endif

        this->fn_();
        this->fn_ = TaskF(); //让协程function对象的析构也在协程中执行

#if ENABLE_DEBUGGER
        if (Listener::GetTaskListener()) {
            Listener::GetTaskListener()->onCompleted(this->id_);
        }
#endif
    };

    if (CoroutineOptions::getInstance().exception_handle == eCoExHandle::immedaitely_throw) {
        call_fn();
    } else {
        try {
            call_fn();
        } catch (...) {
            this->fn_ = TaskF();

            std::exception_ptr eptr = std::current_exception();
            DebugPrint(dbg_exception, "task(%s) catched exception.", DebugInfo());

#if ENABLE_DEBUGGER
            if (Listener::GetTaskListener()) {
                Listener::GetTaskListener()->onException(this->id_, eptr);
            }
#endif
        }
    }

#if ENABLE_DEBUGGER
    if (Listener::GetTaskListener()) {
        Listener::GetTaskListener()->onFinished(this->id_);
    }
#endif

    state_ = TaskState::done;
    Processer::StaticCoYield();
}

void Task::StaticRun(intptr_t vp)
{
    Task* tk = (Task*)vp;
    tk->Run();
}

Task::Task(TaskF const& fn, std::size_t stack_size)
    : ctx_(&Task::StaticRun, (intptr_t)this, stack_size), fn_(fn), stateHistory_(64)
{
//    DebugPrint(dbg_task, "task(%s) construct. this=%p", DebugInfo(), this);
}

Task::~Task()
{
//    printf("delete Task = %p, impl = %p, weak = %ld\n", this, this->impl_, (long)this->impl_->weak_);
    assert(!this->prev);
    assert(!this->next);
//    DebugPrint(dbg_task, "task(%s) destruct. this=%p", DebugInfo(), this);
}

const char* Task::DebugInfo()
{
    if (reinterpret_cast<void*>(this) == nullptr) return "nil";

    return TaskDebugInfo(this);
}

} //namespace co
