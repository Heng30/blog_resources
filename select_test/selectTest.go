package main

import (
    "fmt"
)

func main() {
    count := 0
    chans := [3]chan int {
        make(chan int, 1),
        make(chan int ,1),
        make(chan int ,1),
    }

    go func() {
        for i := 0; i < 3; i++ {
            chans[i] <- i
            close(chans[i])
        }
    }()

    for {
        select {
        case elem, ok := <-chans[0]:
            if !ok { count++; chans[0] = nil; break }
            fmt.Printf("first chan: %d\n", elem)
        case elem, ok := <-chans[1]:
            if !ok { count++; chans[1] = nil; break }
            fmt.Printf("second chan: %d\n", elem)
        case elem, ok := <-chans[2]:
            if !ok { count++; chans[2] = nil;  break }
            fmt.Printf("third chan: %d\n", elem)
        // default:
        //     break
        }
        if 3 == count { break }
    }
}
