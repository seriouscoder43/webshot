{
  config,
  lib,
  pkgs,
  ...
}: let
  cfg = config.services.webshot;
  configVarsFormat = pkgs.formats.yaml {};
  optionalValue = name: value:
    lib.optionalAttrs (value != null) {
      ${name} = value;
    };
  unitName = "webshot";
  unitFile = "${unitName}.service";
  configVars =
    optionalValue "pg_mode" cfg.pgMode
    // optionalValue "s3_mode" cfg.s3Mode
    // optionalValue "s3_bucket" cfg.s3Bucket
    // optionalValue "public_base_url" cfg.publicBaseUrl
    // lib.optionalAttrs (cfg.secdistPath != null) {
      secdist_path = "/run/credentials/${unitFile}/secdist.json";
    }
    // optionalValue "allowlist_only" cfg.allowlistOnly
    // optionalValue "https_only" cfg.httpsOnly
    // lib.optionalAttrs (cfg.pgMode == "external") (
      optionalValue "pg_capture_meta_db_dsn" cfg.pgCaptureMetaDbDsn
      // optionalValue "pg_shared_state_db_dsn" cfg.pgSharedStateDbDsn
    )
    // lib.optionalAttrs (cfg.s3Mode == "external") (
      optionalValue "s3_endpoint" cfg.s3Endpoint
      // optionalValue "s3_region" cfg.s3Region
      // optionalValue "s3_use_sts" cfg.s3UseSts
    )
    // lib.optionalAttrs (cfg.s3Mode == "external" && cfg.s3UseSts == true) (
      optionalValue "s3_credentials_endpoint" cfg.s3CredentialsEndpoint
    );

  ncfg = cfg.nginx;
  storagePrefix = "/webshot/";
  proto =
    if ncfg.forceSSL
    then "https"
    else "http";
  rateLimitZone =
    lib.optionalString ncfg.rateLimit.enable
    "limit_req_zone $binary_remote_addr zone=webshot:10m rate=${ncfg.rateLimit.rate};";
  rateLimitDirective =
    lib.optionalString ncfg.rateLimit.enable
    "limit_req zone=webshot burst=${toString ncfg.rateLimit.burst} nodelay;";

  pkg =
    if cfg.package != null && cfg.s3Mode == "local"
    then
      pkgs.symlinkJoin {
        name = "webshot";
        paths = [cfg.package];
        postBuild = ''
          rm $out/lib/systemd/system/webshot.service
          substitute ${cfg.package}/lib/systemd/system/webshot.service \
            $out/lib/systemd/system/webshot.service \
            --replace-fail \
              '--config-vars-source /etc/webshot/config_vars.yaml' \
              '--config-vars-source /etc/webshot/config_vars.yaml --seaweedfs-s3-config ''${CREDENTIALS_DIRECTORY}/seaweedfs_s3_config.json'
        '';
      }
    else cfg.package;
