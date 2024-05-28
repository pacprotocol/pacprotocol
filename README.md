PAC Protocol staging tree 0.17
=============================


What is PAC Protocol?
--------------------

PAC Protocol is built on the foundation of a first-of-its-kind blockchain technology utilizing both proof-of-stake (POSv3, economically friendly) and deterministic Masternodes to offer a large globally decentralized network, proven to boast scalability with over 19,000 Masternodes worldwide and growing. This creates the digital architecture required for decentralized data storage, content management, and more using IPFS software.

For more information, as well as an immediately useable, binary version of
the pacprotocol software, see https://pacprotocol.com


How do I build the software?
----------------------------

The most troublefree and reproducable method of building the repository is via
the depends method:

    git clone https://github.com/pacprotocol/pacprotocol
    cd pacprotocol/depends
    make HOST=x86_64-linux-gnu
    cd ..
    ./autogen.sh
    CONFIG_SITE=$PWD/depends/x86_64-linux-gnu/share/config.site ./configure
    make


License
-------

pacprotocol is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is meant to be stable. Development is normally done in separate branches.
[Tags](https://github.com/pacprotocol/pacprotocol/tags) are created to indicate new official,
stable release versions of pacprotocol.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

Explorer
--------
Please use following explorers of PAC Protocol chain

https://chainz.cryptoid.info/pac/

https://www.coinexplorer.net/PAC

Neither https://explorer.pacglobal.io/ nor https://pacscan.io/ are providing the service. 
