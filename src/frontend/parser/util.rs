#[inline]
fn sp_of(rule: &Pair<Rule>, file: FileId) -> Span {
    let s = rule.as_span();
    Span { file, start: s.start(), end: s.end() }
}

#[inline]
fn span_of_expr(e: &Expr) -> Span {
    match e {
        Expr::Int   { span, .. }
        | Expr::Long  { span, .. }
        | Expr::Float { span, .. }
        | Expr::Double{ span, .. }
        | Expr::Char  { span, .. }
        | Expr::Bool  { span, .. }
        | Expr::Str   { span, .. }
        | Expr::Var   { span, .. }
        | Expr::Unary { span, .. }
        | Expr::Binary{ span, .. }
        | Expr::Cast  { span, .. }
        | Expr::If    { span, .. }
        | Expr::Match { span, .. }
        | Expr::Call  { span, .. }
        | Expr::Block { span, .. }
        | Expr::Field { span, .. }
        | Expr::StructLit { span, .. } => *span,
    }
}

fn parse_int_expr(s: &str, span: Span) -> Result<Expr> {
    let v = s.parse::<i128>()?;
    if v >= i32::MIN as i128 && v <= i32::MAX as i128 {
        Ok(Expr::Int { value: v as i32, span })
    } else if v >= i64::MIN as i128 && v <= i64::MAX as i128 {
        Ok(Expr::Long { value: v as i64, span })
    } else {
        bail!("integer literal out of i64 range: {}", s);
    }
}

// 保留旧名：用于解析 Double（无后缀）
fn parse_float_lit(s: &str) -> Result<f64> { Ok(s.parse::<f64>()?) }

// 新增：解析 Float（带 f/F 后缀）
fn parse_float32_lit(s: &str) -> Result<f32> {
    let ns = s.strip_suffix('f')
        .or_else(|| s.strip_suffix('F'))
        .ok_or_else(|| anyhow!("invalid float32 literal suffix: {}", s))?;
    Ok(ns.parse::<f32>()?)
}

fn parse_long_lit(s: &str) -> Result<i64> {
    let ns = s.strip_suffix('L')
        .or_else(|| s.strip_suffix('l'))
        .ok_or_else(|| anyhow!("invalid long literal suffix: {}", s))?;
    Ok(ns.parse::<i64>()?)
}

fn parse_char_lit(s: &str) -> Result<u32> {
    let bytes = s.as_bytes();
    if bytes.len() < 3 || bytes[0] != b'\'' || bytes[bytes.len() - 1] != b'\'' {
        bail!("invalid char literal: {}", s);
    }
    let inner = &s[1..s.len() - 1];
    let ch = if inner.starts_with('\\') {
        let rest = &inner[1..];
        match rest {
            "'" => '\'',
            "\\" => '\\',
            "n" => '\n',
            "r" => '\r',
            "t" => '\t',
            "0" => '\0',
            _ => {
                if let Some(hex) = rest.strip_prefix("u{").and_then(|t| t.strip_suffix('}')) {
                    let v = u32::from_str_radix(hex, 16)
                        .map_err(|_| anyhow!("invalid unicode escape in char: {}", s))?;
                    char::from_u32(v).unwrap_or('\u{FFFD}')
                } else {
                    rest.chars().next().unwrap_or('\u{FFFD}')
                }
            }
        }
    } else {
        inner.chars().next().ok_or_else(|| anyhow!("empty char"))?
    };
    Ok(ch as u32)
}

fn unescape_string(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out = String::new();
    let mut i = 1; // skip opening "
    while i + 1 < bytes.len() {
        let c = bytes[i] as char;
        if c == '"' { break; }
        if c == '\\' {
            i += 1;
            let e = bytes[i] as char;
            match e {
                'n' => out.push('\n'),
                't' => out.push('\t'),
                '\\' => out.push('\\'),
                '"' => out.push('"'),
                'r' => out.push('\r'),
                '0' => out.push('\0'),
                _ => out.push(e),
            }
        } else {
            out.push(c);
        }
        i += 1;
    }
    out
}