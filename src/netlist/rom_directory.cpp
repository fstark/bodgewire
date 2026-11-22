#include "netlist/rom_directory.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <system_error>

namespace bw {

namespace {

// The ROM Part `reference` (a Part with contents=), or nullptr.
PartDecl* RomPart(Netlist& netlist, const std::string& reference) {
    const auto part = std::ranges::find(netlist.parts, reference, &PartDecl::reference);
    const bool rom =
        part != netlist.parts.end() &&
        std::ranges::any_of(part->params, [](const PartParam& p) { return p.name == "contents"; });
    return rom ? &*part : nullptr;
}

// Fills `part` from `path`, at `offset` in it (none: the file is one chip).
void Fill(PartDecl& part, const std::filesystem::path& path,
          const std::optional<std::string>& offset) {
    std::erase_if(part.params, [](const PartParam& p) { return p.name == "offset"; });
    for (PartParam& param : part.params) {
        if (param.name == "contents") {
            param.value = path.lexically_normal().string();
        }
    }
    if (offset) {
        part.params.push_back({"offset", *offset});
    }
}

// The sockets.txt of `root`.
std::expected<void, std::string> ApplySockets(Netlist& netlist, const std::filesystem::path& root,
                                              const std::string& shown) {
    const std::string where = shown + "/" + kRomSocketsFile;
    std::ifstream in(root / kRomSocketsFile);
    if (!in) {
        return std::unexpected("cannot read " + where);
    }
    std::set<std::string> seen;
    int number = 0;
    for (std::string line; std::getline(in, line);) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream words(line);
        std::string reference, file, offset, extra;
        if (!(words >> reference)) {
            continue;
        }
        const std::string at = where + " line " + std::to_string(number) + ": ";
        words >> file >> offset >> extra;
        if (file.empty() || !extra.empty() || (!offset.empty() && !offset.starts_with("offset="))) {
            return std::unexpected(at + "expected REF FILE [offset=N]");
        }
        PartDecl* part = RomPart(netlist, reference);
        if (part == nullptr) {
            return std::unexpected(at + reference + " is no ROM in " + netlist.file);
        }
        if (!seen.insert(reference).second) {
            return std::unexpected(at + reference + " twice");
        }
        std::error_code ec;
        if (!std::filesystem::is_regular_file(root / file, ec)) {
            return std::unexpected(at + "no file " + file);
        }
        Fill(*part, root / file,
             offset.empty() ? std::nullopt
                            : std::optional<std::string>(offset.substr(sizeof("offset=") - 1)));
    }
    return {};
}

}  // namespace

std::expected<void, std::string> ApplyRomDirectory(Netlist& netlist,
                                                   const std::filesystem::path& directory) {
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::absolute(directory, ec);
    if (ec || !std::filesystem::is_directory(root, ec)) {
        return std::unexpected("no ROM directory " + directory.string());
    }
    if (std::filesystem::exists(root / kRomSocketsFile, ec)) {
        return ApplySockets(netlist, root, directory.string());
    }
    // REF.bin and REF.rom by reference.
    std::map<std::string, std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        const std::filesystem::path name = entry.path().filename();
        if (name.extension() != ".bin" && name.extension() != ".rom") {
            continue;
        }
        const std::string reference = name.stem().string();
        if (const auto [it, added] = files.emplace(reference, entry.path()); !added) {
            return std::unexpected(directory.string() + ": two files for " + reference + " (" +
                                   it->second.filename().string() + ", " + name.string() + ")");
        }
    }
    if (ec) {
        return std::unexpected("cannot read ROM directory " + directory.string());
    }
    for (const auto& [reference, path] : files) {
        PartDecl* part = RomPart(netlist, reference);
        if (part == nullptr) {
            return std::unexpected(directory.string() + ": " + path.filename().string() +
                                   " matches no ROM in " + netlist.file);
        }
        Fill(*part, path, std::nullopt);
    }
    return {};
}

}  // namespace bw
