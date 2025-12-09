{
  description = "Parametrized userver builds (Release and Debug with addr+ub sanitizers)";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    systems = ["x86_64-linux"];

    forAllSystems = f:
      nixpkgs.lib.genAttrs systems
      (system: let
        pkgs = import nixpkgs {inherit system;};

        llvm = pkgs.llvmPackages_21;
        stdenv = llvm.stdenv;

        chaoticPython = pkgs.python3.withPackages (ps: [
          ps.jinja2
          ps.pyyaml
          ps.pydantic
        ]);

        userverDeps = import ./deps.nix {
          inherit pkgs chaoticPython;
        };

        userverSrc = pkgs.fetchFromGitHub {
          owner = "userver-framework";
          repo = "userver";
          rev = "51022a3ff78b64bcd26347446e02953dd1e20248";
          hash = "sha256-H78Rfy6GnL0oCgXjlVPeRsWT0n1L8ZUmM8rEX9s6RQE=";
        };

        baseCmakeFlags = [
          "-DUSERVER_DOWNLOAD_PACKAGES=OFF"
          "-DUSERVER_USE_STATIC_LIBS=OFF"
          "-DUSERVER_CHECK_PACKAGE_VERSIONS=0"
          "-DUSERVER_FEATURE_POSTGRESQL=ON"
          "-DUSERVER_FEATURE_S3API=ON"
          "-DUSERVER_FEATURE_PATCH_LIBPQ=OFF"

          "-DUSERVER_FEATURE_TESTSUITE=ON"
          "-DUSERVER_TESTSUITE_USE_VENV=OFF"

          "-DUSERVER_FEATURE_MONGODB=OFF"
          "-DUSERVER_FEATURE_REDIS=OFF"
          "-DUSERVER_FEATURE_GRPC=OFF"
          "-DUSERVER_FEATURE_CLICKHOUSE=OFF"
          "-DUSERVER_FEATURE_KAFKA=OFF"
          "-DUSERVER_FEATURE_RABBITMQ=OFF"
          "-DUSERVER_FEATURE_MYSQL=OFF"
          "-DUSERVER_FEATURE_ROCKS=OFF"
          "-DUSERVER_FEATURE_YDB=OFF"
          "-DUSERVER_FEATURE_OTLP=OFF"
          "-DUSERVER_FEATURE_SQLITE=OFF"
          "-DUSERVER_FEATURE_ODBC=OFF"
        ];

        mkUserver = {
          buildType,
          sanitize ? "",
        }:
          stdenv.mkDerivation {
            pname = "userver";
            version = "dev";

            src = userverSrc;

            patches = [
              ../../patches/userver-testsuite-no-venv.patch
              ../../patches/userver-chaotic-no-venv.patch
              ../../patches/userver-sql-no-venv.patch
              ../../patches/userver-openssl-imported-targets.patch
              ../../patches/userver-cctz-cmake-version.patch
              ../../patches/userver-stdlib-cxx17-variant.patch
              ../../patches/userver-stacktrace-basic-fallback.patch
            ];

            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              python3
              pkg-config
              llvm.clang
            ];

            buildInputs = userverDeps;

            cmakeFlags =
              baseCmakeFlags
              ++ [
                "-DUSERVER_INSTALL=ON"
                "-DUSERVER_CHAOTIC_USE_VENV=OFF"
                "-DUSERVER_CHAOTIC_PYTHON_BINARY=${chaoticPython}/bin/python3"
                "-DUSERVER_SQL_USE_VENV=OFF"
                "-DUSERVER_SQL_PYTHON_BINARY=${chaoticPython}/bin/python3"
                "-DIconv_IS_BUILT_IN=ON"
                "-DCMAKE_BUILD_TYPE=${buildType}"
                "-DOPENSSL_ROOT_DIR=${pkgs.openssl.dev}"
                "-DOPENSSL_INCLUDE_DIR=${pkgs.openssl.dev}/include"
                "-DOPENSSL_CRYPTO_LIBRARY=${pkgs.openssl}/lib/libcrypto.so"
                "-DOPENSSL_SSL_LIBRARY=${pkgs.openssl}/lib/libssl.so"
                "-Dfmt_DIR=${pkgs.fmt.dev}/lib/cmake/fmt"
                "-Dcctz_DIR=${pkgs.cctz}/lib/cmake/cctz"
              ]
              ++ (
                if sanitize == ""
                then []
                else ["-DUSERVER_SANITIZE=${sanitize}"]
              );
            hardeningDisable = ["all"];
          };

        userver-release = mkUserver {
          buildType = "Release";
          sanitize = "";
        };

        userver-debug-addr-ub = mkUserver {
          buildType = "Debug";
          sanitize = "addr;ub";
        };
      in {
        inherit userver-release userver-debug-addr-ub;
        default = userver-release;
      });
  in {
    packages = forAllSystems (p: p);
  };
}
