#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <atomic>
#include <chrono>
#include <sys/resource.h>
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::chrono;
using boost::system::error_code;
using std::shared_ptr;

// 由于socket的析构要依赖于io_service, 所以注意控制
// io_service的生命期要长于socket
io_service ios;
tcp::endpoint addr(address::from_string("127.0.0.1"), 43334);
const int g_buflen = 4096;

std::atomic<long long unsigned> g_sendbytes{0}, g_recvbytes{0}, g_qps{0};
std::atomic<int> session_count{0};
uint32_t qdata = 4096;
int thread_count = 1;
int conn_count = 1024;

void on_err(shared_ptr<tcp::socket> s, char* buf)
{
    delete buf;
}

void async_write(shared_ptr<tcp::socket> s, char* buf)
{
    s->async_write_some(buffer(buf, qdata), [s, buf](error_code const& ec, size_t n) {
                if (ec) {
                    on_err(s, buf);
                    return ;
                }

                g_sendbytes += n;
                async_write(s, buf);
            });
}

void async_read(shared_ptr<tcp::socket> s, char* buf)
{
    s->async_read_some(buffer(buf, g_buflen), [s, buf](error_code const& ec, size_t n) {
                if (ec) {
                    on_err(s, buf);
                    --session_count;
                    return ;
                }

                g_recvbytes += n;
                async_read(s, buf);
            });
}

void on_connected(error_code ec, shared_ptr<tcp::socket> s)
{
    ++session_count;
    char *writebuf = new char[g_buflen];
    char *recvbuf = new char[g_buflen];
    async_write(s, writebuf);
    async_read(s, recvbuf);
}

void connect(shared_ptr<tcp::socket> s)
{
    s->async_connect(addr, std::bind(&on_connected, std::placeholders::_1, s));
}

void echo_client()
{
    shared_ptr<tcp::socket> s(new tcp::socket(ios));
    connect(s);
}

void show_status()
{
    static int show_title = 0;
    static long long unsigned last_sendbytes = 0, last_recvbytes = 0;
    static auto start_time = system_clock::now();
    static auto last_time = system_clock::now();
    auto now = system_clock::now();
    if (show_title++ % 10 == 0) {
        printf("thread:%d, qdata:%d\n", thread_count, qdata);
        printf("  conn   send(KB)   recv(KB)     qps   AverageQps  time_delta(ms)\n");
    }
    printf("%6d  %9llu  %9llu  %7d  %7d    %7d\n",
            (int)session_count, (g_sendbytes - last_sendbytes) / 1024, (g_recvbytes - last_recvbytes) / 1024,
            (int)(double)(g_recvbytes - last_recvbytes) / qdata,
            (int)((double)g_recvbytes / qdata / std::max<int>(1, duration_cast<seconds>(now - start_time).count() + 1)),
            (int)duration_cast<milliseconds>(now - last_time).count()
            );
    last_time = now;
    last_sendbytes = g_sendbytes;
    last_recvbytes = g_recvbytes;
}

int main(int argc, char **argv)
{
    if (argc > 1)
        thread_count = atoi(argv[1]);
    if (argc > 2)
        conn_count = atoi(argv[2]);
    if (argc > 3)
        qdata = atoi(argv[3]);

    rlimit of = {8192, 8192};
    if (-1 == setrlimit(RLIMIT_NOFILE, &of)) {
        perror("setrlimit");
        exit(1);
    }

    for (int i = 0; i < conn_count; ++i)
        echo_client();

    boost::thread_group tg;
    for (int i = 0; i < thread_count; ++i)
        tg.create_thread([]{ ios.run(); });
    for (;;) {
        sleep(1);
        show_status();
    }
    return 0;
}

