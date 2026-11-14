#[cfg(test)]
mod tests {
    #[test]
    fn addition_keeps_a_long_but_readable_test_name() { println!("Rust captured success"); assert_eq!(2+2,4); }
    #[test]
    fn duplicate() { assert_eq!(2+2,5, "Rust failure payload"); }
    mod nested {
        #[test]
        fn duplicate() { assert!(true); }
    }
    #[test]
    #[ignore]
    fn skipped() {}
}
