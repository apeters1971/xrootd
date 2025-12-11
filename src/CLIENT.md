# XRootD client (XrdCl) overview

This repository’s `src/XrdCl` directory is the C++ client stack for XRootD.
It builds a composable set of transport, scheduling, and protocol helpers that
higher-level APIs (`XrdCl::File`, `XrdCl::FileSystem`, copy jobs, EC helpers)
use to talk to XRootD servers.

## Top-level environment
- `XrdCl::DefaultEnv` is the singleton entry point. It holds global services:
  `PostMaster`, `JobManager`, `TaskManager`, `Log`, `TransportManager`,
  checksum manager, monitor, plugin manager, and the fork handler.
- `DefaultEnv::GetPostMaster()` gives the shared messaging hub; most public APIs
  use it indirectly via `MessageUtils` helpers.

## Messaging pipeline
- `PostMaster` orchestrates channels per target URL. It wires:
  - `Channel` → owns a `Stream` (or substreams) bound to a transport.
  - `Poller` → waits on sockets and dispatches read/write readiness.
  - `TaskManager` → schedules periodic tasks (timeouts, reconnect).
  - `JobManager` → background job pool for user callbacks.
- Send path: `PostMaster::Send` → `Channel::Send` → `Stream::Send`
  enqueues to an `OutQueue`; `Stream::OnReadyToWrite` pops, installs the
  handler into `InQueue`, and hands the message to the transport.
- Receive path: `Stream::OnIncoming` matches messages via `InQueue` to the
  right `MsgHandler` and queues user callbacks on `JobManager`.
- Timeouts: `Stream::Tick` runs periodically to expire `OutQueue` entries and
  ask `InQueue` to report timeouts.

## Core utilities
- `JobManager` is a fixed-size worker pool using a synchronized queue
  (`XrdCl::SyncQueue`).
- `InQueue` tracks message handlers waiting for responses (map keyed by stream
  id) with timeout bookkeeping.
- `OutQueue` holds pending outbound messages with per-message timeout/flags.
- `Poller`/`PollerBuiltIn` provide evented socket readiness; `Stream` registers
  its sockets and toggles up/down links.

## High-level APIs
- `File`, `FileSystem`, and `Operations` build XRootD protocol messages and use
  `MessageUtils` to send/receive via `PostMaster`.
- Copy layer (`CopyJob`, `CopyProcess`, `ClassicCopyJob`, `ThirdPartyCopyJob`,
  `TPFallBackCopyJob`) orchestrates multi-request transfers.
- Plugin layer (`PlugInManager`, `PlugInFactory`, `PlugInInterface`) lets
  transports or auth be overridden per URL.
- Checksum support (`CheckSumManager`, `CheckSumHelper`) and monitoring hooks
  integrate at the environment level.

## Threading model
- Network IO: `Poller` thread(s) drive socket readiness; `TaskManager` drives
  periodic tasks (timeouts/reconnect).
- User callbacks: queued onto `JobManager` workers.
- Internal locking:
  - `Stream` protects per-stream state (`pMutex`) around queues and status.
  - `InQueue` uses `XrdSysRecMutex`.
  - `OutQueue` is not internally locked; callers (e.g., `Stream`) must hold
    their mutex while mutating it.
  - `DefaultEnv` initialization guarded by `sInitMutex`.

## Typical flow
1) A high-level API builds a request and calls `MessageUtils::SendMessage`
   (uses `DefaultEnv::GetPostMaster()`).
2) `Stream` enqueues the message; when writable, the handler is moved to
   `InQueue` and the message is sent.
3) On response, `Stream` matches it to the handler, validates, and dispatches
   a job to `JobManager`, which invokes the user callback.
4) Periodic ticks expire stale requests and perform reconnects as needed.

