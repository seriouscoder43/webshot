{
  pkgs,
  config,
  inputs,
  ...
}: let
  lib = pkgs.lib;
  buildDeps = import ./nix/common_deps.nix {inherit pkgs;};
  toolchain = import ./nix/toolchain.nix {inherit pkgs;};
  llvm21 = pkgs.llvmPackages_21;
  system = pkgs.stdenv.system;

  python = pkgs.python3;
  chaoticPython = python.withPackages (ps: [
    ps.jinja2
    ps.pyyaml
    ps.pydantic
    ps.psycopg2
    ps.websockets
    ps.minio
  ]);

  userverDeps = import ./nix/userver/deps.nix {
    inherit pkgs chaoticPython;
  };

  # Extra libs needed by C++ tests; used only in a dedicated wrapper,
  # not exported globally into the dev shell.
  testLibs = userverDeps ++ [pkgs.stdenv.cc.cc];

  webshotTestSan = pkgs.writeShellScriptBin "webshot-test-san" ''
    set -euo pipefail
    export LD_LIBRARY_PATH='${lib.makeLibraryPath testLibs}'
    cd /tmp/build-webshot-san
    ctest --output-on-failure
  '';

  userverPkgs = inputs.userver.packages.${system};
  uniAlgoPkgs = inputs."uni-algo".packages.${system};
  yttsPkgs = inputs."yandex-taxi-testsuite".packages.${system};
in {
  cachix.enable = false;
  packages =
    buildDeps.native
    ++ buildDeps.runtime
    ++ [
      toolchain.cc
      llvm21.llvm
      userverPkgs.userver-debug-addr-ub
      uniAlgoPkgs.default
      yttsPkgs.default
    ]
    ++ userverDeps
    ++ [webshotTestSan]
    ++ (with pkgs; [gdb]);

  treefmt = {
    enable = true;
    config = {
      programs = {
        alejandra.enable = true;
        clang-format.enable = true;
        cmake-format.enable = true;
        ruff-format.enable = true;
        sqlfluff.enable = true;
      };
      settings.global.excludes = [
        ".git/**"
        ".devenv/**"
        ".direnv/**"
        ".cache/**"
        ".pytest_cache/**"
        "secrets/**"
      ];
    };
  };

  git-hooks.hooks = {
    treefmt = {
      enable = true;
      settings.formatters = builtins.attrValues config.treefmt.config.build.programs;
    };
    ruff.enable = true;
    sqlfluff-lint = {
      enable = true;
      entry = "sqlfluff lint";
      package = pkgs.sqlfluff;
      language = "system";
      files = "\\.sql$";
    };
  };
  env.CMAKE_PREFIX_PATH = lib.makeSearchPath "lib/cmake" [
    userverPkgs.userver-debug-addr-ub
    pkgs.boost183.dev
    pkgs.fmt.dev
    pkgs.zstd.dev
    pkgs.cctz
    pkgs.yaml-cpp
  ];

  env.PKG_CONFIG_PATH = lib.makeSearchPath "lib/pkgconfig" [
    pkgs.cryptopp.dev
  ];

  env.USERVER_PYTHON = "${chaoticPython}/bin/python3";
  env.USERVER_PYTHON_PATH = "${chaoticPython}/bin/python3";

  env.USERVER_DIR = "${userverPkgs.userver-debug-addr-ub}/lib/cmake/userver";

  # Expose the yandex-taxi-testsuite Python package so pytest_userver
  # can import `testsuite` (for chaos, pgsql helpers, etc.).
  env.PYTHONPATH = lib.makeSearchPath python.sitePackages [
    yttsPkgs.default
  ];

  env.WEBSHOT_LLVM_SYMBOLIZER_BIN = "${llvm21.llvm}/bin/llvm-symbolizer";

  tasks."webshot:infraUp" = {
    exec = "bash containers/quadlet/install_quadlet.sh && systemctl --user start webshot-stack.target";
    cwd = config.git.root;
  };

  tasks."webshot:infraDown" = {
    exec = "systemctl --user stop webshot-stack.target";
    cwd = config.git.root;
  };

  tasks."webshot:configureSan" = {
    exec = "cmake --preset configure-preset-clang-san";
    cwd = config.git.root;
  };

  tasks."webshot:buildSan" = {
    exec = "cmake --build --preset build-preset-clang-san";
    cwd = config.git.root;
  };

  tasks."webshot:testSan" = {
    package = webshotTestSan;
    exec = "webshot-test-san";
  };
}
