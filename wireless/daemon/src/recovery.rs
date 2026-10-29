//! RFCOMM is a bootstrap lifetime, not a media-session lifetime. This ledger
//! never infers native teardown from TCP/DNS failure or SI INITIALIZED alone.
use crate::receiver_control::ReceiverPhase;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Stage {
    Dormant,
    Starting,
    AwaitingMedia,
    Streaming,
    Draining,
    Blocked,
    AwaitPeer,
}

pub struct Recovery {
    generation: Option<u32>,
    template: Option<Vec<u8>>,
    stage: Stage,
    due: u64,
    probing: bool,
}
impl Default for Recovery {
    fn default() -> Self {
        Self {
            generation: None,
            template: None,
            stage: Stage::Dormant,
            due: 0,
            probing: false,
        }
    }
}
impl Recovery {
    pub fn reset(&mut self, generation: Option<u32>) {
        *self = Self {
            generation,
            ..Self::default()
        };
    }
    pub fn offer(&mut self, generation: u32, reply: Vec<u8>) -> bool {
        if self.generation != Some(generation) {
            return false;
        }
        self.template = Some(reply);
        if !matches!(self.stage, Stage::Dormant | Stage::AwaitPeer) {
            return false;
        }
        self.stage = Stage::Starting;
        self.probing = false;
        true
    }
    pub fn started(&mut self, generation: u32, success: bool, now: u64) {
        if self.generation != Some(generation) || self.stage != Stage::Starting {
            return;
        }
        self.stage = if success {
            Stage::AwaitingMedia
        } else {
            Stage::AwaitPeer
        };
        self.due = now.saturating_add(2000);
        self.probing = false;
    }
    pub fn observe(&mut self, generation: u32, phase: ReceiverPhase, now: u64) -> bool {
        if self.generation != Some(generation) {
            return false;
        }
        let next = match phase {
            ReceiverPhase::Streaming => Stage::Streaming,
            ReceiverPhase::Draining => Stage::Draining,
            ReceiverPhase::Blocked => Stage::Blocked,
            ReceiverPhase::Ended | ReceiverPhase::Restarted => Stage::AwaitPeer,
            _ => return false,
        };
        self.stage = next;
        self.probing = false;
        self.due = now.saturating_add(1000);
        true // Cancel any old discovery/connect task on this native transition.
    }
    pub fn probe_due(&mut self, now: u64, available: bool) -> Option<u32> {
        if !available
            || self.stage != Stage::AwaitPeer
            || self.probing
            || self.template.is_none()
            || now < self.due
        {
            return None;
        }
        self.probing = true;
        self.generation
    }
    pub fn reachable(&mut self, generation: u32, available: bool) -> Option<Vec<u8>> {
        if self.generation != Some(generation)
            || self.stage != Stage::AwaitPeer
            || !self.probing
            || !available
        {
            return None;
        }
        self.stage = Stage::Starting;
        self.probing = false;
        self.template.clone()
    }
    pub fn probe_cancelled(&mut self) {
        if self.stage == Stage::AwaitPeer {
            self.probing = false;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn unchanged_bt_waits_for_native_end_then_same_peer_probe() {
        let mut r = Recovery::default();
        r.reset(Some(7));
        assert!(r.offer(7, vec![1]));
        assert!(!r.offer(7, vec![1]));
        r.started(7, true, 0);
        r.observe(7, ReceiverPhase::Streaming, 1);
        assert!(!r.offer(7, vec![2]));
        assert_eq!(r.probe_due(99999, true), None);
        r.observe(7, ReceiverPhase::Draining, 100000);
        assert_eq!(r.probe_due(200000, true), None);
        r.observe(7, ReceiverPhase::Ended, 200001);
        assert_eq!(r.probe_due(201000, true), None);
        assert_eq!(r.probe_due(201001, false), None);
        assert_eq!(r.probe_due(201001, true), Some(7));
        assert_eq!(r.probe_due(999999, true), None);
        assert_eq!(r.reachable(6, true), None);
        assert_eq!(r.reachable(7, true), Some(vec![2]));
        r.started(7, true, 201100);
        r.observe(7, ReceiverPhase::Streaming, 201200);
        assert!(!r.offer(7, vec![3])); // Repeated phone availability cannot re-activate.
        r.reset(Some(8));
        assert!(!r.observe(7, ReceiverPhase::Ended, 400000));
        assert_eq!(r.probe_due(500000, true), None);
    }
    #[test]
    fn native_si_child_restart_rechecks_peer_without_repeating_bluetooth_auth() {
        let mut r = Recovery::default();
        r.reset(Some(7));
        assert!(r.offer(7, vec![1]));
        r.started(7, true, 0);
        r.observe(7, ReceiverPhase::Streaming, 1);
        assert!(r.observe(7, ReceiverPhase::Restarted, 2));
        assert_eq!(r.probe_due(1001, true), None);
        assert_eq!(r.probe_due(1002, false), None);
        assert_eq!(r.probe_due(1002, true), Some(7));
        assert_eq!(r.reachable(7, true), Some(vec![1]));
        assert!(!r.offer(7, vec![2]));
    }
    #[test]
    fn wired_preemption_and_failed_start_never_skip_cleanup_signal() {
        let mut r = Recovery::default();
        r.reset(Some(1));
        assert!(r.offer(1, vec![1]));
        r.started(1, false, 0);
        r.observe(1, ReceiverPhase::Draining, 1);
        assert_eq!(r.probe_due(999999, true), None);
        r.observe(1, ReceiverPhase::Blocked, 2);
        assert!(!r.offer(1, vec![2]));
        assert_eq!(r.probe_due(999999, true), None);
        r.observe(1, ReceiverPhase::Ended, 1000);
        assert_eq!(r.probe_due(2000, true), Some(1));
    }
}
