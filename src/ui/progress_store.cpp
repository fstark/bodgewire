#include "ui/progress_store.hpp"

#include <cstdlib>
#include <expected>
#include <print>
#include <utility>

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>
#endif

namespace bw {

#ifdef __EMSCRIPTEN__

namespace {

// clang-format off: JavaScript (it would split ===)
EM_JS_DEPS(bw_progress, "$stringToNewUTF8,$UTF8ToString");

// localStorage's text, malloc'ed (the caller frees it); 0 when absent or not allowed.
EM_JS(char*, BwProgressGet, (), {
    try {
        const text = localStorage.getItem("bodgewire.progress");
        return text === null ? 0 : stringToNewUTF8(text);
    } catch (e) {
        return 0;
    }
});

// 1 when kept.
EM_JS(int, BwProgressSet, (const char* text), {
    try {
        localStorage.setItem("bodgewire.progress", UTF8ToString(text));
        return 1;
    } catch (e) {
        return 0;
    }
});
// clang-format on

}  // namespace

#endif

ProgressStore ProgressStore::For(const Options& options) {
    ProgressStore store;
#ifdef __EMSCRIPTEN__
    // A --progress through the page's ?arg= would be a file in memory, lost on reload.
    (void)options;
    store.where_ = Where::Browser;
#else
    if (options.progress) {
        store.where_ = Where::File;
        store.file_ = *options.progress;
    } else if (!options.screenshot) {
        char* dir = SDL_GetPrefPath("bodgewire", "bw_ui");
        if (dir == nullptr) {
            std::println(stderr, "bw_ui: no place for the saved progress: {}", SDL_GetError());
        } else {
            store.where_ = Where::File;
            store.file_ = std::filesystem::path(dir) / "progress.txt";
            SDL_free(dir);
        }
    }
#endif
    return store;
}

Progress ProgressStore::load() {
    switch (where_) {
    case Where::None:
        return {};
    case Where::File: {
        auto read = ReadProgressFile(file_);
        if (!read) {
            std::println(stderr, "bw_ui: {}: the progress is not saved this time", read.error());
            where_ = Where::None;
            return {};
        }
        return std::move(*read);
    }
    case Where::Browser: {
#ifdef __EMSCRIPTEN__
        char* text = BwProgressGet();
        if (text == nullptr) {
            return {};
        }
        Progress progress = Progress::Parse(text);
        std::free(text);
        return progress;
#else
        return {};
#endif
    }
    }
    return {};
}

void ProgressStore::write(const Progress& progress) {
    switch (where_) {
    case Where::None:
        return;
    case Where::File:
        if (const auto written = WriteProgressFile(file_, progress); !written) {
            std::println(stderr, "bw_ui: {}", written.error());
        }
        return;
    case Where::Browser:
#ifdef __EMSCRIPTEN__
        if (BwProgressSet(progress.text().c_str()) == 0) {
            std::println(stderr, "bw_ui: the browser keeps no progress");
        }
#endif
        return;
    }
}

void ProgressStore::add(Progress& progress, std::string key) {
    if (where_ == Where::None) {
        progress.add(std::move(key));
        return;
    }
    Progress saved = load();      // another instance's ticks since
    if (where_ == Where::None) {  // unreadable now: kept for the run only
        progress.add(std::move(key));
        return;
    }
    progress.merge(saved);
    progress.add(std::move(key));
    write(progress);
}

void ProgressStore::clear(Progress& progress) {
    progress.clear();
    write(progress);
}

}  // namespace bw
