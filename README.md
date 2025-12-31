# SwiftCom

## **A decentralized communication application where you host servers directly on your computer. No central server controls your data.**

## SwiftCom is built on my custom **[SwiftNet networking library](https://github.com/Morcules/SwiftNet)** for full control over performance, usability, and privacy. The GUI is powered by **wxWidgets** for cross-platform support.

## Any contributions or help are very welcome!

## Features

- Fully decentralized: Peer-to-peer servers hosted on your machine
- No central authority: Complete data ownership and privacy
- Cross-platform GUI with (via wxWidgets)
- Custom low-level networking for optimal performance

## Quick Start
- Install vcpkg
- vcpkg install morcules-swiftnet && vcpkg install wxwidgets && vcpkg install sqlite3

### Building

```bash
git clone https://github.com/Morcules/SwiftCom
cd SwiftCom
cd build
./compile.sh
