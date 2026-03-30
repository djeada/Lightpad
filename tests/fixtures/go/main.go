package main

import (
	"fmt"
	"sort"
	"strings"
)

type WordFrequency struct {
	Word  string
	Count int
}

func countWords(text string) []WordFrequency {
	counts := make(map[string]int)
	for _, word := range strings.Fields(text) {
		word = strings.ToLower(word)
		counts[word]++
	}

	result := make([]WordFrequency, 0, len(counts))
	for word, count := range counts {
		result = append(result, WordFrequency{Word: word, Count: count})
	}

	sort.Slice(result, func(i, j int) bool {
		return result[i].Count > result[j].Count
	})

	return result
}

func main() {
	text := "hello world hello go world hello"
	frequencies := countWords(text)
	for _, wf := range frequencies {
		fmt.Printf("%s: %d\n", wf.Word, wf.Count)
	}
}
