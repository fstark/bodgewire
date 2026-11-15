#include "netlist/rom_directory.hpp"

#include <algorithm>
#include <map>
#include <system_error>

namespace bw {

std::expected<void, std::string> ApplyRomDirectory(Netlist& netlist,
                                                   const std::filesystem::path& directory) {
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::absolute(directory, ec);
    if (ec || !std::filesystem::is_directory(root, ec)) {
        return std::unexpected("no ROM directory " + directory.string());
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
        const auto part = std::ranges::find(netlist.parts, reference, &PartDecl::reference);
        const bool rom = part != netlist.parts.end() &&
                         std::ranges::any_of(
                             part->params, [](const PartParam& p) { return p.name == "contents"; });
        if (!rom) {
            return std::unexpected(directory.string() + ": " + path.filename().string() +
                                   " matches no ROM in " + netlist.file);
        }
        std::erase_if(part->params, [](const PartParam& p) { return p.name == "offset"; });
        for (PartParam& param : part->params) {
            if (param.name == "contents") {
                param.value = path.lexically_normal().string();
            }
        }
    }
    return {};
}

}  // namespace bw
