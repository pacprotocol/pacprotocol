pacprotocol staging tree 0.17
=============================


What is pacprotocol?
--------------------

pacprotocol is an experimental digital currency that enables anonymous, instant
payments to anyone, anywhere in the world. pacprotocol uses peer-to-peer technology
to operate with no central authority: managing transactions and issuing money
are carried out collectively by the network. pacprotocol is the name of the open
source software which enables the use of this currency; a product of pacglobal.

For more information, as well as an immediately useable, binary version of
the pacprotocol software, see https://pacglobal.io/


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

