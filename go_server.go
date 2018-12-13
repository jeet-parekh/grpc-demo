package main

import (
	"fmt"
	"io"
	"net"

	maths "./pb_go"
	"google.golang.org/grpc"
)

const (
	addr = "localhost:50051"
)

type MathsServer struct {
	server *grpc.Server
}

func newMathsServer() *MathsServer {
	return &MathsServer{
		server: grpc.NewServer(),
	}
}

func op(num1 float64, num2 float64, oper string) float64 {
	if oper == "+" {
		return num1 + num2
	} else if oper == "-" {
		return num1 - num2
	} else if oper == "*" {
		return num1 * num2
	} else if oper == "/" {
		return num1 / num2
	}
	return 0
}

func (s *MathsServer) DoMathStream(stream maths.Maths_DoMathStreamServer) error {
	var result float64
	var response maths.MathResponse
	for {
		n, err := stream.Recv()
		if err == io.EOF {
			return nil
		} else if err != nil {
			return err
		}

		result = op(n.Number1, n.Number2, n.Operation)
		response.Result = result
		if err := stream.Send(&response); err != nil {
			panic(err)
		}
	}
	return nil
}

func (s *MathsServer) Start() {
	lis, err := net.Listen("tcp", addr)
	if err != nil {
		panic(err)
	}
	maths.RegisterMathsServer(s.server, s)
	// Register reflection service on gRPC server.
	// reflection.Register(s)
	if err := s.server.Serve(lis); err != nil {
		panic(err)
	}
}

func main() {
	s := newMathsServer()
	fmt.Println("Server listening on " + addr)
	s.Start()
}
