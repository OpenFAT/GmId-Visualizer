//! The committed `gmid_lut.wasm` through the real Schemify host (`schemify_plugin`, the crate the
//! editor loads packages with, default fuel): `Load` composes the `gmid.*` functions and the two
//! commands without a fault, and a function called before any table is loaded answers `E_RANGE`
//! (no table) rather than trapping. `tests/guest.rs` covers the I/O chain with a stand-in host;
//! this test is the one that breaks when Schemify's host or ABI moves under the module.

use schemify_plugin::abi::{Code, FUEL_DEFAULT, Kind};
use schemify_plugin::{PluginEvents, Plugins, RequestQueues, Snapshots, deliver};

#[test]
fn loads_and_composes_through_the_real_host() {
    let wasm = std::fs::read(concat!(env!("CARGO_MANIFEST_DIR"), "/gmid_lut.wasm"))
        .expect("gmid_lut.wasm is committed; run ./build.sh");
    let mut plugins = Plugins::new();
    let id = plugins
        .load(
            "gmid",
            &wasm,
            &["read_project", "write_project", "spawn:ngspice"],
            FUEL_DEFAULT,
        )
        .expect("the module matches the host's ABI");
    let events = PluginEvents::new();
    let mut snap = Snapshots::new();
    let mut queues = RequestQueues::default();
    deliver(&mut plugins, &events, &mut snap, &mut queues);

    assert!(
        plugins.health().faults().is_empty(),
        "{:?}",
        plugins.health().faults()
    );
    let c = plugins.composed();
    for cmd in ["gmid.characterize", "gmid.reload"] {
        assert!(
            c.of(Kind::Commands).iter().any(|r| r.text.contains(cmd)),
            "{cmd} missing"
        );
    }
    let fns = c.of(Kind::Functions);
    assert!(
        fns.iter().any(|r| r.text.contains("gmid.vgs")),
        "{} function records",
        fns.len()
    );

    // fn_id 0 is gmid.vgs at generation 0; nothing is loaded yet (no project files served).
    let mut out = String::new();
    assert_eq!(
        plugins.call(id, 0, "\"nfet\" 15 5e-7", &mut out),
        Err(Code::Range)
    );
    assert!(
        plugins.health().faults().is_empty(),
        "a range error is not a fault"
    );
}
