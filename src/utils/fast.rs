// src/util/fast.rs
#![allow(dead_code)]

pub use ahash::RandomState as AHashBuilder;

// —— “默认快表” = hashbrown + AHash —— //
pub type FastMap<K, V> = hashbrown::HashMap<K, V, AHashBuilder>;
pub type FastSet<K>    = hashbrown::HashSet<K, AHashBuilder>;

// 常用构造（带初始容量，避免反复 rehash）
#[inline]
pub fn fast_map_with_cap<K, V>(cap: usize) -> FastMap<K, V> {
    FastMap::with_capacity_and_hasher(cap, AHashBuilder::default())
}
#[inline]
pub fn fast_set_with_cap<K>(cap: usize) -> FastSet<K> {
    FastSet::with_capacity_and_hasher(cap, AHashBuilder::default())
}

// —— “整型键”专用：零哈希（键本身即散列），适合 SymId/u32/u64 —— //
pub type IntBuild<K> = nohash_hasher::BuildNoHashHasher<K>;

pub type IntMap<K, V> = hashbrown::HashMap<K, V, IntBuild<K>>;
pub type IntSet<K>    = hashbrown::HashSet<K, IntBuild<K>>;

// —— 小向量别名：根据你的使用场景挑一个合适的内联容量 —— //
pub use smallvec::SmallVec;

// 常用模板：最多 2/4/8 个元素时不分配堆内存
pub type SmallVec2<T> = SmallVec<[T; 2]>;
pub type SmallVec4<T> = SmallVec<[T; 4]>;
pub type SmallVec8<T> = SmallVec<[T; 8]>;
