#pragma once

#include <waggle/engine_runner.h>

#include <brood/launcher/launcher_types.h>
#include <cstdio>
#include <cstring>

namespace brood::launcher
{

    inline const char* GetLauncherModeArgumentName(LauncherRequestedMode mode)
    {
        switch (mode)
        {
            case LauncherRequestedMode::EDITOR:
                return "--editor";
            case LauncherRequestedMode::GAME:
                return "--game";
            case LauncherRequestedMode::HEADLESS:
                return "--headless";
            case LauncherRequestedMode::AUTO:
            default:
                return "auto";
        }
    }

    inline const char* GetCompiledLauncherModeName()
    {
#if HIVE_MODE_EDITOR
        return "editor";
#elif HIVE_MODE_HEADLESS
        return "headless";
#else
        return "game";
#endif
    }

    inline LauncherRequestedMode GetDefaultLauncherMode()
    {
#if HIVE_MODE_EDITOR
        return LauncherRequestedMode::EDITOR;
#elif HIVE_MODE_HEADLESS
        return LauncherRequestedMode::HEADLESS;
#else
        return LauncherRequestedMode::GAME;
#endif
    }

    inline bool IsLauncherModeSupported(LauncherRequestedMode mode)
    {
        switch (mode)
        {
            case LauncherRequestedMode::AUTO:
                return true;
#if HIVE_MODE_EDITOR
            case LauncherRequestedMode::EDITOR:
            case LauncherRequestedMode::HEADLESS:
                return true;
            case LauncherRequestedMode::GAME:
                return false;
#elif HIVE_MODE_HEADLESS
            case LauncherRequestedMode::HEADLESS:
                return true;
            case LauncherRequestedMode::EDITOR:
            case LauncherRequestedMode::GAME:
                return false;
#else
            case LauncherRequestedMode::GAME:
            case LauncherRequestedMode::HEADLESS:
                return true;
            case LauncherRequestedMode::EDITOR:
                return false;
#endif
        }

        return false;
    }

    inline bool TryAssignRequestedMode(LauncherRequestedMode mode, LauncherCommandLine* commandLine)
    {
        if (commandLine->m_requestedMode == LauncherRequestedMode::AUTO || commandLine->m_requestedMode == mode)
        {
            commandLine->m_requestedMode = mode;
            return true;
        }

        std::fprintf(stderr, "Error: conflicting launcher modes: %s and %s\n",
                     GetLauncherModeArgumentName(commandLine->m_requestedMode), GetLauncherModeArgumentName(mode));
        return false;
    }

    inline bool ResolveLauncherMode(const LauncherCommandLine& commandLine, waggle::EngineMode* outMode)
    {
        LauncherRequestedMode mode = commandLine.m_requestedMode;
        if (mode == LauncherRequestedMode::AUTO)
        {
            mode = GetDefaultLauncherMode();
        }

        if (!IsLauncherModeSupported(mode))
        {
            std::fprintf(stderr, "Error: this launcher binary was built in %s mode and does not support %s\n",
                         GetCompiledLauncherModeName(), GetLauncherModeArgumentName(mode));
            return false;
        }

        switch (mode)
        {
            case LauncherRequestedMode::EDITOR:
                *outMode = waggle::EngineMode::EDITOR;
                return true;
            case LauncherRequestedMode::GAME:
                *outMode = waggle::EngineMode::GAME;
                return true;
            case LauncherRequestedMode::HEADLESS:
                *outMode = waggle::EngineMode::HEADLESS;
                return true;
            case LauncherRequestedMode::AUTO:
            default:
                break;
        }

        return false;
    }

    inline void PrintUsage()
    {
        std::fprintf(stderr, "Usage: hive_launcher [--editor|--game|--headless] [--project path/to/project.hive]\n"
                             "                     [--exit-after-setup] [path/to/project.hive]\n"
                             "       Positional project paths remain supported for compatibility.\n");
    }

    inline bool TryParseCommandLine(int argc, char* argv[], LauncherCommandLine* commandLine)
    {
        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (std::strcmp(arg, "--editor") == 0)
            {
                if (!TryAssignRequestedMode(LauncherRequestedMode::EDITOR, commandLine))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }

            if (std::strcmp(arg, "--game") == 0)
            {
                if (!TryAssignRequestedMode(LauncherRequestedMode::GAME, commandLine))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }

            if (std::strcmp(arg, "--headless") == 0)
            {
                if (!TryAssignRequestedMode(LauncherRequestedMode::HEADLESS, commandLine))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }

            if (std::strcmp(arg, "--exit-after-setup") == 0)
            {
                commandLine->m_exitAfterSetup = true;
                continue;
            }

            if (std::strcmp(arg, "--project") == 0)
            {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "Error: --project requires a file path\n");
                    PrintUsage();
                    return false;
                }

                if (!commandLine->m_projectPath.empty())
                {
                    std::fprintf(stderr, "Error: multiple project paths provided\n");
                    PrintUsage();
                    return false;
                }

                commandLine->m_projectPath = argv[++i];
                continue;
            }

            if (arg[0] == '-')
            {
                std::fprintf(stderr, "Error: unknown option: %s\n", arg);
                PrintUsage();
                return false;
            }

            if (!commandLine->m_projectPath.empty())
            {
                std::fprintf(stderr, "Error: multiple project paths provided\n");
                PrintUsage();
                return false;
            }

            commandLine->m_projectPath = arg;
        }

        return true;
    }

} // namespace brood::launcher
