#pragma once
#include <filesystem.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <thread>
#ifdef TEST_ONLY
#include <../src/mesa/util/os_time.h>
#else
#include <mesa/util/os_time.h>
#endif
#include "gpu_metrics_util.h"
#include <atomic>
#include <spdlog/spdlog.h>
#include <map>

class GPU_fdinfo {
private:
    bool init = false;

    const std::string module;
    const std::string pci_dev;

    std::thread thread;
    std::condition_variable cond_var;

    std::atomic<bool> stop_thread { false };
    std::atomic<bool> paused { false };

    struct gpu_metrics metrics;
    mutable std::mutex metrics_mutex;

    std::vector<std::ifstream> fdinfo;
    std::ifstream energy_stream;

    std::string drm_engine_type = "EMPTY";
    std::string drm_memory_type = "EMPTY";

    std::vector<std::map<std::string, std::string>> fdinfo_data;
    void gather_fdinfo_data();

    void main_thread();

    void find_fd();
    void find_intel_hwmon();

    int get_gpu_load();
    uint64_t get_gpu_time();

    std::vector<uint64_t> xe_fdinfo_last_cycles;
    int get_xe_load();
    std::pair<uint64_t, uint64_t> get_gpu_time_xe();

    float get_memory_used();

    float get_current_power();
    float get_power_usage();

public:
    GPU_fdinfo(const std::string module, const std::string pci_dev)
        : module(module)
        , pci_dev(pci_dev)
    {
        SPDLOG_DEBUG("GPU driver is \"{}\"", module);

        if (module == "i915") {
            drm_engine_type = "drm-engine-render";
            drm_memory_type = "drm-total-local0";
        } else if (module == "xe") {
            drm_engine_type = "drm-total-cycles-rcs";
            drm_memory_type = "drm-total-vram0";
        } else if (module == "amdgpu") {
            drm_engine_type = "drm-engine-gfx";
            drm_memory_type = "drm-memory-vram";
        } else if (module == "msm") {
            // msm driver does not report vram usage
            drm_engine_type = "drm-engine-gpu";
        }

        if (module == "i915" || module == "xe")
            find_intel_hwmon();

        find_fd();

        std::thread thread(&GPU_fdinfo::main_thread, this);
        thread.detach();
    }

    gpu_metrics copy_metrics() const
    {
        return metrics;
    };

    void pause()
    {
        paused = true;
        cond_var.notify_one();
    }

    void resume()
    {
        paused = false;
        cond_var.notify_one();
    }
};
