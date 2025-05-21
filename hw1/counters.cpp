#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <climits>

#define MSG_HELP "usage: %s n=[counter_n] freq=[freq_hz] max=[cnt_max]\n"

static void EnvSetup();
static void EnvClear();
static int EnvGetChar(bool is_nonblock = false);

#if defined(__unix__)

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

static void EnvSetup() {
    termios attr;

    if (tcgetattr(STDIN_FILENO, &attr) == -1)
        return;

    attr.c_lflag &= ~(ICANON | ECHO);
    attr.c_cc[VTIME] = 0;
    attr.c_cc[VMIN] = 1;

    tcsetattr(STDIN_FILENO, TCSANOW, &attr);
}

static void EnvClear() {
    std::puts("\033[H\033[J");
}

static int EnvGetChar(bool is_nonblock) {
    int len;

    if (ioctl(STDIN_FILENO, FIONREAD, &len) == -1)
        return -1;

    if (is_nonblock && len < 1)
        return -1;

    return std::getchar();
}

#elif defined(_WIN32)

#include <windows.h>
#include <conio.h>

static void EnvSetup() {}

static void EnvClear() {
    auto hdl = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hdl, &csbi);

    COORD coord = { 0, 0 };
    DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD rowCount;

    FillConsoleOutputCharacter(hdl, ' ', cellCount, coord, &rowCount);
    FillConsoleOutputAttribute(hdl, csbi.wAttributes, cellCount, coord, &rowCount);
    SetConsoleCursorPosition(hdl, coord);
}

static int EnvGetChar(bool is_nonblock) {
    if (is_nonblock && !_kbhit())
        return -1;

    return _getch();
}

#endif

template <typename T>
static void TrimTextL(T &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](auto ch) {
        return !std::isspace(ch);
    }));
}

template <typename T>
static void TrimTextR(T &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](auto ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

template <typename T>
static void TrimText(T &s) {
    TrimTextL(s);
    TrimTextR(s);
}

class CounterTask {
public:
    CounterTask(unsigned long freq_hz_, int cnt_max_, int cnt_ = 0):
        freq_hz(freq_hz_), cnt_max(cnt_max_), cnt(cnt_),
        th([this]() { thread_task(); }) {}

    ~CounterTask() {
        is_stop = true;
        th.join();
    }

    CounterTask(const CounterTask &) = delete;
    void operator=(const CounterTask &) = delete;

    int get_cnt() const {
        return cnt.load(std::memory_order_relaxed);
    }

    bool is_stopped() const {
        return is_stop;
    }

    bool is_paused() const {
        return is_pause;
    }

    void pause(bool is_pause_) {
        is_pause = is_pause_;
    }

private:
    void thread_task() {
        while (!is_stop) {
            if (!is_pause)
                cnt.store((cnt.load(std::memory_order_relaxed) + 1) % (cnt_max + 1), std::memory_order_release);

            std::this_thread::sleep_for(std::chrono::nanoseconds(1000000000 / freq_hz));
        }
    }

    unsigned long freq_hz;
    int cnt_max;

    std::atomic_int cnt;
    std::thread th;

    bool is_pause = true;
    bool is_stop = false;
};

class CounterTaskPool {
public:
    CounterTaskPool(std::size_t counter_n, unsigned long freq_hz_, int cnt_max_, int cnt_ = 0) {
        task_vec.reserve(counter_n);

        for (std::size_t i = 0; i < counter_n; i++)
            task_vec.emplace_back(std::make_unique<CounterTask>(freq_hz_, cnt_max_, cnt_));
    }

    CounterTaskPool(const CounterTaskPool &) = delete;
    void operator=(const CounterTaskPool &) = delete;

    std::size_t get_task_count() const {
        return task_vec.size();
    }

    CounterTask &task_at(std::size_t idx) {
        return *task_vec.at(idx);
    }

    const CounterTask &task_at(std::size_t idx) const {
        return *task_vec.at(idx);
    }

private:
    std::vector<std::unique_ptr<CounterTask>> task_vec;
};

class UiRenderer {
public:
    UiRenderer(long tick_ms_):
        tick_ms(tick_ms_) {}

    void run(const std::function<bool (std::ostream &)> &cb) const {
        std::string last_out;

        EnvSetup();

        while (true) {
            std::ostringstream ss;

            auto is_continue = cb(ss);
            auto out = ss.str();

            if (out != last_out) {
                EnvClear();
                std::puts(out.c_str());

                last_out = out;
            }

            if (!is_continue)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(tick_ms));
        }
    }

private:
    long tick_ms;
};

class UiTask {
public:
    UiTask(CounterTaskPool &task_pool_):
        task_pool(task_pool_),
        th([this]() { thread_task(); }) {}

    ~UiTask() {
        th.join();
    }

    UiTask(const UiTask &) = delete;
    void operator=(const UiTask &) = delete;

private:
    void thread_task() {
        std::string alert;
        auto alert_time = std::chrono::steady_clock::now();
        std::size_t cur_task_idx = 0;

        UiRenderer(1).run([&](auto &out) {
            auto &task = task_pool.task_at(cur_task_idx);
            auto ch = EnvGetChar(true);

            if (!alert.empty())
                if ((std::chrono::steady_clock::now() - alert_time) >= std::chrono::milliseconds(2000))
                    alert.clear();

            switch (ch) {
            case 'q':
                out << "stopping, please wait...\n";
                return false;

            case 'n':
                alert = "counter" + std::to_string(cur_task_idx);
                cur_task_idx = (cur_task_idx + 1) % task_pool.get_task_count();
                alert += " -> counter" + std::to_string(cur_task_idx);
                alert_time = std::chrono::steady_clock::now();
                break;

            case ' ':
                task.pause(!task.is_paused());
                alert = "counter" + std::to_string(cur_task_idx) + (task.is_paused() ? " paused" : " activated");
                alert_time = std::chrono::steady_clock::now();
                break;
            }

            for (std::size_t i = 0; i < task_pool.get_task_count(); i++) {
                auto &task = task_pool.task_at(i);
                out << "counter" << i << " : " << task.get_cnt() << " (" << (task.is_paused() ? "paused" : "counting") << ")\n";
            }

            out << '\n' << alert;
            out << "\ncurrent: counter" << cur_task_idx << " (" << (task.is_paused() ? "paused" : "counting") << ")\n";

            return true;
        });
    }

    CounterTaskPool &task_pool;
    std::thread th;
};

int main(int argc, char **argv) {
    std::size_t counter_n = 0;
    unsigned long freq_hz = 0;
    long cnt_max = -1;

    for (int i = 1; i < argc; i++) {
        std::string part(argv[i]);
        TrimText(part);

        if (part.find("n=") == 0)
            counter_n = std::strtoul(part.substr(sizeof("n")).c_str(), nullptr, 0);
        else if (part.find("freq=") == 0)
            freq_hz = std::strtoul(part.substr(sizeof("freq")).c_str(), nullptr, 0);
        else if (part.find("max=") == 0)
            cnt_max = std::strtol(part.substr(sizeof("max")).c_str(), nullptr, 0);
    }

    if (counter_n == 0 || freq_hz == 0 || cnt_max < 0 || cnt_max > INT_MAX) {
        std::printf(MSG_HELP, argc > 0 ? argv[0] : "");
        std::exit(1);
    }

    CounterTaskPool task_pool(counter_n, freq_hz, cnt_max);
    UiTask ui_task(task_pool);

    return 0;
}
