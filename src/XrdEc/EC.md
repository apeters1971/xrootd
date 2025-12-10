# XrdEc module guide

This directory implements client-side erasure coding (EC) support for XRootD. The code builds a shared library (`XrdEc`) that the client loads either through server-provided redirection hints (`xrdec.*` query params) or via a client plug-in config.

## High-level responsibilities
- Build-time flag `-DENABLE_XRDEC=TRUE` (and ISA-L dependency) enables the module.
- `ObjCfg` describes one EC-protected object: striping layout, chunk size, placement group URLs, and optional per-location CGI tokens.
- `StrmWriter` writes data stripes (and parity) into per-host ZIP archives and optional metadata ZIPs; `Reader` does the inverse, pulling stripes back, validating CRCs, and repairing stripes with parity if needed.
- `RedundancyProvider` wraps ISA-L to compute parity and reconstruct missing stripes.
- `ThreadPool` and `BufferPool` provide shared worker threads and reusable block buffers.

## Data layout
- Each object is split into blocks of `nbdata + nbparity` stripes, each of size `chunksize`.
- A block is stored as individual ZIP entries named `obj.<block>.<stripe>` in per-host archives (one archive per placement URL).
- Checksums: every stripe is CRC32c (or ISA-L gzip CRC when requested) stored in the ZIP central directory; optional per-block digest metadata is written when `xrdec.nomtfile=false`.
- Metadata ZIP (when enabled) aggregates the central directories from all data archives so the reader can learn which stripes exist without opening every archive.
- Object size is recorded as xattr `xrdec.filesize` on the data archives; `xrdec.strpver` marks the close time/version.

## Key classes
- `ObjCfg`: immutable per-object layout (counts, sizes, URLs, CGI tokens) and checksum function selector.
- `Config`: singleton holding cached `RedundancyProvider` instances keyed by layout and flag to allow client plug-ins.
- `RedundancyProvider`: prepares ISA-L matrices in the ctor; `compute()` fills missing stripes using parity or replication (for `nbdata==1`).
- `WrtBuff`: accumulates a full data block, computes parity (via `RedundancyProvider`), and spawns CRC jobs in the thread pool.
- `BufferPool`: recycles fixed-size block buffers to avoid reallocations.
- `ThreadPool`: thin wrapper over `XrdCl::JobManager`, used for CRC and recovery tasks.
- `StrmWriter`:
  - `Open()`: opens all data archives in parallel.
  - `Write()`: buffers data, encodes/parities per block, and enqueues background writes to archives (shuffling placement to spread load).
  - `Close()`: flushes residual data, writes optional metadata ZIP, sets xattrs, and closes archives once quorum succeeds.
- `Reader`:
  - `Open()`: opens data archives, reads size xattr or metadata ZIP, and builds a map of available stripes.
  - `Read()`: stripes-aware reads with a per-block cache that validates CRCs; triggers recovery if stripes are missing or corrupted.
  - `VectorRead()`: batch read across hosts, then repairs and copies data into caller buffers.
  - `Close()`: closes any open archives.

## Typical flow
1) Server redirect (or client plug-in config) provides `xrdec.*` parameters → caller builds `ObjCfg`.
2) Writer: `Open` → repeated `Write` → `Close` with optional commit query (`xrdec.close=true&size=...&cksum=...`).
3) Reader: `Open` (fetch metadata/size) → `Read`/`VectorRead` → `Close`.

## Threading and async model
- User callbacks are scheduled via `XrdCl::JobManager` (`ScheduleHandler`) to keep caller threads non-blocking.
- Encoding/CRC work runs in the shared `ThreadPool` (64 workers).
- `StrmWriter` uses a dedicated writer thread that drains encoded buffers and issues parallel writes.
- `Reader` serializes per-block state with mutexes; recovery uses the shared thread pool to run callbacks and checksum validation.

## Notes on metadata options
- With `xrdec.nomtfile=true` the reader derives available stripes from ZIP contents and size xattr only; no side metadata file is produced on write.
- With metadata enabled, a separate `.mt` ZIP is written per placement URL containing the central directory of each data archive.

## Building/tests
- Build with `cmake -DENABLE_XRDEC=TRUE ...`.
- Unit/micro-tests live in `tests/XrdEc` (see `XrdEcTests` and `MicroTest` references); the main library is otherwise exercised via client/server integration.

