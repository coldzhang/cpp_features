#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <gtest/gtest.h>
#include "test_server.h"
#include "coroutine.h"
#include <chrono>
#include <time.h>
#include <poll.h>
using namespace std;
using namespace co;

using ::testing::TestWithParam;
using ::testing::Values;

struct MassCo : public TestWithParam<int>
{
    int n_;
    void SetUp() { n_ = GetParam(); }
};

static uint32_t c = 0;
void foo()
{
    ++c;
    yield;
    ++c;
    yield;
}

TEST_P(MassCo, CnK)
{
    c = 0;
//    if (n_ == 1)
//        co_sched.GetOptions().debug = dbg_scheduler;

    int n = n_ * 1000;
    for (int i = 0; i < n; ++i)
        go foo;

    co_sched.Run();
    EXPECT_EQ(c, n);

    co_sched.Run();
    EXPECT_EQ(c, n * 2);

    co_sched.RunUntilNoTask();
    EXPECT_TRUE(co_sched.IsEmpty());

    printf("press anykey to continue. task_c=%u\n", (uint32_t)co::Task::GetTaskCount());
    getchar();
//    co_sched.GetOptions().debug = dbg_none;

    co_sched.RunUntilNoTask();
    EXPECT_EQ(co::Task::GetTaskCount(), 0);
    printf("press anykey to continue. task_c=%u\n", (uint32_t)co::Task::GetTaskCount());
    getchar();
}

INSTANTIATE_TEST_CASE_P(
        MassCoTest,
        MassCo,
        Values(1, 10, 100, 1000, 1000));

