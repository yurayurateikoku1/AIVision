#include <QApplication>
#include "ui/main_window.h"
#include "context.h"
#include "camera/camera_mgr.h"
#include "communication/comm_mgr.h"
#include "workflow/workflow_mgr.h"
#include <spdlog/spdlog.h>
#include <QFile>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char *argv[])
{
    // Haclon 环境变量
    _putenv_s("HALCONROOT", "D:\\C++Library\\haclon-25.11SDK");
    _putenv_s("HALCONARCH", "x64-win64");

    spdlog::set_default_logger(spdlog::stdout_color_mt("console"));

    QApplication app(argc, argv);
    QFile file(":/src/ui/style/style.qss");
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QString styleSheet = QLatin1String(file.readAll());
        app.setStyleSheet(styleSheet);
        file.close();
    }

    auto &ctx = Context::getInstance();

    CameraMgr cam_mgr;
    CommMgr comm_mgr;
    WorkflowMgr wf_mgr(ctx, cam_mgr, comm_mgr);

    QObject::connect(&app, &QApplication::aboutToQuit, [&comm_mgr]()
                     { comm_mgr.shutdown(); });

    wf_mgr.buildAll();

    MainWindow w(ctx, cam_mgr, comm_mgr, wf_mgr);
    w.show();
    return app.exec();
}
