#include "Windows.h"

#include <cstdio>
#include <thread>

namespace debug {
    static const auto size = 256;
    static char buffer[size] = "[example-dll] ";

    template<typename... Args>
    inline void log(const char *message, Args... args) {
        // +/- 9 because of the initial buffer contents
        sprintf_s(buffer + 14, size - 14, message, args...);

        OutputDebugStringA(buffer);
    }
}

namespace global {
    bool exiting = false;
    bool should_exit = false;
}

DWORD WINAPI cleanup(void *instance) {
    debug::log("cleaning up");

    FreeLibraryAndExitThread(static_cast<HMODULE>(instance), 0);
}

DWORD WINAPI main_thread(void *instance) {
    debug::log("main thread started, waiting for one second");

    MessageBoxA(nullptr, "Main thread running", "Success", MB_ICONINFORMATION);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    global::exiting = true;
    cleanup(instance);

    return 0;
}

DWORD WINAPI create_thread(LPTHREAD_START_ROUTINE function, LPVOID parameter) {
    auto handle = CreateThread(nullptr, 0, function, parameter, 0, nullptr);

    if (handle) {
        if (CloseHandle(handle)) {
            return 1;
        }

        debug::log("failed closing handle to thread");

        return 0;
    } else {
        MessageBoxA(nullptr, "Failed creating thread", "Error", MB_ICONERROR);

        return 0;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        debug::log("got DLL_PROCESS_ATTACH");

        create_thread(&main_thread, instance);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (global::exiting) {
            debug::log("got DLL_PROCESS_DETACH");
        } else {
            debug::log("got DLL_PROCESS_DETACH while main thread was running");

            create_thread(&cleanup, instance);
        }
    }

    return TRUE;
}