#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

// Sensitive-word / ad filter (todo.md #68).
//
// Drop-in word filter for chat / nickname / any user-generated text path.
// The chat handler chain itself is not built yet (only proto exists today —
// see proto/chat/chat.proto and the generated grpc stubs in
// cpp/generated/grpc_client/chat/), so this header sits ready for
// integration when chat lands. See docs/design/chat-sensitive-word-filter.md
// for the rollout plan.
//
// Design:
//   - Word list is held in a flat unordered_set<string> (case-folded). For
//     a few thousand words this is constant-time lookup and well under
//     1ms per scan.
//   - Scanner walks the input over a moving substring window. We pick a
//     "longest match wins" left-to-right scan: at position i, try the
//     longest word that fits in input[i..], down to the shortest. First
//     hit is replaced (with '*' by default) and the cursor advances past
//     the match.
//   - For Chinese / multi-byte UTF-8 we treat the input as a byte string
//     and the words as byte strings. Words in the dictionary should be
//     provided in UTF-8; matches are byte-exact. This avoids depending
//     on ICU / boost.locale at the cost of "near-miss" detection (e.g.
//     full-width vs half-width variants must be enumerated separately
//     in the dictionary). That's the same trade-off most game projects
//     make in v1.
//
// What this is NOT:
//   - Not Aho-Corasick. For ≤10k words and ≤512-char messages, the
//     "try each suffix length per position" scan is ~O(N·K) where K is
//     the max word length; with K ≈ 8 bytes and N = 512 that's 4k
//     comparisons per message — still well under the chat path's
//     latency budget. Aho-Corasick is the right next step once we
//     pass 50k+ words or longer messages; it's a separate slice (#68
//     slice C in the design doc).
//   - Not a regex engine. Pattern matching with wildcards belongs in a
//     follow-up if/when ops needs it.
//
// Thread-safety: instances are immutable after Load(). Multiple readers
// (gate threads scanning incoming chats) can hit the same SensitiveWordFilter
// concurrently. Word-list reload should construct a new instance and
// atomically swap an owning pointer at the call site.

class SensitiveWordFilter {
public:
    // Build a filter from an iterable of words. Empty / whitespace-only
    // words are dropped. The caller owns lifetime of the source range.
    template <typename Iterable>
    void Load(const Iterable& words)
    {
        words_.clear();
        maxWordLen_ = 0;
        for (const auto& w : words) {
            std::string normalized = ToLower(std::string(w));
            if (normalized.empty()) continue;
            if (normalized.size() > maxWordLen_) maxWordLen_ = normalized.size();
            words_.insert(std::move(normalized));
        }
    }

    // True iff `input` contains any sensitive word (byte-substring match).
    // Cheap probe — use when the caller only needs reject/accept and not
    // the redacted form.
    bool Contains(std::string_view input) const
    {
        if (words_.empty() || input.empty()) return false;
        const std::string lowered = ToLower(std::string(input));
        return FindFirstMatch(lowered).has_value();
    }

    // Return a copy of `input` with each sensitive word replaced by the
    // same length of `mask` chars (default '*'). When `mask` is the
    // empty string, matches are deleted. Bytes are preserved 1:1 with
    // input except inside a match span.
    std::string Redact(std::string_view input, char mask = '*') const
    {
        if (words_.empty() || input.empty()) return std::string(input);

        // Two-buffer approach so callers get UTF-8 bytes unchanged outside
        // match spans. We scan over a case-folded copy but emit from the
        // original input — important because we don't want case-folding
        // to leak into the user's stored message.
        const std::string lowered = ToLower(std::string(input));
        std::string out;
        out.reserve(input.size());

        size_t i = 0;
        while (i < lowered.size()) {
            if (const auto match = LongestMatchAt(lowered, i); match.has_value()) {
                const size_t matchLen = *match;
                if (mask != '\0') {
                    out.append(matchLen, mask);
                }
                i += matchLen;
            } else {
                out.push_back(input[i]);
                ++i;
            }
        }
        return out;
    }

    size_t Size() const { return words_.size(); }
    size_t MaxWordLen() const { return maxWordLen_; }

private:
    std::unordered_set<std::string> words_;
    size_t maxWordLen_{0};

    static std::string ToLower(std::string s)
    {
        // ASCII-only fold. UTF-8 multi-byte sequences pass through
        // unchanged (their high bits keep them outside 'A'..'Z').
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; });
        return s;
    }

    // Longest-match-wins: at position `i` in `lowered`, try suffix lengths
    // from min(maxWordLen_, remaining) down to 1. First hit wins, returns
    // its length. nullopt if no match starts here.
    std::optional<size_t> LongestMatchAt(const std::string& lowered, size_t i) const
    {
        const size_t remaining = lowered.size() - i;
        const size_t maxTry = std::min(maxWordLen_, remaining);
        for (size_t len = maxTry; len >= 1; --len) {
            if (words_.find(lowered.substr(i, len)) != words_.end()) {
                return len;
            }
        }
        return std::nullopt;
    }

    std::optional<size_t> FindFirstMatch(const std::string& lowered) const
    {
        for (size_t i = 0; i < lowered.size(); ++i) {
            if (const auto m = LongestMatchAt(lowered, i); m.has_value()) {
                return i;
            }
        }
        return std::nullopt;
    }
};
