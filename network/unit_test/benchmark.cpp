#include <iostream>
#include <unistd.h>
#include <gtest/gtest.h>
#include <coroutine/coroutine.h>
#include <boost/thread.hpp>
#include <atomic>
#include "network.h"
using namespace std;
using namespace co;
using namespace network;

std::string g_url = "tcp://127.0.0.1:3050";
int g_thread_count = 4;
std::atomic<int> g_conn{0};
std::atomic<unsigned long long> g_server_send{0};
std::atomic<unsigned long long> g_server_send_err{0};
std::atomic<unsigned long long> g_server_recv{0};
std::atomic<unsigned long long> g_client_send{0};
std::atomic<unsigned long long> g_client_send_err{0};
std::atomic<unsigned long long> g_client_recv{0};
std::atomic<int> g_max_pack{0};

char g_data[4096] = {1, 2};

void start_server(std::string url, bool *bexit)
{
    Server s;
    auto proto = s.GetProtocol();
    s.SetConnectedCb([&](SessionId){ ++g_conn; })
        .SetDisconnectedCb([&](SessionId, boost_ec const&){ --g_conn; })
        .SetReceiveCb(
                [&, proto](SessionId sess, const void* data, size_t bytes)
                {
                    g_server_recv += bytes;
                    if (!bytes)
                        printf("error bytes is zero.\n");

                    if ((int)bytes > g_max_pack)
                        g_max_pack = bytes;

                    proto->Send(sess, data, bytes, [&, bytes](boost_ec ec){
                            if (ec) g_server_send_err += bytes;
                            else g_server_send += bytes;
                        });
                });
    boost_ec ec = s.goStart(url);
    ASSERT_FALSE(!!ec);

    while (!*bexit)
        sleep(1);
}

void start_client(std::string url, bool *bexit)
{
    Client c;
    auto proto = c.GetProtocol();
    c.SetReceiveCb(
            [&, proto](SessionId sess, const void* data, size_t bytes)
            {
                g_client_recv += bytes;

                proto->Send(sess, data, bytes, [&, bytes](boost_ec ec){
                    if (ec) g_client_send_err += bytes;
                    else g_client_send += bytes;
                    });
            });
    boost_ec ec = c.Connect(url);
    if (ec) {
        if (!*bexit) {
            sleep(1);
            go [=]{ start_client(url, bexit); };
        }
        return ;
    }

    c.Send(g_data, sizeof(g_data));
    while (!*bexit) {
        sleep(1);

        if (!proto->IsEstab(c.GetSessId())) {
            go [=]{ start_client(url, bexit); };
            return ;
        }
    }
}

void show_status()
{
    static int s_c = 0;
    if (s_c++ % 10 == 0) {
        // print title
        printf("--------------------------------------------------------------------------------------------------------\n");
        printf(" index |  conn  |   s_send   | s_send_err |   s_recv   |   c_send   | c_send_err |   c_recv   | max_pack\n");
    }

    static unsigned long long last_server_send{0};
    static unsigned long long last_server_send_err{0};
    static unsigned long long last_server_recv{0};
    static unsigned long long last_client_send{0};
    static unsigned long long last_client_send_err{0};
    static unsigned long long last_client_recv{0};

    unsigned long long server_send = g_server_send - last_server_send;
    unsigned long long server_send_err = g_server_send_err - last_server_send_err;
    unsigned long long server_recv = g_server_recv - last_server_recv;
    unsigned long long client_send = g_client_send - last_client_send;
    unsigned long long client_send_err = g_client_send_err - last_client_send_err;
    unsigned long long client_recv = g_client_recv - last_client_recv;

    printf("%6d | %6d | %10llu | %10llu | %10llu | %10llu | %10llu | %10llu | %d\n",
            s_c, (int)g_conn,
            server_send, server_send_err, server_recv,
            client_send, client_send_err, client_recv,
            (int)g_max_pack);

    last_server_send = g_server_send;
    last_server_send_err = g_server_send_err;
    last_server_recv = g_server_recv;
    last_client_send = g_client_send;
    last_client_send_err = g_client_send_err;
    last_client_recv = g_client_recv;
}

using ::testing::TestWithParam;
using ::testing::Values;

struct Benchmark : public TestWithParam<int>
{
    int n_;
    void SetUp() { n_ = GetParam(); }
};

TEST_P(Benchmark, BenchmarkT)
{
//    co_sched.GetOptions().debug = network::dbg_session_alive;

    bool bexit = false;
    go [&]{ start_server(g_url, &bexit); };
    
    for (int i = 0; i < n_; ++i)
        go [&]{ start_client(g_url, &bexit); };

    co_timer_add(std::chrono::seconds(10), [&]{ bexit = true; });

    boost::thread_group tg;
    for (int i = 0; i < g_thread_count; ++i)
        tg.create_thread([]{ co_sched.RunUntilNoTask(); });

    for (;;) {
        if (bexit) {
            tg.join_all();
            break;
        }

        sleep(1);
        show_status();
    }
}


INSTANTIATE_TEST_CASE_P(
        BenchmarkTest,
        Benchmark,
        Values(100, 1000, 10000));
