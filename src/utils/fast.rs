// src/util/fast.rs
#![allow(dead_code)]

//! “快表”工具集：默认用 `hashbrown + AHash`，整型键使用 `nohash-hasher` 零哈希。
//!
//! # Quick Start
//! ```rust
//! use crate::util::fast::*;
//!
//! // 默认快表：AHash
//! let mut m: FastMap<&str, i32> = fast_map();
//! m.insert("a", 1);
//!
//! // 预分配容量
//! let s: FastSet<i32> = fast_set_with_cap(64);
//!
//! // 整型键零哈希（Key 自身即散列）
//! let mut im: IntMap<u32, &str> = int_map();
//! im.insert(42, "answer");
//!
//! // 便捷宏
//! let m2 = fast_map! {
//!     "x" => 10,
//!     "y" => 20,
//! };
//! let s2 = fast_set![1, 2, 3, 3]; // 重复会被去重
//! ```

pub use ahash::RandomState as AHashBuilder;
pub use smallvec::SmallVec;

/// hashbrown + AHash
pub type FastMap<K, V> = hashbrown::HashMap<K, V, AHashBuilder>;
/// hashbrown + AHash
pub type FastSet<K>    = hashbrown::HashSet<K, AHashBuilder>;

/// 新建空 `FastMap`
#[inline]
pub fn fast_map<K, V>() -> FastMap<K, V> {
    FastMap::with_hasher(AHashBuilder::default())
}

/// 新建空 `FastSet`
#[inline]
pub fn fast_set<K>() -> FastSet<K> {
    FastSet::with_hasher(AHashBuilder::default())
}

/// 预分配容量的 `FastMap`
#[inline]
pub fn fast_map_with_cap<K, V>(cap: usize) -> FastMap<K, V> {
    FastMap::with_capacity_and_hasher(cap, AHashBuilder::default())
}

/// 预分配容量的 `FastSet`
#[inline]
pub fn fast_set_with_cap<K>(cap: usize) -> FastSet<K> {
    FastSet::with_capacity_and_hasher(cap, AHashBuilder::default())
}

/// `nohash-hasher` 的构建器（Key 自身即散列）
pub type IntBuild<K> = nohash_hasher::BuildNoHashHasher<K>;

/// 适用于 `u8/u16/u32/u64/usize` 等整型键
pub type IntMap<K, V> = hashbrown::HashMap<K, V, IntBuild<K>>;
/// 适用于 `u8/u16/u32/u64/usize` 等整型键
pub type IntSet<K>    = hashbrown::HashSet<K, IntBuild<K>>;

/// 新建空 `IntMap`
#[inline]
pub fn int_map<K, V>() -> IntMap<K, V> {
    IntMap::with_hasher(IntBuild::<K>::default())
}

/// 新建空 `IntSet`
#[inline]
pub fn int_set<K>() -> IntSet<K> {
    IntSet::with_hasher(IntBuild::<K>::default())
}

/// 预分配容量的 `IntMap`
#[inline]
pub fn int_map_with_cap<K, V>(cap: usize) -> IntMap<K, V> {
    IntMap::with_capacity_and_hasher(cap, IntBuild::<K>::default())
}

/// 预分配容量的 `IntSet`
#[inline]
pub fn int_set_with_cap<K>(cap: usize) -> IntSet<K> {
    IntSet::with_capacity_and_hasher(cap, IntBuild::<K>::default())
}

/// 常用模板：最多 2/4/8 个元素时不分配堆内存
pub type SmallVec2<T> = SmallVec<[T; 2]>;
pub type SmallVec4<T> = SmallVec<[T; 4]>;
pub type SmallVec8<T> = SmallVec<[T; 8]>;


#[macro_export]
macro_rules! fast_map {
    () => {{
        $crate::util::fast::fast_map()
    }};
    ($($k:expr => $v:expr),+ $(,)?) => {{
        let mut __m = $crate::util::fast::fast_map_with_cap::<_, _>({
            // 简单估计容量
            let mut __n = 0usize;
            $(let _ = & $k; __n += 1; )+
            __n
        });
        $( __m.insert($k, $v); )+
        __m
    }};
}


#[macro_export]
macro_rules! fast_set {
    () => {{
        $crate::util::fast::fast_set()
    }};
    ($($v:expr),+ $(,)?) => {{
        let mut __s = $crate::util::fast::fast_set_with_cap({
            let mut __n = 0usize;
            $(let _ = & $v; __n += 1; )+
            __n
        });
        $( __s.insert($v); )+
        __s
    }};
}

