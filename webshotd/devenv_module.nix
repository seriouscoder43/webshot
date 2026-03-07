{
  pkgs,
  config,
  inputs,
  ...
}: let
  common = import ../devenv/lib.nix {inherit pkgs config inputs;};
in {
  outputs.webshot = common.mkWebshotOutput {
    userverPkg = common.userverPkgs.userver-debug-addr-ub;
  };
  outputs.squidImageDev = common.squidImageDev;
  outputs.squidImageProdlike = common.squidImageProdlike;

  packages =
    common.buildDeps.native
    ++ common.buildDeps.runtime
    ++ [
      common.chaoticPython
      common.toolchain.cc
      common.llvm21.llvm
      common.llvm21.clang-tools
      common.userverPkgs.userver-debug-addr-ub
      common.uniAlgoPkgs.default
      common.yttsPkgs.default
      common.squidLoadDev
      common.squidLoadProdlike
    ]
    ++ common.userverDeps
    ++ [common.webshotTestSan common.webshotTestCov]
    ++ (with common.pkgsWithOverlay; [git gdb nssTools]);

  env.CMAKE_PREFIX_PATH = common.lib.makeSearchPath "lib/cmake" [
    common.userverPkgs.userver-debug-addr-ub
    common.pkgsWithOverlay.boost183.dev
    common.pkgsWithOverlay.fmt.dev
    common.pkgsWithOverlay.zstd.dev
    common.pkgsWithOverlay.cctz
    common.pkgsWithOverlay.yaml-cpp
  ];

  env.PKG_CONFIG_PATH = common.lib.makeSearchPath "lib/pkgconfig" [
    common.pkgsWithOverlay.cryptopp.dev
  ];

  env.USERVER_PYTHON = "${common.chaoticPython}/bin/python3";
  env.USERVER_PYTHON_PATH = "${common.chaoticPython}/bin/python3";
  env.USERVER_DIR = "${common.userverPkgs.userver-debug-addr-ub}/lib/cmake/userver";

  # Expose the yandex-taxi-testsuite Python package so pytest_userver
  # can import `testsuite` (for chaos, pgsql helpers, etc.).
  env.PYTHONPATH =
    "${config.devenv.root}:"
    + (common.lib.makeSearchPath common.python.sitePackages [
      common.yttsPkgs.default
    ]);

  env.WEBSHOTD_RUNTIME_LD_LIBRARY_PATH = common.lib.makeLibraryPath common.testLibs;
  env.WEBSHOTD_BUILD_DIR = common.buildDirs.san;

  tasks."webshot:infraDevUp" = {
    exec = "python3 container/compose/infra.py dev up";
    cwd = config.devenv.root;
  };

  tasks."webshot:infraDevDown" = {
    exec = "python3 container/compose/infra.py dev down";
    cwd = config.devenv.root;
  };

  tasks."webshot:devUp" = {
    exec = "python3 container/compose/infra.py dev up && python3 container/compose/webshotd.py dev up";
    cwd = config.devenv.root;
  };

  tasks."webshot:devDown" = {
    exec = "python3 container/compose/webshotd.py dev down && python3 container/compose/infra.py dev down";
    cwd = config.devenv.root;
  };

  tasks."webshot:devStatus" = {
    exec = "python3 container/compose/infra.py dev status && python3 container/compose/webshotd.py dev status";
    cwd = config.devenv.root;
  };

  tasks."webshot:devLogs" = {
    exec = "python3 container/compose/webshotd.py dev logs";
    cwd = config.devenv.root;
  };

  tasks."webshot:infraProdlikeUp" = {
    exec = "python3 container/compose/infra.py prodlike up";
    cwd = config.devenv.root;
  };

  tasks."webshot:infraProdlikeDown" = {
    exec = "python3 container/compose/infra.py prodlike down";
    cwd = config.devenv.root;
  };

  tasks."webshot:prodlikeUp" = {
    exec = "python3 container/compose/infra.py prodlike up && python3 container/compose/webshotd.py prodlike up";
    cwd = config.devenv.root;
  };

  tasks."webshot:prodlikeDown" = {
    exec = "python3 container/compose/webshotd.py prodlike down && python3 container/compose/infra.py prodlike down";
    cwd = config.devenv.root;
  };

  tasks."webshot:prodlikeStatus" = {
    exec = "python3 container/compose/infra.py prodlike status && python3 container/compose/webshotd.py prodlike status";
    cwd = config.devenv.root;
  };

  tasks."webshot:prodlikeLogs" = {
    exec = "python3 container/compose/webshotd.py prodlike logs";
    cwd = config.devenv.root;
  };

  tasks."webshot:configureSan" =
    common.mkConfigureTask common.buildDirs.san common.clangdConfigs.san common.buildVariants.san;

  tasks."webshot:configureTidy" =
    common.mkConfigureTask common.buildDirs.tidy common.clangdConfigs.tidy common.buildVariants.tidy;

  tasks."webshot:configureCov" =
    common.mkConfigureTask common.buildDirs.cov common.clangdConfigs.cov common.buildVariants.cov;

  tasks."webshot:configureRelease" =
    common.mkConfigureTask common.buildDirs.release common.clangdConfigs.release common.buildVariants.release;

  tasks."webshot:buildSan" =
    common.mkBuildTask common.buildDirs.san common.clangdConfigs.san common.buildVariants.san;

  tasks."webshot:buildTidy" =
    common.mkBuildTask common.buildDirs.tidy common.clangdConfigs.tidy common.buildVariants.tidy;

  tasks."webshot:buildCov" =
    common.mkBuildTask common.buildDirs.cov common.clangdConfigs.cov common.buildVariants.cov;

  tasks."webshot:buildRelease" =
    common.mkBuildTask common.buildDirs.release common.clangdConfigs.release common.buildVariants.release;

  tasks."webshot:testSan" = {
    package = common.webshotTestSan;
    exec = "webshot_test_san";
  };

  tasks."webshot:testCov" = {
    package = common.webshotTestCov;
    exec = "webshot_test_cov";
  };
}
