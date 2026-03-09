{
  pkgs,
  config,
  inputs,
  ...
}: let
  common = import ../devenv/lib.nix {inherit pkgs config inputs;};
  processCompose = common.pkgsWithOverlay."process-compose";

  modeConfigs = {
    dev = {
      buildDir = common.buildDirs.san;
      clangdConfig = common.clangdConfigs.san;
      buildVariant = common.buildVariants.san;
      infraMode = "dev";
      configVarsSource = "${config.devenv.root}/webshotd/config/config_vars.dev.yaml";
      processComposeSource = "${config.devenv.root}/process_compose/dev.yaml";
      containerNames = [
        "egress_proxy"
        "servicedb"
        "seaweedfs"
        "scalar"
        "reverse_proxy"
        "test_target"
      ];
    };

    prodlike = {
      buildDir = common.buildDirs.san;
      clangdConfig = common.clangdConfigs.san;
      buildVariant = common.buildVariants.san;
      infraMode = "prodlike";
      configVarsSource = "${config.devenv.root}/webshotd/config/config_vars.prodlike.yaml";
      processComposeSource = "${config.devenv.root}/process_compose/prodlike.yaml";
      containerNames = [
        "egress_proxy"
        "servicedb"
      ];
    };
  };

  runtimeLdLibraryPath = common.lib.makeLibraryPath common.testLibs;

  processComposeCtlArgs = ''
    -U
    -u "$state_dir/process-compose.sock"
    -L "$state_dir/process-compose-client.log"
  '';

  mkModeRuntimeScript = name: mode: let
    cfg = modeConfigs.${mode};
    containers = common.lib.concatStringsSep " " cfg.containerNames;
    quotedContainers = common.lib.escapeShellArg containers;
    buildTask = common.mkBuildTask cfg.buildDir cfg.clangdConfig cfg.buildVariant;
  in
    common.pkgsWithOverlay.writeShellScriptBin "webshot_${name}_${mode}" ''
      set -euo pipefail

      repo_root=${common.lib.escapeShellArg config.devenv.root}
      cd "$repo_root"
      state_dir="/tmp/webshot-$UID/${mode}"
      process_compose_file="$state_dir/process-compose.yaml"
      config_vars_file="$state_dir/config_vars.yaml"
      binary_path=${common.lib.escapeShellArg "${cfg.buildDir}/webshotd"}
      runtime_ld_library_path=${common.lib.escapeShellArg runtimeLdLibraryPath}
      config_vars_source=${common.lib.escapeShellArg cfg.configVarsSource}
      process_compose_source=${common.lib.escapeShellArg cfg.processComposeSource}
      containers=${quotedContainers}
      cpu_limit=""
      deploy_vcpu_limit="''${DEPLOY_VCPU_LIMIT-}"

      render_runtime_files() {
        mkdir -p "$state_dir"
        cp "$config_vars_source" "$config_vars_file"
        printf 'crawlerd_socket_path: "%s"\n' "$state_dir/crawlerd.sock" >> "$config_vars_file"
        sed \
          -e "s|@REPO_ROOT@|$repo_root|g" \
          -e "s|@STATE_DIR@|$state_dir|g" \
          -e "s|@WEBSHOTD_BIN@|$binary_path|g" \
          -e "s|@WEBSHOTD_RUNTIME_LD_LIBRARY_PATH@|$runtime_ld_library_path|g" \
          -e "s|@CPU_LIMIT@|$cpu_limit|g" \
          -e "s|@DEPLOY_VCPU_LIMIT@|$deploy_vcpu_limit|g" \
          "$process_compose_source" > "$process_compose_file"
      }

      process_compose() {
        process-compose \
          ${processComposeCtlArgs} \
          -f "$process_compose_file" \
          --disable-dotenv \
          "$@"
      }

      case "${name}" in
        build)
          ${buildTask.exec}
          ;;
        test)
          ${buildTask.exec}
          export LD_LIBRARY_PATH="$runtime_ld_library_path"
          cd "${cfg.buildDir}"
          ctest --progress --output-on-failure -V
          ;;
        up)
          ${buildTask.exec}
          python3 "$repo_root/container/compose/infra.py" ${cfg.infraMode} up
          cpu_limit="$(python3 "$repo_root/process_compose/resolve_cpu_limit.py")"
          render_runtime_files
          process_compose up -D
          process_compose project is-ready --wait
          ;;
        down)
          mkdir -p "$state_dir"
          render_runtime_files
          if [ -S "$state_dir/process-compose.sock" ]; then
            process_compose down || true
          fi
          python3 "$repo_root/container/compose/infra.py" ${cfg.infraMode} down
          rm -rf "$state_dir"
          ;;
        status)
          python3 "$repo_root/container/compose/infra.py" ${cfg.infraMode} status
          render_runtime_files
          if [ -S "$state_dir/process-compose.sock" ]; then
            process_compose project state
            process_compose process get crawlerd
            process_compose process get webshotd
          else
            echo "process-compose: not running"
          fi
          ;;
        logs)
          render_runtime_files
          cleanup() {
            jobs -p | xargs -r kill || true
          }
          trap cleanup EXIT INT TERM
          for container in $containers; do
            podman logs -f "$container" &
          done
          if [ -S "$state_dir/process-compose.sock" ]; then
            process_compose process logs -f crawlerd,webshotd &
          else
            echo "process-compose: not running"
          fi
          wait
          ;;
        *)
          echo "unsupported action: ${name}" >&2
          exit 2
          ;;
      esac
    '';
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
      processCompose
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

  tasks."webshot:devBuild" = {
    package = mkModeRuntimeScript "build" "dev";
    exec = "webshot_build_dev";
  };

  tasks."webshot:devUp" = {
    package = mkModeRuntimeScript "up" "dev";
    exec = "webshot_up_dev";
  };

  tasks."webshot:devDown" = {
    package = mkModeRuntimeScript "down" "dev";
    exec = "webshot_down_dev";
  };

  tasks."webshot:devStatus" = {
    package = mkModeRuntimeScript "status" "dev";
    exec = "webshot_status_dev";
  };

  tasks."webshot:devLogs" = {
    package = mkModeRuntimeScript "logs" "dev";
    exec = "webshot_logs_dev";
  };

  tasks."webshot:devTest" = {
    package = mkModeRuntimeScript "test" "dev";
    exec = "webshot_test_dev";
  };

  tasks."webshot:prodlikeBuild" = {
    package = mkModeRuntimeScript "build" "prodlike";
    exec = "webshot_build_prodlike";
  };

  tasks."webshot:prodlikeUp" = {
    package = mkModeRuntimeScript "up" "prodlike";
    exec = "webshot_up_prodlike";
  };

  tasks."webshot:prodlikeDown" = {
    package = mkModeRuntimeScript "down" "prodlike";
    exec = "webshot_down_prodlike";
  };

  tasks."webshot:prodlikeStatus" = {
    package = mkModeRuntimeScript "status" "prodlike";
    exec = "webshot_status_prodlike";
  };

  tasks."webshot:prodlikeLogs" = {
    package = mkModeRuntimeScript "logs" "prodlike";
    exec = "webshot_logs_prodlike";
  };
}
