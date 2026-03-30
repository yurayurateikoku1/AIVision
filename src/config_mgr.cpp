#include "config_mgr.h"
#include "context.h"
#include <fstream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

ConfigMgr &ConfigMgr::getInstance()
{
    static ConfigMgr inst;
    return inst;
}

ConfigMgr::ConfigMgr()
{
    loadConfig();
    // SPDLOG_INFO("Config loaded");
}

ConfigMgr::~ConfigMgr()
{
    saveConfig();
    // SPDLOG_INFO("Config saved");
}

bool ConfigMgr::loadConfig(const std::string &path)
{
    return true;
}

bool ConfigMgr::saveConfig(const std::string &path)
{
    return true;
}