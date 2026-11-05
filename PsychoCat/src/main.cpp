#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <slint.h>

#include "main.h"

#ifdef __linux__
// X11 头文件会定义 None/Bool 等宏，放在最后引入避免污染其它头文件
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#endif

namespace {

constexpr int kSize = 9;       // 9x9 棋盘
constexpr int kDiameter = 40;  // 圆点直径
constexpr int kSpace = 5;      // 圆点间隙
constexpr int kXOffset = 31;   // 棋盘内水平偏移
constexpr int kYOffset = 210;  // 棋盘内垂直偏移
constexpr int kCatWidth = 48;
constexpr int kCatHeight = 73;

struct Pos {
    int row = 0;
    int col = 0;
};

int index_of(const Pos &p) { return p.row * kSize + p.col; }

bool in_bounds(const Pos &p)
{
    return p.row >= 0 && p.row < kSize && p.col >= 0 && p.col < kSize;
}

bool on_edge(const Pos &p)
{
    return p.row == 0 || p.col == 0 || p.row == kSize - 1 || p.col == kSize - 1;
}

// 六边形相邻格：奇数行整体向右偏移半个圆
std::array<Pos, 6> neighbors(const Pos &p)
{
    const int odd = p.row % 2;
    return { Pos{p.row - 1, p.col - 1 + odd}, Pos{p.row - 1, p.col + odd},
             Pos{p.row, p.col - 1}, Pos{p.row, p.col + 1},
             Pos{p.row + 1, p.col - 1 + odd}, Pos{p.row + 1, p.col + odd} };
}

int cell_x(int row, int col)
{
    return (kDiameter + kSpace) * col + kXOffset + (kDiameter / 2) * (row % 2);
}

int cell_y(int row) { return kDiameter * row + kYOffset; }

// 多源 BFS：以所有可用的边缘格为起点，算出每个空格到边缘的最短步数
// 走不到边缘的格子为 -1
std::array<int, kSize * kSize> distance_to_edge(const std::array<bool, kSize * kSize> &blocked)
{
    std::array<int, kSize * kSize> dist;
    dist.fill(-1);
    std::deque<Pos> queue;

    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            const Pos p{ r, c };
            if (!on_edge(p) || blocked[r * kSize + c])
                continue;
            dist[r * kSize + c] = 0;
            queue.push_back(p);
        }
    }

    while (!queue.empty()) {
        const Pos p = queue.front();
        queue.pop_front();
        for (const Pos &n : neighbors(p)) {
            if (!in_bounds(n))
                continue;
            const int ni = index_of(n);
            if (dist[ni] != -1 || blocked[ni])
                continue;
            dist[ni] = dist[index_of(p)] + 1;
            queue.push_back(n);
        }
    }
    return dist;
}

class Game
{
public:
    explicit Game(MainWindow &ui) : ui(ui), rng(std::random_device{}()) { reset(); }

    void reset();
    void click(int row, int col);
    void tick();

private:
    bool choose_step(Pos &next, bool &has_path);
    void update_cat();
    void finish(bool win);

    MainWindow &ui;
    std::shared_ptr<slint::VectorModel<Cell>> model;
    std::array<bool, kSize * kSize> blocked{};
    Pos cat;
    int steps = 0;
    int frame = 0;
    bool over = false;
    bool enclosed = false;
    std::mt19937 rng;
};

void Game::reset()
{
    blocked.fill(false);

    // 随机放置 6~12 个路障，避开正中心（小猫出生点）
    std::uniform_int_distribution<int> count_dist(6, 12);
    std::uniform_int_distribution<int> cell_dist(0, kSize - 1);
    const int count = count_dist(rng);
    int placed = 0;
    while (placed < count) {
        const int r = cell_dist(rng);
        const int c = cell_dist(rng);
        if (r == kSize / 2 && c == kSize / 2)
            continue;
        if (blocked[r * kSize + c])
            continue;
        blocked[r * kSize + c] = true;
        ++placed;
    }

    cat = { kSize / 2, kSize / 2 };
    steps = 0;
    frame = 0;
    over = false;
    enclosed = false;

    auto cells = std::make_shared<slint::VectorModel<Cell>>();
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            Cell cell;
            cell.row = r;
            cell.col = c;
            cell.x = cell_x(r, c);
            cell.y = cell_y(r);
            cell.blocked = blocked[r * kSize + c];
            cells->push_back(cell);
        }
    }
    model = cells;
    ui.set_cells(model);

    ui.set_show_result(false);
    ui.set_won(false);
    ui.set_enclosed(false);
    ui.set_cat_frame_row(0);
    ui.set_cat_frame_col(0);
    update_cat();
}

