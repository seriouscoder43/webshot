{
  nix,
  drv,
}: rec {
  buildNative = with nix; [
    cmake
    ninja
    ada
    pkg-config
    ccache
    drv.boostSml
    boost183
    libarchive
    libarchive.dev
    libseccomp.dev
    openssl.dev
    jemalloc
    mold
  ];

  devTools = with nix; [
    rsync
  ];

  cmakePrefix = with nix; [
    drv.boostSml
    boost183.dev
    fmt.dev
    zstd.dev
    cctz
    yaml-cpp
  ];

  shellRuntime = with nix; [
    bash
    coreutils
    gnused
  ];

  browserSandboxRuntime =
    shellRuntime
    ++ (with nix; [
      ungoogled-chromium
      socat
      util-linux
    ]);

  crawlerRuntime =
    browserSandboxRuntime
    ++ (with nix; [
      bubblewrap
    ]);

  runtimeTools = crawlerRuntime;
  browserSandboxClosure = nix.closureInfo {rootPaths = browserSandboxRuntime;};
  browserSandboxFontconfigFile = "${nix.fontconfig.out}/etc/fonts/fonts.conf";
  browserSandboxPath = nix.lib.makeBinPath browserSandboxRuntime;

  runtime = with nix;
    [
      postgresql_18
      nginx
      s6
      util-linux
      openssl
      nssTools.tools
    ]
    ++ [drv.seaweedfs];

  systemdRuntime = runtime ++ runtimeTools ++ [drv.pgmigrate drv.repoPy];
  systemdRuntimePath = nix.lib.makeBinPath systemdRuntime;

  sharedLibs = with nix; [
    drv.unialgo
    libarchive
    libseccomp
  ];

  userverLibs = drv.userverLibs;
  userver = userverLibs ++ [drv.repoPy];
  buildInputsFor = userverPkg: [userverPkg] ++ sharedLibs ++ userver;
  rpathLibsFor = userverPkg: [userverPkg] ++ sharedLibs ++ userver ++ [nix.stdenv.cc.cc.lib];
  testLibs = userver ++ [nix.libarchive nix.libseccomp nix.stdenv.cc.cc];
}
