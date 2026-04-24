package main

import "testing"

func TestAddrsFromCSV(t *testing.T) {
	got := addrsFromCSV(" 127.0.0.1:1 , 127.0.0.1:2 ")
	if len(got) != 2 || got[0] != "127.0.0.1:1" {
		t.Fatalf("got %#v", got)
	}
	if addrsFromCSV("") != nil {
		t.Fatal("empty should be nil")
	}
}

func TestSameDataBackend(t *testing.T) {
	if !sameDataBackend([]string{"a:1"}, []string{"a:1"}, 0, 0) {
		t.Fatal("expected same")
	}
	if sameDataBackend([]string{"a:1"}, []string{"a:2"}, 0, 0) {
		t.Fatal("expected diff")
	}
	if sameDataBackend([]string{"a:1", "b:1"}, []string{"a:1"}, 0, 0) {
		t.Fatal("expected diff len")
	}
	if sameDataBackend([]string{"a:1"}, []string{"a:1"}, 0, 1) {
		t.Fatal("expected diff db")
	}
}
