package main

import (
    "fmt"
    "os"
    "runtime/pprof"
    "runtime"
)

var memProfile *string
var memProfileRate *int

func startMemProfile() {
	if memProfile != nil && *memProfile != "" &&  memProfileRate != nil && *memProfileRate > 0 {
		runtime.MemProfileRate = *memProfileRate
	}
}

func stopMemProfile() {
	if memProfile != nil && *memProfile != "" {
		f, err := os.Create(*memProfile)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Can not create mem profile output file: %s", err)
			return
		}
		if err = pprof.WriteHeapProfile(f); err != nil {
			fmt.Fprintf(os.Stderr, "Can not write %s: %s", *memProfile, err)
		}
		f.Close()
	}
}

func main() {
    memProfile = new(string)
    memProfileRate = new(int)

    *memProfile = "./mem_profile.dat"
    *memProfileRate = 512

    startMemProfile()
    for i := 0; i < 100000; i++ {
        str := new(string)
        *str = *memProfile
    }
    stopMemProfile()
}
