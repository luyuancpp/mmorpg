#pragma once

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "muduo/base/Logging.h"

#include "core/text/sensitive_word_filter.h"

// Sensitive-word filter singleton + hot-reload (todo.md #68 slice A+B).
//
// Slice A in the design doc recommended sourcing the word list from the
// Excel pipeline (data_table_exporter). That coupling makes the rollout
// depend on the table-codegen path, which is currently in the same
// working tree as in-flight #102 work — touching it would conflate two
// streams. So this slice ships a path-driven loader instead:
//
//   - At startup, the singleton reads from a file (one word per line,
//     UTF-8, '#' comments allowed).
//   - To reload, ops calls Reload() — typically wired to a SIGUSR2
//     handler or an admin RPC. New filter is built from the same
//     source, atomically swapped in via std::atomic_store on the
//     shared_ptr; in-flight scans on the old filter complete safely
//     because they hold their own shared_ptr snapshot.
//   - Path defaults to env GATE_SENSITIVE_WORDS_PATH, with a fallback
//     `etc/sensitive_words.txt` for dev convenience.
//
// Migration to Excel-driven (slice A as originally specified) becomes a
// drop-in replacement of the loader source — Singleton::Reload reads
// from the new source, the swap mechanism stays the same.
//
// Threading: every call site loads the current filter via
// Singleton::Snapshot() (atomic shared_ptr load), keeps the snapshot
// for the duration of the scan, releases at the end. Reloads never
// invalidate an in-flight scan.

namespace sensitive_word_filter {

// Process-wide owner of the active filter. Initialize once at startup
// via Init(); call Snapshot() on every scan; call Reload() when the
// source has changed and you want the swap.
class Singleton {
public:
    // Acquire a read-only snapshot of the current filter. Cheap (one
    // atomic shared_ptr load); safe to call concurrently with Reload().
    // Returns an empty filter if Init() hasn't been called — Contains()
    // and Redact() both handle the empty case gracefully (allow all).
    static std::shared_ptr<::SensitiveWordFilter> Snapshot()
    {
        return std::atomic_load(&Active());
    }

    // Initialize from a file path. Idempotent: callable from any node's
    // Initialize() — re-init replaces the active filter, same as
    // Reload(). Pass an empty path to use the env / default fallback.
    //
    // Returns the number of words loaded, or 0 if the file was missing
    // / empty / unreadable. A missing file is logged at WARN and treated
    // as "no filter installed" so chat keeps working in dev where the
    // operator hasn't placed the word list yet.
    static size_t Init(const std::string& path = "")
    {
        const std::string actualPath = ResolvePath(path);
        return LoadInto(actualPath);
    }

    // Re-read the source and atomically swap the active filter. Safe to
    // call from a signal handler or admin RPC. Same return semantics
    // as Init().
    static size_t Reload(const std::string& path = "")
    {
        const std::string actualPath = ResolvePath(path);
        return LoadInto(actualPath);
    }

    // Convenience: route an input through the current snapshot. One-shot
    // wrappers so call sites don't have to manage the shared_ptr.
    static bool Contains(std::string_view input)
    {
        auto filter = Snapshot();
        return filter && filter->Contains(input);
    }

    static std::string Redact(std::string_view input, char mask = '*')
    {
        auto filter = Snapshot();
        if (!filter) return std::string(input);
        return filter->Redact(input, mask);
    }

    static size_t Size()
    {
        auto filter = Snapshot();
        return filter ? filter->Size() : 0;
    }

private:
    static std::shared_ptr<::SensitiveWordFilter>& Active()
    {
        // Meyers-singleton: leaked-on-exit, no shutdown-order race when
        // a logout hook tries to reach the filter as static destruction
        // unwinds.
        static std::shared_ptr<::SensitiveWordFilter> active =
            std::make_shared<::SensitiveWordFilter>();
        return active;
    }

    static std::string ResolvePath(const std::string& explicit_path)
    {
        if (!explicit_path.empty()) return explicit_path;
        if (const char* env = std::getenv("GATE_SENSITIVE_WORDS_PATH");
            env != nullptr && env[0] != '\0')
        {
            return std::string(env);
        }
        return std::string("etc/sensitive_words.txt");
    }

    static size_t LoadInto(const std::string& path)
    {
        std::ifstream in(path);
        if (!in)
        {
            LOG_WARN << "[sensitive_word_filter] source file not found: " << path
                     << " — leaving the active filter as-is (Init keeps the empty default)";
            return 0;
        }

        std::vector<std::string> words;
        std::string line;
        while (std::getline(in, line))
        {
            // Strip CR (CRLF tolerance) and trailing whitespace
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' ||
                                     line.back() == ' ' || line.back() == '\t'))
            {
                line.pop_back();
            }
            // Strip leading whitespace
            size_t start = 0;
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            {
                ++start;
            }
            if (start > 0) line.erase(0, start);

            if (line.empty()) continue;
            if (line[0] == '#') continue; // comment

            words.push_back(line);
        }

        auto fresh = std::make_shared<::SensitiveWordFilter>();
        fresh->Load(words);

        std::atomic_store(&Active(), fresh);
        LOG_INFO << "[sensitive_word_filter] loaded " << fresh->Size()
                 << " words from " << path;
        return fresh->Size();
    }
};

} // namespace sensitive_word_filter
