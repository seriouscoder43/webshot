{
  pkgs,
  src,
}:
pkgs.stdenv.mkDerivation {
  name = "nikolays-pgque";
  inherit src;
  meta = {
    description = "PgQue - Zero-bloat Postgres queue. One SQL file to install, pg_cron to tick.";
    homepage = "https://github.com/NikolayS/pgque";
    license = pkgs.lib.licenses.asl20;
    platforms = pkgs.lib.platforms.all;
  };
  dontBuild = true;
  installPhase = ''
    runHook preInstall
    mkdir -p $out/share/pgque
    cp sql/*.sql $out/share/pgque/
    runHook postInstall
  '';
}
