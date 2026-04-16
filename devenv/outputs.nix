{
  pkgs,
  config,
  inputs,
  ...
}: let
  ctx = import ./ctx.nix {inherit pkgs config inputs;};
in {
  outputs.packaging = {
    pgmigrate = ctx.drv.pgmigrate;
    uniAlgo = ctx.drv.unialgo;
    userverRelease = ctx.drv.userver;
    userverDebugAddrUb = ctx.drv.userverDbg;
    yandexTaxiTestsuite = ctx.drv.testsuite;
  };
}
