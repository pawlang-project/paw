fn build_pattern(p: Pair<Rule>) -> Result<Pattern> {
    Ok(match p.as_rule() {
        Rule::pattern => {
            let mut inner = p.into_inner();
            if let Some(q) = inner.next() {
                build_pattern(q)?
            } else {
                Pattern::Wild
            }
        }
        Rule::long_lit => Pattern::Long(parse_long_lit(p.as_str())?),
        Rule::int_lit  => parse_int_pattern(p.as_str())?,
        Rule::char_lit => Pattern::Char(parse_char_lit(p.as_str())?),
        Rule::bool_lit => Pattern::Bool(p.as_str() == "true"),
        r => return Err(anyhow!("unexpected pattern: {:?}", r)),
    })
}

fn parse_int_pattern(s: &str) -> Result<Pattern> {
    let v = s.parse::<i128>()?;
    if v >= i32::MIN as i128 && v <= i32::MAX as i128 {
        Ok(Pattern::Int(v as i32))
    } else if v >= i64::MIN as i128 && v <= i64::MAX as i128 {
        Ok(Pattern::Long(v as i64))
    } else {
        bail!("pattern integer out of i64 range: {}", s);
    }
}