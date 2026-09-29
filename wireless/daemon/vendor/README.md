# Vendored LIVI iAP2 crates

`iap2-link` and `iap2-csm` are verbatim copies of the corresponding crates
from `https://github.com/f-io/LIVI`, commit
`6bc9ddb8c26319665b91ed934ca8ccf717c8c44c` (local checkout under
`/Users/luka/Desktop/AUDI_2/Refferences/LIVI`). Their original crate tests
and vectors are included. The copies were compared to that checkout with
`diff -qr` before integration.

The upstream project states `GPL-3.0-or-later`; its `LICENSE` file is copied
here. No source files in these two crates were changed. The enclosing daemon
workspace supplies the crates' inherited Rust edition, 2024.

This directory lets the daemon build without the sibling `Refferences/LIVI`
checkout. It does not change the runtime status of the QNX/CarPlay mod.
