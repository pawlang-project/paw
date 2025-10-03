pub fn parse_program(src: &str, file: FileId) -> Result<Program> {
    let mut items = Vec::new();
    let mut pairs = PawParser::parse(Rule::program, src)?;
    let root = pairs.next().ok_or_else(|| anyhow!("empty program"))?;
    debug_assert_eq!(root.as_rule(), Rule::program);

    for it in root.into_inner() {
        match it.as_rule() {
            Rule::item => {
                let inner = it.into_inner().next().ok_or_else(|| anyhow!("empty item"))?;
                items.push(build_item(inner, file)?);
            }
            Rule::EOI => { /* ignore */ }
            other => return Err(anyhow!("program: unexpected node: {:?}", other)),
        }
    }
    Ok(Program { items })
}