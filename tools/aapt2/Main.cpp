/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

#include <iostream>
#include <vector>

#include "android-base/stringprintf.h"
#include "android-base/utf8.h"
#include "androidfw/StringPiece.h"

#include "Diagnostics.h"
#include "cmd/Command.h"
#include "cmd/Compile.h"
#include "cmd/Convert.h"
#include "cmd/Diff.h"
#include "cmd/Dump.h"
#include "cmd/Link.h"
#include "cmd/Optimize.h"
#include "util/Files.h"
#include "util/Util.h"

using ::android::StringPiece;
using ::android::base::StringPrintf;

namespace aapt {

// DO NOT UPDATE, this is more of a marketing version.
static const char* sMajorVersion = "2";

// Update minor version whenever a feature or flag is added.
static const char* sMinorVersion = "19";

/** Prints the version information of AAPT2. */
class VersionCommand : public Command {
 public:
  explicit VersionCommand() : Command("version") {
    SetDescription("Prints the version of aapt.");
  }

  int Action(const std::vector<std::string>& /* args */) override {
    std::cerr << StringPrintf("Android Asset Packaging Tool (aapt) %s:%s", sMajorVersion,
                              sMinorVersion)
              << std::endl;
    return 0;
  }
};

/** The main entry point of AAPT. */
class MainCommand : public Command {
 public:
  explicit MainCommand(IDiagnostics* diagnostics) : Command("aapt2"), diagnostics_(diagnostics) {
    AddOptionalSubcommand(util::make_unique<CompileCommand>(diagnostics));
    AddOptionalSubcommand(util::make_unique<LinkCommand>(diagnostics));
    AddOptionalSubcommand(util::make_unique<DumpCommand>());
    AddOptionalSubcommand(util::make_unique<DiffCommand>());
    AddOptionalSubcommand(util::make_unique<OptimizeCommand>());
    AddOptionalSubcommand(util::make_unique<ConvertCommand>());
    AddOptionalSubcommand(util::make_unique<VersionCommand>());
  }

  int Action(const std::vector<std::string>& args) override {
    if (args.size() == 0) {
      diagnostics_->Error(DiagMessage() << "no subcommand specified");
    } else {
      diagnostics_->Error(DiagMessage() << "unknown subcommand '" << args[0] << "'");
    }

    Usage(&std::cerr);
    return -1;
  }

 private:
  IDiagnostics* diagnostics_;
};

/*
 * Run in daemon mode. The first line of input is the command. This can be 'quit' which ends
 * the daemon mode. Each subsequent line is a single parameter to the command. The end of a
 * invocation is signaled by providing an empty line. At any point, an EOF signal or the
 * command 'quit' will end the daemon mode.
 */
class DaemonCommand : public Command {
 public:
  explicit DaemonCommand(IDiagnostics* diagnostics) : Command("daemon", "m"),
                                                      diagnostics_(diagnostics) {
    SetDescription("Runs aapt in daemon mode. Each subsequent line is a single parameter to the\n"
        "command. The end of an invocation is signaled by providing an empty line.");
  }

  int Action(const std::vector<std::string>& /* args */) override {
    std::cout << "Ready" << std::endl;

    while (true) {
      std::vector<std::string> raw_args;
      for (std::string line; std::getline(std::cin, line) && !line.empty();) {
        raw_args.push_back(line);
      }

      if (!std::cin) {
        break;
      }

      // An empty command does nothing.
      if (raw_args.empty()) {
        continue;
      }

      // End the dameon
      if (raw_args[0] == "quit") {
        break;
      }

      std::vector<StringPiece> args;
      args.insert(args.end(), raw_args.begin(), raw_args.end());
      if (MainCommand(diagnostics_).Execute(args, &std::cerr) != 0) {
        std::cerr << "Error" << std::endl;
      }
      std::cerr << "Done" << std::endl;
    }
    std::cout << "Exiting daemon" << std::endl;

    return 0;
  }

 private:
  IDiagnostics* diagnostics_;
};

}  // namespace aapt

int MainImpl(int argc, char** argv) {
  if (argc < 1) {
    return -1;
  }

  // Collect the arguments starting after the program name and command name.
  std::vector<StringPiece> args;
  for (int i = 1; i < argc; i++) {
    args.push_back(argv[i]);
  }

  // Add the daemon subcommand here so it cannot be called while executing the daemon
  aapt::StdErrDiagnostics diagnostics;
  auto main_command = new aapt::MainCommand(&diagnostics);
  main_command->AddOptionalSubcommand(aapt::util::make_unique<aapt::DaemonCommand>(&diagnostics));

  return main_command->Execute(args, &std::cerr);
}

int main(int argc, char** argv) {
#ifdef _WIN32
  LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  CHECK(wide_argv != nullptr) << "invalid command line parameters passed to process";

  std::vector<std::string> utf8_args;
  for (int i = 0; i < argc; i++) {
    std::string utf8_arg;
    if (!::android::base::WideToUTF8(wide_argv[i], &utf8_arg)) {
      std::cerr << "error converting input arguments to UTF-8" << std::endl;
      return 1;
    }
    utf8_args.push_back(std::move(utf8_arg));
  }
  LocalFree(wide_argv);

  std::unique_ptr<char* []> utf8_argv(new char*[utf8_args.size()]);
  for (int i = 0; i < argc; i++) {
    utf8_argv[i] = const_cast<char*>(utf8_args[i].c_str());
  }
  argv = utf8_argv.get();
#endif
  return MainImpl(argc, argv);
}
