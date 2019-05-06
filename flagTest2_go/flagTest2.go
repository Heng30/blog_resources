package main

import (
	"flag"
	"fmt"
	"os"
)

var name string

func main() {
	var cmdLine = flag.NewFlagSet("", flag.ExitOnError)

	cmdLine.StringVar(&name, "name", "everyone", "The greeting object")
	cmdLine.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage of %s:\n", "question")
		cmdLine.PrintDefaults()
	}

	cmdLine.Parse(os.Args[1:])
	fmt.Printf("program name, %s\n", os.Args[0])
    fmt.Printf("name: %s\n", name)
}
