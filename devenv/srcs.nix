{
  inputs,
  lib,
  lockFile,
}: let
  lock = builtins.fromJSON (builtins.readFile lockFile);

  validate = name: let
    node = lock.nodes.${name};
    locked = node.locked or null;
    kind =
      if locked == null
      then null
      else locked.type or null;
  in
    if name == lock.root || locked == null || kind == "path"
    then []
    else if kind == "github"
    then lib.optional (!(builtins.hasAttr "narHash" locked)) "${name} (github)"
    else [
      "${name} (${
        if kind == null
        then "missing type"
        else kind
      })"
    ];

  invalid = lib.concatMap validate (builtins.attrNames lock.nodes);
in
  assert (
    if invalid == []
    then true
    else
      throw ''
        devenv.lock is missing locked.narHash for remote inputs or contains unsupported remote input types: ${lib.concatStringsSep ", " invalid}
        Update devenv.lock to include locked.narHash for each remote node.
      ''
  ); {
    pgmigrate = inputs.pgmigrateSrc;
    testsuite = inputs.yandexTaxiTestsuiteSrc;
    unialgo = inputs.uniAlgoSrc;
    userver = inputs.userverSrc;
  }
