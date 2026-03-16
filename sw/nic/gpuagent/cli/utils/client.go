//
// Copyright(C) Advanced Micro Devices, Inc. All rights reserved.
//
// You may not use this software and documentation (if any) (collectively,
// the "Materials") except in compliance with the terms and conditions of
// the Software License Agreement included with the Materials or otherwise as
// set forth in writing and signed by you and an authorized signatory of AMD.
// If you do not have a copy of the Software License Agreement, contact your
// AMD representative for a copy.
//
// You agree that you will not reverse engineer or decompile the Materials,
// in whole or in part, except as allowed by applicable law.
//
// THE MATERIALS ARE DISTRIBUTED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
// REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
//

package utils

import (
	"context"
	"fmt"
	"log"
	"math"
	"os"
	"strconv"
	"time"

	"google.golang.org/grpc"
)

var (
	GRPCDefaultBaseURL    = "127.0.0.1"
	GRPCDefaultPort       = "50061"
	GRPCDefaultSocketPath = "/var/run/gpuagent.sock"
)

const (
	GRPCDefaultPortConnTimeout = 30 // timeout in seconds
)

// timeout for grpc port conn
func getClientPortConnTimeout() (uint, error) {
	timeoutStr, present := os.LookupEnv("GRPC_CONN_TIMEOUT")
	if !present {
		return GRPCDefaultPortConnTimeout, nil
	}
	timeout, err := strconv.Atoi(timeoutStr)
	if err != nil || timeout < 0 {
		return 0, fmt.Errorf("Failed to parse GRPC_CONN_TIMEOUT(%s), err | %+v",
			timeoutStr, err)
	}
	return uint(timeout), nil
}

// timeout for rpc calls
func getClientReqTimeout() (uint, error) {
	timeoutStr, present := os.LookupEnv("GRPC_TIMEOUT")
	if !present {
		return 0, nil
	}
	timeout, err := strconv.Atoi(timeoutStr)
	if err != nil || timeout < 0 {
		return 0, fmt.Errorf("Failed to parse GRPC_TIMEOUT(%s), err | %+v",
			timeoutStr, err)
	}
	return uint(timeout), nil
}

// createNewGRPCClient creates a grpc connection to GPU agent
// supports both TCP/IP and Unix socket connections
func createNewGRPCClient() (*grpc.ClientConn, error) {
	var srvURL string

	// check if Unix socket path is specified
	if GRPCDefaultSocketPath != "" {
		// use Unix socket
		srvURL = "unix:" + GRPCDefaultSocketPath
	} else {
		// use TCP/IP
		agaPort := os.Getenv("AGA_GRPC_PORT")
		if agaPort == "" {
			agaPort = GRPCDefaultPort
		}
		srvURL = GRPCDefaultBaseURL + ":" + agaPort
	}

	timeout, err := getClientPortConnTimeout()
	if err != nil {
		return nil, err
	}

	ctxt, cancel := context.WithTimeout(context.Background(),
		time.Duration(timeout)*time.Second)
	defer cancel()

	var grpcOpts []grpc.DialOption
	grpcOpts = append(grpcOpts, grpc.WithMaxMsgSize(math.MaxInt32-1))
	grpcOpts = append(grpcOpts, grpc.WithInsecure())
	grpcOpts = append(grpcOpts, grpc.WithBlock())
	rpcClient, err := grpc.DialContext(ctxt, srvURL, grpcOpts...)
	if err != nil {
		log.Fatalf("Creating gRPC Client failed, server URL: %s, err %v",
			srvURL, err)
		return nil, err
	}
	return rpcClient, err
}

func CreateNewAGAGRPClient() (*grpc.ClientConn, context.Context,
	context.CancelFunc, error) {
	var ctxt context.Context
	var cancel context.CancelFunc

	client, err := createNewGRPCClient()
	if err != nil {
		return nil, nil, nil, err
	}
	timeout, err := getClientReqTimeout()
	if err != nil {
		return nil, nil, nil, err
	}
	if timeout != 0 {
		ctxt, cancel = context.WithTimeout(context.Background(),
			time.Duration(timeout)*time.Second)
	} else {
		ctxt, cancel = context.WithCancel(context.Background())
	}
	return client, ctxt, cancel, nil
}
