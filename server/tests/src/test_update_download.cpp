#include "tests/includes/test_framework.h"
#include "tests/includes/test_utf8_path.h"

#include "settings/native/native_settings_update_download.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

TEST_CASE(update_download_rejects_installer_names_it_would_never_write)
{
    using native_settings::update_download::IsInstallerName;
    REQUIRE(IsInstallerName("MetasequoiaIME_Setup_v0.11.1.exe"));
    REQUIRE(IsInstallerName("MetasequoiaIME_Setup_v0.11.1-dev.20261022.7fa6fb1a.exe"));
    // 只要不是这个名字形状，就说明清单不可信，不下载。
    REQUIRE(!IsInstallerName("setup.exe"));
    REQUIRE(!IsInstallerName("MetasequoiaIME_Setup_v0.11.1.exe.part"));
    REQUIRE(!IsInstallerName("MetasequoiaIME_Setup_v0.11.1.zip"));
    REQUIRE(!IsInstallerName("..\\MetasequoiaIME_Setup_v1.exe"));
    REQUIRE(!IsInstallerName(std::string(200, 'a') + ".exe"));
}

TEST_CASE(update_download_rejects_bad_digests_before_touching_the_network)
{
    using native_settings::update_download::IsSha256Hex;
    REQUIRE(IsSha256Hex(std::string(64, 'a')));
    REQUIRE(IsSha256Hex("321b09fab2c86c1072ae35e56158d8616698af2416a8a0ffbed6dafecb6f2a38"));
    REQUIRE(!IsSha256Hex(std::string(63, 'a')));
    REQUIRE(!IsSha256Hex(std::string(65, 'a')));
    // 大写不接受：清单一直是小写，大小写混用说明来源变了。
    REQUIRE(!IsSha256Hex(std::string(64, 'A')));
    REQUIRE(!IsSha256Hex(std::string(64, 'g')));
    REQUIRE(!IsSha256Hex(""));
}

TEST_CASE(update_download_builds_github_first_then_the_mirror)
{
    using namespace native_settings::update_download;
    const std::string release = "https://github.com/metasequoiaime/MSIME-Windows/releases/tag/v0.11.1";
    const std::vector<std::string> channels = ChannelUrls(release, "MetasequoiaIME_Setup_v0.11.1.exe");
    REQUIRE_EQ(channels.size(), size_t{2});
    REQUIRE_EQ(channels[0], std::string("https://github.com/metasequoiaime/MSIME-Windows/releases/download/v0.11.1/"
                                        "MetasequoiaIME_Setup_v0.11.1.exe"));
    REQUIRE_EQ(channels[1], "https://dl.msime.app/gh/" + channels[0]);

    // 不是本仓库的发布页地址时不产生任何通道，调用方据此报错而不是乱下。
    REQUIRE(
        ChannelUrls("https://github.com/other/repo/releases/tag/v1.0.0", "MetasequoiaIME_Setup_v1.0.0.exe").empty());
    REQUIRE(ChannelUrls("https://msime.app/download/", "MetasequoiaIME_Setup_v1.0.0.exe").empty());
    // 安装包名不合法时同样不产生通道。
    REQUIRE(ChannelUrls(release, "setup.exe").empty());
}

TEST_CASE(update_download_refuses_to_start_without_a_digest)
{
    using namespace native_settings::update_download;
    Request request;
    request.release_url = "https://github.com/metasequoiaime/MSIME-Windows/releases/tag/v0.11.1";
    request.installer_name = "MetasequoiaIME_Setup_v0.11.1.exe";
    request.directory = test::TempRoot() / L"update-download-unit";

    // 没有摘要：直接失败，一次网络请求都不发。
    request.installer_sha256.clear();
    Result result = Download(request);
    REQUIRE(result.outcome == Outcome::Failed);
    REQUIRE(result.attempts.empty());
    REQUIRE(!result.message.empty());

    // 摘要格式不对同理。
    request.installer_sha256 = "not-a-digest";
    result = Download(request);
    REQUIRE(result.outcome == Outcome::Failed);
    REQUIRE(result.attempts.empty());
}

TEST_CASE(update_download_pruning_leaves_only_the_kept_installer)
{
    namespace fs = std::filesystem;
    using namespace native_settings::update_download;
    const fs::path directory = test::TempRoot() / L"update-download-prune";
    std::error_code ec;
    fs::remove_all(directory, ec);
    fs::create_directories(directory, ec);
    REQUIRE(!ec);

    const fs::path keep = directory / "MetasequoiaIME_Setup_v0.11.1.exe";
    const fs::path older = directory / "MetasequoiaIME_Setup_v0.11.0.exe";
    const fs::path part = directory / "MetasequoiaIME_Setup_v0.11.1.exe.part";
    const fs::path unrelated = directory / "notes.txt";
    for (const fs::path &path : {keep, older, part, unrelated})
    {
        std::ofstream file(path, std::ios::binary);
        file << "x";
    }

    PruneOldInstallers(directory, keep.filename().u8string());
    REQUIRE(fs::is_regular_file(keep));
    // 旧安装包与上次中断留下的临时文件都清掉，别的文件一概不动。
    REQUIRE(!fs::exists(older));
    REQUIRE(!fs::exists(part));
    REQUIRE(fs::is_regular_file(unrelated));

    fs::remove_all(directory, ec);
}
