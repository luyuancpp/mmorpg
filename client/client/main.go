package main

import (
	"net"
	"os"
	"time"
)

func main() {
	conn, err := net.Dial("tcp", os.Args[1])
	if err != nil {
		panic(err)
	}
	defer func(conn net.Conn) {
		err := conn.Close()
		if err != nil {

		}
	}(conn)
	for {
		time.Sleep(1000 * time.Millisecond)
	}
}
