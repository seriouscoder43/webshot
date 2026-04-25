let
  inherit
    (builtins)
    attrNames
    filter
    fetchTree
    fromJSON
    hasAttr
    listToAttrs
    map
    pathExists
    readFile
    ;

  nodeToValue = nodes: name: let
    node = nodes.${name};
    src = fetchTree node.locked;
    isNixpkgs =
      pathExists (src + "/lib/default.nix")
      && pathExists (src + "/pkgs/top-level/default.nix");
    lib = import (src + "/lib");
  in
    if isNixpkgs
    then
      src
      // {
        inherit lib;
        legacyPackages = lib.genAttrs lib.systems.flakeExposed (system: import src {inherit system;});
      }
    else src;

  mkInputs = lockFile: let
    lock = fromJSON (readFile lockFile);
    names = filter (name: name != lock.root) (attrNames lock.nodes);
  in
    listToAttrs (map (name: {
        inherit name;
        value = nodeToValue lock.nodes name;
      })
      names);

  callModule = {
    inputs,
    root,
    system,
    path,
  }:
    import "${root}/${path}" {
      inherit inputs;
      pkgs = inputs.nixpkgs.legacyPackages.${system};
      config.devenv.root = root;
    };
in {
  outputs = {
    lockFile,
    root,
  }: let
    inputs = mkInputs lockFile;
    systems = inputs.nixpkgs.lib.systems.flakeExposed;
    forEachSystem = inputs.nixpkgs.lib.genAttrs systems;
    packages = forEachSystem (system: let
      webshot = callModule {
        inherit inputs root system;
        path = "webshotd/devenv_module.nix";
      };
      packaging = callModule {
        inherit inputs root system;
        path = "devenv/outputs.nix";
      };
    in
      packaging.outputs.packaging
      // {
        default = webshot.outputs.webshot;
        webshot = webshot.outputs.webshot;
      });

    nixosModule = {
      pkgs,
      lib,
      ...
    }: {
      imports = [
        (import "${root}/packaging/nixos/webshot.nix")
      ];

      services.webshot.package = lib.mkDefault packages.${pkgs.system}.webshot;
    };
  in {
    inherit packages;

    nixosModules = {
      default = nixosModule;
      webshot = nixosModule;
    };
  };
}
