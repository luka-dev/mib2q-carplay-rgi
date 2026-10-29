//! File-session flow control. Image bytes stay in the common native parser;
//! this ledger owns only negotiated sizes, Start/Success/Cancel replies.
#[derive(Clone, Copy)]
struct Transfer { id:u8, expected:Option<usize>, received:usize, started:bool }
#[derive(Default)]
pub(crate) struct Transfers { pending:Vec<Transfer> }
impl Transfers {
    pub fn clear(&mut self) { self.pending.clear(); }
    pub fn accept(&mut self,data:&[u8])->(bool,Vec<[u8;2]>) {
        if data.len()<2 {return (false,vec![]);}
        let id=data[0];let flags=data[1];let op=flags&15;
        if op==4 {
            self.pending.retain(|t|t.id!=id);
            if !matches!(data.len(),10|12..=8191) { return (true,vec![[id,2]]); }
            let size=u64::from_be_bytes(data[2..10].try_into().unwrap());
            if size>800_000 || (data.len()>=12&&data[10..12]!=[0,2]) {return (true,vec![[id,2]]);}
            if size==0&&data.len()>=12 {return (true,vec![[id,1],[id,5]]);}
            let mut replies=vec![];
            if self.pending.len()==4 {replies.push([self.pending.remove(0).id,2]);}
            self.pending.push(Transfer{id,expected:if size==0{None}else{Some(size as usize)},received:0,started:false});
            replies.push([id,1]);return (true,replies);
        }
        let Some(index)=self.pending.iter().position(|t|t.id==id) else {return (false,vec![]);};
        if matches!(op,2|5|6) {self.pending.remove(index);return (true,vec![]);}
        if op!=0 || flags&0x30!=0 {return (false,vec![]);}
        let t=&mut self.pending[index];
        if flags&0x80!=0 {t.received=0;t.started=true;}
        if !t.started || data.len()-2>800_000-t.received || t.expected.is_some_and(|n|data.len()-2>n-t.received) {
            self.pending.remove(index);return (false,vec![[id,2]]);
        }
        t.received+=data.len()-2;
        if flags&0x40!=0 {
            let valid=t.expected.is_none_or(|n|n==t.received);self.pending.remove(index);
            return (valid,vec![[id,if valid{5}else{2}]]);
        }
        (true,vec![])
    }
}
#[cfg(test)]mod tests {
    use super::*;
    fn setup(id:u8,size:u64)->Vec<u8>{let mut p=vec![id,4];p.extend_from_slice(&size.to_be_bytes());p.extend([0,2]);p}
    #[test]fn native_artwork_handshake_and_bounds() {
        let mut t=Transfers::default();
        assert_eq!(t.accept(&setup(129,5)),(true,vec![[129,1]]));
        assert_eq!(t.accept(&[129,0x80,1,2]),(true,vec![]));
        assert_eq!(t.accept(&[129,0x40,3,4,5]),(true,vec![[129,5]]));
        assert_eq!(t.accept(&[129,0x40,5]),(false,vec![]));
        assert_eq!(t.accept(&setup(1,800_001)),(true,vec![[1,2]]));
        t.accept(&setup(1,4));assert_eq!(t.accept(&[1,0xc0,1]),(false,vec![[1,2]]));
        t.accept(&setup(1,4));assert_eq!(t.accept(&[1,0,1]),(false,vec![[1,2]]));
        for id in 0..4 {t.accept(&setup(id,8));}
        assert_eq!(t.accept(&setup(5,8)),(true,vec![[0,2],[5,1]]));
        t.clear();assert!(t.pending.is_empty());
        let mut random=19u32;
        for n in 0..20_000 {let mut p=setup((n%256)as u8,8);random=random.wrapping_mul(1664525).wrapping_add(1013904223);let i=random as usize%p.len();p[i]^=(random>>24)as u8;t.accept(&p);assert!(t.pending.len()<=4);}
    }
}
