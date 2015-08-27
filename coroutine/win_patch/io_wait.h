#pragma once
#include <vector>
#include <list>
#include <set>
#include "task.h"

namespace co
{

    class IoWait
    {
    public:
        IoWait();

        // ��Э���е��õ�switch, �ݴ�״̬��yield
        void CoSwitch(std::vector<FdStruct> && fdsts, int timeout_ms);

        // �ڵ������е��õ�switch, ����ɹ������ȴ����У����ʧ�������¼ӻ�runnable����
        void SchedulerSwitch(Task* tk);

        int WaitLoop();

    private:
        void Cancel(Task *tk, uint32_t id);
        
        std::list<CoTimerPtr> timeout_list_;
        LFLock timeout_list_lock_;
        CoTimerMgr timer_mgr_;

        typedef TSQueue<Task> TaskList;
        TaskList wait_tasks_;
    };


} //namespace co
