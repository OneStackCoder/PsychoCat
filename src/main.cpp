#include <slint.h>

#include "app.h"

int main()
{
    // 创建 UI 实例
    auto ui = AppWindow::create();

    // 把 .slint 里声明的 increment 回调绑定到 C++ 逻辑
    ui->on_increment([&] {
        ui->set_counter(ui->get_counter() + 1);
    });

    // 进入事件循环，窗口关闭后返回
    ui->run();
    return 0;
}
