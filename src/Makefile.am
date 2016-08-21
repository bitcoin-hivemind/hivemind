DIST_SUBDIRS = fdlibm linalg secp256k1
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS)


if EMBEDDED_LEVELDB
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/include
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/helpers/memenv
LIBLEVELDB += $(builddir)/leveldb/libleveldb.a
LIBMEMENV  += $(builddir)/leveldb/libmemenv.a

# NOTE: This dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the LevelDB build system in a race
$(LIBLEVELDB): $(LIBMEMENV)

$(LIBLEVELDB) $(LIBMEMENV):
	@echo "Building LevelDB ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" PLATFORM=$(TARGET_OS) AR="$(AR)" $(LEVELDB_TARGET_FLAGS) \
          OPT="$(CXXFLAGS) $(CPPFLAGS)"
endif

HIVEMIND_CONFIG_INCLUDES=-I$(builddir)/config
HIVEMIND_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

HIVEMIND_INCLUDES += -I$(srcdir)/fdlibm/include -I$(srcdir)/linalg/include -I$(srcdir)/secp256k1/include

LIBHIVEMIND_SERVER=libhivemind_server.a
LIBHIVEMIND_WALLET=libhivemind_wallet.a
LIBHIVEMIND_COMMON=libhivemind_common.a
LIBHIVEMIND_CLI=libhivemind_cli.a
LIBHIVEMIND_UTIL=libhivemind_util.a
LIBHIVEMIND_CRYPTO=crypto/libhivemind_crypto.a
LIBHIVEMIND_UNIVALUE=univalue/libhivemind_univalue.a
LIBHIVEMINDQT=qt/libhivemindqt.a
LIBFDLIBM=fdlibm/libfdlibm.la
LIBLINALG=linalg/liblinalg.la
LIBSECP256K1=secp256k1/libsecp256k1.la

