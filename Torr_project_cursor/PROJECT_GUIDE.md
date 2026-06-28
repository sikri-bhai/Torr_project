# Torr_project — The Complete Guide

A friendly, from-scratch walkthrough of how this peer-to-peer file-sharing
system works. Written for someone who has never seen the code before.

> If you can read this guide cover-to-cover and run the project, you
> understand BitTorrent at a conceptual level — most of the same ideas
> are used by the real BitTorrent protocol, just with more bells.

---

## Table of contents

1. [What this project actually is](#1-what-this-project-actually-is)
2. [Why peer-to-peer? The problem we're solving](#2-why-peer-to-peer-the-problem-were-solving)
3. [The 60-second mental model](#3-the-60-second-mental-model)
4. [Words you must know (glossary up front)](#4-words-you-must-know-glossary-up-front)
5. [The two programs we built](#5-the-two-programs-we-built)
6. [The wire protocol — how programs talk](#6-the-wire-protocol--how-programs-talk)
7. [Project structure — every file in plain English](#7-project-structure--every-file-in-plain-english)
8. [Configuration knobs](#8-configuration-knobs)
9. [How the program thinks in parallel (threading)](#9-how-the-program-thinks-in-parallel-threading)
10. [Three complete walkthroughs](#10-three-complete-walkthroughs)
11. [What lives in memory — the data structures](#11-what-lives-in-memory--the-data-structures)
12. [The journey of one piece](#12-the-journey-of-one-piece)
13. [How hashing & verification keeps you honest](#13-how-hashing--verification-keeps-you-honest)
14. [How to build it](#14-how-to-build-it)
15. [How to run it](#15-how-to-run-it)
16. [Common problems and what they mean](#16-common-problems-and-what-they-mean)
17. [A real bug we hit and fixed](#17-a-real-bug-we-hit-and-fixed)
18. [Things to improve next](#18-things-to-improve-next)
19. [Cheat sheet](#19-cheat-sheet)

---

## 1. What this project actually is

Imagine you want to share a movie with 100 friends. There are two ways:

**Way 1 — One central server**: You upload the movie to a website. All
100 friends download it from the website. The website has to send the
movie 100 times. Slow, expensive, and if the website goes down, nobody
gets the movie.

**Way 2 — Peer to peer**: You give the first friend the movie. Now both
you and that friend can give it to the next two friends. Now four people
have it. Then eight. The more people who join, the faster everyone gets
the file. No single server is overloaded, and even if one person leaves,
others still have copies.

This project is **Way 2**. It's a small BitTorrent-style file sharing
system in C++. Anybody on the network can join, share a file, and have
others download it directly from them — chunk by chunk, in parallel.

The whole thing fits in roughly 2,000 lines of C++.

---

## 2. Why peer-to-peer? The problem we're solving

A normal "client-server" setup (one big server, many clients) has three
weaknesses:

| Weakness | What goes wrong |
|---|---|
| **Bottleneck** | Server's upload speed limits everyone |
| **Cost** | All bandwidth gets paid for by whoever runs the server |
| **Single point of failure** | If the server dies, nobody can download |

Peer-to-peer flips this. Everybody contributes a little upload, so the
total upload capacity grows with the number of users — exactly when you
need it most.

The classic insight from BitTorrent: **split the file into small pieces,
let people exchange pieces directly, and the swarm gets faster as it
grows**.

---

## 3. The 60-second mental model

There are exactly **two programs** in this system:

### `tracker.exe` — the phonebook

A small server that runs on one machine (usually a fixed one everyone
knows the address of). It does **NOT** store any files. All it does is
remember:

- Who is online (list of peers)
- Which peers have which files
- Which exact pieces of which file each peer has

When you want to download a file, you ask the tracker:
> "Hey, who has `movie.mp4`?"

The tracker replies:
> "Alice has all of it. Bob has pieces 0-12. Carol has 50-99."

That's it. The tracker is just a coordinator.

### `peer.exe` — does everything else

A program that every user runs on their own machine. It plays **two
roles at the same time**:

- **Seeder** — listens on a port, hands out pieces of any file it has,
  to anyone who asks.
- **Leecher** — downloads pieces of files it wants, from anyone the
  tracker says has them.

A leecher who finishes downloading a file becomes a full seeder for
that file. A leecher who has downloaded *some* pieces is already a
partial seeder — they can hand out the pieces they have, even before
they finish their own download. That's the magic.

### The diagram (in words)

```
                ┌─────────────────────────┐
                │       TRACKER           │   one server, knows
                │       port 8080         │   who has what
                └────┬────────────────┬───┘
        REGISTER ▲   │  ▲ REGISTER    ▲
        GET_PEERS│   │  │ HEARTBEAT   │
                 │   ▼  │             │
       ┌─────────┴─┐    ┌─────────────┴─┐
       │  PEER A    │    │   PEER B      │   peers register
       │ (seeder)   │◄──►│  (leecher)    │   with tracker,
       │ port 5001  │ ←──→│  port 5002   │   then exchange
       └────────────┘     └───────────────┘   pieces directly
            ▲     pieces flow only between
            │     peers, never through tracker
            └── peer C connects directly too
```

Two completely different conversations are happening:

- **Peer ↔ Tracker** (control plane): short text messages — "register
  me", "who has this file?", "I now own piece 5".
- **Peer ↔ Peer** (data plane): the actual file pieces flow as raw
  bytes. The tracker never sees them.

---

## 4. Words you must know (glossary up front)

Read these once and the rest will make sense.

| Word | Meaning |
|---|---|
| **Piece** | A fixed-size chunk of a file. Default: 1 MB. A 25 MB file = 25 pieces. |
| **Piece index** | The number of the piece (0, 1, 2, …). Piece 0 is the first 1 MB of the file. |
| **Swarm** | All the peers currently exchanging the same file. |
| **Seeder** | A peer that has *all* pieces of a file. Just uploads, doesn't download. |
| **Leecher** | A peer that is currently *downloading* — has only some pieces. |
| **Tracker** | The directory server that knows who is in each swarm. |
| **Hash** | A short fingerprint (string) that uniquely identifies a piece of data. If two hashes match, the data is identical. |
| **Verification** | Re-computing the hash of bytes you received, and checking it matches the hash the tracker gave you. |
| **Heartbeat** | A "still alive" message a peer sends the tracker every few seconds. |
| **Assembly** | Stitching all the downloaded pieces back into one final file. |
| **Bitvector** | An array of true/false, one slot per piece, saying which pieces you own. |

---

## 5. The two programs we built

### 5.1 `tracker.exe` — the registry

**Job:** track who is in the swarm and who has which pieces.

**Stores in memory:**
- A peer table: `peerId → { ip, port, lastHeartbeat }`
- A file table: `fileName → { metadata, peerPieces }`
  - `metadata` = file size, piece count, list of hashes
  - `peerPieces` = `peerId → set of piece indices`

**Listens on:** one TCP port (default 8080). Accepts requests from peers,
answers them, then closes the connection. Each request is one round trip.

**Threads it runs:**
1. **Main thread** — just waits for you to press Enter to shut down.
2. **Accept loop** — accepts incoming TCP connections.
3. **Heartbeat monitor** — every few seconds, sweeps the peer table and
   removes any peer that hasn't said "I'm still alive" in too long.

**Does not:**
- Hold any file content.
- Initiate connections.
- Talk to the file system meaningfully (everything is in RAM).

If the tracker dies, no new peers can find each other, but ongoing
peer-to-peer transfers continue uninterrupted because they don't go
through the tracker.

### 5.2 `peer.exe` — the actual file-sharer

**Job:** simultaneously share files you have, and download files you want.

**Stores on disk:**
- A `shared/` directory: files you want to seed.
- A `downloads/` directory: files you are downloading.
  - For each in-progress file, a `<fileName>.pieces/` subdirectory holds
    each chunk as `piece_0.bin`, `piece_1.bin`, etc.
  - When the download is complete, those chunks get concatenated into
    `downloads/<fileName>`.

**Listens on:** one TCP port (whatever you pass to `--port`). This is
the "I'm a seeder" port — other peers connect here to ask for pieces.

**Connects out to:**
- The tracker (for control messages).
- Other peers (for piece requests).

**Threads it runs:**
1. **Main thread** — orchestrates, runs the REPL or `--share`/`--download`
   logic.
2. **Upload accept loop** — accepts incoming connections from other peers.
3. **Upload worker pool** (10 threads by default) — handles incoming
   piece requests in parallel.
4. **Heartbeat thread** — sends `HEARTBEAT` to the tracker every 5 s.
5. **Dashboard thread** (during download) — prints the status box every 1 s.
6. **Download workers** (one per peer in the swarm, during a download) —
   pulls pieces from each peer in parallel.
7. **Download sync thread** — every 5 s during a download, pushes
   "here are my owned pieces" to the tracker so other peers see the
   leecher as a partial seeder.

That's up to ~17 threads in one process during an active download. Most
of them sleep waiting for I/O.

---

## 6. The wire protocol — how programs talk

Both kinds of conversations (peer↔tracker and peer↔peer) use the **same
text-based message format**, with one twist for binary data.

### 6.1 Message format

Every control message is one line:

```
COMMAND|field1|field2|...|fieldN\n
```

- `COMMAND` is a short string like `REGISTER_PEER` or `REQUEST_PIECE`.
- Fields are separated by `|` (pipe).
- The line ends with a newline `\n`.

Building one: `Protocol::buildMsg("REGISTER_PEER", {"seed", "5001"})`
produces `"REGISTER_PEER|seed|5001\n"`.

Parsing one: `Protocol::split(line)` returns `["REGISTER_PEER", "seed", "5001"]`.

### 6.2 Peer ↔ Tracker commands

| Sent by peer | Tracker replies | Meaning |
|---|---|---|
| `REGISTER_PEER\|peerId\|port` | `OK\|registered` | Tell the tracker I'm online. |
| `HEARTBEAT\|peerId` | `OK\|alive` | I'm still here, don't forget me. |
| `GOODBYE\|peerId` | `OK\|bye` | I'm shutting down cleanly. |
| `REGISTER_FILE\|peerId\|name\|size\|count\|hash0,hash1,...` | `OK\|file registered` | I'm sharing this file. Here are its hashes. |
| `UPDATE_PIECES\|peerId\|name\|0,1,2,...` | `OK\|updated` | These are the pieces I currently own. |
| `SEARCH_FILE\|name` | `FILE_FOUND\|name\|size\|count` | Does this file exist? |
| `GET_METADATA\|name` | `FILE_METADATA\|name\|size\|count\|hash0,hash1,...` | Give me the piece hashes. |
| `GET_PEERS\|name` | one `PEER_LIST\|id\|ip\|port\|pieces` line per peer | Who has this file? |
| `GET_AVAILABILITY\|name` | `AVAILABILITY\|name\|0:2,1:1,2:3` | Piece N is held by M peers. |
| `DOWNLOAD_COMPLETE\|name` | `OK\|noted` | I'm done downloading. |

### 6.3 Peer ↔ Peer commands

Same format, different commands. The peers handshake then ask for pieces
one at a time:

| Sent | Reply | Meaning |
|---|---|---|
| `HELLO\|peerId` | (none, just acknowledged) | Identify yourself. |
| `REQUEST_PIECE\|name\|idx` | `SEND_PIECE\|name\|idx\|size\n<size raw bytes>` | Give me piece idx. |
| `REQUEST_PIECE\|name\|idx` | `PIECE_NOT_FOUND\|name\|idx` | I don't have that piece. |

The special bit: `SEND_PIECE`'s reply has a header line followed by
**raw binary**. The receiver reads the header (using `recvLine`), parses
the size, then reads *exactly that many bytes* using `recvExact`. This
is why both helpers exist in `Net.cpp`.

### 6.4 Why text + binary?

Pure text would mean encoding binary as base64 or hex — 33–100% wasted
bandwidth. Pure binary would mean writing a parser that handles every
message type byte-by-byte. The compromise is: small control messages
stay readable text (easy to debug, easy to extend), only the bulk
payload (piece bytes) is raw binary.

---

## 7. Project structure — every file in plain English

```
Torr_project_cursor/
├── common/         ← shared between tracker and peer
├── tracker/        ← only the tracker uses these
├── peer/           ← only the peer uses these
└── CMakeLists.txt  ← build configuration
```

### 7.1 Common (`common/`)

| File | What it does |
|---|---|
| `Config.h` | Bunch of tunable defaults (piece size, ports, heartbeat interval). |
| `Types.h` | Plain data structs (`PeerInfo`, `FileInfo`, `TrackerDatabase`, `SwarmPeer`) used everywhere. |
| `Protocol.h` | Message names, `buildMsg`, `split` — the wire-format helpers. |
| `Net.h` / `Net.cpp` | Thin wrapper over Winsock. Listen, accept, connect, send, receive. All network code goes through here. |
| `Threading.h` | Wrappers over Windows threading APIs: `Mutex`, `LockGuard`, `CondVar`, `Thread`. Lets the rest of the code be "Win32 free". |
| `Utilities.h` / `Utilities.cpp` | File I/O helpers, path parsing, dir creation. |

### 7.2 Tracker (`tracker/`)

The tracker is small because it doesn't do any heavy lifting.

| File | What it does |
|---|---|
| `main.cpp` | Boots Winsock, starts the server, waits for Enter to quit. |
| `TrackerServer.h` / `TrackerServer.cpp` | Accepts connections, parses commands, dispatches to one of the managers below. |
| `PeerManager.cpp` | Handles `REGISTER_PEER`, `HEARTBEAT`, `GOODBYE`. Owns the peer table. |
| `FileManager.cpp` | Handles `REGISTER_FILE`, `SEARCH_FILE`, `GET_METADATA`. Owns the file metadata. |
| `SwarmManager.cpp` | Handles `UPDATE_PIECES`, `GET_PEERS`, `GET_AVAILABILITY`. Owns the "who has which piece" map. |
| `HeartbeatMonitor.cpp` | Background sweeper that evicts peers who stop heart-beating. |

### 7.3 Peer (`peer/`)

| File | What it does |
|---|---|
| `main.cpp` | Parses CLI args, runs REPL or one-shot `--share` / `--download` mode. |
| `Peer.cpp` | The orchestrator. Starts/stops all the submanagers in the right order. |
| `TrackerClient.cpp` | Speaks to the tracker. Sends register, heartbeat, file search, piece updates. |
| `UploadServer.cpp` | TCP server on `--port`. Worker pool handles `REQUEST_PIECE` from other peers — this is the "seeder" half. |
| `DownloadManager.cpp` | Drives a download: get peers, spawn workers, request pieces, verify, assemble. |
| `Scheduler.cpp` | Decides *which piece to ask for next*. Uses rarest-first. |
| `PieceManager.cpp` | The bitvector of owned pieces + disk I/O for each piece. |
| `HashManager.cpp` | Hashes a piece (currently FNV-1a; will be SHA-256 later). |
| `FileAssembler.cpp` | After all pieces are downloaded, concatenates them into the final file. |
| `ResumeManager.cpp` | Saves "which pieces I own" to disk so an interrupted download can resume. |
| `StatsManager.cpp` | Counts bytes uploaded / downloaded for display. |
| `Dashboard.cpp` | Prints the periodic status box you see on the leecher. |

### 7.4 Build (`CMakeLists.txt`)

CMake produces two executables, `tracker` and `peer`, both linking
`ws2_32` (Winsock). The common files get compiled into both.

---

## 8. Configuration knobs

All defaults live in `common/Config.h`:

```cpp
int pieceSize          = 1048576;   // 1 MB chunks
int heartbeatInterval  = 5;         // peer pings tracker every 5 s
int dashboardRefresh   = 1;         // leecher prints status every 1 s
int maxUploadThreads   = 10;        // seeder serves up to 10 peers at once
int connectTimeout     = 5;         // 5 s before giving up on a TCP connect
int trackerPort        = 8080;
int peerPort           = 5001;
string trackerHost     = "127.0.0.1";
string peerId          = "peer1";
string sharedDir       = "shared";
string downloadDir     = "downloads";
```

Most of these are overridable from the command line: `--id`, `--port`,
`--tracker host:port`. The rest are baked in at compile time.

Why 1 MB pieces? Sweet spot:
- Smaller pieces → more parallelism, finer-grained verification, but
  more protocol overhead per byte.
- Larger pieces → less overhead, but a bad piece wastes more bandwidth
  before you notice.

---

## 9. How the program thinks in parallel (threading)

C++ doesn't do concurrency for free — you spawn threads, lock shared
data, signal between them. This project uses simple Win32 wrappers in
`Threading.h`.

### 9.1 What gets shared and locked

Anything that more than one thread reads/writes:

- **Tracker's `db_`** — guarded by mutexes in each manager
  (`PeerManager::mu_`, etc.).
- **Peer's `owned_` bitvector** — guarded by `PieceManager::mu_`.
- **Upload server's pending-client queue** — guarded by `qMu_` and
  signaled via `qCv_`.
- **Download status struct** — guarded by `statusMu_`.

### 9.2 The pattern you'll see everywhere

```cpp
class Foo {
    mutable Mutex mu_;
    SomeMap data_;
public:
    void modify(...) {
        LockGuard lock(mu_);    // RAII: locks here, unlocks at end of scope
        data_[k] = v;
    }
};
```

`LockGuard` is RAII. The constructor locks; the destructor (at end of
scope) unlocks. You can't accidentally forget to unlock.

### 9.3 Producer–consumer with `CondVar`

In `UploadServer`, the accept thread *produces* incoming clients, and
worker threads *consume* them. They communicate through a queue + a
condition variable:

```cpp
// producer
{
    LockGuard lock(qMu_);
    pending_.push(client);
}
qCv_.notify_one();              // wake one worker

// consumer
UniqueLock lock(qMu_);
qCv_.wait(lock, [&]{ return !pending_.empty() || !running_; });
sock_t client = pending_.front();
pending_.pop();
```

`wait` releases the lock while sleeping and re-acquires it when signaled.

### 9.4 Atomic flags for shutdown

`atomic<bool> running_{true}` is set to false on stop. Threads check it
in their loops:

```cpp
while (running_) { ... }
```

Atomic avoids the need to lock just to check a single bool.

---

## 10. Three complete walkthroughs

### 10.1 First peer (the seeder) starts up and shares a file

You type:
```
peer.exe --id seed --port 5001 --tracker 127.0.0.1:8080 --share shared/movie.mp4
```

Inside `main.cpp`:

1. Parse args into a `Config`.
2. `Net::initWinsock()` — Windows requires this before any sockets.
3. Construct `Peer` and call `peer.start()`.
4. Call `peer.shareFile("shared/movie.mp4")`.
5. Block on `cin.get()` (wait for user to press Enter).

Inside `Peer::start()`:

1. Ensure `shared/` and `downloads/` directories exist.
2. **Start the upload server** (`UploadServer::start()`):
   - `Net::tcpListen(5001)` — bind and listen on port 5001.
   - Spawn 10 worker threads, each blocking on the pending-client queue.
   - Spawn an accept thread that loops `accept()` and enqueues sockets.
3. **Start the tracker client** (`TrackerClient::start()`):
   - Open a TCP connection to the tracker.
   - Send `REGISTER_PEER|seed|5001\n`.
   - Read back `OK|registered`.
   - Close the connection.
   - Spawn the heartbeat thread.

Inside `Peer::shareFile("shared/movie.mp4")`:

1. `HashManager::hashFile("shared/movie.mp4", 1MB)` — open the file,
   read it in 1 MB chunks, compute one hash per chunk, return the list.
2. `Util::calcPieceCount(fileSize, 1MB)` — total piece count.
3. `PieceManager::initSeed(...)` — note the source path, mark `owned_`
   as all-true (we have every piece).
4. `TrackerClient::registerFile(...)`:
   - Open a TCP connection.
   - Send `REGISTER_FILE|seed|movie.mp4|<size>|<count>|<hash0>,<hash1>,...\n`.
   - Read `OK|file registered`.
   - Close.
5. Print `sharing movie.mp4 (N pieces)`.

Seeder is now idle on the main thread, but its background threads keep
running: upload workers waiting, heartbeat thread pinging tracker.

### 10.2 A second peer (the leecher) downloads the file

You type on another machine (or terminal):
```
peer.exe --id leech1 --port 5002 --tracker 127.0.0.1:8080 --download movie.mp4
```

Boot is the same as the seeder up through `Peer::start()`. Then
`main.cpp` calls `peer.downloadFile("movie.mp4")`.

Inside `Peer::downloadFile("movie.mp4")`:

1. Spawn a download thread that calls `dlMgr_.startDownload("movie.mp4")`.

Inside `DownloadManager::startDownload("movie.mp4")`:

1. **Find the file.** Send `SEARCH_FILE|movie.mp4` → receive
   `FILE_FOUND|movie.mp4|<size>|<count>`.
2. **Get hashes.** Send `GET_METADATA|movie.mp4` → receive
   `FILE_METADATA|...|<hashCsv>`. Store the hash list in `hashes_`.
3. **Set up local state.** `PieceManager::initDownload(...)`:
   - Create `downloads/movie.mp4.pieces/`.
   - Mark `owned_` as all-false.
4. **Try resume.** `ResumeManager::loadState` — if we crashed mid-download
   before, set the bits we already have. (Fresh download: no-op.)
5. **Get peer list.** Send `GET_PEERS|movie.mp4` → receive one
   `PEER_LIST|seed|<ip>|5001|0,1,...,N-1` line per peer. Build
   `vector<SwarmPeer> peers_`.
6. **Get piece availability.** Send `GET_AVAILABILITY|movie.mp4` →
   receive counts per piece. Feed into scheduler so it can pick rarest
   pieces first.
7. **Spawn one worker thread per peer.** Each worker connects to that
   peer and pulls pieces.
8. **Spawn a sync thread** that every 5 s sends `UPDATE_PIECES` to the
   tracker (so other peers see this leecher as a partial seeder) and
   refreshes availability.
9. **Join all workers and the sync thread.**
10. When all pieces are owned:
    - Save the final resume state.
    - `FileAssembler::assemble("movie.mp4", pieceCount, expectedSize)`:
      - Open `downloads/movie.mp4` for writing.
      - For i = 0..N-1: read `downloads/movie.mp4.pieces/piece_i.bin`,
        write to output.
      - Verify final size.
    - Send `DOWNLOAD_COMPLETE|movie.mp4` to tracker.
    - Print `download complete: downloads/movie.mp4`.

Inside each **worker thread** (`workerLoop(SwarmPeer peer)`):

```text
connect to peer.ip:peer.port
send HELLO|leech1

while we still need pieces and the peer might have some:
    idx = scheduler.pickPiece(peer.pieces)
        — looks at pieces this peer has minus pieces I already own,
          picks the one with lowest availability count
    if no good idx:
        sleep 200ms
        refresh availability
        continue

    send REQUEST_PIECE|movie.mp4|idx
    read header line
    if PIECE_NOT_FOUND:
        mark idx as missing again, try a different peer
        break out of this worker

    if SEND_PIECE|movie.mp4|idx|<size>:
        recvExact(size) bytes into buffer
        verify hash matches hashes_[idx]
            yes → write piece_idx.bin, mark owned, update resume
            no  → mark missing (retry from someone else)

close the socket
```

### 10.3 A third peer joins mid-download

Now leech1 is halfway through. A third peer starts up:

```
peer.exe --id leech2 --port 5003 --tracker 127.0.0.1:8080 --download movie.mp4
```

When leech2 does `GET_PEERS|movie.mp4`, the tracker returns **two**
peers:
- `seed` with pieces 0..N-1 (full seeder).
- `leech1` with the pieces leech1 has already downloaded.

leech2 spawns two worker threads, one per peer. It pulls pieces from
both **in parallel**. The rarest-first scheduler ensures it grabs the
pieces leech1 has *but seed somehow doesn't yet had — actually here it
prefers the pieces leech1 has that seed also has and that fewer peers
have overall, prefers diversity*.

If leech2 finishes piece 7 before leech1 does, and leech1 hasn't gotten
piece 7 yet, the next `UPDATE_PIECES` makes leech1 see leech2 as a
source for piece 7. Now leech2 is uploading to leech1 even though
leech2 isn't done downloading. This is the swarm working.

---

## 11. What lives in memory — the data structures

### 11.1 On the tracker

```cpp
struct PeerInfo {
    string peerId;
    string ipAddress;
    int port;
    time_t lastHeartbeat;
};

struct FileInfo {
    string fileName;
    long long fileSize;
    int pieceCount;
    vector<string> pieceHashes;     // one hash per piece
};

struct TrackerRecord {
    FileInfo metadata;
    unordered_map<string, set<int>> peerPieces;  // peerId → owned pieces
};

struct TrackerDatabase {
    unordered_map<string, PeerInfo>     peers;   // peerId → info
    unordered_map<string, TrackerRecord> files;  // fileName → record
};
```

To answer "who has movie.mp4?" the tracker iterates
`db_.files["movie.mp4"].peerPieces` and combines each peerId with the
peer's IP/port from `db_.peers[peerId]`.

### 11.2 On a peer

For each file it's involved with:

```cpp
class PieceManager {
    string fileName_;
    long long fileSz_;
    int pieceCnt_;
    bool isSeed_;             // are we seeding (have all pieces) or downloading?
    string seedPath_;         // if seeding, the path to the source file
    string pieceDir_;         // if downloading, where pieces are staged
    vector<bool> owned_;      // bitvector, owned_[i] == "I have piece i"
};
```

When downloading, pieces are stored on disk as separate files:
```
downloads/
  movie.mp4.pieces/
    piece_0.bin
    piece_1.bin
    ...
    piece_N-1.bin
```

When done, `FileAssembler` reads them in order, concatenates into
`downloads/movie.mp4`, and you can delete the `.pieces/` dir (the
current code keeps it — minor cleanup TODO).

---

## 12. The journey of one piece

Let's follow piece 7 from the seeder's disk to the leecher's disk.
This is the most important section if you want to understand the
plumbing.

### Step 1 — leecher decides it wants piece 7

`Scheduler::pickPiece` looks at:
- Pieces this peer has (set: e.g., 0..24)
- Pieces I don't own yet (owned_[i] == false for many i)
- Availability counts (piece 7 is held by 1 peer, piece 8 by 5)

Returns 7 because it's rare.

### Step 2 — leecher asks for it

```cpp
auto req = Protocol::buildMsg("REQUEST_PIECE", {"movie.mp4", "7"});
// req == "REQUEST_PIECE|movie.mp4|7\n"
Net::sendAll(sock, req.c_str(), req.size());
```

`sendAll` keeps calling `send()` until all bytes are written
(`send` may write only some bytes per call).

### Step 3 — seeder receives the request

The seeder's `UploadServer` already accepted this leecher's connection
and queued it. A worker thread picked it up and is sitting in
`handleClient`, calling `recvLine` in a loop.

`recvLine` reads one byte at a time until it hits `\n`, dropping `\r`:

```cpp
bool recvLine(sock_t sock, string& line) {
    line.clear();
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') break;
        if (c != '\r') line += c;
    }
    return true;
}
```

After this call, `line == "REQUEST_PIECE|movie.mp4|7"`. The seeder
splits it on `|`:
- `parts[0] == "REQUEST_PIECE"`
- `parts[1] == "movie.mp4"`
- `parts[2] == "7"`

### Step 4 — seeder reads piece 7 from its source file

```cpp
PieceManager::readSeedPiece(7, data):
    f.seekg(7 * 1048576)            // jump to byte 7,340,032
    data.resize(1048576)
    f.read(data.data(), 1048576)    // read 1 MB
    data.resize(actually read)      // shrink if last piece is smaller
```

### Step 5 — seeder sends the header line + raw bytes

```cpp
auto header = Protocol::buildMsg("SEND_PIECE",
                                 {"movie.mp4", "7", to_string(data.size())});
// header == "SEND_PIECE|movie.mp4|7|1048576\n"
Net::sendAll(sock, header.c_str(), header.size());
Net::sendAll(sock, data.data(), data.size());
```

Two `sendAll` calls back-to-back. Under the hood TCP may merge them
into the same packet; doesn't matter — the leecher will read them
deterministically.

### Step 6 — leecher reads the header line

```cpp
Net::recvLine(sock, line);
auto parts = Protocol::split(line);
// parts == ["SEND_PIECE", "movie.mp4", "7", "1048576"]
int payloadSz = stoi(parts[3]);  // 1048576
```

### Step 7 — leecher reads exactly that many raw bytes

```cpp
data.resize(payloadSz);
Net::recvExact(sock, data.data(), payloadSz);
```

`recvExact` loops until it has all `payloadSz` bytes:

```cpp
bool recvExact(sock_t sock, char* buf, int sz) {
    int got = 0;
    while (got < sz) {
        int n = recv(sock, buf + got, sz - got, 0);
        if (n <= 0) return false;
        got += n;
    }
    return true;
}
```

### Step 8 — leecher verifies the bytes

```cpp
if (!HashManager::verify(data, hashes_[7])) {
    scheduler.markMissing(7);   // someone lied or wire corrupted it
    continue;                   // try from a different peer
}
```

`verify` recomputes the FNV hash of `data` and compares to the hash
the tracker handed out in `GET_METADATA`. If they match, the bytes are
exactly what the original file had at offset `7 * 1MB`.

### Step 9 — leecher writes the piece to disk

```cpp
PieceManager::writePiece(7, data):
    Util::writeFile("downloads/movie.mp4.pieces/piece_7.bin", data.data(), data.size());
    owned_[7] = true;
```

### Step 10 — leecher tells the tracker

The sync thread next time it runs:
```
UPDATE_PIECES|leech1|movie.mp4|0,1,2,4,7\n
```

Now, when any other peer does `GET_PEERS|movie.mp4`, the tracker
includes `leech1` with piece 7 in its list. leech1 has just become
a source for piece 7.

### Step 11 — eventually, assembly

When `owned_` is all true:

```cpp
FileAssembler::assemble("movie.mp4", 25, expectedSize):
    open downloads/movie.mp4 for binary writing
    for i in 0..24:
        read downloads/movie.mp4.pieces/piece_i.bin
        write its bytes to the output file
    verify size matches
```

Done. The output file is byte-for-byte identical to the seeder's
original — verifiable by computing SHA-256 of both and comparing.

---

## 13. How hashing & verification keeps you honest

**Why we hash:** networks corrupt data, attackers send malicious bytes,
disks fail. Without verification you'd happily save garbage and assemble
a corrupted file.

**What a hash is:** a function that takes any bytes and produces a
short fixed-length fingerprint. Two key properties:
- **Deterministic** — same input always produces same hash.
- **Collision-resistant** — different inputs almost never produce the
  same hash.

So if you compute the hash of bytes you received and it matches the
hash the metadata claims, you can be confident the bytes are the same
ones the seeder sent.

**Current implementation** (`HashManager::hashPiece`):

```cpp
uint64_t h1 = fnv64(data, sz, seed1);
uint64_t h2 = fnv64(data, sz, seed2);
uint64_t h3 = fnv64(data, sz, h1 ^ h2);
uint64_t h4 = fnv64(data, sz, h3 ^ seed3);
return toHex16(h1) + toHex16(h2) + toHex16(h3) + toHex16(h4);
```

Four FNV-1a hashes with different seeds, concatenated → 64 hex
characters. FNV-1a is fast and good enough for accidental corruption.
It is **not cryptographically secure** — a determined attacker could
build colliding data. That's fine for this project where you trust
your peers.

**The plan** is to replace this with **SHA-256** later. SHA-256 also
produces 64 hex characters, so swapping is a drop-in change in
`HashManager.cpp` — no protocol change.

---

## 14. How to build it

### Prerequisites

- Windows 10/11.
- A C++ toolchain that supports C++17. Options:
  - MinGW-w64 (any recent winlibs/UCRT build)
  - MSVC via Visual Studio
- CMake 3.16+.

### Configure + build (MinGW)

```bash
cd Torr_project_cursor
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4
```

Output: `build/tracker.exe`, `build/peer.exe`.

### Configure + build (Visual Studio)

```bash
cd Torr_project_cursor
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### What's in `CMakeLists.txt` worth knowing

- C++17 standard, project name `DakshTorrent`.
- Both targets link `ws2_32` (Winsock).
- `WIN32_LEAN_AND_MEAN` and `NOMINMAX` are defined to keep `<windows.h>`
  from polluting the global namespace with junk (especially the
  `byte` typedef that clashes with `std::byte`).
- MSVC builds with `/W4`.

---

## 15. How to run it

You always need at least 2 processes (tracker + 1 peer). For a real demo,
4 processes (tracker + seeder + 2 leechers).

### Work-area layout

Each peer creates `shared/` and `downloads/` in *its current working
directory*. Give each peer its own directory:

```
demo/
  tracker/
  seed/        ← cd here, put file in seed/shared/
    shared/
    downloads/
  leech1/
    shared/
    downloads/
  leech2/
    shared/
    downloads/
```

### Terminal 1 — tracker

```bash
cd demo/tracker
path/to/tracker.exe 8080
```

### Terminal 2 — seeder

```bash
cd demo/seed
# put a file in shared/ first
path/to/peer.exe --id seed --port 5001 --tracker 127.0.0.1:8080 --share shared/movie.mp4
```

### Terminal 3 — leecher

```bash
cd demo/leech1
path/to/peer.exe --id leech1 --port 5002 --tracker 127.0.0.1:8080 --download movie.mp4
```

### Terminal 4 — another leecher

```bash
cd demo/leech2
path/to/peer.exe --id leech2 --port 5003 --tracker 127.0.0.1:8080 --download movie.mp4
```

Both leechers download in parallel; they also share pieces with each
other once they each have some.

### Verifying

```powershell
Get-FileHash demo/seed/shared/movie.mp4 -Algorithm SHA256
Get-FileHash demo/leech1/downloads/movie.mp4 -Algorithm SHA256
Get-FileHash demo/leech2/downloads/movie.mp4 -Algorithm SHA256
```

All three hashes must match.

### Across machines

Replace `127.0.0.1` with the tracker machine's LAN IP, and make sure
TCP ports 8080 (tracker) and whatever peer port you pick are reachable
through the OS firewall. The architecture supports this — it's just
sockets.

---

## 16. Common problems and what they mean

| What you see | What it usually means |
|---|---|
| Seeder prints `cannot read file: shared/x` | Wrong path or you didn't `cd` into the seed working directory before running. |
| Leecher: `file not found on tracker` | Started leecher before seeder finished registering. Wait for the seeder to print `sharing X (N pieces)`. |
| Leecher dashboard stuck at `Pieces: 0/N`, `Connected Peers: 0` | The seeder isn't reachable from the leecher. Wrong tracker IP, blocked port, or the seeder isn't running. |
| Leecher: `assembly failed` | Either disk full, or piece dir collides with output file (the bug we fixed). |
| Peer: `bind failed` or `tracker registration failed` | Port already in use. Pick different `--port`. |
| Tracker connection works, file shows as found, but no peers returned | Seeder hasn't sent `UPDATE_PIECES` yet, or it's been > 3 × heartbeat interval since the seeder's last heartbeat and the monitor evicted it. |

---

## 17. A real bug we hit and fixed

A great teaching example. Originally:

```cpp
// PieceManager.cpp::initDownload
pieceDir_ = cfg_.downloadDir + "/" + fileName;   // "downloads/x.pptx"

// FileAssembler.cpp::assemble
string outPath  = outputPath(fileName);              // "downloads/x.pptx"
string pieceDir = cfg_.downloadDir + "/" + fileName; // "downloads/x.pptx"   ← SAME PATH
ofstream out(outPath, ios::binary | ios::trunc);     // fails — that path is a directory
```

**Two pieces of code used the same path for two different things** —
the chunk-staging *directory* and the final assembled *file*. The OS
can't have both a directory and a file with the same name. The chunk
files all got downloaded and verified successfully, but at the very
end, `ofstream` couldn't create the output file, and you'd see
`assembly failed`.

**Fix** (one line in each file):

```cpp
pieceDir_ = cfg_.downloadDir + "/" + fileName + ".pieces";
```

Now the staging dir is `downloads/x.pptx.pieces/` and the output file
is `downloads/x.pptx`. No collision.

**Lessons:**
- When two unrelated bits of code compute the same path, they're
  coupled by accident. Centralize that into one helper that returns
  both paths together.
- An "assembly failed" message that doesn't say *why* costs hours.
  Better error reporting saves time.

---

## 18. Things to improve next

If you want to keep extending this, here are ideas roughly in order of
value.

### Easy

- **Better logging.** Switch the random `cerr <<` lines to a `log(level,
  fmt, ...)` helper that includes timestamp + module name.
- **Cleanup of `.pieces/` after assembly.** Currently it stays behind.
- **`.gitignore` cleanup** (we already added the file; could also run
  `git rm --cached -r build/ .vs/` once).

### Medium

- **Replace FNV with SHA-256.** Drop-in change in `HashManager.cpp`.
  Same 64-char hex output, no protocol change.
- **Cross-platform.** Replace Win32 in `Threading.h` with `std::thread`,
  `std::mutex`, `std::condition_variable`; replace Winsock-specific
  parts of `Net.cpp` with POSIX sockets (#ifdef around the small
  differences). Suddenly the project builds on Linux/macOS.
- **Resume across sessions.** Currently `ResumeManager` exists and
  the bits are wired; verify it actually resumes after a crash.
- **One-line metadata file.** A small `.torr` file alongside a shared
  file containing tracker addr + name + hashes would let you share
  the file by passing around the tiny `.torr` rather than typing the
  name and trusting the tracker has it.

### Hard

- **Encryption.** End-to-end encrypt the piece transfer with TLS or a
  Noise handshake. Right now anyone sniffing your LAN sees plaintext.
- **NAT traversal.** Make peers behind home routers reach each other
  using UPnP, STUN, or a relay. Otherwise it works on LAN only.
- **Web UI.** A small HTTP server inside `peer.exe` that serves an
  HTML page on `localhost:7000` so users control sharing/downloading
  from a browser instead of a terminal. (We sketched this.)

---

## 19. Cheat sheet

### Build

```bash
cd Torr_project_cursor
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4
```

### Run a minimal 1-seeder + 1-leecher demo

```bash
# T1
./tracker.exe 8080

# T2 (in directory with shared/myfile.bin)
./peer.exe --id seed --port 5001 --tracker 127.0.0.1:8080 --share shared/myfile.bin

# T3 (in different directory)
./peer.exe --id leech --port 5002 --tracker 127.0.0.1:8080 --download myfile.bin
```

### CLI flags for `peer.exe`

| Flag | Meaning |
|---|---|
| `--id <name>` | Unique peer identifier |
| `--port <n>` | TCP port this peer listens on |
| `--tracker <host:port>` | Where the tracker lives |
| `--share <path>` | One-shot: share this file and stay alive |
| `--download <name>` | One-shot: download this file and exit |
| `--help` | Print usage |

If you pass neither `--share` nor `--download`, you get an interactive
REPL with commands `share <path>`, `download <name>`, `status`, `help`,
`quit`.

### CLI for `tracker.exe`

```
tracker.exe <port>
```

That's it.

---

## Closing thought

This whole project is about **one big idea**: every user is both a
consumer and a contributor. The more popular a file becomes, the
*more* available it is, not less. The code that makes that work is
honestly not that complicated — a directory server, a piece-by-piece
download loop, a hash check, and some threading glue. The cleverness
is in the protocol design, not the lines of code.

If you can explain to a friend why a swarm gets faster as more people
join, you've understood the heart of it.

Good luck and happy hacking.
