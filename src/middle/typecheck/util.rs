fn show_ty(t: &Ty) -> String {
    use std::fmt::Write;
    let mut s = String::new();
    let _ = write!(&mut s, "{t}");
    s
}