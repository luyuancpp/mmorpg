package muduo

import (
	"client/pb/game"
	"testing"
	"time"
)

func TestClient(t *testing.T) {
	client, err := NewClient("127.0.0.1", 8000)
	if err != nil {
		panic(err)
	}

	defer func(client *Client) {
		err := client.Close()
		if err != nil {
			panic(err)
		}
	}(client)

	rq := &game.ClientRequest{Id: 1}

	for {
		client.Send(rq)
		time.Sleep(1 * time.Second)
	}
}
