#include "operation_view.h"
#include "ui_operation_view.h"
#include "camera_window.h"
#include "../context.h"
#include "../workflow/workflow_mgr.h"
#include <spdlog/spdlog.h>

OperationView::OperationView(Context &ctx, WorkflowMgr &wf_mgr, QWidget *parent)
    : QWidget(parent), ui(new Ui::OperationView), ctx_(ctx), wf_mgr_(wf_mgr)
{
    ui->setupUi(this);
}

OperationView::~OperationView()
{
    delete ui;
}

void OperationView::setImagePaths(const std::vector<QString> &paths)
{
    image_paths_ = paths;
    image_index_ = 0;
    if (!image_paths_.empty())
        showCurrentImage();
}

void OperationView::showCurrentImage()
{
    HalconCpp::HObject image = loadCurrentImage();
    if (!image.IsInitialized())
        return;
    if (camera_view_)
        camera_view_->updateFrame(image);
}

HalconCpp::HObject OperationView::loadCurrentImage() const
{
    if (image_paths_.empty())
        return {};
    HalconCpp::HObject image;
    try { HalconCpp::ReadImage(&image, image_paths_[image_index_].toStdString().c_str()); }
    catch (...) {}
    return image;
}

void OperationView::on_pushButton_testImage_clicked()
{
    int di = ctx_.selected_workflow_index;

    HalconCpp::HObject image = loadCurrentImage();
    if (!image.IsInitialized())
    {
        SPDLOG_WARN("No offline image available");
        return;
    }

    wf_mgr_.setOfflineImage(di, image);
    wf_mgr_.triggerOnce(di);
    SPDLOG_INFO("Test triggered: DI{}", di);
}

void OperationView::on_pushButton_nextImage_clicked()
{
    if (image_paths_.empty()) return;
    image_index_ = (image_index_ + 1) % static_cast<int>(image_paths_.size());
    showCurrentImage();
}

void OperationView::on_pushButton_previousImage_clicked()
{
    if (image_paths_.empty()) return;
    image_index_ = (image_index_ - 1 + static_cast<int>(image_paths_.size())) % static_cast<int>(image_paths_.size());
    showCurrentImage();
}
