//! 性能优化工具模块
//! 提供缓存、池化、并行处理等性能优化功能

use std::sync::Arc;
use once_cell::sync::Lazy;
use dashmap::DashMap;

/// 全局字符串缓存，避免重复分配
pub static STRING_CACHE: Lazy<DashMap<String, Arc<str>>> = Lazy::new(|| {
    DashMap::with_capacity(1024)
});

/// 获取或插入缓存的字符串
pub fn intern_string(s: &str) -> Arc<str> {
    if let Some(cached) = STRING_CACHE.get(s) {
        cached.clone()
    } else {
        let arc_str: Arc<str> = Arc::from(s);
        STRING_CACHE.insert(s.to_string(), arc_str.clone());
        arc_str
    }
}

/// 对象池，用于重用昂贵的对象
pub struct ObjectPool<T> {
    objects: Vec<T>,
    factory: Box<dyn Fn() -> T + Send + Sync>,
}

impl<T> ObjectPool<T> {
    pub fn new<F>(factory: F) -> Self 
    where 
        F: Fn() -> T + Send + Sync + 'static
    {
        Self {
            objects: Vec::new(),
            factory: Box::new(factory),
        }
    }

    pub fn get(&mut self) -> T {
        self.objects.pop().unwrap_or_else(|| (self.factory)())
    }

    pub fn put(&mut self, obj: T) {
        self.objects.push(obj);
    }
}

/// 编译时缓存，避免重复计算
pub struct CompileCache<K, V> {
    cache: DashMap<K, V>,
    hits: std::sync::atomic::AtomicUsize,
    misses: std::sync::atomic::AtomicUsize,
}

impl<K, V> CompileCache<K, V> 
where 
    K: std::hash::Hash + Eq + Clone + Send + Sync,
    V: Clone + Send + Sync,
{
    pub fn new() -> Self {
        Self {
            cache: DashMap::new(),
            hits: std::sync::atomic::AtomicUsize::new(0),
            misses: std::sync::atomic::AtomicUsize::new(0),
        }
    }

    pub fn get_or_insert<F>(&self, key: K, f: F) -> V 
    where 
        F: FnOnce() -> V
    {
        if let Some(value) = self.cache.get(&key) {
            self.hits.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
            value.clone()
        } else {
            self.misses.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
            let value = f();
            self.cache.insert(key, value.clone());
            value
        }
    }

    pub fn stats(&self) -> (usize, usize) {
        (
            self.hits.load(std::sync::atomic::Ordering::Relaxed),
            self.misses.load(std::sync::atomic::Ordering::Relaxed)
        )
    }
}

/// 批量处理工具，减少函数调用开销
pub struct BatchProcessor<T> {
    batch: Vec<T>,
    batch_size: usize,
}

impl<T> BatchProcessor<T> {
    pub fn new(batch_size: usize) -> Self {
        Self {
            batch: Vec::with_capacity(batch_size),
            batch_size,
        }
    }

    pub fn add(&mut self, item: T) -> Option<Vec<T>> {
        self.batch.push(item);
        if self.batch.len() >= self.batch_size {
            Some(std::mem::replace(&mut self.batch, Vec::with_capacity(self.batch_size)))
        } else {
            None
        }
    }

    pub fn flush(&mut self) -> Vec<T> {
        std::mem::replace(&mut self.batch, Vec::with_capacity(self.batch_size))
    }
}

/// 性能计时器
pub struct Timer {
    start: std::time::Instant,
    name: &'static str,
}

impl Timer {
    pub fn new(name: &'static str) -> Self {
        Self {
            start: std::time::Instant::now(),
            name,
        }
    }
}

impl Drop for Timer {
    fn drop(&mut self) {
        let duration = self.start.elapsed();
        if duration.as_millis() > 0 {
            eprintln!("[PERF] {}: {}ms", self.name, duration.as_millis());
        } else {
            eprintln!("[PERF] {}: {}μs", self.name, duration.as_micros());
        }
    }
}

/// 内存使用统计
pub struct MemoryStats {
    pub allocated: usize,
    pub peak: usize,
}

impl MemoryStats {
    pub fn new() -> Self {
        Self {
            allocated: 0,
            peak: 0,
        }
    }

    pub fn allocate(&mut self, size: usize) {
        self.allocated += size;
        self.peak = self.peak.max(self.allocated);
    }

    pub fn deallocate(&mut self, size: usize) {
        self.allocated = self.allocated.saturating_sub(size);
    }
}

/// 全局内存统计
pub static MEMORY_STATS: Lazy<std::sync::Mutex<MemoryStats>> = Lazy::new(|| {
    std::sync::Mutex::new(MemoryStats::new())
});

#[macro_export]
macro_rules! perf_timer {
    ($name:expr) => {
        let _timer = $crate::utils::performance::Timer::new($name);
    };
}

#[macro_export]
macro_rules! memory_track {
    ($size:expr) => {
        if let Ok(mut stats) = $crate::utils::performance::MEMORY_STATS.lock() {
            stats.allocate($size);
        }
    };
}

