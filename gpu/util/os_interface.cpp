#include "gpu/util/os_interface.h"

#include <csignal>
#include <cstdlib>

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/wait.h>
#elif _WIN32
#include <Windows.h>
#endif

bool OsInterface::isDebuggerAttached() {
#ifdef __linux__
    const int pid = fork();
    if (pid == -1) {
        // Failed to fork... Just assume there's no debugger
        return false;
    }

    if (pid == 0) {
        // We're in child process. Try to trace the parent process. If it can be
        // traced, it means it's not already traced, so there's no debugger.
        const int ppid = getppid();
        if (ptrace(PTRACE_ATTACH, ppid, NULL, NULL) == 0) {
            // Successfully traced parent process, meaning it's not being debugged.
            waitpid(ppid, NULL, 0);
            ptrace(PTRACE_CONT, NULL, NULL);
            ptrace(PTRACE_DETACH, getppid(), NULL, NULL);
            exit(0);
        } else {
            // Tracing parent process failed, meaning it's already being debugger.
            exit(1);
        }
    } else {
        // We're in parent process. Wait for child process, which will check if we're
        // being already traced by some debugger.
        int status{};
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
#elif _WIN32
    return IsDebuggerPresent();
#else
#error "Unknown OS"
#endif
}

void OsInterface::breakpoint() {
#ifdef __linux__
    std::raise(SIGTRAP);
#elif _WIN32
    std::raise(SIGABRT);
#else
#error "Unknown OS"
#endif
}
