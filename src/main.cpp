#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "shine/codegen.h"
#include "shine/error.h"
#include "shine/lexer.h"
#include "shine/parser.h"
#include "shine/typecheck.h"

using namespace shine;

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "shinec: can't open '" << path << "'\n"; std::exit(1); }
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

static std::string stripExt(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path;
    return path.substr(0, dot);
}

static std::filesystem::path exeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return std::filesystem::current_path();
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

static std::filesystem::path toolchainRoot() { return exeDir() / "toolchain"; }

static std::string quote(const std::filesystem::path& p) {
    return "\"" + p.string() + "\"";
}

static std::string quote(const std::string& s) {
    return "\"" + s + "\"";
}

static bool fileExists(const std::filesystem::path& p) {
    return std::filesystem::exists(p) && std::filesystem::is_regular_file(p);
}

#ifdef _WIN32
static int runProcess(const std::filesystem::path& exe, const std::vector<std::string>& args) {
    std::string cmd = quote(exe);
    for (const auto& arg : args) cmd += " " + arg;

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    std::vector<char> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back('\0');

    if (!CreateProcessA(exe.string().c_str(), mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "shinec: failed to launch '" << exe.string() << "' (error " << GetLastError() << ")\n";
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
}
#endif

static void writeRspLine(std::ofstream& f, const std::filesystem::path& p) {
    f << '"' << p.generic_string() << '"' << '\n';
}

static void writeRspLine(std::ofstream& f, const char* s) {
    f << s << '\n';
}

template <class Mod>
static auto setTriple(Mod& mod, const llvm::Triple& t, int)
    -> decltype(mod.setTargetTriple(t), void()) {
    mod.setTargetTriple(t);
}

template <class Mod>
static void setTriple(Mod& mod, const llvm::Triple& t, long) {
    mod.setTargetTriple(t.getTriple());
}

template <class Tgt>
static auto makeTargetMachine(const Tgt& target, const llvm::Triple& t, const llvm::TargetOptions& opts,
                               llvm::Reloc::Model reloc, int)
    -> decltype(target.createTargetMachine(t, "generic", "", opts, reloc)) {
    return target.createTargetMachine(t, "generic", "", opts, reloc);
}

template <class Tgt>
static llvm::TargetMachine* makeTargetMachine(const Tgt& target, const llvm::Triple& t,
                                               const llvm::TargetOptions& opts, llvm::Reloc::Model reloc, long) {
    return target.createTargetMachine(t.getTriple(), "generic", "", opts, reloc);
}

static void emitObj(llvm::Module& mod, const std::string& objPath, bool target32) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::Triple triple(target32 ? "i686-elf" : llvm::sys::getDefaultTargetTriple());
    setTriple(mod, triple, 0);

    std::string lookupErr;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), lookupErr);
    if (!target) { std::cerr << "shinec: " << lookupErr << "\n"; std::exit(1); }

    llvm::TargetOptions opts;
    llvm::Reloc::Model reloc = target32 ? llvm::Reloc::Static : llvm::Reloc::PIC_;
    std::unique_ptr<llvm::TargetMachine> tm(makeTargetMachine(*target, triple, opts, reloc, 0));
    mod.setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(objPath, ec, llvm::sys::fs::OF_None);
    if (ec) { std::cerr << "shinec: " << ec.message() << "\n"; std::exit(1); }


    llvm::legacy::PassManager pass;
    if (tm->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        std::cerr << "shinec: can't emit object file for this target\n";
        std::exit(1);
    }
    pass.run(mod);
    dest.flush();
}

static void link(const std::string& objPath, const std::string& exePath) {
    std::filesystem::path bundle = toolchainRoot();
    std::filesystem::path ld = bundle / "bin" / "ld.exe";

#ifdef _WIN32
    if (fileExists(ld)) {
        std::filesystem::path libDir = bundle / "lib";
        std::filesystem::path gccDir = libDir / "gcc" / "current";
        std::filesystem::path rsp = std::filesystem::temp_directory_path() / "shine-link.rsp";

        {
            std::ofstream f(rsp, std::ios::binary | std::ios::trunc);
            if (!f) {
                std::cerr << "shinec: can't write link response file\n";
                std::exit(1);
            }

            writeRspLine(f, "-o");
            writeRspLine(f, std::filesystem::path(exePath));
            writeRspLine(f, "-m");
            writeRspLine(f, "i386pep");
            writeRspLine(f, "--subsystem");
            writeRspLine(f, "console");
            writeRspLine(f, "-e");
            writeRspLine(f, "mainCRTStartup");
            writeRspLine(f, libDir / "crt2.o");
            writeRspLine(f, gccDir / "crtbegin.o");
            writeRspLine(f, std::filesystem::path(objPath));
            writeRspLine(f, "-L");
            writeRspLine(f, gccDir);
            writeRspLine(f, "-L");
            writeRspLine(f, libDir);
            writeRspLine(f, "-lmingw32");
            writeRspLine(f, "-lgcc_s");
            writeRspLine(f, "-lgcc");
            writeRspLine(f, "-lmingwex");
            writeRspLine(f, "-lmsvcrt");
            writeRspLine(f, "-lpthread");
            writeRspLine(f, "-lkernel32");
            writeRspLine(f, "-luser32");
            writeRspLine(f, "-lshell32");
            writeRspLine(f, "-ladvapi32");
            writeRspLine(f, libDir / "default-manifest.o");
            writeRspLine(f, gccDir / "crtend.o");
        }

        std::string rspArg = quote("@" + rsp.string());
        if (runProcess(ld, {rspArg}) != 0) {
            std::cerr << "shinec: bundled linker failed\n";
            std::filesystem::remove(rsp);
            std::exit(1);
        }
        std::filesystem::remove(rsp);
        return;
    }

    std::string cmd = "g++ \"" + objPath + "\" -o \"" + exePath + "\"";
#else
    std::string cmd = "cc \"" + objPath + "\" -o \"" + exePath + "\"";
#endif
    if (std::system(cmd.c_str()) != 0) { std::cerr << "shinec: link failed\n"; std::exit(1); }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: shinec <input.shine> [-o <output>] [-32]\n"; return 1; }

    std::string in, out;
    bool target32 = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) out = argv[++i];
        else if (a == "-32") target32 = true;
        else in = a;
    }
    if (in.empty()) { std::cerr << "usage: shinec <input.shine> [-o <output>] [-32]\n"; return 1; }
    if (out.empty()) {
        out = stripExt(in);
        if (target32) out += ".o";
#ifdef _WIN32
        else out += ".exe";
#endif
    }

    try {
        Lexer lex(readFile(in), in);
        Parser parser(lex.tokenize());
        Module mod = parser.parseModule(in);

        TypeChecker().check(mod);

        CodeGen cg;
        auto ir = cg.generate(mod);

        if (target32) {
            emitObj(*ir, out, true);
            std::cout << "shinec: built '" << out << "' (32-bit freestanding ELF object, not linked)\n";
            return 0;
        }

        std::string objPath = stripExt(in) + ".o";
        emitObj(*ir, objPath, false);
        link(objPath, out);

        std::cout << "shinec: built '" << out << "'\n";
        return 0;
    } catch (const CompileError& e) {
        std::cerr << e.what() << "\n";
        std::ifstream lf(e.loc().file, std::ios::binary);
        if (lf) {
            std::string line;
            for (int i = 0; i < e.loc().line && std::getline(lf, line); i++) {}
            if (!line.empty()) std::cerr << "    " << line << "\n";
        }
        return 1;
    }
}