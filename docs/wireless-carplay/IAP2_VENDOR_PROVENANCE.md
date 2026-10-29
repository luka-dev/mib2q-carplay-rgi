# Vendored iAP2 and JSON crates

_MHI2Q wireless CarPlay — technical reference, integrated 2026-09-29._

---

[Wireless overview](README.md) · [Current stock210 lifecycle](STOCK210_RECEIVER_LIFECYCLE.md)

Maintained technical reference. The evidence below distinguishes static analysis, local QNX tests and observations from the unit.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../wireless/daemon/vendor/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📋 Implementation and evidence

`iap2-link` and `iap2-csm` are verbatim copies of the corresponding crates
from LIVI[^1], commit
`6bc9ddb8c26319665b91ed934ca8ccf717c8c44c` (local checkout under
`<workspace>/Refferences/LIVI`). Their original crate tests
and vectors are included. The copies were compared to that checkout with
`diff -qr` before integration.

The upstream project states `GPL-3.0-or-later`; its [LICENSE](../../wireless/daemon/vendor/LICENSE) remains beside the
vendored source. No source files in these two crates were changed. The enclosing daemon
workspace supplies the crates' inherited Rust edition, 2024.

The vendored source directory lets the daemon build without the sibling `Refferences/LIVI`
checkout. It does not change the runtime status of the QNX/CarPlay mod.

## 📦 Factory configuration parser

[`vendor/json`](../../wireless/daemon/vendor/json/) contains the published
`json` crate version **0.12.4**[^2], including its upstream tests and fixtures.
It is a local path dependency with no transitive dependencies, so host and
QNX builds do not need a package registry for this parser. The upstream source
is unchanged; its own `LICENSE-MIT` and `LICENSE-APACHE` files apply to that
directory. The sibling LIVI crates retain the license described above.

The daemon removes factory hash comments before invoking the JSON parser;
that adapter and field validation live in
[`factory_identity.rs`](../../wireless/daemon/src/factory_identity.rs).

## 🔗 Upstream reference

[^1]: f-io. LIVI source snapshot. https://github.com/f-io/LIVI/tree/6bc9ddb8c26319665b91ed934ca8ccf717c8c44c
[^2]: Maciej Hirsz. Published `json` 0.12.4 crate; source repository and dual license are recorded in its included Cargo manifest. https://crates.io/crates/json/0.12.4
