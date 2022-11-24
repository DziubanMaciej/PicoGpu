#pragma once

struct OsInterface {
    static bool isDebuggerAttached();
    static void breakpoint();
};
