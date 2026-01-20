#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

bool should_ignore(const fs::directory_entry& entry) {
    const std::string name = entry.path().filename().string();

    // Ignore hidden and build-system artifacts.
    if(name.starts_with('.') || name.starts_with("cmake-")) {
        return true;
    }

    // Ignore specific unwanted files.
    static constexpr std::string_view ignored_files[] = {
        "conanfile.py",
        // "project_tree.txt"
    };
    return std::ranges::any_of(ignored_files, [&](std::string_view ign) {
        return name == ign;
    });
}

void print_dir_tree(const fs::path& root, std::ofstream& out, const std::string& prefix = "") {
    std::vector<fs::directory_entry> entries;
    for(const auto& entry: fs::directory_iterator(root)) {
        if(!should_ignore(entry))
            entries.emplace_back(entry);
    }

    auto sort_pred = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        if(a.is_directory() != b.is_directory()) {
            return a.is_directory() > b.is_directory();
        }
        return a.path().filename().string() < b.path().filename().string();
    };
    std::ranges::sort(entries, sort_pred);

    for(size_t i = 0; i < entries.size(); ++i) {
        const auto& entry     = entries[i];
        bool is_last          = (i == entries.size() - 1);
        std::string connector = is_last ? "└── " : "├── ";
        out << prefix << connector << entry.path().filename().string() << '\n';

        if(entry.is_directory()) {
            if(entry.path().filename().string() == "range-v3") {
                continue;
            }
            std::string next_prefix = prefix + (is_last ? "    " : "│   ");
            print_dir_tree(entry.path(), out, next_prefix);
        }
    }
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
	std::cout << "Usage: {}" << argv[0] << " <directory> [output_file]" << std::endl;
        return 1;
    }

    fs::path root = fs::absolute(argv[1]);
    if(!fs::exists(root) || !fs::is_directory(root)) {
	std::cout << "Error: " << root.string() << " is not a valid directory.";
        return 1;
    }

    fs::path output_path = (argc >= 3) ? fs::path(argv[2]) : fs::path("blueprint.txt");
    std::ofstream out(output_path);
    if(!out) {
	std::cout << "Error: cannot open output file " << output_path.string() << std::endl;
        return 1;
    }

    out << root.filename().string() << '\n';
    print_dir_tree(root, out, "   ");

	std::cout << "Directory tree written to: " << output_path.string() << std::endl;
    return 0;
}
