package main

import (
	"context"
	"sync"

	maths "./pb_go"
	"google.golang.org/grpc"
)

const (
	address      = "localhost:50051"
	messageCount = 1000000
)

func main() {
	// connect to the server
	conn, err := grpc.Dial(address, grpc.WithInsecure())
	if err != nil {
		panic(err)
	}
	defer conn.Close()

	// create a new client
	c := maths.NewMathsClient(conn)

	// rpc call for streaming
	stream, err := c.DoMathStream(context.Background())
	if err != nil {
		panic(err)
	}
	defer stream.CloseSend()

	wg := &sync.WaitGroup{}
	wg.Add(1)
	go func() {
		var resp *maths.MathResponse
		var err error
		for i := 0; i < messageCount; i++ {
			if resp, err = stream.Recv(); err != nil {
				panic(err)
			} else if resp.Result != 32 {
				panic("WRONG RESULT!")
			}
		}
		wg.Done()
	}()

	var request maths.MathRequest
	for i := 0; i < messageCount; i++ {
		request.Number1 = 64
		request.Number2 = 2
		request.Operation = "/"
		if err := stream.Send(&request); err != nil {
			panic(err)
		}
	}

	wg.Wait()
}
