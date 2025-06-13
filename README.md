# MMORPG Server Framework

A lightweight and educational MMORPG server-side framework written in modern C++.  
Designed for learning, experimentation, and prototyping of game server architectures.

---

## ✨ Features

- 🧩 **Entity-Component System (ECS)** for flexible game object modeling
- ⚡ **High-performance networking** using Linux `epoll` and non-blocking sockets
- 📜 **Lua scripting** for game logic extension and rapid iteration
- 🔌 Modular server core with decoupled components
- 🗺️ Base framework for building MMO-style simulations

---

## 🧰 Tech Stack

- C++17
- `epoll`, non-blocking IO
- Lua scripting integration
- STL, custom ECS implementation
- CMake for build system

---

## 🚀 Getting Started

### Prerequisites

- Linux system (recommended Ubuntu)
- C++17 compiler (e.g., `g++`)
- `cmake` (version ≥ 3.12)
- Lua library and headers

### Build & Run

```bash
git clone https://github.com/luyuancpp/mmorpg.git
cd mmorpg
mkdir build && cd build
cmake ..
make
./mmorpg
```

## 📝 License

This project is licensed under the **MIT License** – see the [LICENSE](./LICENSE) file for details.
