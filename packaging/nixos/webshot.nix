{
  config,
  lib,
  ...
}: let
  cfg = config.services.webshot;
in {
  options.services.webshot = {
    enable = lib.mkEnableOption "webshot";

    package = lib.mkOption {
      type = lib.types.nullOr lib.types.package;
      default = null;
      description = ''
        The webshot package that provides lib/systemd/system/webshot.service.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    assertions = [
      {
        assertion = cfg.package != null;
        message = "services.webshot.package must be set when services.webshot.enable is true.";
      }
    ];

    systemd.packages = lib.optional (cfg.package != null) cfg.package;

    systemd.targets.multi-user.wants = lib.optional (cfg.package != null) "webshot.service";
  };
}
