impl<'a> TyCk<'a> {
    /* ---------- 字面量辅助：Byte / Float / Double ---------- */

    /// 如果是整数字面量，取其值（用于常量范围判断）
    fn int_literal_value(&self, e: &Expr) -> Option<i128> {
        match e {
            Expr::Int  { value, .. } => Some(*value as i128),
            Expr::Long { value, .. } => Some(*value as i128),
            _ => None,
        }
    }

    /// “字面量是否可隐式收窄到 Byte(0..=255)”
    fn literal_fits_byte(&self, e: &Expr) -> bool {
        match self.int_literal_value(e) {
            Some(v) if v >= 0 && v <= 255 => true,
            _ => false,
        }
    }

    /// 是否是浮点字面量
    fn is_float_literal(&self, e: &Expr) -> bool {
        matches!(e, Expr::Float{..} | Expr::Double{..})
    }

    /// Double 字面量能否精确表示为 Float
    fn double_literal_exact_fits_f32(&self, e: &Expr) -> bool {
        if let Expr::Double { value: x, .. } = e {
            let v32 = *x as f32;
            (v32 as f64) == *x
        } else {
            false
        }
    }

    /// 通用放宽：字面量能否隐式“直接视作”目标类型
    fn literal_coerces_to(&self, expr: &Expr, dst: &Ty) -> bool {
        match dst {
            Ty::Byte   => self.literal_fits_byte(expr),
            Ty::Float  => matches!(expr, Expr::Float{..}) || self.double_literal_exact_fits_f32(expr),
            Ty::Double => self.is_float_literal(expr), // Float 或 Double 字面量都可提升到 Double
            _ => false,
        }
    }
}