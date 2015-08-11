#include "sleep_wait.h"
#include "scheduler.h"
#include <chrono>
#include <assert.h>

namespace co
{

void SleepWait::CoSwitch(int timeout_ms)
{
    Task *tk = g_Scheduler.GetCurrentTask();
    if (!tk) return ;

    tk->sleep_ms_ = timeout_ms;
    tk->state_ = TaskState::sleep;

    DebugPrint(dbg_sleepblock, "task(%s) will sleep %d ms", tk->DebugInfo(), tk->sleep_ms_);
    g_Scheduler.Yield();
}

void SleepWait::SchedulerSwitch(Task* tk)
{
    DebugPrint(dbg_sleepblock, "task(%s) begin sleep %d ms", tk->DebugInfo(), tk->sleep_ms_);
    wait_tasks_.push(tk);
    timer_mgr_.ExpireAt(std::chrono::milliseconds(tk->sleep_ms_),
            [=] {
                this->Wakeup(tk);
            });
}

uint32_t SleepWait::WaitLoop()
{
    std::list<CoTimerPtr> timers;
    timer_mgr_.GetExpired(timers, 128);
    for (auto &sp_timer : timers)
    {
        DebugPrint(dbg_sleepblock, "enter timer callback %llu", (long long unsigned)sp_timer->GetId());
        (*sp_timer)();
        DebugPrint(dbg_sleepblock, "leave timer callback %llu", (long long unsigned)sp_timer->GetId());
    }

    return timers.size();
}

void SleepWait::Wakeup(Task* tk)
{
    DebugPrint(dbg_sleepblock, "task(%s) wakeup", tk->DebugInfo());
    bool ok = wait_tasks_.erase(tk);
    assert(ok);
    g_Scheduler.AddTaskRunnable(tk);
}


} //namespace co
