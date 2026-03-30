use std::collections::HashMap;

struct WordCounter {
    counts: HashMap<String, usize>,
}

impl WordCounter {
    fn new() -> Self {
        WordCounter {
            counts: HashMap::new(),
        }
    }

    fn count_words(&mut self, text: &str) {
        for word in text.split_whitespace() {
            let word = word.to_lowercase();
            *self.counts.entry(word).or_insert(0) += 1;
        }
    }

    fn top_words(&self, n: usize) -> Vec<(&String, &usize)> {
        let mut sorted: Vec<_> = self.counts.iter().collect();
        sorted.sort_by(|a, b| b.1.cmp(a.1));
        sorted.into_iter().take(n).collect()
    }
}

fn main() {
    let mut counter = WordCounter::new();
    counter.count_words("hello world hello rust world hello");
    for (word, count) in counter.top_words(3) {
        println!("{}: {}", word, count);
    }
}
