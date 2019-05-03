package main

import (
    "fmt"
    "os"
    "runtime/pprof"
    "runtime"
)

var blockProfile *string
var blockProfileRate *int

func startBlockProfile() {
	if blockProfile != nil && *blockProfile != "" && blockProfileRate != nil && *blockProfileRate > 0 {
		runtime.SetBlockProfileRate(*blockProfileRate)
	}
}

func stopBlockProfile() {
	if blockProfile != nil && *blockProfile != "" && blockProfileRate != nil && *blockProfileRate >= 0 {
		f, err := os.Create(*blockProfile)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Can not create block profile output file: %s", err)
			return
		}

		if err = pprof.Lookup("block").WriteTo(f, 0); err != nil {
			fmt.Fprintf(os.Stderr, "Can not write %s: %s", *blockProfile, err)
		}

		f.Close()
	}
}

func main() {
    sum := 0
    blockProfile = new(string)
    blockProfileRate = new(int)

    *blockProfile = "./block_profile.dat"
    *blockProfileRate = 2

    startBlockProfile()
    for i := 0; i < 100000; i++ {
        sum += i
    }
    stopBlockProfile()
}