$(LIBFDLIBM): $(wildcard fdlibm/src/*) $(wildcard fdlibm/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

$(LIBLINALG): $(wildcard linalg/src/*) $(wildcard lingalg/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)


# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES = \
  crypto/libhivemind_crypto.a \
  libhivemind_util.a \
  libhivemind_common.a \
  univalue/libhivemind_univalue.a \
  libhivemind_server.a \
  libhivemind_cli.a
if ENABLE_WALLET
HIVEMIND_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libhivemind_wallet.a
endif

if BUILD_HIVEMIND_LIBS
lib_LTLIBRARIES = libhivemindconsensus.la
LIBHIVEMIND_CONSENSUS=libhivemindconsensus.la
else
LIBHIVEMIND_CONSENSUS=
endif

bin_PROGRAMS =
TESTS =

if BUILD_HIVEMINDD
  bin_PROGRAMS += hivemindd
endif

if BUILD_HIVEMIND_UTILS
  bin_PROGRAMS += hivemind-cli hivemind-tx
endif

.PHONY: FORCE
# hivemind core #
HIVEMIND_CORE_H = \
  addrman.h \
  alert.h \
  allocators.h \
  amount.h \
  arith_uint256.h \
  base58.h \
  bloom.h \
  chain.h \
  chainparamsbase.h \
  chainparams.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compressor.h \
  core_io.h \
  crypter.h \
  db.h \
  eccryptoverify.h \
  ecwrapper.h \
  hash.h \
  init.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  netbase.h \
  net.h \
  noui.h \
  pow.h \
  primitives/block.h \
  primitives/market.h \
  primitives/transaction.h \
  protocol.h \
  pubkey.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  script/interpreter.h \
  script/script_error.h \
  script/script.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  serialize.h \
  streams.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utilstrencodings.h \
  utiltime.h \
  version.h \
  walletdb.h \
  wallet.h \
  wallet_ismine.h \
  compat/sanity.h

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libhivemind_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between hivemindd and hivemind-qt
libhivemind_server_a_CPPFLAGS = $(HIVEMIND_INCLUDES) $(MINIUPNPC_CPPFLAGS)
libhivemind_server_a_SOURCES = \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  script/sigcache.cpp \
  timedata.cpp \
  txdb.cpp \
  txmempool.cpp \
  $(JSON_H) \
  $(HIVEMIND_CORE_H)

# wallet: shared between hivemindd and hivemind-qt, but only linked
# when wallet enabled
libhivemind_wallet_a_CPPFLAGS = $(HIVEMIND_INCLUDES)
libhivemind_wallet_a_SOURCES = \
  db.cpp \
  crypter.cpp \
  rpcdump.cpp \
  rpcwallet.cpp \
  wallet.cpp \
  wallet_ismine.cpp \
  walletdb.cpp \
  $(HIVEMIND_CORE_H)

# crypto primitives library
crypto_libhivemind_crypto_a_CPPFLAGS = $(HIVEMIND_CONFIG_INCLUDES)
crypto_libhivemind_crypto_a_SOURCES = \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/common.h \
  crypto/sha256.h \
  crypto/sha512.h \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.h \
  crypto/sha1.h \
  crypto/ripemd160.h

# univalue JSON library
univalue_libhivemind_univalue_a_SOURCES = \
  univalue/univalue.cpp \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp \
  univalue/univalue_escapes.h \
  univalue/univalue.h

# common: shared between hivemindd, and hivemind-qt and non-server tools
libhivemind_common_a_CPPFLAGS = $(HIVEMIND_INCLUDES)
libhivemind_common_a_SOURCES = \
  allocators.cpp \
  arith_uint256.cpp \
  amount.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  primitives/block.cpp \
  primitives/market.cpp \
  primitives/transaction.cpp \
  core_read.cpp \
  core_write.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  protocol.cpp \
  pubkey.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/sign.cpp \
  script/standard.cpp \
  script/script_error.cpp \
  $(HIVEMIND_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libhivemind_util_a_CPPFLAGS = $(HIVEMIND_INCLUDES)
libhivemind_util_a_SOURCES = \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  random.cpp \
  rpcprotocol.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(HIVEMIND_CORE_H)

if GLIBC_BACK_COMPAT
libhivemind_util_a_SOURCES += compat/glibc_compat.cpp
libhivemind_util_a_SOURCES += compat/glibcxx_compat.cpp
endif

# cli: shared between hivemind-cli and hivemind-qt
libhivemind_cli_a_CPPFLAGS = $(HIVEMIND_INCLUDES)
libhivemind_cli_a_SOURCES = \
  rpcclient.cpp \
  $(HIVEMIND_CORE_H)

nodist_libhivemind_util_a_SOURCES = $(srcdir)/obj/build.h
#

# hivemindd binary #
hivemindd_LDADD = \
  $(LIBHIVEMIND_SERVER) \
  $(LIBHIVEMIND_COMMON) \
  $(LIBHIVEMIND_UNIVALUE) \
  $(LIBHIVEMIND_UTIL) \
  $(LIBHIVEMIND_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBFDLIBM) \
  $(LIBLINALG) \
  $(LIBSECP256K1)

if ENABLE_WALLET
hivemindd_LDADD += libhivemind_wallet.a
endif
hivemindd_SOURCES = hivemindd.cpp
#

if TARGET_WINDOWS
hivemindd_SOURCES += hivemindd-res.rc
endif

hivemindd_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS)
hivemindd_CPPFLAGS = $(HIVEMIND_INCLUDES)
hivemindd_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

# hivemind-cli binary #
hivemind_cli_LDADD = \
  $(LIBHIVEMIND_CLI) \
  $(LIBHIVEMIND_UTIL) \
  $(BOOST_LIBS) \
  $(SSL_LIBS) \
  $(CRYPTO_LIBS)

hivemind_cli_SOURCES = \
  hivemind-cli.cpp

hivemind_cli_CPPFLAGS = $(HIVEMIND_INCLUDES)
#

# hivemind-tx binary #
hivemind_tx_LDADD = \
  $(LIBHIVEMIND_UNIVALUE) \
  $(LIBHIVEMIND_COMMON) \
  $(LIBHIVEMIND_UTIL) \
  $(LIBHIVEMIND_CRYPTO) \
  $(LIBFDLIBM) \
  $(LIBLINALG) \
  $(LIBSECP256K1) \
  $(BOOST_LIBS) \
  $(CRYPTO_LIBS)

hivemind_tx_SOURCES = hivemind-tx.cpp
hivemind_tx_CPPFLAGS = $(HIVEMIND_INCLUDES)
#
hivemind_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
hivemind_cli_SOURCES += hivemind-cli-res.rc
endif
hivemind_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if BUILD_HIVEMIND_LIBS
include_HEADERS = script/hivemindconsensus.h
libhivemindconsensus_la_SOURCES = \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  primitives/market.cpp \
  primitives/transaction.cpp \
  pubkey.cpp \
  script/hivemindconsensus.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libhivemindconsensus_la_SOURCES += compat/glibc_compat.cpp
  libhivemindconsensus_la_SOURCES += compat/glibcxx_compat.cpp
endif

libhivemindconsensus_la_LDFLAGS = -no-undefined $(RELDFLAGS)
libhivemindconsensus_la_LIBADD = $(BOOST_LIBS) $(CRYPTO_LIBS)
libhivemindconsensus_la_CPPFLAGS = $(CRYPTO_CFLAGS) -I$(builddir)/obj -DBUILD_HIVEMIND_INTERNAL
if USE_LIBSECP256K1
libhivemindconsensus_la_LIBADD += secp256k1/libsecp256k1.la
endif
libhivemindconsensus_la_LIBADD += linalg/liblinalg.la
libhivemindconsensus_la_LIBADD += fdlibm/libfdlibm.la
endif

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	-$(MAKE) -C fdlibm clean
	-$(MAKE) -C linalg clean
	-$(MAKE) -C secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
