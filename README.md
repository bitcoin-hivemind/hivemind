Hivemind Core integration/staging tree
=====================================

[http://www.bitcoin-hivemind.com](http://www.bitcoin-hivemind.com)

What is Hivemind?
----------------

Hivemind is an ambitious modification of Bitcoin, which extends Bitcoin's abilities (to send an receive value-tokens) by adding the ability to create and participate in [markets for event derivatives ("prediction markets")](https://en.wikipedia.org/wiki/Prediction_market).

Although substantially more complex, Hivemind offers correspondingly substantial benefits, having potentially world-changing implications for science, taxation, corporate governance and politics. 

Hivemind includes two types of value-token.  One, "Bitcoin", is used for storing value, buying/selling in markets and paying transaction fees to miners. This coin will resemble Bitcoin-testnet-money (an "Altcoin") for the duration of pre-release testing, and will then be a [sidechain](http://www.blockstream.com/) of Bitcoin. Therefore, the network will "accept" Bitcoin (indeed, this will be the only currency the network accepts). The second (less important) token, "VoteCoin", is used to track a smaller group of special users and reward them for providing reports to the blockchain on the status of the bet-upon events.

For an immediately useable, binary version of the Hivemind Core software, see [https://github.com/bitcoin-hivemind/hivemind-binaries](https://github.com/bitcoin-hivemind/hivemind-binaries).

License
-------

Hivemind Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see http://opensource.org/licenses/MIT.


Current Install Instructions
---------------------------

To compile on Debian 64-bit Precise for Linux, Windows and Mac, see the Gitian build instructions in ```/doc```.

To compile on Ubuntu 14.04:

```
# Install Git
sudo apt-get install git

# Download Hivemind
git clone https://github.com/bitcoin-hivemind/hivemind

# (permission is required)
# Username:
XXXXXXX
# Personal Access Token:
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

# Download Build Essential
sudo apt-get install build-essential

# Download BerkeleyDB (note that Ubuntu's BDB 5.1 is not backwards compatible).
wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
echo '12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef  db-4.8.30.NC.tar.gz' | sha256sum -c
# db-4.8.30.NC.tar.gz: OK

# Install BerkelyDB
tar -xvf db-4.8.30.NC.tar.gz
cd db-4.8.30.NC/build_unix
mkdir -p build
cd build
BDB_PREFIX=$(pwd)/build
../dist/configure --disable-shared --enable-cxx --with-pic --prefix=$BDB_PREFIX
make install
cd ../..

# Install Remaining Dependencies
sudo apt-get install autoconf libboost-all-dev libssl-dev libtool libdb++-dev libprotobuf-dev protobuf-compiler libqt4-dev libqrencode-dev 


# Install Hivemindd (no GUI)
cd hivemind-cpp/src/
make hivemindd
make hivemind-cli

# # Install Hivemind
# cd hivemind-cpp
# ./autogen.sh
# ./configure --with-incompatible-bdb
# make
# make install # optional
```


<!--

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the Hivemind
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion (if they haven't already) on the
[mailing list](http://sourceforge.net/mailarchive/forum.php?forum_name=hivemind-development).

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see [doc/developer-notes.md](doc/developer-notes.md)) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/hivemind/hivemind/tags) are created
regularly to indicate new official, stable release versions of Hivemind.


Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

-->

Testing
-------

### Automated Testing

Developers are strongly encouraged to write unit tests for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run (assuming they weren't disabled in configure) with: `make check`

<!--
Every pull request is built for both Windows and Linux on a dedicated server,
and unit and sanity tests are automatically run. The binaries produced may be
used for manual QA testing — a link to them will appear in a comment on the
pull request posted by [HivemindPullTester](https://github.com/HivemindPullTester). See https://github.com/TheBlueMatt/test-scripts
for the build/test scripts.
-->

### Manual Quality Assurance (QA) Testing

Large changes should have a test plan, and should be tested by somebody other
than the developer who wrote the code.
See https://github.com/hivemind/QA/ for how to create a test plan.

<!--
Translations
------------

Changes to translations as well as new translations can be submitted to
[Hivemind Core's Transifex page](https://www.transifex.com/projects/p/hivemind/).

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.

Translators should also subscribe to the [mailing list](https://groups.google.com/forum/#!forum/hivemind-translators).

-->
