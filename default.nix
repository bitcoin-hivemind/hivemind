with import <nixpkgs> {}; {
  hivemindEnv = stdenv.mkDerivation {
    name = "hivemind-env";
    buildInputs = [
      stdenv
      pkgconfig
      autoreconfHook
      openssl
      db48
      boost
      zlib
      miniupnpc
      protobuf
      libevent
      qt4
      qrencode
    ]
    ++ stdenv.lib.optionals stdenv.isLinux [ utillinux ];

    BOOST_LDFLAGS = "-L${boost}/lib";
  };
}