void Game::click(int row, int col)
{
    if (over || row < 0 || row >= kSize || col < 0 || col >= kSize)
        return;

    const int i = row * kSize + col;
    if (blocked[i])
        return;

    blocked[i] = true;
    Cell cell;
    cell.row = row;
    cell.col = col;
    cell.x = cell_x(row, col);
    cell.y = cell_y(row);
    cell.blocked = true;
    model->set_row_data(i, cell);

    ++steps;

    if (on_edge(cat)) {
        finish(false);
        return;
    }

    Pos next;
    bool has_path = true;
    if (!choose_step(next, has_path)) {
        finish(true); // 四周全被堵死
        return;
    }

    cat = next;
    update_cat();

    if (!has_path && !enclosed) {
        // 已经没有通往边缘的路了，换成"被包围"动画
        enclosed = true;
        frame = 0;
        ui.set_enclosed(true);
    }

    if (on_edge(cat))
        finish(false);
}

// 选下一步：向边缘推进的同时避免钻进死角；等好的走法之间随机选，行为不机械
// 返回 false 表示小猫四周全被堵死（玩家获胜）
bool Game::choose_step(Pos &next, bool &has_path)
{
    std::vector<Pos> free;
    for (const Pos &n : neighbors(cat)) {
        if (in_bounds(n) && !blocked[index_of(n)])
            free.push_back(n);
    }
    if (free.empty())
        return false;

    const auto dist = distance_to_edge(blocked);
    has_path = dist[index_of(cat)] >= 0;

    if (!has_path) {
        // 已经被围在一片区域里：在能走的空格中随机挪动
        next = free[std::uniform_int_distribution<size_t>(0, free.size() - 1)(rng)];
        return true;
    }

    // 优先走向离边缘更近的格（通常是唯一方向，也可能有几个并列）
    int best_dist = std::numeric_limits<int>::max();
    for (const Pos &n : free) {
        const int d = dist[index_of(n)];
        if (d >= 0 && d < best_dist)
            best_dist = d;
    }

    // 等距时看机动性：走完后周围空格越多，越不容易马上被围死
    int best_mobility = -1;
    std::vector<Pos> moves;
    for (const Pos &n : free) {
        if (dist[index_of(n)] != best_dist)
            continue;
        int mobility = 0;
        for (const Pos &m : neighbors(n)) {
            if (in_bounds(m) && !blocked[index_of(m)])
                ++mobility;
        }
        if (mobility > best_mobility) {
            best_mobility = mobility;
            moves.clear();
        }
        if (mobility == best_mobility)
            moves.push_back(n);
    }

    next = moves[std::uniform_int_distribution<size_t>(0, moves.size() - 1)(rng)];
    return true;
}

void Game::update_cat()
{
    const int center_x = cell_x(cat.row, cat.col) + kDiameter / 2;
    const int center_y = cell_y(cat.row) + kDiameter / 2;
    ui.set_cat_x(center_x - kCatWidth / 2);
    ui.set_cat_y(center_y - kCatHeight);
}

void Game::tick()
{
    if (over)
        return;
    frame = (frame + 1) % (enclosed ? 15 : 16);
    ui.set_cat_frame_row(frame / 4);
    ui.set_cat_frame_col(frame % 4);
}

void Game::finish(bool win)
{
    over = true;
    ui.set_won(win);
    ui.set_show_result(true);

    if (!win)
        return;

    std::string title;
    if (steps <= 5)
        title = "获得称号：精神病院长的帮手";
    else if (steps <= 10)
        title = "获得称号：隔壁的王伯伯";
    else if (steps <= 15)
        title = "获得称号：根本停不下来";
    else if (steps <= 20)
        title = "获得称号：喵了个咪的";
    else
        title = "获得称号：小菜猫";

    ui.set_result_info(slint::SharedString("您用 " + std::to_string(steps) + " 步抓住了神经猫"));
    ui.set_result_title(slint::SharedString(title));
    ui.set_enclosed(true);
}

