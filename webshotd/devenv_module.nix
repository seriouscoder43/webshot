{
  pkgs,
  config,
  inputs,
  ...
}: let
  ctx = import ../devenv/ctx.nix {inherit pkgs config inputs;};
  lib = ctx.nix.lib;
  repoPythonPath = "${ctx.drv.repoPy}/bin/python3";
in {
  outputs.webshot = ctx.mkProjPkg {
    userver = ctx.drv.userver;
    variant = ctx.variants.release;
  };

  packages =
    ctx.sets.buildNative
    ++ ctx.sets.devTools
    ++ ctx.sets.runtime
    ++ ctx.sets.runtimeTools
    ++ [
      ctx.drv.repoPy
      ctx.toolchain.cc
      ctx.nix.llvmPackages_22.llvm
      ctx.nix.llvmPackages_22.clang-tools
      ctx.drv.includeWhatYouUse
      ctx.drv.userverDbg
      ctx.drv.unialgo
      ctx.drv.testsuite
      ctx.drv.pgmigrate
    ]
    ++ ctx.sets.userverLibs
    ++ [ctx.drv.testCov];

  env.CMAKE_PREFIX_PATH = ctx.paths.cmakePrefix;

  env.PKG_CONFIG_PATH = lib.makeSearchPath "lib/pkgconfig" [
    ctx.nix.cryptopp.dev
  ];

  env.USERVER_PYTHON = repoPythonPath;
  env.USERVER_PYTHON_PATH = repoPythonPath;
  env.USERVER_DIR = "${ctx.drv.userverDbg}/lib/cmake/userver";

  # Expose the yandex-taxi-testsuite Python package so pytest_userver
  # can import `testsuite` (for chaos, pgsql helpers, etc.).
  env.PYTHONPATH =
    "${config.devenv.root}:"
    + (lib.makeSearchPath ctx.nix.python3.sitePackages [
      ctx.drv.testsuite
    ]);
}
