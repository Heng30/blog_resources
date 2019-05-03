package main

import (
    "fmt"
    "os"
    "runtime/pprof"
)

var cpuProfile *string

func startCPUProfile() {
	if cpuProfile != nil && *cpuProfile != "" {
		f, err := os.Create(*cpuProfile)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Can not create cpu profile output file: %s",
				err)
			return
		}
		if err := pprof.StartCPUProfile(f); err != nil {
			fmt.Fprintf(os.Stderr, "Can not start cpu profile: %s", err)
			f.Close()
			return
		}
	}
}

func stopCPUProfile() {
	if cpuProfile != nil && *cpuProfile != "" {
		pprof.StopCPUProfile() // 把记录的概要信息写到已指定的文件
	}
}

func main() {
    cpuProfile = new(string)
    *cpuProfile = "./cpu_profile.dat"
    sum := 0
    startCPUProfile()
    for i := 0; i < 100000; i++ {
        sum += i
    }
    stopCPUProfile()
}