#ifdef __linux__
// winit 的 X11 后端会把非 ASCII 标题按 Latin-1 编码写进 WM_NAME，
// 部分窗口管理器读这个属性就会显示乱码（如 "å›´ä½..."）。
// 这里在窗口创建后按 X11 规范重写标题属性：
//   WM_NAME      -> COMPOUND_TEXT
//   _NET_WM_NAME -> UTF-8
bool fix_x11_window_title(const char *title)
{
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy)
        return false;

    // 遍历窗口树，找到属于本进程（_NET_WM_PID 相同）的窗口
    Window target = None;
    {
        const Window root = DefaultRootWindow(dpy);
        const Atom pid_atom = XInternAtom(dpy, "_NET_WM_PID", False);
        const auto pid = static_cast<unsigned long>(getpid());

        std::vector<Window> todo{ root };
        while (!todo.empty() && target == None) {
            const Window w = todo.back();
            todo.pop_back();

            Atom type = None;
            int fmt = 0;
            unsigned long n = 0, left = 0;
            unsigned char *data = nullptr;
            if (XGetWindowProperty(dpy, w, pid_atom, 0, 1, False, XA_CARDINAL, &type, &fmt,
                                   &n, &left, &data) == Success
                && data != nullptr) {
                if (n == 1 && *reinterpret_cast<unsigned long *>(data) == pid)
                    target = w;
                XFree(data);
            }
            if (target != None)
                break;

            Window r, p, *children = nullptr;
            unsigned int nchildren = 0;
            if (XQueryTree(dpy, w, &r, &p, &children, &nchildren)) {
                for (unsigned int i = 0; i < nchildren; ++i)
                    todo.push_back(children[i]);
                XFree(children);
            }
        }
    }

    const bool found = target != None;
    if (found) {
        char *text = const_cast<char *>(title);
        XTextProperty prop;
        if (Xutf8TextListToTextProperty(dpy, &text, 1, XCompoundTextStyle, &prop) == Success) {
            XSetTextProperty(dpy, target, &prop, XA_WM_NAME);
            XFree(prop.value);
        }
        const Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
        const Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
        XChangeProperty(dpy, target, net_wm_name, utf8_string, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(title),
                        static_cast<int>(strlen(title)));
        XFlush(dpy);
    }

    XCloseDisplay(dpy);
    return found;
}

// 窗口由 winit 在事件循环启动后才创建，这里延迟+重试直到找到为止。
// 若程序跑在 Wayland/Windows 上（无本进程的 X11 窗口），重试几次后自动放弃，无副作用。
void schedule_x11_title_fix(int attempt)
{
    slint::Timer::single_shot(std::chrono::milliseconds(200), [attempt] {
        const int next = attempt + 1;
        if (fix_x11_window_title("围住神经猫") || next >= 10)
            return;
        schedule_x11_title_fix(next);
    });
}
#endif

} // namespace

int main()
{
#ifdef __linux__
    // winit 0.30 已移除 WINIT_UNIX_BACKEND 环境变量，只要 WAYLAND_DISPLAY 存在就
    // 优先走原生 Wayland；而 GNOME Shell 对原生 Wayland 窗口的中文标题栏会渲染成
    // 方块（tofu）。XWayland 路径配合上面的 fix_x11_window_title 已验证可正常显示
    // 中文，因此在有 X 服务时隐藏 WAYLAND_DISPLAY 让 winit 回退到 X11。
    // 纯 Wayland 环境（无 DISPLAY）不受影响，仍走原生 Wayland。
    if (getenv("DISPLAY") != nullptr)
        unsetenv("WAYLAND_DISPLAY");
#endif

    auto ui = MainWindow::create();
    Game game(*ui);

    ui->on_cell_clicked([&game](int row, int col) { game.click(row, col); });
    ui->on_restart([&game] { game.reset(); });
    ui->on_quit([] { slint::quit_event_loop(); });

    // 38ms 换一帧，和原版计时器一致
    auto timer = std::make_shared<slint::Timer>();
    timer->start(slint::TimerMode::Repeated, std::chrono::milliseconds(38),
                 [&game] { game.tick(); });

#ifdef __linux__
    // 修复 X11 下中文标题乱码（详见函数注释）
    schedule_x11_title_fix(0);
#endif

    ui->run();
    return 0;
}
