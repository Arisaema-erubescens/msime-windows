#pragma once

// Matching for the settings search box.
//
// Split out of native_settings_controls.cpp on purpose: this is pure string work over a list of
// titles, it touches no Windows API, and keeping it separate lets it live in the portable test
// target where AddressSanitizer and UndefinedBehaviorSanitizer run on a Linux runner. The control
// header itself cannot go there, it pulls in D2D.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace native_settings
{
// One indexed settings row.
//
// The entry owns its row rather than pointing at a page's copy. Pages are built for the index
// without being shown, so if the index only pointed weakly the rows would die the moment the
// builder returned; holding them keeps a background-indexed page's rows alive for as long as the
// index needs them, which is what lets every setting be searchable before it has been opened.
// The cost is one visual tree per page that was indexed but never visited, which is what the
// window would have held anyway once the user visited it.
struct SettingsIndexEntry
{
    std::string pageId;
    std::wstring pageTitle;
    std::wstring title;
    std::wstring description;
    std::shared_ptr<void> row;
};

// Matches `query` against entry titles and descriptions, best first.
//
// Not a full-text search. The settings tree has a few hundred rows with titles like
// "候选框最大高度" and what people type is a fragment of one ("候选", "高度"). Every query
// character must appear in the title in order, which is what a CJK user expects from a search box
// with no pinyin input; a title match outranks a description-only match, then the tighter match
// wins, then the shorter title, so "候选" surfaces the candidate-box rows before long rows that
// merely mention the word.
//
// Comparison folds ASCII case and full-width forms, so "ai" finds "AI 与云联想" and "ＡＩ". Equal
// scores keep index order, so the same query always gives the same list.
std::vector<SettingsIndexEntry> SearchSettings(const std::vector<SettingsIndexEntry> &entries,
                                               const std::wstring &query, size_t limit = 12);
} // namespace native_settings
