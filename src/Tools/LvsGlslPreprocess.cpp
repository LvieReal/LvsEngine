#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path input{};
    fs::path output{};
    fs::path rootDir{};
    fs::path depfile{};
    fs::path depTarget{};
};

std::string ToUtf8(const fs::path& path) {
    // MinGW's std::filesystem may expose u8string() as char8_t; keep it simple.
    return path.string();
}

[[noreturn]] void Fail(const std::string& message) {
    std::cerr << "lvs_glsl_preprocess: " << message << std::endl;
    std::exit(1);
}

std::string ReadFileText(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        Fail("failed to open: " + ToUtf8(path));
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void WriteFileText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        Fail("failed to write: " + ToUtf8(path));
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string TrimLeft(std::string_view view) {
    std::size_t pos = 0;
    while (pos < view.size()) {
        const char c = view[pos];
        if (c != ' ' && c != '\t' && c != '\r') {
            break;
        }
        ++pos;
    }
    return std::string(view.substr(pos));
}

std::string Trim(std::string_view view) {
    std::size_t begin = 0;
    while (begin < view.size()) {
        const char c = view[begin];
        if (c != ' ' && c != '\t' && c != '\r') {
            break;
        }
        ++begin;
    }
    std::size_t end = view.size();
    while (end > begin) {
        const char c = view[end - 1];
        if (c != ' ' && c != '\t' && c != '\r') {
            break;
        }
        --end;
    }
    return std::string(view.substr(begin, end - begin));
}

bool StartsWith(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

std::string EscapeDepPath(const std::string& path) {
    std::string out;
    out.reserve(path.size() + 8);
    for (const char c : path) {
        if (c == ' ') {
            out.push_back('\\');
            out.push_back(' ');
        } else {
            out.push_back(c);
        }
    }
    return out;
}

struct Context {
    fs::path rootDir{};
    std::unordered_set<std::string> dependencies{};
    std::unordered_set<std::string> pragmaOnceIncluded{};
    std::vector<std::string> includeStack{};
};

fs::path MakeAbsoluteNormalized(const fs::path& path) {
    std::error_code ec;
    fs::path abs = fs::absolute(path, ec);
    if (ec) {
        abs = path;
    }
    abs = abs.lexically_normal();
    return abs;
}

std::string MakeKey(const fs::path& path) {
    // Normalize to forward slashes for stable keying.
    std::string key = MakeAbsoluteNormalized(path).generic_string();
    return key;
}

fs::path ResolveIncludePath(const fs::path& fromFile, const fs::path& rootDir, const std::string& includePath) {
    const fs::path inc = fs::path(includePath);
    if (inc.is_absolute()) {
        Fail("absolute include paths are not allowed: " + includePath);
    }

    const fs::path fromDir = fromFile.parent_path();
    const fs::path candidateLocal = (fromDir / inc).lexically_normal();
    if (fs::exists(candidateLocal)) {
        return candidateLocal;
    }

    const fs::path candidateRoot = (rootDir / inc).lexically_normal();
    if (fs::exists(candidateRoot)) {
        return candidateRoot;
    }

    Fail("include not found: " + includePath + " (from " + ToUtf8(fromFile) + ")");
}

std::string PreprocessFile(Context& ctx, const fs::path& filePath) {
    const fs::path absPath = MakeAbsoluteNormalized(filePath);
    const std::string fileKey = MakeKey(absPath);
    ctx.dependencies.insert(fileKey);

    for (const auto& frame : ctx.includeStack) {
        if (frame == fileKey) {
            std::ostringstream oss;
            oss << "include cycle detected:\n";
            for (const auto& entry : ctx.includeStack) {
                oss << "  " << entry << "\n";
            }
            oss << "  " << fileKey;
            Fail(oss.str());
        }
    }

    std::string src = ReadFileText(absPath);

    // Split into lines (normalized on '\n'); keep '\r' out of content.
    std::vector<std::string_view> lines;
    lines.reserve(256);
    std::size_t start = 0;
    while (start <= src.size()) {
        const std::size_t end = src.find('\n', start);
        const std::size_t len = (end == std::string::npos) ? (src.size() - start) : (end - start);
        lines.emplace_back(src.data() + start, len);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    bool hasPragmaOnce = false;
    for (const auto lineView : lines) {
        const std::string trimmed = Trim(lineView);
        if (trimmed.empty()) {
            continue;
        }
        if (StartsWith(trimmed, "//")) {
            continue;
        }
        if (trimmed == "#pragma once") {
            hasPragmaOnce = true;
        }
        break;
    }
    if (hasPragmaOnce) {
        if (ctx.pragmaOnceIncluded.contains(fileKey)) {
            return {};
        }
        ctx.pragmaOnceIncluded.insert(fileKey);
    }

    ctx.includeStack.push_back(fileKey);

    std::ostringstream out;
    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::string left = TrimLeft(lines[lineIndex]);
        const std::string_view leftView(left);
        if (StartsWith(leftView, "#include")) {
            std::string rest = TrimLeft(leftView.substr(std::string_view("#include").size()));
            if (rest.empty()) {
                Fail("malformed #include (empty path) in " + ToUtf8(absPath) + ":" + std::to_string(lineIndex + 1));
            }

            const char open = rest[0];
            char close = '"';
            if (open == '"') {
                close = '"';
            } else if (open == '<') {
                close = '>';
            } else {
                Fail("malformed #include (expected '\"' or '<') in " + ToUtf8(absPath) + ":" + std::to_string(lineIndex + 1));
            }

            const std::size_t endPos = rest.find(close, 1);
            if (endPos == std::string::npos) {
                Fail("malformed #include (unterminated path) in " + ToUtf8(absPath) + ":" + std::to_string(lineIndex + 1));
            }
            const std::string includePath = rest.substr(1, endPos - 1);

            const fs::path resolved = ResolveIncludePath(absPath, ctx.rootDir, includePath);
            out << "\n// --- include begin: " << ToUtf8(resolved) << "\n";
            out << PreprocessFile(ctx, resolved);
            out << "\n// --- include end: " << ToUtf8(resolved) << "\n";
            continue;
        }

        const std::string trimmed = Trim(leftView);
        if (trimmed == "#pragma once") {
            // Consume pragma once inside included files.
            continue;
        }

        out << std::string(lines[lineIndex]) << "\n";
    }

    ctx.includeStack.pop_back();
    return out.str();
}

void WriteDepfile(const fs::path& depfile, const fs::path& target, const std::unordered_set<std::string>& deps) {
    if (depfile.empty()) {
        return;
    }

    std::vector<std::string> depList;
    depList.reserve(deps.size());
    for (const auto& dep : deps) {
        depList.push_back(dep);
    }
    std::sort(depList.begin(), depList.end());

    std::ostringstream out;
    out << EscapeDepPath(MakeKey(target)) << ":";
    for (const auto& dep : depList) {
        out << " " << EscapeDepPath(dep);
    }
    out << "\n";

    WriteFileText(depfile, out.str());
}

Options ParseArgs(int argc, char** argv) {
    Options opts{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        auto takeValue = [&](const std::string_view name) -> std::string_view {
            if (i + 1 >= argc) {
                Fail("missing value after " + std::string(name));
            }
            return std::string_view(argv[++i]);
        };

        if (arg == "--input") {
            opts.input = fs::path(std::string(takeValue(arg)));
        } else if (arg == "--output") {
            opts.output = fs::path(std::string(takeValue(arg)));
        } else if (arg == "--root-dir") {
            opts.rootDir = fs::path(std::string(takeValue(arg)));
        } else if (arg == "--depfile") {
            opts.depfile = fs::path(std::string(takeValue(arg)));
        } else if (arg == "--dep-target") {
            opts.depTarget = fs::path(std::string(takeValue(arg)));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: lvs_glsl_preprocess --input <file> --output <file> --root-dir <dir> [--depfile <file>] [--dep-target <file>]\n";
            std::exit(0);
        } else {
            Fail("unknown argument: " + std::string(arg));
        }
    }

    if (opts.input.empty() || opts.output.empty() || opts.rootDir.empty()) {
        Fail("missing required arguments (use --help)");
    }
    return opts;
}

} // namespace

int main(int argc, char** argv) {
    const Options opts = ParseArgs(argc, argv);

    Context ctx{};
    ctx.rootDir = MakeAbsoluteNormalized(opts.rootDir);

    const std::string preprocessed = PreprocessFile(ctx, opts.input);
    WriteFileText(opts.output, preprocessed);
    const fs::path depTarget = opts.depTarget.empty() ? opts.output : opts.depTarget;
    WriteDepfile(opts.depfile, depTarget, ctx.dependencies);

    return 0;
}
