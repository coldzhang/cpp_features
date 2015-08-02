/************************************************
 * coroutine sample9
************************************************
 * 我们再来看一个有实用价值的例子：
 * 基于curl同步调用做的HTTP性能测试工具。
************************************************/
#include <chrono>
#include <boost/thread.hpp>
#include <curl/curl.h>
#include "coroutine.h"

static std::atomic<int> g_ok{0}, g_error{0};

size_t curl_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return size * nmemb;
}

void request_once(const char* url)
{
    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
        CURLcode res = curl_easy_perform(curl);
//        printf("res:%d\n", res);
        curl_easy_cleanup(curl);
        if (CURLE_OK == res)
            ++g_ok;
        else
            ++g_error;
    }

    go [=]{ request_once(url); };
}

void show_status()
{
    static int last_ok = 0, last_error = 0;
    printf("ok:%d, error:%d\n", g_ok - last_ok, g_error - last_error);
    last_ok = g_ok, last_error = g_error;

    co_timer_add(std::chrono::seconds(1), show_status);
}

int main(int argc, char** argv)
{
    if (argc <= 2) {
        printf("Usage %s url concurrency\n", argv[0]);
        exit(1);
    }

    int concurrency = atoi(argv[2]);

    for (int i = 0; i < concurrency; ++i)
        go [=]{ request_once(argv[1]); };

    co_timer_add(std::chrono::seconds(1), show_status);

    // 创建8个线程去并行执行所有协程
    boost::thread_group tg;
    for (int i = 0; i < 8; ++i)
        tg.create_thread([]{ g_Scheduler.RunLoop(); });
    tg.join_all();
    return 0;
}

