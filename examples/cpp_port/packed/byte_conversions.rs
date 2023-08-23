pub fn usize_to_u8(n: usize) -> u8 {
    n.try_into().expect("Value too large!") // TODO
}

pub fn u8_to_usize(n: u8) -> usize {
    n.into()
}