in {
  options.services.webshot = {
    enable = lib.mkEnableOption "webshot";

    # The shipped systemd units use DynamicUser=yes, which is incompatible with
    # persisting the embedded Postgres data dir across restarts (the numeric UID
    # changes and Postgres requires strict data dir ownership).
    #
    # The NixOS module pins execution to a stable system user.
    user = lib.mkOption {
      type = lib.types.str;
      default = "webshot";
      description = "System user account used to run the webshot systemd service.";
    };

    group = lib.mkOption {
      type = lib.types.str;
      default = "webshot";
      description = "System group used to run the webshot systemd service.";
    };

    package = lib.mkOption {
      type = lib.types.nullOr lib.types.package;
      default = null;
      description = ''
        The webshot package that provides the service unit.
      '';
    };

    pgMode = lib.mkOption {
      type = lib.types.nullOr (lib.types.enum ["local" "external"]);
      default = null;
      description = "Whether webshot manages PostgreSQL locally or connects to an external PostgreSQL.";
    };

    pgCaptureMetaDbDsn = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "External PostgreSQL DSN for the capture metadata database.";
    };

    pgSharedStateDbDsn = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "External PostgreSQL DSN for the shared state database.";
    };

    s3Mode = lib.mkOption {
      type = lib.types.nullOr (lib.types.enum ["local" "external"]);
      default = null;
      description = "Whether webshot stores captures in local SeaweedFS or external S3.";
    };

    s3Bucket = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "S3 bucket name used for capture objects.";
    };

    s3Endpoint = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "External S3 endpoint URL.";
    };

    s3Region = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "External S3 region.";
    };

    s3UseSts = lib.mkOption {
      type = lib.types.nullOr lib.types.bool;
      default = null;
      description = "Whether to fetch temporary S3 credentials from STS in external S3 mode.";
    };

    s3CredentialsEndpoint = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "STS endpoint used when s3UseSts is enabled.";
    };

    publicBaseUrl = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "Externally visible base URL used to build direct capture storage URLs.";
    };

    secdistPath = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "Path to the webshotd secdist JSON file.";
    };

    seaweedfsS3ConfigPath = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      description = "Path to the SeaweedFS S3 config JSON file; required when s3Mode is local.";
    };

    allowlistOnly = lib.mkOption {
      type = lib.types.nullOr lib.types.bool;
      default = null;
      description = "Whether captures are restricted to allowlisted links.";
    };

    httpsOnly = lib.mkOption {
      type = lib.types.nullOr lib.types.bool;
      default = null;
      description = "Whether capture links must use HTTPS.";
    };

    nginx = lib.mkOption {
      type = lib.types.submodule {
        options = {
          hostName = lib.mkOption {
            type = lib.types.nullOr lib.types.str;
            default = null;
            description = "Hostname for the managed Nginx virtual host.";
          };
          enableACME = lib.mkOption {
            type = lib.types.bool;
            default = true;
            description = "Whether to enable ACME, Let's Encrypt, for the managed vhost.";
          };
          forceSSL = lib.mkOption {
            type = lib.types.bool;
            default = true;
            description = "Whether to force SSL, HTTP to HTTPS redirect, on the managed vhost.";
          };
          openFirewall = lib.mkOption {
            type = lib.types.bool;
            default = true;
            description = "Whether to open firewall ports for the managed Nginx; 80 always, 443 when enableACME or forceSSL is enabled.";
          };
          rateLimit = lib.mkOption {
            type = lib.types.submodule {
              options = {
                enable = lib.mkOption {
                  type = lib.types.bool;
                  default = true;
                  description = "Whether to enable rate limiting on the managed vhost.";
                };
                rate = lib.mkOption {
                  type = lib.types.str;
                  default = "1r/s";
                  description = "Rate limit for all proxied endpoints.";
                };
                burst = lib.mkOption {
                  type = lib.types.ints.unsigned;
                  default = 15;
                  description = "Burst allowance for the rate limit.";
                };
              };
            };
            default = {};
            description = "Rate limiting configuration for the managed vhost.";
          };
        };
      };
      default = {};
      description = "Managed Nginx configuration.";
    };
  };

  config = lib.mkIf cfg.enable {
    assertions = [
      {
        assertion = ncfg.hostName != null;
        message = "services.webshot.nginx.hostName is required when services.webshot.enable is true.";
      }
    ];

    users.groups = lib.mkIf (pkg != null) {
      ${cfg.group} = {};
    };
    users.users = lib.mkIf (pkg != null) {
      ${cfg.user} = {
        isSystemUser = true;
        group = cfg.group;
      };
    };

    systemd.packages = lib.optional (pkg != null) pkg;

    environment.etc."webshot/config_vars.yaml".source =
      configVarsFormat.generate "webshot-config-vars.yaml" configVars;

    systemd.targets.multi-user.wants = lib.optional (pkg != null) unitFile;

    systemd.services.${unitName} = lib.mkIf (pkg != null) {
      serviceConfig = {
        DynamicUser = lib.mkForce false;
        User = cfg.user;
        Group = cfg.group;
        RuntimeDirectory = "webshot";

        LoadCredential =
          lib.optional (cfg.secdistPath != null) "secdist.json:${cfg.secdistPath}"
          ++ lib.optional (cfg.s3Mode == "local" && cfg.seaweedfsS3ConfigPath != null)
          "seaweedfs_s3_config.json:${cfg.seaweedfsS3ConfigPath}";
      };
    };

    services.nginx = {
      enable = true;
      recommendedProxySettings = lib.mkDefault true;
      commonHttpConfig = rateLimitZone;
      virtualHosts =
        if ncfg.hostName == null
        then {}
        else {
          ${ncfg.hostName} = {
            enableACME = ncfg.enableACME;
            forceSSL = ncfg.forceSSL;
            extraConfig = ''
              # Convert nginx-generated rate limit responses into the JSON error envelope
              # expected by the web UI client-side templates.
              error_page 429 = @webshot_rate_limited_429;
              error_page 503 = @webshot_rate_limited_503;

              location @webshot_rate_limited_429 {
                internal;
                default_type application/json;
                return 429 '{"error":{"message":"client IP rate limited"}}';
              }

              location @webshot_rate_limited_503 {
                internal;
                default_type application/json;
                return 503 '{"error":{"message":"client IP rate limited"}}';
              }
            '';
            locations =
              {
                "= /v1/capture" = {
                  proxyPass = "http://127.0.0.1:8080";
                  extraConfig = rateLimitDirective;
                };
                "/" = {
                  proxyPass = "http://127.0.0.1:8080";
                  extraConfig = rateLimitDirective;
                };
              }
              // lib.optionalAttrs (cfg.s3Mode == "local") {
                "^~ ${storagePrefix}" = {
                  proxyPass = "http://127.0.0.1:8333";
                  extraConfig = rateLimitDirective;
                };
              };
          };
        };
    };

    networking.firewall.allowedTCPPorts = lib.mkIf ncfg.openFirewall (
      [80] ++ lib.optional (ncfg.enableACME || ncfg.forceSSL) 443
    );

    services.webshot.publicBaseUrl =
      lib.mkIf (cfg.s3Mode == "local" && ncfg.hostName != null)
      (lib.mkDefault "${proto}://${ncfg.hostName}${storagePrefix}");
  };
}
