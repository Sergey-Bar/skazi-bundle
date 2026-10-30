#pragma once
// Background work that the plugin never waits for.
//
//   skazi::runDetached ([state] { ... });
//
// Stopping a juce::Thread from a destructor blocks the DAW's UI until the thread notices, and if it
// is stuck in a slow call (network, a big folder, a cloud drive) JUCE kills it by force after the
// timeout, which can leave Windows locks held and hang the whole DAW. A detached job instead keeps
// what it needs in shared_ptrs, checks a cancel flag when it can, and simply finishes on its own.
// On Windows it holds a reference to the plugin DLL while it runs, so the DAW can't unload the code
// out from under it; the reference is dropped by the job's very last instruction.
#include <juce_core/juce_core.h>
#include <functional>
#include <thread>

namespace skazi
{
inline void runDetached (std::function<void()> job)
{
   #if JUCE_WINDOWS
    using GetModuleHandleExFn = int (__stdcall*) (unsigned long, const wchar_t*, void**);
    using FreeLibraryAndExitThreadFn = void (__stdcall*) (void*, unsigned long);
    constexpr unsigned long fromAddress = 0x4; // GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS: counts as a LoadLibrary

    static const char insideThisModule = 0;
    juce::DynamicLibrary kernel ("kernel32.dll");
    auto getModule = reinterpret_cast<GetModuleHandleExFn> (kernel.getFunction ("GetModuleHandleExW"));
    auto freeAndExit = reinterpret_cast<FreeLibraryAndExitThreadFn> (kernel.getFunction ("FreeLibraryAndExitThread"));
    void* self = nullptr;
    if (getModule != nullptr && freeAndExit != nullptr
        && getModule (fromAddress, reinterpret_cast<const wchar_t*> (&insideThisModule), &self) != 0)
    {
        std::thread ([job = std::move (job), self, freeAndExit]() mutable
        {
            {
                auto run = std::move (job); // destroyed (with everything it captured) before the DLL can go
                try { run(); } catch (...) {}
            }
            freeAndExit (self, 0); // never returns into this DLL
        }).detach();
        return;
    }
   #endif
    std::thread (std::move (job)).detach();
}
} // namespace skazi
