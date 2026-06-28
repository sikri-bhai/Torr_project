# Torr_project — 100 Interview Questions & Detailed Answers

Organized from concept to advanced. Each answer is interview-prep length —
substantive enough to actually use, focused enough to memorize.

**Sections**
- [A. Concepts & terminology (1–15)](#a-concepts--terminology-115)
- [B. Architecture & design (16–30)](#b-architecture--design-1630)
- [C. Networking & wire protocol (31–45)](#c-networking--wire-protocol-3145)
- [D. C++ language & STL (46–60)](#d-c-language--stl-4660)
- [E. Threading & concurrency (61–75)](#e-threading--concurrency-6175)
- [F. Implementation deep dive (76–85)](#f-implementation-deep-dive-7685)
- [G. System design & trade-offs (86–95)](#g-system-design--trade-offs-8695)
- [H. Bugs, testing, improvements (96–100)](#h-bugs-testing-improvements-96100)

---

## A. Concepts & terminology (1–15)

### 1. What is this project?

A small BitTorrent-style peer-to-peer file-sharing system in C++. It has
two executables: a **tracker** that knows who is online and which file
pieces each peer has, and a **peer** that simultaneously serves pieces
it owns and downloads pieces it wants. Files are split into 1 MB
chunks, transferred chunk-by-chunk in parallel across all peers that
have them, hash-verified on arrival, and re-assembled locally.

### 2. What is BitTorrent / peer-to-peer file sharing?

A protocol in which downloaders don't get a file from one central
server, but from many peers who each have part of it. Each downloader
also uploads pieces they've already received to others. The total
upload capacity of the system grows with the number of users, so popular
files get faster as more people join — the opposite of a centralized
download server.

### 3. Difference between client-server and peer-to-peer?

In **client-server**, one server holds all the data and serves all
clients. Simple, but the server is a bottleneck (one machine's
bandwidth limits everyone) and a single point of failure. In **P2P**,
every node is both client and server. There's no central bottleneck;
load is distributed; the system survives individual node failures. The
cost is more complex coordination — discovering who has what is the
hard part.

### 4. What is a "swarm"?

The set of all peers currently exchanging a particular file. A peer who
just registered with the tracker but has no pieces is technically in
the swarm too — they're just contributing nothing yet. As they
download, they start uploading to others and contribute.

### 5. What is a seeder vs leecher?

- **Seeder** — has the complete file. Only uploads (because they have
  nothing to download).
- **Leecher** — is currently downloading. They own some pieces and
  upload those while still fetching the rest.

A leecher becomes a seeder the moment all their pieces are owned.

### 6. What is a "piece"?

A fixed-size chunk of a file. In this project, 1 MB. A 25 MB file is
divided into 25 pieces, each independently downloadable and verifiable.
Piece indexes are 0-based — piece 0 is bytes 0 to 1,048,575 of the
file, piece 1 is the next 1 MB, etc. The last piece may be smaller.

### 7. Why split files into pieces?

Three reasons:
- **Parallelism**: you can download different pieces from different
  peers simultaneously instead of being limited to one source.
- **Resumability**: if your download is interrupted, you only re-fetch
  the pieces you didn't finish, not the whole file.
- **Bad-actor tolerance**: a peer sending corrupt data only wastes one
  piece's worth of bandwidth before it's caught by the hash check, not
  the whole file.

### 8. What's the role of the tracker?

A coordinator / phonebook. It tracks **who** is online and **which
pieces** they have. It answers:
- "I'm a new peer, register me."
- "Who has file X?"
- "What's the metadata (size, piece count, hashes) of file X?"
- "I now own these pieces — please update."

The tracker never sees, stores, or transmits file content — only
metadata.

### 9. Why doesn't the tracker hold files?

Three reasons:
- **Scale** — a tracker can handle thousands of peers with tiny
  metadata exchanges. If it had to serve file data too, it would
  become the bottleneck we wanted to avoid.
- **Legal/operational** — the tracker operator isn't liable for the
  content. They just maintain a directory.
- **Architectural cleanliness** — separation of control plane
  (tracker) and data plane (peer-to-peer transfers).

### 10. What is a hash, and why use it?

A hash is a fixed-length fingerprint of arbitrary data. Two key
properties:
- **Deterministic** — same input → same hash.
- **Collision-resistant** — different inputs almost never produce the
  same hash.

When the seeder shares a file, it hashes each piece and sends the
hashes to the tracker. When a leecher receives a piece, it computes the
hash and compares. If they match, the bytes are unchanged. If not — the
piece is corrupt or malicious, and you discard it and retry from
another peer.

### 11. What's a heartbeat?

A periodic "I'm still alive" message that each peer sends the tracker
(every 5 s by default). The tracker records the timestamp of the last
heartbeat per peer. A separate **HeartbeatMonitor** thread sweeps the
peer list and evicts anyone whose last heartbeat is older than ~3× the
interval. Without heartbeats, the tracker could never tell the
difference between an idle peer and a crashed one.

### 12. What is rarest-first piece selection?

A strategy where, given a choice of pieces to download next, you prefer
the one held by the *fewest* peers in the swarm.

**Why**: rare pieces are most at risk of vanishing if their few holders
disconnect. Fetching rare pieces first spreads them faster, keeps the
swarm balanced and resilient. Common pieces will still be around in 10
seconds; the rare ones might not.

### 13. What happens if a peer leaves mid-download?

For the leecher: the worker thread connected to that peer fails its
next `recv`, returns, the worker exits. The download continues from
other peers. The pieces it was about to fetch from the gone peer get
marked missing and re-picked from someone else.

For the swarm: the next time the tracker is asked `GET_PEERS`, the
dead peer is already gone (heartbeat monitor evicted it), so it
doesn't appear in the response.

### 14. What's the difference between control plane and data plane?

- **Control plane** = metadata, signaling, coordination. In this
  project: peer ↔ tracker messages, port 8080. Small, frequent, latency-
  sensitive but bandwidth-light.
- **Data plane** = actual payload (the file pieces). In this project:
  peer ↔ peer connections on dynamic ports. Bandwidth-heavy.

Separating them lets each be optimized differently and lets the data
plane scale independently of the central tracker.

### 15. Why TCP and not UDP?

We need **ordered, reliable delivery** of multi-megabyte pieces. TCP
gives us that for free: retransmission, ordering, flow control,
congestion control. UDP would force us to reimplement all of that on
top, which would essentially become TCP. Real BitTorrent does use UDP
in some places (for the tracker via uTP) but only because it handles
the retransmission logic itself.

---

## B. Architecture & design (16–30)

### 16. Describe the high-level architecture.

Two binaries:
- `tracker.exe` — one per system, listens on port 8080. Stateful in
  memory: peer table + file table.
- `peer.exe` — many instances. Each runs an UploadServer (TCP listen
  port) and a DownloadManager when downloading. Each peer talks to the
  tracker for discovery and to other peers for piece exchange.

Two protocol families on top of TCP:
- Peer ↔ Tracker (text, pipe-delimited)
- Peer ↔ Peer (text headers + raw binary payload)

### 17. Why two binaries (tracker and peer)?

They serve fundamentally different roles. The tracker is a long-lived,
centrally-deployed coordinator. The peer is a per-user end client.
Different deployment, different lifecycle, different scaling needs.
Same binary would conflate the responsibilities and force unused code
to be linked into both.

### 18. Why is each peer both client and server?

That's the essence of P2P. If a peer were only a client (download-only),
the swarm would have no uploaders besides the original seeder. By being
both, every peer that finishes a piece can immediately re-upload it.
The swarm's upload capacity grows with the number of peers.

### 19. How does a peer find other peers?

It asks the tracker. After registering, it sends `GET_PEERS|fileName`
and receives one `PEER_LIST|peerId|ip|port|pieces` line per peer in the
swarm. It then opens its own TCP connections to those peers' ports.
There's no broadcast or gossip — the tracker is the single source of
truth.

### 20. What components run inside the tracker?

- `TrackerServer` — TCP listener and request dispatcher.
- `PeerManager` — REGISTER_PEER, HEARTBEAT, GOODBYE handlers.
- `FileManager` — REGISTER_FILE, SEARCH_FILE, GET_METADATA handlers.
- `SwarmManager` — UPDATE_PIECES, GET_PEERS, GET_AVAILABILITY handlers.
- `HeartbeatMonitor` — background sweeper that evicts dead peers.

All three managers share one in-memory `TrackerDatabase`, each guarded
by its own mutex.

### 21. What components run inside the peer?

- `Peer` — orchestrator that owns all the rest.
- `TrackerClient` — speaks to tracker (register, heartbeat, file ops).
- `UploadServer` — TCP listener that serves piece requests.
- `DownloadManager` — drives a download from start to finish.
- `Scheduler` — picks which piece to request next (rarest-first).
- `PieceManager` — disk I/O for individual pieces, bitvector of owned.
- `HashManager` — hash and verify.
- `FileAssembler` — stitch pieces into the final file.
- `ResumeManager` — persist owned-pieces bitvector to disk.
- `StatsManager` — byte counters for the dashboard.
- `Dashboard` — periodic status output.

### 22. How is the codebase organized (folders)?

```
Torr_project_cursor/
  common/       ← code shared between tracker and peer
  tracker/      ← tracker-only code
  peer/         ← peer-only code
  CMakeLists.txt
```

`common/` reduces duplication: both sides need the wire protocol, the
network helpers, the threading wrappers, and the shared types.

### 23. Why is `common/` shared?

`Protocol.h` defines the wire format — both ends must agree. `Net.cpp`
abstracts Winsock so we don't reimplement TCP helpers on each side.
`Types.h` defines structs that flow between them. Putting these in a
shared folder ensures one definition; if it changes, both sides
re-build.

### 24. Why separate Net / Threading / Protocol headers?

Single responsibility:
- `Net.h/cpp` — anything to do with sockets.
- `Threading.h` — anything to do with threads, mutexes, condvars.
- `Protocol.h` — anything to do with the wire format.

This isolates platform-specific code (Win32 sockets, Win32 threads) to
two files. Replacing them with POSIX equivalents would only touch
those two files, not the entire codebase.

### 25. What's the role of FileAssembler?

After all pieces are downloaded and verified, FileAssembler reads each
piece file (`piece_0.bin`, `piece_1.bin`, …) in order and writes them
into one continuous output file. It also sanity-checks the final size.
This separates the "download chunks" logic from the "produce a usable
file" logic.

### 26. What's the role of ResumeManager?

Persists the "which pieces I own" bitvector to a `.resume` file on
disk. If you crash or kill the process mid-download, on restart the
ResumeManager loads that bitvector, marks those pieces as already
owned, and the scheduler skips them. Saves you from re-downloading
gigabytes after a power loss.

### 27. What does the Scheduler do?

Decides which piece to request next, given:
- The pieces I don't own yet (bitvector).
- The pieces this particular peer has.
- The availability map (how rare each piece is in the swarm).

It picks the rarest piece this peer has that I still need. Maintains
state on which pieces are currently being requested (so two workers
don't race on the same piece) and which have completed.

### 28. How does PieceManager work?

It's the disk + bitvector layer. Two modes:
- **Seed mode**: `seedPath_` set to the source file; all pieces marked
  owned; `readSeedPiece(idx)` seeks to `idx * pieceSize` in the source
  file and reads.
- **Download mode**: `pieceDir_` set to `downloads/file.pieces/`; each
  piece written as `piece_N.bin`; `hasPiece(idx)` checks the bitvector
  before reading.

Internal mutex protects the bitvector across threads.

### 29. Why have a Dashboard component?

Separation of concerns — printing progress shouldn't leak into the
download logic. Dashboard runs on its own timer thread, reads a
snapshot of status from `DownloadManager::status()` and prints it. The
download code stays focused on actually downloading.

### 30. UploadServer vs DownloadManager — what's the split?

- **UploadServer**: the "seeder" half of a peer. Listens for incoming
  TCP connections from other peers, replies to `REQUEST_PIECE` with
  `SEND_PIECE` + raw bytes. Always running, even on a leecher.
- **DownloadManager**: the "leecher" half. Orchestrates outbound
  connections to discovered peers, requests pieces, verifies, writes,
  assembles. Only active during a download.

They run side-by-side in the same process, on different threads. A
leecher can re-upload a piece via UploadServer the moment
DownloadManager has finished writing it.

---

## C. Networking & wire protocol (31–45)

### 31. Describe the wire protocol.

Pipe-delimited, line-terminated ASCII for control messages:

```
COMMAND|field1|field2|...|fieldN\n
```

For piece transfer, the response is two parts:
1. Header line: `SEND_PIECE|fileName|idx|size\n`
2. Exactly `size` raw bytes immediately following.

Both ends use `Protocol::buildMsg` to construct and `Protocol::split`
to parse. Reading is `recvLine` for headers, `recvExact(size)` for
binary payload.

### 32. Why pipe-delimited text?

- **Debuggable** — you can `printf` a message and read it.
- **Easy to extend** — new fields just become a new piece between
  pipes.
- **Pipe is rare** in filenames and IDs, so collision is unlikely.

If we used JSON we'd need a JSON parser (extra dependency), and a fully
binary format would be harder to debug. Pipe-text is the lazy-but-
correct compromise.

### 33. Why mix text headers with raw binary payload?

Pure text would force us to base64-encode binary, wasting 33%
bandwidth. Pure binary would force us to define byte-precise framing
for every message. The mix gives you readability for control + zero-
overhead for bulk data.

### 34. Why `\n` terminator instead of length-prefixing every message?

Two reasons:
- **Simpler parsing** — you don't have to read a length first,
  allocate a buffer, then read. You just read until newline.
- **Streaming-friendly** — a peer can send many control messages in a
  row and the receiver reads them as fast as they arrive.

For the binary payload, where length-prefixing matters, we *do* prefix
with the `size` field in the header.

### 35. What does `Net::recvLine` do?

Reads one byte at a time using `recv`, accumulating into a string,
stopping when it sees `\n`. It also drops `\r` for cross-platform line
endings. Returns false on connection close or error.

It's slow (one syscall per byte) but it's only used for short control
headers, so it doesn't matter. The bulk data goes through `recvExact`.

### 36. What does `Net::recvExact` do?

Loops on `recv` until it has read exactly `sz` bytes into the buffer.
Each `recv` may return fewer bytes than requested — TCP doesn't
preserve message boundaries — so you must loop. Returns false if
`recv` returns 0 (peer closed) or negative (error) before `sz` bytes
arrive.

### 37. What does `sendAll` do, and why not just `send`?

`send` may write only some of the bytes you asked it to (especially on
large buffers or slow networks). `sendAll` loops on `send`, advancing
the pointer, until all bytes are written. Without this, you'd silently
truncate big sends.

### 38. What is Winsock and why initialize it?

Winsock is Windows' socket API. Unlike POSIX systems where you can
just call `socket()` directly, Windows requires you to first call
`WSAStartup` to initialize the network DLLs. Forgetting this returns
mysterious errors. `WSACleanup` is the matching shutdown call.

### 39. SOCK_STREAM vs SOCK_DGRAM?

- `SOCK_STREAM` → TCP, byte-stream, reliable, ordered, connection-
  oriented. What we use.
- `SOCK_DGRAM` → UDP, packet, unreliable, unordered, connectionless.

For multi-megabyte file pieces you want SOCK_STREAM. For tiny
heartbeat-like messages where loss is OK, UDP might be considered, but
mixing two protocols complicates code for little gain here.

### 40. How is a TCP connection established?

Three-way handshake:
1. Client → SYN (sequence number A)
2. Server → SYN-ACK (acknowledges A, includes server seq B)
3. Client → ACK (acknowledges B)

After this both sides agree on initial sequence numbers and the
connection is open. The kernel does all this for us when we call
`connect` (client) and `accept` (server).

### 41. What's Nagle's algorithm / TCP_NODELAY?

Nagle's algorithm delays sending small packets, hoping to batch them
together. Good for chatty protocols, bad for latency-sensitive ones.
`TCP_NODELAY` disables it. We don't set it explicitly — our pieces are
1 MB, so Nagle has no effect; for the small control messages we don't
care about a few extra ms.

### 42. Why `SO_REUSEADDR`?

When a server stops, the OS keeps the listening port in `TIME_WAIT`
for a couple of minutes. Restarting the server would fail to bind to
the same port. `SO_REUSEADDR` lets you bind anyway. We set it in
`Net::tcpListen` so you can restart the tracker/peer freely during
development.

### 43. Why use non-blocking + select for connect, but blocking for everything else?

To enforce a connect timeout. A blocking `connect` can hang ~75 s on
some unreachable hosts before erroring. We instead:
1. Set the socket non-blocking.
2. Call `connect` (returns immediately, "in progress").
3. `select()` with a 5 s timeout on writability.
4. If the socket becomes writable in time, the connect succeeded.
5. Set the socket back to blocking for normal use.

This bounds the worst-case time to fail-fast on dead peers.

### 44. What's `INET_ADDRSTRLEN`?

The maximum length of an IPv4 address string ("255.255.255.255" + null
terminator = 16). We use it when converting a `struct in_addr` to a
human-readable string via `inet_ntop`.

### 45. What happens when peers are behind NAT / on different subnets?

Out of the box: it works on the same LAN (or anywhere both peers can
directly reach each other's IPs). Across the internet with NAT'd home
routers, neither peer can accept incoming TCP from the other unless a
port is forwarded. Solutions:
- **Port forwarding** (manual).
- **UPnP / NAT-PMP** to ask the router.
- **STUN / hole-punching** for UDP-based NAT traversal.
- **A relay server** as a fallback.

This project doesn't implement any — it's LAN-grade.

---

## D. C++ language & STL (46–60)

### 46. Why C++17?

We use a few C++17 features:
- Structured bindings (in some places).
- `if constexpr` (not heavily used, but available).
- `<filesystem>` could be useful (we use it sparingly).
- Inline variables — used in `Protocol.h` for the command name
  constants without requiring a `.cpp`.

C++17 is the minimum that gives us inline variables, which makes
header-only constant definitions clean.

### 47. What does `using namespace std;` do? Why might it be discouraged in headers?

It pulls every name from `std` into the current scope, so you can
write `string` instead of `std::string`. Convenient.

**In headers**, every translation unit that includes the header gets
the pollution. If your code or a third-party library defines its own
`vector` or `string`, ambiguous-name errors explode. Best practice: use
`using namespace std;` only in `.cpp` files, and write `std::` in
headers. We follow our project's existing style (it's used in headers
too), but acknowledge the risk.

### 48. What's `#pragma once`?

A header guard. Tells the compiler to include this file at most once
per translation unit. Equivalent to the older idiom:

```cpp
#ifndef FOO_H
#define FOO_H
...
#endif
```

`#pragma once` is non-standard but supported everywhere and easier to
maintain.

### 49. What does `LockGuard` demonstrate? Explain RAII.

RAII = **Resource Acquisition Is Initialization**. The lifetime of a
resource (mutex lock, file handle, memory) is bound to the lifetime of
an object. The constructor acquires; the destructor releases.

```cpp
{
    LockGuard lock(mu_);    // constructor locks mu_
    data_[k] = v;
    // destructor unlocks mu_ when scope ends, even if exception thrown
}
```

You can't forget to unlock, and exceptions don't leak locks. The same
idea applies to `unique_ptr`, `lock_guard`, `ifstream` (closes file on
destruction).

### 50. What's a smart pointer? Why `unique_ptr<Scheduler>`?

A smart pointer manages a raw pointer with RAII semantics. `unique_ptr`
owns the object exclusively and deletes it when the unique_ptr goes out
of scope.

In `DownloadManager`, `sched_` is `unique_ptr<Scheduler>` because the
Scheduler is created on `startDownload` and destroyed when the download
ends. `unique_ptr` makes the ownership explicit and automatic — no
manual `delete`, no memory leak.

### 51. What is `move` semantics? Where used?

Move transfers ownership of a resource from one object to another
without copying. The source ends up in a valid-but-unspecified state
(typically empty).

We use it in `Thread::start(function<void()> fn)` — `fn` is captured
into a new heap allocation via `move(fn)` so we don't copy the lambda
state (which could be large).

### 52. Why `vector<char>` for piece buffers?

- Resizable, owns its memory, no manual `delete`.
- `data()` gives us a raw `char*` for `recv`/`send`.
- Iterator-friendly, integrates with the rest of the STL.

A raw buffer (`new char[n]`) would work but requires manual cleanup and
is exception-unsafe. `vector<char>` gives the same performance with
none of the headaches.

### 53. STL containers used and why?

- `vector` — for hash lists, peer lists, owned-pieces bitvector
  (technically `vector<bool>`), piece buffers. Always for sequences
  with index access.
- `set<int>` — for owned pieces of a peer in the tracker. Constant-time
  membership test; sorted output for free.
- `unordered_map<string, X>` — for tracker DB indexed by peerId or
  fileName. Hash-map: average O(1) lookup. We chose unordered over
  ordered because we never need sorted traversal.
- `queue` — for the pending-client queue inside UploadServer.

### 54. What's `inline` for the constants in `Protocol.h`?

```cpp
inline const char* REGISTER_PEER = "REGISTER_PEER";
```

Without `inline`, this is a definition. If multiple .cpp files include
the header, the linker complains about duplicate definitions.

`inline` (C++17) tells the linker "any of these definitions is
acceptable, pick one." Lets us define constants in a header without
needing a separate `Protocol.cpp`.

### 55. Header-only `Threading.h` — pros and cons?

**Pros:**
- No separate .cpp to maintain.
- The compiler can inline tiny wrapper methods (lock, unlock) directly.

**Cons:**
- Every .cpp that includes it compiles the bodies fresh — slow if it
  grows.
- A change to `Threading.h` triggers rebuild of every file that
  includes it.

For tiny wrappers around Win32, header-only is fine.

### 56. `mutable Mutex mu_` — why `mutable`?

A `const` member function promises not to modify the object's
observable state. But to lock a mutex you call `lock()` on it, which
modifies it. `mutable` allows a member to be modified even from a
`const` function.

So `DownloadStatus DownloadManager::status() const { LockGuard lock(statusMu_); ... }`
can lock `statusMu_` even though `status()` is `const`.

### 57. What's `atomic<bool>`? Why use it?

A primitive that lets multiple threads read/write a bool without races
or torn reads, without using a mutex. Operations are lock-free at the
hardware level.

We use `running_` flags so a worker thread can periodically check "am
I supposed to stop?" without paying for a lock acquisition. Setting
`running_ = false` from another thread is also race-safe.

### 58. Default member initializers — what do they buy?

```cpp
struct PeerInfo {
    int port = 0;
    time_t lastHeartbeat = 0;
};
```

You don't have to remember to initialize every member in every
constructor. Bare struct literal `PeerInfo p;` already has predictable
zero values. Reduces "garbage-value" bugs.

### 59. `static` vs instance member functions in `HashManager`?

```cpp
class HashManager {
public:
    static string hashPiece(const char* data, size_t sz);
};
```

A `static` member function doesn't operate on an instance — no `this`
pointer. Used here because hashing is stateless — you just feed in
bytes and get a hash out. You'd call `HashManager::hashPiece(...)`, no
instance needed.

### 60. What's `streamsize` and why use it?

The signed integer type returned by stream operations like
`f.gcount()` (bytes read in the last operation). It's signed because -1
could mean failure on some operations. We use it where we receive byte
counts from streams, then cast appropriately.

---

## E. Threading & concurrency (61–75)

### 61. What threads run inside a peer?

During an active download, up to ~17 threads:
- 1 main thread.
- 1 UploadServer accept thread.
- 10 UploadServer worker threads (default max).
- 1 TrackerClient heartbeat thread.
- 1 Dashboard print thread.
- N DownloadManager worker threads (one per swarm peer).
- 1 DownloadManager sync thread (updates tracker every 5 s).

Most are idle waiting on I/O most of the time.

### 62. What is a race condition?

When the result of a program depends on the relative timing of two or
more threads accessing shared data, without synchronization. E.g., two
threads incrementing a shared counter without a lock can both read 0,
both increment to 1, both write 1 — final value 1 instead of 2.

### 63. How does this project prevent race conditions?

Three tools:
- **Mutex around shared state** — each manager has its own `mu_` and
  every read/write goes through `LockGuard`.
- **Atomic flags** for single boolean state checks like `running_`.
- **Condition variables** (`CondVar`) for producer-consumer signaling.

Plus an architectural choice: each component owns its data; nothing is
silently shared without a lock.

### 64. What's a deadlock? How do we avoid it?

Two threads each holding a lock and waiting for the other's lock —
both stuck forever. We avoid it primarily by:
- **Short critical sections** — locks held only for the bare minimum.
- **Never holding two locks** in this codebase. Each manager has one
  mutex and code rarely nests them.
- **Consistent ordering** if you ever do need multiple locks — same
  order everywhere.

### 65. What's a condition variable, where used?

A primitive that lets a thread block until another thread signals it.
Cheaper than spin-polling.

In `UploadServer`: the accept thread pushes a client socket onto a
queue, then calls `qCv_.notify_one()`. Worker threads sit in
`qCv_.wait(lock, ...)` until they're signaled. The lock is released
during wait and re-acquired on wakeup.

### 66. Why a worker pool in UploadServer?

To bound concurrency. We could spawn a thread per incoming connection,
but that scales poorly — 1,000 incoming peers → 1,000 threads → lots
of context switching and memory overhead. A bounded pool (10 workers)
means at most 10 piece transfers are happening concurrently. Excess
clients wait in the queue.

### 67. Why one worker per peer in DownloadManager?

Per-peer worker = one open TCP connection to one source. Different
peers can be reached in parallel without any one peer being a
bottleneck. If you only had one download thread, you'd be limited to
one source's bandwidth.

### 68. Atomic flag for shutdown — explain.

```cpp
atomic<bool> running_{true};

// thread:
while (running_) { ... }

// shutdown:
running_ = false;
```

The atomic guarantees the write is visible to the reading thread
without a lock. Each loop iteration the thread sees the updated value
and exits cleanly.

### 69. Producer-consumer with an example.

In `UploadServer`:
- **Producer**: accept thread loops `accept()`, pushes the resulting
  socket into a shared queue.
- **Consumer**: worker threads loop `pop` from the queue and handle
  the client.

They communicate via:
- A shared queue (the bounded buffer).
- A mutex (so push/pop aren't racy).
- A condition variable (so workers can sleep until there's work).

### 70. How does the heartbeat thread work?

Background loop:

```cpp
while (running_) {
    sendCommand(buildMsg(HEARTBEAT, {peerId}));
    for (int i = 0; i < interval && running_; ++i)
        sleepSec(1);
}
```

Note the inner sleep loop checks `running_` every second, so shutdown
is responsive — you don't wait the full 5 s after stopping.

### 71. Win32 CRITICAL_SECTION vs std::mutex?

Both serve the same purpose. `CRITICAL_SECTION` is Win32-native, very
fast for in-process locks (no kernel transition for uncontended
locks). `std::mutex` is portable and may wrap `CRITICAL_SECTION` on
Windows anyway. We use `CRITICAL_SECTION` here because the original
author wanted to learn the Win32 primitives, but `std::mutex` would
work identically.

### 72. Why join all threads on shutdown?

To wait for them to finish cleanly. If you just exit `main`, the
process dies and threads are terminated abruptly mid-syscall — file
handles may be left open, sockets not properly closed, memory leaked.
`join()` blocks until the thread function returns, guaranteeing
graceful termination.

### 73. detach vs join?

- **join()** — wait for the thread to finish, then we can reuse the
  handle.
- **detach()** — let the thread run independently; we lose the handle.
  Useful for fire-and-forget threads, but their cleanup is invisible
  to us.

We always `join` in this project because we want deterministic
shutdown.

### 74. What are spurious wakeups?

A `wait` on a condition variable can return without anyone having
signaled it (rare but possible due to OS implementation details).
Therefore you always wait inside a loop:

```cpp
while (!condition) cv.wait(lock);
```

Or use the predicate form: `cv.wait(lock, [&]{ return condition; })`,
which loops internally for you.

### 75. Are STL containers thread-safe?

No — by default. Concurrent reads of an unmodified container are safe.
Concurrent writes or write+read are not. We protect all shared
containers (the tracker DB, the bitvectors, the pending queue) with
mutexes.

---

## F. Implementation deep dive (76–85)

### 76. Trace what happens when a peer starts.

1. `main` parses CLI args into a `Config`.
2. `Net::initWinsock()` initializes Winsock.
3. `Peer` constructor wires up all sub-objects (TrackerClient,
   UploadServer, PieceManager, ...).
4. `Peer::start()`:
   - Ensures `shared/` and `downloads/` dirs.
   - `UploadServer::start()` — bind port, spawn accept thread + 10
     worker threads.
   - `TrackerClient::start()` — send REGISTER_PEER, get OK, spawn
     heartbeat thread.
   - `Dashboard::start()` — spawn print thread (if applicable).
5. Either run REPL or one-shot `--share`/`--download` from `main`.

### 77. How is a file hashed for sharing?

`HashManager::hashFile(path, pieceSize)`:

```cpp
ifstream f(path, ios::binary);
vector<char> buf(pieceSize);
while (true) {
    f.read(buf.data(), pieceSize);
    streamsize n = f.gcount();
    if (n <= 0) break;
    hashes.push_back(hashPiece(buf.data(), (size_t)n));
    if (n < pieceSize) break;  // last piece (smaller than 1MB)
}
```

Open the file in binary mode, read 1 MB at a time, hash each chunk,
stop when EOF reached. Returns the vector of hashes — one per piece.

### 78. Trace the journey of one piece (see also section 12 of the guide).

1. Leecher's scheduler picks piece N (rarest one this peer has).
2. Leecher sends `REQUEST_PIECE|fileName|N\n`.
3. Seeder's UploadServer worker reads the line.
4. Seeder's PieceManager seeks to `N * 1MB` in source file, reads 1 MB
   into `data`.
5. Seeder sends `SEND_PIECE|fileName|N|size\n` then the raw bytes.
6. Leecher reads header line via `recvLine`, parses size.
7. Leecher reads `size` raw bytes via `recvExact`.
8. Leecher computes FNV hash, compares to `hashes_[N]`.
9. If match: write to `downloads/file.pieces/piece_N.bin`, mark owned.
10. Sync thread next iteration sends `UPDATE_PIECES` with the new piece.

### 79. How is the assembled file produced?

`FileAssembler::assemble(name, pieceCount, expectedSize)`:

```cpp
ofstream out(outPath, ios::binary | ios::trunc);
for (int i = 0; i < pieceCount; ++i) {
    vector<char> data;
    Util::readFile(pieceDir + "/piece_" + to_string(i) + ".bin", data);
    out.write(data.data(), data.size());
}
out.flush();
if (Util::fileSize(outPath) != expectedSize) return false;
return true;
```

Open output file, sequentially read each piece file in order, write to
output, verify size at end. Sequential I/O is fast.

### 80. How does resume work?

`ResumeManager::saveState(fileName, bitvector)` writes a tiny file
listing which piece indices are owned. On startup before a download,
`DownloadManager::startDownload` calls `loadState`, which reads the
file and sets the bitvector. The scheduler initializes its "complete"
set from this bitvector. Workers skip those pieces.

If you crash right after writing piece N but before saving state,
piece N is on disk but resume thinks it's missing. Next download will
re-fetch it. Minor inefficiency, not correctness bug.

### 81. What's stored in memory on the tracker?

```cpp
TrackerDatabase {
    unordered_map<string, PeerInfo>      peers;
    unordered_map<string, TrackerRecord> files;
}
```

Where:
- `PeerInfo` = peerId, ip, port, lastHeartbeat.
- `TrackerRecord` = file metadata (size, count, hashes) + `peerPieces`
  (peerId → set of owned piece indices).

All in RAM. No persistence. Tracker restart loses everything.

### 82. What's stored on disk on a peer?

In the peer's working directory:
- `shared/` — files this peer offers (read by the seeder logic).
- `downloads/<name>.pieces/piece_N.bin` — chunks being staged during
  download.
- `downloads/<name>` — final assembled file.
- (Optional) `.resume/<name>` — bitvector for resume.

In memory:
- `PieceManager::owned_` bitvector.
- `DownloadManager::peers_` list.
- Network buffers in transit.

### 83. Directory structure at runtime.

```
<peer-cwd>/
  shared/
    movie.mp4               ← what this peer shares
  downloads/
    book.pdf.pieces/         ← in-progress chunks
      piece_0.bin
      piece_1.bin
      ...
    book.pdf                 ← assembled output (after completion)
```

The `.pieces` suffix on the chunk dir prevents collision with the
output file name — the bug we fixed.

### 84. How does the tracker know a peer is dead?

It doesn't, until the heartbeat monitor catches on. Every peer sends
`HEARTBEAT|peerId` every 5 s. The tracker stamps `lastHeartbeat = now`
on each receipt. `HeartbeatMonitor` runs in its own thread and every
~5 s sweeps the peer table: if `now - lastHeartbeat > 3 * interval`
(~15 s), the peer is evicted from the table and removed from every
swarm.

### 85. What happens if hash verification fails?

```cpp
if (!HashManager::verify(data, hashes_[idx])) {
    scheduler.markMissing(idx);
    continue;
}
```

The piece is discarded. It's marked as missing in the scheduler so it
will be re-picked, ideally from a different peer next time. We don't
"punish" the offending peer (real BitTorrent's optimistic unchoke
algorithm does), but we also don't write garbage to disk.

---

## G. System design & trade-offs (86–95)

### 86. Why fixed 1 MB piece size? What if too big or too small?

A trade-off:

- **Smaller pieces**: more parallelism, finer-grained verification,
  but more protocol overhead per byte (more headers, more round
  trips), more in-memory state (longer bitvectors).
- **Larger pieces**: less overhead, simpler scheduler state, but more
  wasted bandwidth on a corrupt/discarded piece, and longer time to
  share first piece.

1 MB is a sweet spot for our LAN-grade use case. Real BitTorrent
clients use anywhere from 256 KB to 16 MB depending on file size.

### 87. Why FNV-1a now, SHA-256 later? Trade-offs?

- **FNV-1a**: very fast (a few CPU cycles per byte), simple, no
  dependency. Good enough to detect accidental corruption.
- **SHA-256**: cryptographically secure (resists deliberate
  collisions), industry standard, slower but still fast on modern
  hardware.

FNV gets us working end-to-end. SHA-256 is the eventual upgrade
because:
- An attacker on the network could craft bytes that FNV-collide and
  pass verification, planting bad data.
- Standard P2P protocols use SHA-256 (or SHA-1 in BitTorrent v1) for
  interoperability.

Both produce the same 64-char hex output, so the swap is a one-file
change.

### 88. Why centralized tracker vs DHT?

- **Centralized tracker**: simpler to implement and reason about.
  Single point of truth. Easy debugging.
- **DHT (distributed hash table)**: peers themselves act as the
  directory, replacing the tracker. Resilient to a single node failure,
  censorship-resistant.

We chose centralized because it's an order of magnitude simpler. Real
BitTorrent uses both — DHT as a fallback when trackers are down.

### 89. Why not HTTP for the protocol?

HTTP is request/response oriented and would force one TCP connection
per request — expensive for piece transfers. It also has headers in
each message (~hundreds of bytes overhead), which hurts on small
control messages. Our custom protocol is leaner. The downside is no
ecosystem: no browser support, no off-the-shelf load balancers.

### 90. Scalability of this design?

Reasonable bottlenecks:
- **Tracker** is single-threaded-ish (one mutex per manager, many
  short critical sections). Could handle thousands of peers but not
  millions.
- **Per-peer 10-worker upload pool** caps concurrent uploads. A
  popular seeder with 100 leechers would queue 90 of them.
- **In-memory tracker state** dies on restart. No persistence.

To scale 10x: shard the tracker by file name, persist state, increase
the worker pool. To scale 100x: distributed tracker (DHT), gossip
discovery, multi-tracker setup.

### 91. Security weaknesses?

- **Plaintext protocol**: anyone on the LAN can see file names and
  even file contents flying by.
- **No authentication**: a malicious peer can register as any peerId
  and corrupt the swarm state.
- **FNV hash is not collision-resistant** against a determined
  attacker.
- **No DoS protection**: a single client can flood the tracker.

Mitigations: TLS for transport, peer identity signing, SHA-256, rate
limits.

### 92. How does this compare to real BitTorrent?

Similarities:
- Tracker, swarm, pieces, hash verification, rarest-first.

Differences (real BitTorrent has):
- Optimistic unchoking and tit-for-tat to incentivize uploaders.
- Magnet links (DHT-based peer discovery, no tracker needed).
- Multi-file torrents with their own metadata structure (.torrent).
- Endgame mode (request the same piece from multiple peers near
  completion).
- Pipelined block requests (sub-piece transfers).
- Encryption / obfuscation.

Ours is the simpler educational subset.

### 93. Why text protocol vs full binary?

Pure binary would have:
- **Less overhead** (no `|` and ASCII digits for numbers).
- **Faster parsing** (memcpy structs).
- **More opaque debugging** (you need a hex dump tool).

Pure text would have:
- **Wasteful encoding** for binary payload (base64 → 33% overhead).
- **Easier debugging** (just `printf`).

Our hybrid gets the best of both: text where readability matters
(control), binary where bytes matter (payload).

### 94. What's rarest-first and why is it good?

Strategy: among the pieces I don't have, prefer to download the one
held by the fewest peers in the swarm.

Why it's good:
- **Swarm health**: rare pieces could vanish if their few holders
  leave. Fetching them first spreads them to more peers.
- **Self-balancing**: keeps piece availability roughly even across the
  swarm.
- **Avoids the "popular piece" thundering herd**: if everyone
  downloaded piece 0 first, the seeder is overwhelmed.

### 95. Alternative piece selection strategies?

- **Sequential / first-piece-first**: simplest, but no parallelism
  benefit and very fragile.
- **Random**: better than sequential, less coordination than rarest-
  first.
- **Endgame**: near the end of a download, request the same piece
  from multiple peers and take whichever arrives first, to avoid
  one-slow-peer tail latency.
- **Tit-for-tat / priority** based on which peers have uploaded back
  to you (real BitTorrent's incentive system).

---

## H. Bugs, testing, improvements (96–100)

### 96. Describe a real bug you found and fixed.

The **assembly bug**:

Both `PieceManager::initDownload` and `FileAssembler::assemble` computed
the path `downloads/<fileName>` — the first used it as a *directory*
for piece chunks, the second tried to open it as an *output file* for
the assembled result. On the OS, you can't have a directory and a file
with the same name. Result: all pieces downloaded and verified
correctly, but `ofstream` failed at the very end with the misleading
"assembly failed" message.

Fix: suffix the piece directory with `.pieces` so they're distinct:

```cpp
pieceDir_ = cfg_.downloadDir + "/" + fileName + ".pieces";
```

**Lesson**: two unrelated bits of code computing the same path are
coupled-by-coincidence. A helper function returning both paths would
have made the duplication visible and the bug impossible.

### 97. How would you add unit tests?

- **Choose a framework**: GoogleTest is the C++ standard.
- **Isolate testable units**: HashManager (pure function), Protocol
  (build/split), Scheduler (logic on bitvectors), PieceManager (in-
  memory only if you mock the disk).
- **Integration tests**: spin up a tracker + 1 seeder + 1 leecher in
  the test, run a download of a small in-memory file, assert hashes
  match.
- **Test the bug**: a regression test for the assembly bug, asserting
  that pieceDir and outputPath are different.

Build setup: add `enable_testing()` and `add_subdirectory(test)` to
CMakeLists.

### 98. How would you make this cross-platform?

Two files have Win32 / Winsock dependencies:
- `Net.cpp` — `WSAStartup`, `closesocket`, `inet_pton`. Wrap each with
  `#ifdef _WIN32` for Win or POSIX equivalents (`close`, `inet_aton`).
- `Threading.h` — `CRITICAL_SECTION`, `Sleep`, `_beginthreadex`.
  Replace with `std::mutex`, `std::this_thread::sleep_for`,
  `std::thread`.

Then change `CMakeLists.txt` to only link `ws2_32` on Windows. Code
elsewhere is already platform-neutral (STL, iostreams).

### 99. How would you scale to 1000 peers?

- **Tracker**: profile and shard. If one mutex per manager is a
  contention point, switch to finer-grained locking (per-file mutex
  inside SwarmManager). Persist state to SQLite/Redis so restart
  doesn't lose the swarm. Add a worker pool for incoming TCP
  connections.
- **Peer**: raise `maxUploadThreads` (currently 10). Implement
  rate limiting so a single peer doesn't get crushed.
- **Discovery**: at 1000 peers, the `GET_PEERS` response can be huge.
  Cap to N random peers, let leechers do their own filtering.
- **Bandwidth**: enable TCP window scaling (kernel default these days),
  consider TCP_NODELAY for small messages.

### 100. How would you add encryption?

Two layers to consider:

**Transport** (peer ↔ tracker, peer ↔ peer):
- Use TLS via OpenSSL. Each side gets a cert. Wrap the socket with
  `SSL_*` calls — sendAll/recvLine/recvExact still work conceptually
  but route through OpenSSL.
- For LAN-only use, a simpler option is a Noise protocol handshake
  with a pre-shared key.

**Identity** (so peerId can't be spoofed):
- Each peer generates a keypair on first run. Public key becomes their
  peerId. Every message to the tracker is signed; tracker verifies.
- Prevents Sybil attacks (one bad actor pretending to be many peers).

Cost: certificate management, performance overhead (~5–10% CPU for
TLS at GbE speeds), more complex setup. For a learning project,
implementing this is a significant next milestone.

---

*End of 100 questions. If you can answer ~70 of these confidently in
your own words, you understand this codebase thoroughly enough to
discuss it in an interview.*
