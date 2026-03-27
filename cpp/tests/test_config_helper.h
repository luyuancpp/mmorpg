// test_config_helper.h — Shared config-finding logic for all test executables.
// Include this header in a test's main() file and call FindAndLoadTestConfig()
// before loading tables or running tests.
//
// The helper searches multiple candidate directories for etc/base_deploy_config.yaml
// and etc/game_config.yaml, changes the working directory to the match, and loads
// both configs into tlsNodeConfigManager.

#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "engine/config/config.h"
#include <node_config_manager.h>

namespace test_config
{

    namespace fs = std::filesystem;

    // Searches for the runtime directory containing etc/base_deploy_config.yaml
    // and etc/game_config.yaml using multiple candidate paths derived from the
    // current working directory and the test executable location.
    //
    // On success: changes cwd to the runtime directory, loads both configs into
    // tlsNodeConfigManager, and returns true.
    // On failure: prints diagnostics to stderr and returns false.
    inline bool FindAndLoadTestConfig(int argc, char **argv)
    {
        const auto addCandidate = [](std::vector<fs::path> &candidates, const fs::path &path)
        {
            if (path.empty())
                return;
            for (const auto &existing : candidates)
            {
                if (existing == path)
                    return;
            }
            candidates.emplace_back(path);
        };

        std::vector<fs::path> candidates;
        addCandidate(candidates, fs::current_path());
        addCandidate(candidates, fs::current_path() / "bin");

        if (argc > 0 && argv[0] != nullptr)
        {
            const fs::path exeDir = fs::absolute(argv[0]).parent_path();
            addCandidate(candidates, exeDir);
            addCandidate(candidates, exeDir / "bin");
            addCandidate(candidates, exeDir.parent_path());
            addCandidate(candidates, exeDir.parent_path() / "bin");
        }

        for (const auto &dir : candidates)
        {
            const auto baseCfg = dir / "etc" / "base_deploy_config.yaml";
            const auto gameCfg = dir / "etc" / "game_config.yaml";
            if (!fs::exists(baseCfg) || !fs::exists(gameCfg))
                continue;

            fs::current_path(dir);
            if (readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig()) &&
                readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig()))
            {
                return true;
            }
        }

        std::cerr << "Failed to locate/load configs. Tried runtime dirs:\n";
        for (const auto &c : candidates)
        {
            std::cerr << "  " << c.string() << "\n";
        }
        return false;
    }

} // namespace test_config
